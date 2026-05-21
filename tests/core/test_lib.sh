#!/usr/bin/env bash

set -euo pipefail

ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
TEST_ROOT="${TEST_ROOT:-/tmp/alfred_core_test}"
LOG_FILE="${LOG_FILE:-./events.log}"

ALFRED_PID=""

reset_env() {
    rm -rf "$TEST_ROOT"
    mkdir -p "$TEST_ROOT"
    : > "$LOG_FILE"
    : > ./raw.log
    : > ./errors.log
}

start_alfred_core() {
    reset_env

    ALFRED_EVENT_ENGINE=core "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
    ALFRED_PID=$!

    sleep 1
}

stop_alfred() {
    if [[ -n "$ALFRED_PID" ]] && kill -0 "$ALFRED_PID" 2>/dev/null; then
        kill -INT "$ALFRED_PID"
        wait "$ALFRED_PID" || true
    fi
}

cleanup() {
    stop_alfred
    rm -rf "$TEST_ROOT"
    rm -f "$LOG_FILE" ./raw.log ./errors.log
}

fail_with_log() {
    local message="$1"

    echo "FAIL: $message"
    echo "----- events.log -----"
    cat "$LOG_FILE" || true
    exit 1
}

assert_contains() {
    local pattern="$1"

    if ! grep -Eq "$pattern" "$LOG_FILE"; then
        fail_with_log "missing pattern: $pattern"
    fi
}

assert_not_contains() {
    local pattern="$1"

    if grep -Eq "$pattern" "$LOG_FILE"; then
        fail_with_log "unexpected pattern: $pattern"
    fi
}

assert_order() {
    local first="$1"
    local second="$2"
    local first_line
    local second_line

    first_line=$(grep -En "$first" "$LOG_FILE" | head -n 1 | cut -d: -f1 || true)
    second_line=$(grep -En "$second" "$LOG_FILE" | head -n 1 | cut -d: -f1 || true)

    if [[ -z "$first_line" || -z "$second_line" ]]; then
        fail_with_log "cannot check order: $first before $second"
    fi

    if (( first_line >= second_line )); then
        fail_with_log "wrong order: $first must appear before $second"
    fi
}
