#!/usr/bin/env bash

# Expected events.log contract:
# - one "output runtime stats" line is emitted at shutdown
# - enqueue_attempts equals enqueue_success
# - enqueue_failures remains 0
# - pressure_drains is greater than 0
# - drained_records equals enqueue_success
# - max_pending reaches the current bounded queue capacity during the burst
#
# This scenario intentionally creates more events than the current runtime
# output queue can hold during one backend poll. inotify_backend_poll() drains a
# single kernel read buffer before returning to app_run(), so a tight file burst
# can enqueue many records before the normal app_run() drain point executes.
#
# The test protects the v0 pressure-relief contract:
#
#   app_emit_output_record()
#   -> app_enqueue_output_record()
#      -> alfred_record_output_pipeline_enqueue()
#      -> app_drain_output_pipeline() when the queue is full
#      -> alfred_record_output_pipeline_enqueue() retry
#
# This is not the final asynchronous writer design. It is a single-threaded
# safety valve that keeps the queue bounded while preventing legitimate bursts
# from becoming output failures before the future worker thread exists.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_output_queue_pressure}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./output-pressure.conf"
OUTPUT_LOG="./output-pressure.jsonl"
FILES="${FILES:-1200}"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_output_queue_pressure() {
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

trap cleanup_output_queue_pressure EXIT

reset_env
: > "$OUTPUT_LOG"

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$OUTPUT_LOG
EOF

# LSAN can report process_status=1 in restricted test environments even when
# Alfred shutdown is clean and errors.log is empty. This scenario verifies
# writer-runtime queue counters, not leak diagnostics, so it follows the runtime
# performance benchmark and disables leak detection only for this child process.
LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}detect_leaks=0" \
ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

sleep 1

for i in $(seq 1 "$FILES"); do
    printf "queue pressure %s\n" "$i" > "$TEST_ROOT/file-$i.txt"
done

if ! wait_for_event_lines ./events.log "$((FILES * 3))" 15000; then
    fail_with_all_logs "timed out waiting for file lifecycle events"
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

STATS_LINE="$(grep -E "output runtime stats " ./events.log | tail -n 1 || true)"
if [[ -z "$STATS_LINE" ]]; then
    fail_with_all_logs "missing output runtime stats line"
fi

assert_equal_stats "$STATS_LINE" "enqueue_attempts" "enqueue_success"
assert_equal_stats "$STATS_LINE" "enqueue_success" "drained_records"

ENQUEUE_FAILURES="$(stat_value "$STATS_LINE" "enqueue_failures")"
if [[ -z "$ENQUEUE_FAILURES" ]]; then
    fail_with_all_logs "missing stat field: enqueue_failures"
fi
if (( ENQUEUE_FAILURES != 0 )); then
    fail_with_all_logs "enqueue_failures must stay 0, got $ENQUEUE_FAILURES"
fi

assert_positive_stat "$STATS_LINE" "pressure_drains"

MAX_PENDING="$(stat_value "$STATS_LINE" "max_pending")"
if [[ -z "$MAX_PENDING" ]]; then
    fail_with_all_logs "missing stat field: max_pending"
fi
if (( MAX_PENDING < 1024 )); then
    fail_with_all_logs "max_pending must reach the current queue capacity, got $MAX_PENDING"
fi

JSONL_LINES="$(line_count "$OUTPUT_LOG")"
if (( JSONL_LINES != $(stat_value "$STATS_LINE" "drained_records") )); then
    fail_with_all_logs "output.jsonl lines must match drained_records"
fi

echo "PASSED: output queue pressure drains bounded bursts"
