#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_self_events}"
MOVE_TARGET="${MOVE_TARGET:-/tmp/alfred_backend_test_self_events_moved}"

source ../core/test_lib.sh
trap 'rm -rf "$MOVE_TARGET"; cleanup' EXIT

rm -rf "$MOVE_TARGET"
start_alfred_core

touch "$TEST_ROOT/file-before-delete.txt"
mkdir "$TEST_ROOT/dir-before-delete"
sleep 1

rm -rf "$TEST_ROOT"
sleep 1

if ! grep -Eq "IN_DELETE_SELF .*path=.*/alfred_backend_test_self_events name=" \
    ./raw.log; then

    echo "FAIL: missing IN_DELETE_SELF for watched root"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_IGNORED .*path=.*/alfred_backend_test_self_events name=" \
    ./raw.log; then

    echo "FAIL: missing IN_IGNORED after watched root delete"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_DELETE_SELF"
assert_contains "WATCH_REMOVED wd=[0-9]+ path=.*/alfred_backend_test_self_events$"
assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_DELETE_SELF" \
             "WATCH_REMOVED wd=[0-9]+ path=.*/alfred_backend_test_self_events$"

assert_contains "FILE_DELETED path=.*/file-before-delete.txt"
assert_contains "DIR_DELETED path=.*/dir-before-delete"

assert_order "DIR_DELETED path=.*/dir-before-delete" \
             "FILE_DELETED path=.*/file-before-delete.txt"

stop_alfred
reset_env

touch "$TEST_ROOT/file-before-move.txt"
mkdir "$TEST_ROOT/dir-before-move"

ALFRED_EVENT_ENGINE=core "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!
sleep 1

mv "$TEST_ROOT" "$MOVE_TARGET"
sleep 1

if ! grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_self_events name=" \
    ./raw.log; then

    echo "FAIL: missing IN_MOVE_SELF for moved watched root"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF"
assert_not_contains "DIR_RELOCATED path="
assert_not_contains "DIR_MOVED path="
assert_not_contains "DIR_RENAMED path="

if grep -Eq "FILE_DELETED path=.*/file-before-move.txt" "$LOG_FILE"; then
    echo "FAIL: root move unexpectedly produced child file delete"
    echo "----- events.log -----"
    cat "$LOG_FILE" || true
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if grep -Eq "DIR_DELETED path=.*/dir-before-move" "$LOG_FILE"; then
    echo "FAIL: root move unexpectedly produced child dir delete"
    echo "----- events.log -----"
    cat "$LOG_FILE" || true
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

assert_not_contains "core seq="

echo "PASS backend self events root watch"
