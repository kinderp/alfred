#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - normalized raw RAW_CREATE records exist for src, dst and src/before with
#   raw_mask=257
# - normalized raw RAW_MOVED_FROM exists for src/before with raw_mask=288
# - normalized raw RAW_MOVED_TO exists for dst/after with raw_mask=320
# - RAW_MOVED_FROM and RAW_MOVED_TO expose a non-zero matching cookie
# - one semantic DIR_RELOCATED record exists with old_path=src/before and
#   new_path=dst/after
# - no semantic DIR_MOVED or DIR_RENAMED record exists for the same operation
#
# Compatibility logs expected in parallel:
# - raw.log still contains RAW_CREATE, RAW_MOVED_FROM and RAW_MOVED_TO
# - events.log still contains DIR_CREATED and DIR_RELOCATED
# - events.log must not contain DIR_MOVED or DIR_RENAMED for this operation
#
# Meaning:
# This golden test fixes the structured contract for moving and renaming a
# directory in one operation. At the core semantic level, changing both parent
# directory and basename is a relocation, not a pure move and not a pure rename.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_dir_relocated}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-dir-relocated.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_dir_relocated() {
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

trap cleanup_jsonl_dir_relocated EXIT

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

mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
sleep 1

mkdir "$TEST_ROOT/src/before"
sleep 1
mv "$TEST_ROOT/src/before" "$TEST_ROOT/dst/after"
sleep 1

stop_alfred
ALFRED_PID=""

if ! grep -Eq "RAW_CREATE path=.*/src mask=257" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_CREATE for src"
fi

if ! grep -Eq "RAW_CREATE path=.*/dst mask=257" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_CREATE for dst"
fi

if ! grep -Eq "RAW_CREATE path=.*/src/before mask=257" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_CREATE for src/before"
fi

if ! grep -Eq "RAW_MOVED_FROM path=.*/src/before mask=288 cookie=[1-9][0-9]*" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_MOVED_FROM for src/before"
fi

if ! grep -Eq "RAW_MOVED_TO path=.*/dst/after mask=320 cookie=[1-9][0-9]*" ./raw.log; then
    fail_with_all_logs "missing compatibility RAW_MOVED_TO for dst/after"
fi

if ! grep -Eq "DIR_CREATED path=.*/src/before" ./events.log; then
    fail_with_all_logs "missing compatibility DIR_CREATED for src/before"
fi

if ! grep -Eq "DIR_RELOCATED from=.*/src/before to=.*/dst/after" ./events.log; then
    fail_with_all_logs "missing compatibility DIR_RELOCATED for relocation"
fi

if grep -Eq "DIR_MOVED from=.*/src/before to=.*/dst/after" ./events.log; then
    fail_with_all_logs "relocation unexpectedly produced DIR_MOVED"
fi

if grep -Eq "DIR_RENAMED from=.*/src/before to=.*/dst/after" ./events.log; then
    fail_with_all_logs "relocation unexpectedly produced DIR_RENAMED"
fi

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]
src_path = f"{root}/src"
dst_path = f"{root}/dst"
old_path = f"{root}/src/before"
new_path = f"{root}/dst/after"

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


for path in (src_path, dst_path, old_path):
    raw_create = find_record("normalized_raw", "filesystem", "RAW_CREATE", path)
    if raw_create.get("raw_mask") != 257:
        raise SystemExit(
            f"RAW_CREATE path={path} raw_mask={raw_create.get('raw_mask')!r}"
        )

raw_from = find_record("normalized_raw", "filesystem", "RAW_MOVED_FROM", old_path)
raw_to = find_record("normalized_raw", "filesystem", "RAW_MOVED_TO", new_path)

if raw_from.get("raw_mask") != 288:
    raise SystemExit(f"RAW_MOVED_FROM raw_mask={raw_from.get('raw_mask')!r}")
if raw_to.get("raw_mask") != 320:
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

relocations = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("category") == "filesystem"
    and record.get("type") == "DIR_RELOCATED"
    and record.get("old_path") == old_path
    and record.get("new_path") == new_path
]
if len(relocations) != 1:
    raise SystemExit(
        "expected exactly one semantic DIR_RELOCATED record with "
        f"expected old_path/new_path, found {len(relocations)}"
    )

wrong_semantics = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("category") == "filesystem"
    and record.get("type") in {"DIR_MOVED", "DIR_RENAMED"}
    and record.get("old_path") == old_path
    and record.get("new_path") == new_path
]
if wrong_semantics:
    raise SystemExit("directory relocation produced move/rename semantics")
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden directory relocation"
