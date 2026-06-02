#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_identity_match}"
MOVE_TARGET="${MOVE_TARGET:-/tmp/alfred_backend_test_identity_match_moved}"

source ../core/test_lib.sh

cleanup_identity_match() {
    if [[ -n "$ALFRED_PID" ]] && kill -0 "$ALFRED_PID" 2>/dev/null; then
        kill -CONT "$ALFRED_PID" 2>/dev/null || true
    fi

    rm -rf "$MOVE_TARGET"
    cleanup
}

trap cleanup_identity_match EXIT

rm -rf "$MOVE_TARGET"
start_alfred_core

mkdir "$TEST_ROOT/original-child"
sleep 1

# SIGSTOP freezes Alfred before it can drain the inotify queue. This lets the
# test move the watched root away and then move the same directory back while
# the IN_MOVE_SELF event is still pending.
kill -STOP "$ALFRED_PID"
mv "$TEST_ROOT" "$MOVE_TARGET"
mv "$MOVE_TARGET" "$TEST_ROOT"

# SIGCONT resumes Alfred after the old path is reachable again and still points
# to the original inode captured when the watch was installed.
kill -CONT "$ALFRED_PID"
sleep 1

if ! grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_identity_match name=" \
    ./raw.log; then

    echo "FAIL: missing IN_MOVE_SELF for temporarily moved watched root"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"
assert_contains "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"
assert_contains "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=valid"
assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"
assert_order "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=valid"

assert_not_contains "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"
assert_not_contains "DIR_RELOCATED path="
assert_not_contains "DIR_MOVED path="
assert_not_contains "DIR_RENAMED path="

echo "PASS backend self move identity match"
