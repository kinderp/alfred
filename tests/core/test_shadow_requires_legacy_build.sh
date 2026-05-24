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
    echo "FAIL: shadow mode succeeded without legacy build"
    echo "----- startup.log -----"
    cat "$STARTUP_LOG" || true
    echo "----- errors.log -----"
    cat ./errors.log || true
    exit 1
fi

if ! grep -Eq "startup failed" "$STARTUP_LOG"; then
    echo "FAIL: missing startup failure on stderr"
    echo "----- startup.log -----"
    cat "$STARTUP_LOG" || true
    exit 1
fi

if ! grep -Eq "shadow event engine requires build" ./errors.log; then
    echo "FAIL: missing explicit shadow build error"
    echo "----- errors.log -----"
    cat ./errors.log || true
    exit 1
fi

echo "PASS shadow requires legacy build"
