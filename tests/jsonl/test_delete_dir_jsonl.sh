#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - one normalized raw RAW_CREATE record exists for delete-dir with raw_mask=257
# - one normalized raw RAW_DELETE record exists for delete-dir with raw_mask=258
# - one semantic DIR_CREATED record exists for delete-dir
# - one semantic DIR_DELETED record exists for delete-dir
# - one diagnostic WATCH_ADDED record exists for delete-dir
# - one diagnostic WATCH_REMOVED record exists for delete-dir
# - no semantic FILE_DELETED record exists for this directory operation
#
# Compatibility logs expected in parallel:
# - raw.log still contains RAW_CREATE and RAW_DELETE with the directory bit
# - events.log still contains DIR_CREATED, DIR_DELETED, WATCH_ADDED and
#   WATCH_REMOVED
# - events.log must not contain FILE_DELETED for this operation
#
# Meaning:
# This golden test fixes the structured delete-directory contract. Creating a
# directory produces both a filesystem semantic event and a diagnostic watch
# installation record. Removing the same directory later produces DIR_DELETED
# and a separate WATCH_REMOVED diagnostic when the kernel/backend removes the
# now-invalid recursive watch. WATCH_REMOVED is not a second semantic delete:
# it describes backend state cleanup, while DIR_DELETED describes the
# filesystem object lifecycle.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_delete_dir}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-delete-dir.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_delete_dir() {
    stop_alfred
    rm -rf "$TEST_ROOT"

    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
        rm -f "$CONFIG_FILE" "$OUTPUT_LOG" ./raw.log ./events.log ./errors.log
    fi
}

fail_with_all_logs() {
    local message="$1"

    echo "FAIL: $message"
    echo "----- raw.log -----"
    cat ./raw.log || true
    echo "----- events.log -----"
    cat ./events.log || true
    echo "----- errors.log -----"
    cat ./errors.log || true
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
}

assert_log_count() {
    local pattern="$1"
    local expected="$2"
    local file="$3"
    local message="$4"
    local actual

    actual=$(grep -Ec "$pattern" "$file" || true)
    if [[ "$actual" != "$expected" ]]; then
        fail_with_all_logs "$message: expected $expected, got $actual"
    fi
}

trap cleanup_jsonl_delete_dir EXIT

reset_env
: > "$OUTPUT_LOG"

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$OUTPUT_LOG
EOF

ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

sleep 1

mkdir "$TEST_ROOT/delete-dir"
sleep 1
rmdir "$TEST_ROOT/delete-dir"
sleep 1

stop_alfred
ALFRED_PID=""

assert_log_count "RAW_CREATE path=.*/delete-dir mask=257" 1 ./raw.log \
    "wrong compatibility RAW_CREATE count for delete-dir"
assert_log_count "RAW_DELETE path=.*/delete-dir mask=258" 1 ./raw.log \
    "wrong compatibility RAW_DELETE count for delete-dir"
assert_log_count "DIR_CREATED path=.*/delete-dir" 1 ./events.log \
    "wrong compatibility DIR_CREATED count for delete-dir"
assert_log_count "DIR_DELETED path=.*/delete-dir" 1 ./events.log \
    "wrong compatibility DIR_DELETED count for delete-dir"
assert_log_count "WATCH_ADDED wd=[0-9]+ path=.*/delete-dir" 1 ./events.log \
    "wrong compatibility WATCH_ADDED count for delete-dir"
assert_log_count "WATCH_REMOVED wd=[0-9]+ path=.*/delete-dir" 1 ./events.log \
    "wrong compatibility WATCH_REMOVED count for delete-dir"

if grep -Eq "^FILE_DELETED " ./events.log; then
    fail_with_all_logs "directory delete unexpectedly produced FILE_DELETED"
fi

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]
path = f"{root}/delete-dir"

records = []
with open(output_log, "r", encoding="utf-8") as handle:
    for line_number, line in enumerate(handle, 1):
        line = line.rstrip("\n")
        if not line:
            raise SystemExit(f"empty JSONL line at {line_number}")
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            raise SystemExit(f"invalid JSONL line {line_number}: {exc}") from exc
        if record.get("schema_version") != 0:
            raise SystemExit(
                f"line {line_number} has unexpected schema_version="
                f"{record.get('schema_version')!r}"
            )
        records.append(record)


def count_records(layer, category, record_type, raw_mask=None):
    count = 0
    for record in records:
        if (
            record.get("layer") == layer
            and record.get("category") == category
            and record.get("type") == record_type
            and record.get("path") == path
        ):
            if raw_mask is not None and record.get("raw_mask") != raw_mask:
                continue
            count += 1
    return count


expected_counts = [
    ("normalized_raw", "filesystem", "RAW_CREATE", 257, 1),
    ("normalized_raw", "filesystem", "RAW_DELETE", 258, 1),
    ("semantic", "filesystem", "DIR_CREATED", None, 1),
    ("semantic", "filesystem", "DIR_DELETED", None, 1),
    ("diagnostic", "watch", "WATCH_ADDED", None, 1),
    ("diagnostic", "watch", "WATCH_REMOVED", None, 1),
]

failures = []
for layer, category, record_type, raw_mask, expected in expected_counts:
    actual = count_records(layer, category, record_type, raw_mask)
    if actual != expected:
        failures.append(
            f"{layer}/{category}/{record_type} expected {expected}, got {actual}"
        )

file_deleted = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("category") == "filesystem"
    and record.get("type") == "FILE_DELETED"
]
if file_deleted:
    failures.append(f"unexpected FILE_DELETED records: {len(file_deleted)}")

if failures:
    raise SystemExit("; ".join(failures))
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden delete directory"
