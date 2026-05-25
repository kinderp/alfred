#!/usr/bin/env bash

set -euo pipefail

ALFRED_BIN="../../alfred"

TEST_ROOT="/tmp/alfred_test"
LOG_FILE="./events.log"

ALFRED_PID=""

reset_env() {
    rm -rf "$TEST_ROOT"
    mkdir -p "$TEST_ROOT"
    : > "$LOG_FILE"
}

start_alfred() {
    reset_env

    echo "tests/functional is a historical legacy/shadow suite." >&2
    echo "It is not runnable after legacy shadow removal; use make test." >&2
    exit 1

    ALFRED_EVENT_ENGINE=shadow "$ALFRED_BIN" "$TEST_ROOT" > "$LOG_FILE" 2>&1 &
    ALFRED_PID=$!

    sleep 1
}

stop_alfred() {
    if kill -0 "$ALFRED_PID" 2>/dev/null; then
        kill -INT "$ALFRED_PID"
        wait "$ALFRED_PID" || true
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
