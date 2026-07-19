#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - one normalized raw RAW_CREATE record exists for file-jsonl.txt with
#   raw_mask=1
# - one semantic FILE_CREATED record exists for file-jsonl.txt
# - one normalized raw RAW_CREATE record exists for dir-jsonl with raw_mask=257
# - one semantic DIR_CREATED record exists for dir-jsonl
# - one diagnostic WATCH_ADDED record exists for dir-jsonl
#
# Compatibility logs expected in parallel:
# - raw.log still contains RAW_CREATE lines for file-jsonl.txt and dir-jsonl
# - events.log still contains FILE_CREATED, DIR_CREATED and WATCH_ADDED
#
# Meaning:
# This is the first small JSONL golden end-to-end scenario. It does not replace
# the text-log tests. It fixes the external structured contract for a basic file
# creation and a basic directory creation while proving that text compatibility
# output remains present.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_create_file_and_dir}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-golden.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_golden() {
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

trap cleanup_jsonl_golden EXIT

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

printf "hello jsonl\n" > "$TEST_ROOT/file-jsonl.txt"
mkdir "$TEST_ROOT/dir-jsonl"

sleep 1
stop_alfred
ALFRED_PID=""

if ! grep -Eq "RAW_CREATE path=.*/file-jsonl.txt mask=1" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_CREATE for file-jsonl.txt"
fi

if ! grep -Eq "RAW_CREATE path=.*/dir-jsonl mask=257" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_CREATE for dir-jsonl"
fi

if ! grep -Eq "FILE_CREATED path=.*/file-jsonl.txt" ./events.log; then
    fail_with_all_logs "missing compatibility FILE_CREATED for file-jsonl.txt"
fi

if ! grep -Eq "DIR_CREATED path=.*/dir-jsonl" ./events.log; then
    fail_with_all_logs "missing compatibility DIR_CREATED for dir-jsonl"
fi

if ! grep -Eq "WATCH_ADDED wd=[0-9]+ path=.*/dir-jsonl" ./events.log; then
    fail_with_all_logs "missing compatibility WATCH_ADDED for dir-jsonl"
fi

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]

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


def has_record(layer, category, record_type, path_suffix, raw_mask=None):
    expected_path = f"{root}/{path_suffix}"
    for record in records:
        if (
            record.get("layer") == layer
            and record.get("category") == category
            and record.get("type") == record_type
            and record.get("path") == expected_path
        ):
            if raw_mask is not None and record.get("raw_mask") != raw_mask:
                continue
            return True
    return False


required = [
    ("normalized_raw", "filesystem", "RAW_CREATE", "file-jsonl.txt", 1),
    ("semantic", "filesystem", "FILE_CREATED", "file-jsonl.txt", None),
    ("normalized_raw", "filesystem", "RAW_CREATE", "dir-jsonl", 257),
    ("semantic", "filesystem", "DIR_CREATED", "dir-jsonl", None),
    ("diagnostic", "watch", "WATCH_ADDED", "dir-jsonl", None),
]

missing = [
    f"{layer}/{category}/{record_type}/{path_suffix}"
    for layer, category, record_type, path_suffix, raw_mask in required
    if not has_record(layer, category, record_type, path_suffix, raw_mask)
]

if missing:
    raise SystemExit("missing JSONL records: " + ", ".join(missing))

for record in records:
    if (
        record.get("layer") == "semantic"
        and record.get("category") == "filesystem"
        and record.get("type") in {"FILE_CREATED", "DIR_CREATED"}
    ):
        for forbidden in ("old_path", "new_path"):
            if forbidden in record:
                raise SystemExit(
                    f"{record['type']} contains non-applicable {forbidden}: "
                    f"{record[forbidden]!r}"
                )
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden create file and directory"
