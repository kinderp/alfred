#!/usr/bin/env bash

# Expected events.log contract:
# - compatibility semantic events are still emitted
# - one "output runtime stats" line is emitted at shutdown
# - enqueue_attempts is greater than 0
# - enqueue_attempts equals enqueue_success
# - enqueue_failures remains 0
# - drained_records equals enqueue_success
# - no JSONL output file is created by output_format=counter
#
# This scenario verifies output_format=counter as a runtime correctness path,
# not as a performance benchmark. Counter mode must use the same structured
# output boundary as JSONL:
#
#   app_emit_output_record()
#   -> app_enqueue_output_record()
#   -> alfred_record_output_pipeline_enqueue()
#   -> app_drain_output_pipeline()
#   -> alfred_record_runtime_drain_once()
#   -> alfred_record_dispatcher_dispatch_one()
#   -> alfred_record_counter_sink_emit()
#
# The sink counts records and returns success. It must not format JSONL, open
# output_log, write bytes, or allocate JSONL buffers. The config parser still
# validates output_buffer_size as a global output key, so this test supplies the
# minimum accepted value even though counter mode does not use that buffer at
# runtime. This keeps a stable end-to-end test for the queue/drain/dispatcher
# path without using the manual perf-runtime-output benchmark as the only oracle.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_output_counter_runtime}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./output-counter-runtime.conf"
OUTPUT_LOG="./output-counter-should-not-exist.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_output_counter_runtime() {
    stop_alfred
    rm -rf "$TEST_ROOT"

    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
        rm -f "$CONFIG_FILE" "$OUTPUT_LOG" ./raw.log ./events.log ./errors.log
    fi
}

fail_with_all_logs() {
    local message="$1"

    echo "FAIL: $message"
    echo "----- events.log -----"
    cat ./events.log || true
    echo "----- raw.log -----"
    cat ./raw.log || true
    echo "----- errors.log -----"
    cat ./errors.log || true
    exit 1
}

line_count() {
    local file="$1"

    if [[ ! -f "$file" ]]; then
        echo 0
        return
    fi

    wc -l < "$file"
}

wait_for_event_lines() {
    local file="$1"
    local expected="$2"
    local timeout_ms="$3"
    local elapsed_ms=0

    while (( elapsed_ms < timeout_ms )); do
        if (( $(line_count "$file") >= expected )); then
            return 0
        fi

        sleep 0.1
        elapsed_ms=$((elapsed_ms + 100))
    done

    return 1
}

stat_value() {
    local line="$1"
    local key="$2"

    printf '%s\n' "$line" |
        sed -n "s/.*${key}=\([0-9][0-9]*\).*/\1/p"
}

assert_positive_stat() {
    local line="$1"
    local key="$2"
    local value

    value="$(stat_value "$line" "$key")"

    if [[ -z "$value" ]]; then
        fail_with_all_logs "missing stat field: $key"
    fi

    if (( value <= 0 )); then
        fail_with_all_logs "$key must be greater than 0, got $value"
    fi
}

assert_equal_stats() {
    local line="$1"
    local left="$2"
    local right="$3"
    local left_value
    local right_value

    left_value="$(stat_value "$line" "$left")"
    right_value="$(stat_value "$line" "$right")"

    if [[ -z "$left_value" || -z "$right_value" ]]; then
        fail_with_all_logs "missing stat fields: $left or $right"
    fi

    if [[ "$left_value" != "$right_value" ]]; then
        fail_with_all_logs "$left must equal $right ($left_value != $right_value)"
    fi
}

trap cleanup_output_counter_runtime EXIT

reset_env
rm -f "$OUTPUT_LOG"

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=counter
# The parser validates this global output key before app.c chooses the concrete
# sink. Counter mode does not allocate or use the JSONL output buffer.
output_buffer_size=8192
output_log=$OUTPUT_LOG
EOF

LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}detect_leaks=0" \
ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

sleep 1

printf "counter runtime\n" > "$TEST_ROOT/counter-file.txt"
mkdir "$TEST_ROOT/counter-dir"

if ! wait_for_event_lines ./events.log 4 10000; then
    fail_with_all_logs "timed out waiting for compatibility events"
fi

kill -INT "$ALFRED_PID" 2>/dev/null || true

set +e
wait "$ALFRED_PID"
ALFRED_STATUS=$?
set -e
ALFRED_PID=""

if [[ "$ALFRED_STATUS" != "0" ]]; then
    fail_with_all_logs "Alfred exited with status $ALFRED_STATUS"
fi

if [[ -s ./errors.log ]]; then
    fail_with_all_logs "counter runtime must not write errors"
fi

if [[ -e "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "counter runtime must not create output_log"
fi

if ! grep -Eq "FILE_CREATED path=.*/counter-file.txt" ./events.log; then
    fail_with_all_logs "missing compatibility FILE_CREATED event"
fi

if ! grep -Eq "DIR_CREATED path=.*/counter-dir" ./events.log; then
    fail_with_all_logs "missing compatibility DIR_CREATED event"
fi

STATS_LINE="$(grep -E "output runtime stats " ./events.log | tail -n 1 || true)"
if [[ -z "$STATS_LINE" ]]; then
    fail_with_all_logs "missing output runtime stats line"
fi

assert_positive_stat "$STATS_LINE" "enqueue_attempts"
assert_equal_stats "$STATS_LINE" "enqueue_attempts" "enqueue_success"
assert_equal_stats "$STATS_LINE" "enqueue_success" "drained_records"

ENQUEUE_FAILURES="$(stat_value "$STATS_LINE" "enqueue_failures")"
if [[ -z "$ENQUEUE_FAILURES" ]]; then
    fail_with_all_logs "missing stat field: enqueue_failures"
fi
if (( ENQUEUE_FAILURES != 0 )); then
    fail_with_all_logs "enqueue_failures must stay 0, got $ENQUEUE_FAILURES"
fi

echo "PASSED: output counter runtime drains without JSONL output"
