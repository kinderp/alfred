#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

reset_env

STARTUP_LOG="$TEST_ROOT/startup.log"

set +e
ALFRED_EVENT_ENGINE=shadow "$ALFRED_BIN" "$TEST_ROOT" \
    >"$STARTUP_LOG" 2>&1
status=$?
set -e

if [[ "$status" == "0" ]]; then
    echo "FAIL: invalid shadow event engine succeeded"
    echo "----- startup.log -----"
    cat "$STARTUP_LOG" || true
    exit 1
fi

if ! grep -Eq "invalid ALFRED_EVENT_ENGINE=shadow \\(expected core\\)" \
    "$STARTUP_LOG"; then

    echo "FAIL: missing invalid shadow event engine message"
    echo "----- startup.log -----"
    cat "$STARTUP_LOG" || true
    exit 1
fi

echo "PASS invalid shadow event engine"
