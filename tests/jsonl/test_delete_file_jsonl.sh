#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - one normalized raw RAW_CREATE record exists for delete-me.txt with raw_mask=1
# - one normalized raw RAW_MODIFY record exists for delete-me.txt with raw_mask=4
# - one normalized raw RAW_CLOSE_WRITE record exists for delete-me.txt with
#   raw_mask=16
# - one normalized raw RAW_DELETE record exists for delete-me.txt with raw_mask=2
# - one semantic FILE_CREATED record exists for delete-me.txt
# - one semantic FILE_MODIFIED record exists for delete-me.txt
# - one semantic FILE_READY record exists for delete-me.txt
# - one semantic FILE_DELETED record exists for delete-me.txt
# - no semantic DIR_DELETED record exists for this file operation
#
# Compatibility logs expected in parallel:
# - raw.log still contains RAW_CREATE, RAW_MODIFY, RAW_CLOSE_WRITE and RAW_DELETE
# - events.log still contains FILE_CREATED, FILE_MODIFIED, FILE_READY and
#   FILE_DELETED
#
# Meaning:
# This golden test fixes the structured delete-file contract. The file is first
# written, so Alfred records the create/modify/ready lifecycle, then the later
# rm operation produces RAW_DELETE and FILE_DELETED. The test keeps file delete
# separate from directory delete by rejecting DIR_DELETED.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_delete_file}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-delete-file.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_delete_file() {
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

trap cleanup_jsonl_delete_file EXIT

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

printf "temporary\n" > "$TEST_ROOT/delete-me.txt"
sleep 1
rm "$TEST_ROOT/delete-me.txt"
sleep 1

stop_alfred
ALFRED_PID=""

assert_log_count "RAW_CREATE path=.*/delete-me.txt mask=1" 1 ./raw.log \
    "wrong compatibility RAW_CREATE count for delete-me.txt"
assert_log_count "RAW_MODIFY path=.*/delete-me.txt mask=4" 1 ./raw.log \
    "wrong compatibility RAW_MODIFY count for delete-me.txt"
assert_log_count "RAW_CLOSE_WRITE path=.*/delete-me.txt mask=16" 1 ./raw.log \
    "wrong compatibility RAW_CLOSE_WRITE count for delete-me.txt"
assert_log_count "RAW_DELETE path=.*/delete-me.txt mask=2" 1 ./raw.log \
    "wrong compatibility RAW_DELETE count for delete-me.txt"
assert_log_count "FILE_CREATED path=.*/delete-me.txt" 1 ./events.log \
    "wrong compatibility FILE_CREATED count for delete-me.txt"
assert_log_count "FILE_MODIFIED path=.*/delete-me.txt" 1 ./events.log \
    "wrong compatibility FILE_MODIFIED count for delete-me.txt"
assert_log_count "FILE_READY path=.*/delete-me.txt" 1 ./events.log \
    "wrong compatibility FILE_READY count for delete-me.txt"
assert_log_count "FILE_DELETED path=.*/delete-me.txt" 1 ./events.log \
    "wrong compatibility FILE_DELETED count for delete-me.txt"

if grep -Eq "^DIR_DELETED " ./events.log; then
    fail_with_all_logs "file delete unexpectedly produced DIR_DELETED"
fi

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]
path = f"{root}/delete-me.txt"

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
    ("normalized_raw", "filesystem", "RAW_CREATE", 1, 1),
    ("normalized_raw", "filesystem", "RAW_MODIFY", 4, 1),
    ("normalized_raw", "filesystem", "RAW_CLOSE_WRITE", 16, 1),
    ("normalized_raw", "filesystem", "RAW_DELETE", 2, 1),
    ("semantic", "filesystem", "FILE_CREATED", None, 1),
    ("semantic", "filesystem", "FILE_MODIFIED", None, 1),
    ("semantic", "filesystem", "FILE_READY", None, 1),
    ("semantic", "filesystem", "FILE_DELETED", None, 1),
]

failures = []
for layer, category, record_type, raw_mask, expected in expected_counts:
    actual = count_records(layer, category, record_type, raw_mask)
    if actual != expected:
        failures.append(
            f"{layer}/{category}/{record_type} expected {expected}, got {actual}"
        )

dir_deleted = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("category") == "filesystem"
    and record.get("type") == "DIR_DELETED"
]
if dir_deleted:
    failures.append(f"unexpected DIR_DELETED records: {len(dir_deleted)}")

if failures:
    raise SystemExit("; ".join(failures))
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden delete file"
