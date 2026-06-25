#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in the helper output is valid JSON
# - the checked record uses schema_version=0
# - one normalized raw RAW_OVERFLOW record exists
# - the overflow record has raw_mask=128
# - the overflow record has path="" because IN_Q_OVERFLOW is global
#
# Meaning:
# This test does not try to force a real kernel queue overflow. A real
# IN_Q_OVERFLOW depends on timing, queue pressure, and host configuration, so it
# would be unstable in CI. Instead, the C helper builds a synthetic
# IN_Q_OVERFLOW inotify event and passes it through the same backend bridge,
# raw-record adapter, and JSONL formatter that Alfred uses for the structured
# contract.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/jsonl"
HELPER="$BUILD_DIR/alfred_test_overflow_raw_jsonl"
OUTPUT_LOG="./output.jsonl"

mkdir -p "$BUILD_DIR"

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I"$ROOT_DIR/app/include" \
    -I"$ROOT_DIR/core/include" \
    -I"$ROOT_DIR/modules/inotify/include" \
    -I"$ROOT_DIR/core/src" \
    "$ROOT_DIR/tests/jsonl/test_overflow_raw_jsonl.c" \
    "$ROOT_DIR/app/src/fs_scanner.c" \
    "$ROOT_DIR/app/src/logger.c" \
    "$ROOT_DIR/app/src/utils.c" \
    "$ROOT_DIR/core/src/alfred_correlator.c" \
    "$ROOT_DIR/core/src/alfred_record_adapter.c" \
    "$ROOT_DIR/core/src/alfred_record_diagnostic.c" \
    "$ROOT_DIR/core/src/alfred_record_jsonl.c" \
    "$ROOT_DIR/core/src/alfred_record_sink.c" \
    "$ROOT_DIR/core/src/alfred_record_text.c" \
    "$ROOT_DIR/core/src/alfred_record_text_sink.c" \
    "$ROOT_DIR/core/src/alfred_tables.c" \
    "$ROOT_DIR/core/src/alfred_utils.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_adapter.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_config.c" \
    "$ROOT_DIR/modules/inotify/src/watch_manager.c" \
    "$ROOT_DIR/modules/inotify/src/watcher.c" \
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
    "layer": "normalized_raw",
    "category": "filesystem",
    "type": "RAW_OVERFLOW",
    "source": 1,
    "raw_mask": 128,
    "path": "",
}

failures = []
for key, value in expected.items():
    if record.get(key) != value:
        failures.append(f"{key}: expected {value!r}, got {record.get(key)!r}")

unexpected_fields = sorted(set(record) - set(expected) - {"ts_ns"})
if unexpected_fields:
    failures.append(f"unexpected fields: {','.join(unexpected_fields)}")

if failures:
    raise SystemExit("; ".join(failures))
PY
then
    echo "FAIL: JSONL overflow validation failed"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
    rm -f "$OUTPUT_LOG"
fi

echo "PASS jsonl golden raw overflow"
