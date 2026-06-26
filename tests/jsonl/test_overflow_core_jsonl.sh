#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in the helper output is valid JSON
# - the checked record uses schema_version=0
# - one semantic OVERFLOW record exists
# - the record has no path/old_path/new_path fields because overflow is global
#
# Meaning:
# This test does not try to force a real kernel queue overflow. The companion
# raw-overflow JSONL test already validates the backend bridge. This scenario
# feeds ALFRED_RAW_OVERFLOW directly into the core and verifies that the
# semantic OVERFLOW event can be represented as public JSONL.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/jsonl"
HELPER="$BUILD_DIR/alfred_test_overflow_core_jsonl"
OUTPUT_LOG="./output.jsonl"

mkdir -p "$BUILD_DIR"

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I"$ROOT_DIR/core/include" \
    "$ROOT_DIR/tests/jsonl/test_overflow_core_jsonl.c" \
    "$ROOT_DIR/core/src/alfred_correlator.c" \
    "$ROOT_DIR/core/src/alfred_record_adapter.c" \
    "$ROOT_DIR/core/src/alfred_record_jsonl.c" \
    "$ROOT_DIR/core/src/alfred_tables.c" \
    "$ROOT_DIR/core/src/alfred_utils.c" \
    -o "$HELPER"

"$HELPER" > "$OUTPUT_LOG"

if [[ ! -s "$OUTPUT_LOG" ]]; then
    echo "FAIL: output.jsonl is empty"
    exit 1
fi

if ! python3 - "$OUTPUT_LOG" <<'PY'
import json
import sys

output_log = sys.argv[1]

with open(output_log, "r", encoding="utf-8") as handle:
    lines = [line.rstrip("\n") for line in handle]

if len(lines) != 1:
    raise SystemExit(f"expected exactly one JSONL line, got {len(lines)}")

if not lines[0]:
    raise SystemExit("empty JSONL line")

try:
    record = json.loads(lines[0])
except json.JSONDecodeError as exc:
    raise SystemExit(f"invalid JSONL: {exc}") from exc

expected = {
    "schema_version": 0,
    "seq": 1,
    "layer": "semantic",
    "category": "filesystem",
    "type": "OVERFLOW",
    "pid": 123,
}

failures = []
for key, value in expected.items():
    if record.get(key) != value:
        failures.append(f"{key}: expected {value!r}, got {record.get(key)!r}")

for forbidden in ("path", "old_path", "new_path", "raw_mask"):
    if forbidden in record:
        failures.append(f"unexpected {forbidden} field: {record[forbidden]!r}")

unexpected_fields = sorted(set(record) - set(expected) - {"ts_ns"})
if unexpected_fields:
    failures.append(f"unexpected fields: {','.join(unexpected_fields)}")

if failures:
    raise SystemExit("; ".join(failures))
PY
then
    echo "FAIL: JSONL core overflow validation failed"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
    rm -f "$OUTPUT_LOG"
fi

echo "PASS jsonl golden core overflow"
