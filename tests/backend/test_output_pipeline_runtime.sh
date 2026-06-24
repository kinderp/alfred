#!/usr/bin/env bash

# Expected output.jsonl contract:
# - one JSONL raw record for created-file.txt
# - one JSONL raw record for created-dir
# - one JSONL semantic FILE_CREATED record for created-file.txt
# - one JSONL semantic DIR_CREATED record for created-dir
# - raw.log compatibility lines are still present
# - events.log compatibility lines are still present
# - output_enabled=true with output_format=text is rejected at runtime
#
# This test proves the first runtime wiring of the single-writer output
# pipeline. output_enabled=true is additive: app.c still emits the compatibility
# raw.log/events.log text records, and also enqueues the same adapted
# alfred_record_t values into alfred_record_output_pipeline_t for JSONL output.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_output_pipeline_runtime}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./output-runtime.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_output_runtime() {
    stop_alfred
    rm -rf "$TEST_ROOT"

    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
        rm -f "$CONFIG_FILE" "$OUTPUT_LOG" ./raw.log ./events.log ./errors.log
    fi
}

trap cleanup_output_runtime EXIT

reset_env

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=text
output_buffer_size=65536
output_log=$OUTPUT_LOG
EOF

if ALFRED_CONFIG="$CONFIG_FILE" \
   ALFRED_EVENT_ENGINE=core \
       "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1; then
    echo "FAIL: output_enabled=true must reject output_format=text for now"
    exit 1
fi

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

printf "hello\n" > "$TEST_ROOT/created-file.txt"
mkdir "$TEST_ROOT/created-dir"
sleep 1

stop_alfred
ALFRED_PID=""

if ! grep -Eq "RAW_CREATE path=.*/created-file.txt mask=1" ./raw.log; then
    echo "FAIL: missing compatibility RAW_CREATE for created file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_CREATE path=.*/created-dir mask=257" ./raw.log; then
    echo "FAIL: missing compatibility RAW_CREATE for created directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "FILE_CREATED path=.*/created-file.txt" ./events.log; then
    echo "FAIL: missing compatibility FILE_CREATED for created file"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

if ! grep -Eq "DIR_CREATED path=.*/created-dir" ./events.log; then
    echo "FAIL: missing compatibility DIR_CREATED for created directory"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

if ! grep -Eq '"category":"filesystem".*"type":"RAW_CREATE".*"path":".*/created-file.txt"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL create record for created file"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if ! grep -Eq '"category":"filesystem".*"type":"RAW_CREATE".*"path":".*/created-dir"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL create record for created directory"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if ! grep -Eq '"category":"filesystem".*"type":"FILE_CREATED".*"path":".*/created-file.txt"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL FILE_CREATED record for created file"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if ! grep -Eq '"category":"filesystem".*"type":"DIR_CREATED".*"path":".*/created-dir"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL DIR_CREATED record for created directory"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

echo "PASS backend output pipeline runtime"
