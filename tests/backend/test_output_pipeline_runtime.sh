#!/usr/bin/env bash

# Expected output.jsonl contract:
# - one JSONL raw record for created-file.txt
# - one JSONL raw record for created-dir
# - one JSONL semantic FILE_CREATED record for created-file.txt
# - one JSONL semantic DIR_CREATED record for created-dir
# - one JSONL diagnostic WATCH_ADDED record for removed-dir
# - one JSONL diagnostic WATCH_STALE record for removed-dir
# - one JSONL diagnostic WATCH_STALE_EVENT_DROPPED record for removed-dir
# - one JSONL diagnostic WATCH_REMOVED record for removed-dir
# - raw.log compatibility lines are still present
# - events.log compatibility lines are still present
# - output_enabled=true with output_format=text is rejected at runtime
# - output_enabled=true with too-small output_buffer_size is rejected
# - output_enabled=true stops Alfred on runtime JSONL writer failure
#
# This test proves the first runtime wiring of the single-writer output
# pipeline. output_enabled=true is additive: app.c still emits the compatibility
# raw.log/events.log text records, and also enqueues the same adapted
# alfred_record_t values into alfred_record_output_pipeline_t for JSONL output.
# When JSONL output is explicitly enabled, writer failure is fatal: Alfred must
# not keep processing events while producing an incomplete output.jsonl ledger.

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

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=4096
output_log=$OUTPUT_LOG
EOF

if ALFRED_CONFIG="$CONFIG_FILE" \
   ALFRED_EVENT_ENGINE=core \
       "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1; then
    echo "FAIL: output_enabled=true must reject too-small output_buffer_size"
    exit 1
fi

reset_env

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=8192
output_log=/dev/full
EOF

ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

sleep 1

LONG_NAME="$(printf 'x%.0s' $(seq 1 180))"
for i in $(seq 1 600); do
    printf "hello %s\n" "$i" > "$TEST_ROOT/fill-jsonl-$i-$LONG_NAME.txt" || true
done

for _ in $(seq 1 30); do
    if ! kill -0 "$ALFRED_PID" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

if kill -0 "$ALFRED_PID" 2>/dev/null; then
    echo "FAIL: JSONL writer failure must stop Alfred"
    echo "----- errors.log -----"
    cat ./errors.log || true
    kill -TERM "$ALFRED_PID" 2>/dev/null || true
    wait "$ALFRED_PID" 2>/dev/null || true
    ALFRED_PID=""
    exit 1
fi

set +e
wait "$ALFRED_PID"
ALFRED_STATUS=$?
set -e
ALFRED_PID=""

if [[ "$ALFRED_STATUS" == "0" ]]; then
    echo "FAIL: JSONL writer failure must return a non-zero status"
    echo "----- errors.log -----"
    cat ./errors.log || true
    exit 1
fi

if ! grep -Eq "failed to emit output record|structured output failed" ./errors.log; then
    echo "FAIL: missing JSONL writer failure diagnostic"
    echo "----- errors.log -----"
    cat ./errors.log || true
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
mkdir "$TEST_ROOT/removed-dir"
sleep 1
rmdir "$TEST_ROOT/removed-dir"
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

if ! grep -Eq "WATCH_ADDED wd=[0-9]+ path=.*/removed-dir" ./events.log; then
    echo "FAIL: missing compatibility WATCH_ADDED for removed directory"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

if ! grep -Eq "WATCH_STALE wd=[0-9]+ path=.*/removed-dir reason=IN_DELETE_SELF" ./events.log; then
    echo "FAIL: missing compatibility WATCH_STALE for removed directory"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

if ! grep -Eq "WATCH_STALE_EVENT_DROPPED wd=[0-9]+ path=.*/removed-dir mask=.*IN_IGNORED .* name=" ./events.log; then
    echo "FAIL: missing compatibility WATCH_STALE_EVENT_DROPPED for removed directory"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

if ! grep -Eq "WATCH_REMOVED wd=[0-9]+ path=.*/removed-dir" ./events.log; then
    echo "FAIL: missing compatibility WATCH_REMOVED for removed directory"
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

if ! grep -Eq '"category":"watch".*"type":"WATCH_ADDED".*"path":".*/removed-dir"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL WATCH_ADDED record for removed directory"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if ! grep -Eq '"category":"watch".*"type":"WATCH_STALE".*"path":".*/removed-dir".*"reason":"IN_DELETE_SELF"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL WATCH_STALE record for removed directory"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if ! grep -Eq '"category":"watch".*"type":"WATCH_STALE_EVENT_DROPPED".*"path":".*/removed-dir".*"event_mask":"[^"]*IN_IGNORED[^"]*"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL WATCH_STALE_EVENT_DROPPED record for removed directory"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

if ! grep -Eq '"category":"watch".*"type":"WATCH_REMOVED".*"path":".*/removed-dir"' "$OUTPUT_LOG"; then
    echo "FAIL: missing JSONL WATCH_REMOVED record for removed directory"
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
fi

echo "PASS backend output pipeline runtime"
