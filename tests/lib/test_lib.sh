#!/usr/bin/env bash

set -euo pipefail

FSMON_BIN="../../fsmon"

TEST_ROOT="/tmp/fsmon_test"
LOG_FILE="./events.log"

FSMON_PID=""

reset_env() {
    rm -rf "$TEST_ROOT"
    mkdir -p "$TEST_ROOT"
    : > "$LOG_FILE"
}

start_fsmon() {
    reset_env

    "$FSMON_BIN" "$TEST_ROOT" > "$LOG_FILE" 2>&1 &
    FSMON_PID=$!

    sleep 1
}

stop_fsmon() {
    if kill -0 "$FSMON_PID" 2>/dev/null; then
        kill -INT "$FSMON_PID"
        wait "$FSMON_PID" || true
    fi
}

assert_contains() {
    local pattern="$1"

    if ! grep -q "$pattern" "$LOG_FILE"; then
        echo "❌ ASSERT FAILED: $pattern"
        echo "----- LOG -----"
        cat "$LOG_FILE"
        exit 1
    fi
}

assert_not_contains() {
    local pattern="$1"

    if grep -q "$pattern" "$LOG_FILE"; then
        echo "❌ UNEXPECTED FOUND: $pattern"
        exit 1
    fi
}

cleanup() {
    rm -rf "$TEST_ROOT"
    rm -f "$LOG_FILE"
}
