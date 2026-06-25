#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - one normalized raw RAW_CREATE record exists for old-jsonl.txt
#   with raw_mask=1
# - one normalized raw RAW_MOVED_FROM record exists for old-jsonl.txt with
#   raw_mask=32
# - one normalized raw RAW_MOVED_TO record exists for new-jsonl.txt with
#   raw_mask=64
# - RAW_MOVED_FROM and RAW_MOVED_TO expose a non-zero matching cookie
# - one semantic FILE_RENAMED record exists with old_path=old-jsonl.txt and
#   new_path=new-jsonl.txt
#
# Compatibility logs expected in parallel:
# - raw.log still contains RAW_CREATE, RAW_MOVED_FROM and RAW_MOVED_TO
# - events.log still contains FILE_CREATED and FILE_RENAMED
#
# Meaning:
# This golden test fixes the structured contract for the first correlated
# filesystem operation. JSONL must expose both raw move halves with their cookie
# and the single semantic rename produced by the core.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_rename_file}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-rename.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_rename() {
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

trap cleanup_jsonl_rename EXIT

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

printf "hello rename jsonl\n" > "$TEST_ROOT/old-jsonl.txt"
sleep 1
mv "$TEST_ROOT/old-jsonl.txt" "$TEST_ROOT/new-jsonl.txt"
sleep 1

stop_alfred
ALFRED_PID=""

if ! grep -Eq "RAW_CREATE path=.*/old-jsonl.txt mask=1" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_CREATE for old-jsonl.txt"
fi

if ! grep -Eq "RAW_MOVED_FROM path=.*/old-jsonl.txt mask=32 cookie=[1-9][0-9]*" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_MOVED_FROM for old-jsonl.txt"
fi

if ! grep -Eq "RAW_MOVED_TO path=.*/new-jsonl.txt mask=64 cookie=[1-9][0-9]*" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_MOVED_TO for new-jsonl.txt"
fi

if ! grep -Eq "FILE_CREATED path=.*/old-jsonl.txt" ./events.log; then
    fail_with_all_logs "missing compatibility FILE_CREATED for old-jsonl.txt"
fi

if ! grep -Eq "FILE_RENAMED from=.*/old-jsonl.txt to=.*/new-jsonl.txt" ./events.log; then
    fail_with_all_logs "missing compatibility FILE_RENAMED for rename"
fi

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]
old_path = f"{root}/old-jsonl.txt"
new_path = f"{root}/new-jsonl.txt"

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


def find_record(layer, category, record_type, path):
    matches = [
        record
        for record in records
        if record.get("layer") == layer
        and record.get("category") == category
        and record.get("type") == record_type
        and record.get("path") == path
    ]
    if not matches:
        raise SystemExit(
            f"missing JSONL record {layer}/{category}/{record_type}/{path}"
        )
    return matches[0]


raw_create = find_record("normalized_raw", "filesystem", "RAW_CREATE", old_path)
raw_from = find_record("normalized_raw", "filesystem", "RAW_MOVED_FROM", old_path)
raw_to = find_record("normalized_raw", "filesystem", "RAW_MOVED_TO", new_path)

if raw_create.get("raw_mask") != 1:
    raise SystemExit(f"RAW_CREATE raw_mask={raw_create.get('raw_mask')!r}")
if raw_from.get("raw_mask") != 32:
    raise SystemExit(f"RAW_MOVED_FROM raw_mask={raw_from.get('raw_mask')!r}")
if raw_to.get("raw_mask") != 64:
    raise SystemExit(f"RAW_MOVED_TO raw_mask={raw_to.get('raw_mask')!r}")

from_cookie = raw_from.get("cookie")
to_cookie = raw_to.get("cookie")
if not isinstance(from_cookie, int) or from_cookie <= 0:
    raise SystemExit(f"RAW_MOVED_FROM has invalid cookie {from_cookie!r}")
if from_cookie != to_cookie:
    raise SystemExit(
        f"move cookie mismatch: RAW_MOVED_FROM={from_cookie!r} "
        f"RAW_MOVED_TO={to_cookie!r}"
    )

renames = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("category") == "filesystem"
    and record.get("type") == "FILE_RENAMED"
    and record.get("old_path") == old_path
    and record.get("new_path") == new_path
]
if not renames:
    raise SystemExit(
        "missing semantic FILE_RENAMED record with expected old_path/new_path"
    )
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden rename file"
