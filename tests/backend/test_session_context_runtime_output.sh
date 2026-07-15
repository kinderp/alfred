#!/usr/bin/env bash

# Expected SESSION_CONTEXT runtime contract:
# - with output_enabled=true and no workspace/session fields configured, Alfred
#   must not emit a decorative SESSION_CONTEXT record;
# - with at least one workspace/session field configured, Alfred emits exactly
#   one diagnostic/lifecycle SESSION_CONTEXT record per run;
# - the record is enqueued before startup targets produce watch or filesystem
#   records, so it appears before ordinary filesystem records in output.jsonl;
# - filesystem records must not gain workspace or ledger payload fields.
#
# This test checks runtime wiring, not the JSONL formatter itself. Formatter
# golden cases live in test_record_jsonl.c. Here the important path is:
#
#   app_init_workspace_session_context()
#   -> app_init_output_pipeline()
#   -> app_emit_session_context_record()
#   -> app_emit_output_record()
#   -> app_enqueue_output_record()
#   -> alfred_record_output_pipeline_enqueue()
#   -> alfred_record_queue_push()
#   -> app_run()
#   -> app_drain_output_pipeline()
#   -> alfred_record_runtime_drain_once()
#   -> alfred_record_dispatcher_dispatch_one()
#   -> alfred_record_jsonl_writer_emit()

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_session_context_runtime}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./session-context-runtime.conf"
OUTPUT_LOG="./session-context-output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_session_context_runtime() {
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
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
}

wait_for_log_pattern() {
    local file="$1"
    local pattern="$2"
    local timeout_ms="$3"
    local elapsed_ms=0

    while (( elapsed_ms < timeout_ms )); do
        if [[ -f "$file" ]] && grep -Eq "$pattern" "$file"; then
            return 0
        fi

        sleep 0.1
        elapsed_ms=$((elapsed_ms + 100))
    done

    return 1
}

wait_for_event_pattern() {
    local pattern="$1"
    local message="$2"

    if ! wait_for_log_pattern ./events.log "$pattern" 10000; then
        fail_with_all_logs "$message"
    fi
}

stop_and_expect_success() {
    kill -INT "$ALFRED_PID" 2>/dev/null || true

    set +e
    wait "$ALFRED_PID"
    local status=$?
    set -e
    ALFRED_PID=""

    if [[ "$status" != "0" ]]; then
        fail_with_all_logs "Alfred exited with status $status"
    fi

    if [[ -s ./errors.log ]]; then
        fail_with_all_logs "runtime must not write errors"
    fi
}

assert_jsonl_session_count() {
    local expected="$1"
    local actual

    actual="$(grep -Ec '"type":"SESSION_CONTEXT"' "$OUTPUT_LOG" || true)"
    if [[ "$actual" != "$expected" ]]; then
        fail_with_all_logs \
            "expected $expected SESSION_CONTEXT records, got $actual"
    fi
}

assert_no_filesystem_enrichment() {
    if grep -E '"category":"filesystem"' "$OUTPUT_LOG" |
       grep -Eq '"workspace"|"ledger"'; then
        fail_with_all_logs \
            "filesystem records must not contain workspace or ledger payloads"
    fi
}

trap cleanup_session_context_runtime EXIT

reset_env
: > "$OUTPUT_LOG"

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$OUTPUT_LOG
EOF

LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}detect_leaks=0" \
ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

wait_for_event_pattern "application startup complete" \
    "timed out waiting for Alfred startup without session context"

printf "without session context\n" > "$TEST_ROOT/no-session-file.txt"
wait_for_event_pattern "FILE_CREATED path=.*/no-session-file.txt" \
    "timed out waiting for no-session FILE_CREATED"

stop_and_expect_success

assert_jsonl_session_count 0
assert_no_filesystem_enrichment

reset_env
: > "$OUTPUT_LOG"

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$OUTPUT_LOG
workspace_root=$TEST_ROOT
workspace_id=ws-runtime-test
ledger_session_id=ledger-runtime-test
EOF

LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}detect_leaks=0" \
ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

wait_for_event_pattern "application startup complete" \
    "timed out waiting for Alfred startup with session context"

printf "with session context\n" > "$TEST_ROOT/session-file.txt"
wait_for_event_pattern "FILE_CREATED path=.*/session-file.txt" \
    "timed out waiting for session FILE_CREATED"

stop_and_expect_success

assert_jsonl_session_count 1

if ! grep -Eq \
    '"type":"SESSION_CONTEXT".*"workspace":\{"root":"[^"]*alfred_backend_test_session_context_runtime","id":"ws-runtime-test"\}.*"ledger":\{"session_id":"ledger-runtime-test"\}' \
    "$OUTPUT_LOG"; then
    fail_with_all_logs "missing complete SESSION_CONTEXT JSONL payload"
fi

SESSION_LINE="$(grep -En '"type":"SESSION_CONTEXT"' "$OUTPUT_LOG" |
    head -n 1 | cut -d: -f1 || true)"
FILESYSTEM_LINE="$(grep -En '"category":"filesystem"' "$OUTPUT_LOG" |
    head -n 1 | cut -d: -f1 || true)"

if [[ -z "$SESSION_LINE" || -z "$FILESYSTEM_LINE" ]]; then
    fail_with_all_logs "cannot compare SESSION_CONTEXT and filesystem order"
fi

if (( SESSION_LINE >= FILESYSTEM_LINE )); then
    fail_with_all_logs \
        "SESSION_CONTEXT must appear before ordinary filesystem records"
fi

assert_no_filesystem_enrichment

echo "PASSED: session context runtime output"
