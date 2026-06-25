#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - one normalized raw RAW_ATTRIB record exists for metadata.txt with raw_mask=8
# - no semantic filesystem event exists for the chmod operation
#
# Compatibility logs expected in parallel:
# - raw.log still contains the backend IN_ATTRIB line
# - raw.log still contains the normalized RAW_ATTRIB line
# - events.log does not grow after chmod
#
# Meaning:
# chmod changes file metadata, not file contents. The test creates metadata.txt
# before Alfred starts, so startup does not observe a file creation or write.
# Alfred keeps the later IN_ATTRIB visible as a normalized raw/backend fact, but
# the core does not define a public semantic metadata event yet. This golden
# test locks down that boundary: RAW_ATTRIB is part of JSONL v0, while
# FILE_MODIFIED, FILE_READY and future metadata semantics must not appear until
# the core contract explicitly changes.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_attrib_raw}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-attrib-raw.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_attrib_raw() {
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

trap cleanup_jsonl_attrib_raw EXIT

reset_env
: > "$OUTPUT_LOG"
printf "metadata\n" > "$TEST_ROOT/metadata.txt"

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

events_before_chmod=$(wc -l < ./events.log)

chmod 600 "$TEST_ROOT/metadata.txt"
sleep 1

events_after_chmod=$(wc -l < ./events.log)

stop_alfred
ALFRED_PID=""

if [[ "$events_after_chmod" != "$events_before_chmod" ]]; then
    fail_with_all_logs "chmod produced unexpected semantic or diagnostic events"
fi

if ! grep -Eq "IN_ATTRIB .*name=metadata.txt" ./raw.log; then

    fail_with_all_logs "missing compatibility IN_ATTRIB backend raw log"
fi

assert_log_count "RAW_ATTRIB path=.*/metadata.txt mask=8" 1 ./raw.log \
    "wrong compatibility RAW_ATTRIB count for metadata.txt"

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]
path = f"{root}/metadata.txt"

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


failures = []

attrib_count = count_records("normalized_raw", "filesystem", "RAW_ATTRIB", 8)
if attrib_count != 1:
    failures.append(
        f"normalized_raw/filesystem/RAW_ATTRIB expected 1, got {attrib_count}"
    )

for forbidden_type in (
    "FILE_CREATED",
    "FILE_MODIFIED",
    "FILE_READY",
    "FILE_ATTRIB",
    "FILE_METADATA_CHANGED",
    "DIR_METADATA_CHANGED",
):
    actual = count_records("semantic", "filesystem", forbidden_type)
    if actual != 0:
        failures.append(f"unexpected {forbidden_type} records: {actual}")

if failures:
    raise SystemExit("; ".join(failures))
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden attrib raw"
