#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - one normalized raw RAW_CREATE record exists for editable.txt with raw_mask=1
# - two normalized raw RAW_MODIFY records exist for editable.txt with raw_mask=4
# - two normalized raw RAW_CLOSE_WRITE records exist for editable.txt with
#   raw_mask=16
# - one semantic FILE_CREATED record exists for editable.txt
# - two semantic FILE_MODIFIED records exist for editable.txt
# - two semantic FILE_READY records exist for editable.txt
#
# Compatibility logs expected in parallel:
# - raw.log still contains RAW_CREATE, RAW_MODIFY and RAW_CLOSE_WRITE
# - events.log still contains FILE_CREATED, FILE_MODIFIED and FILE_READY
#
# Meaning:
# This golden test fixes the structured write lifecycle contract. Inotify emits
# IN_MODIFY while bytes are written and IN_CLOSE_WRITE when the writer closes the
# file. Alfred keeps those raw facts separate and the core maps them to
# FILE_MODIFIED and FILE_READY. FILE_READY is therefore not a duplicate of
# FILE_MODIFIED: it means the write side has closed the file.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_modify_file}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-modify-file.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_modify_file() {
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

trap cleanup_jsonl_modify_file EXIT

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

printf "initial\n" > "$TEST_ROOT/editable.txt"
sleep 1
printf "second\n" >> "$TEST_ROOT/editable.txt"
sleep 1

stop_alfred
ALFRED_PID=""

assert_log_count "RAW_CREATE path=.*/editable.txt mask=1" 1 ./raw.log \
    "wrong compatibility RAW_CREATE count for editable.txt"
assert_log_count "RAW_MODIFY path=.*/editable.txt mask=4" 2 ./raw.log \
    "wrong compatibility RAW_MODIFY count for editable.txt"
assert_log_count "RAW_CLOSE_WRITE path=.*/editable.txt mask=16" 2 ./raw.log \
    "wrong compatibility RAW_CLOSE_WRITE count for editable.txt"
assert_log_count "FILE_CREATED path=.*/editable.txt" 1 ./events.log \
    "wrong compatibility FILE_CREATED count for editable.txt"
assert_log_count "FILE_MODIFIED path=.*/editable.txt" 2 ./events.log \
    "wrong compatibility FILE_MODIFIED count for editable.txt"
assert_log_count "FILE_READY path=.*/editable.txt" 2 ./events.log \
    "wrong compatibility FILE_READY count for editable.txt"

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]
path = f"{root}/editable.txt"

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
    ("normalized_raw", "filesystem", "RAW_MODIFY", 4, 2),
    ("normalized_raw", "filesystem", "RAW_CLOSE_WRITE", 16, 2),
    ("semantic", "filesystem", "FILE_CREATED", None, 1),
    ("semantic", "filesystem", "FILE_MODIFIED", None, 2),
    ("semantic", "filesystem", "FILE_READY", None, 2),
]

failures = []
for layer, category, record_type, raw_mask, expected in expected_counts:
    actual = count_records(layer, category, record_type, raw_mask)
    if actual != expected:
        failures.append(
            f"{layer}/{category}/{record_type} expected {expected}, got {actual}"
        )

if failures:
    raise SystemExit("; ".join(failures))
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden modify file"
