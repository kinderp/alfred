#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_identity_mismatch}"
MOVE_TARGET="${MOVE_TARGET:-/tmp/alfred_backend_test_identity_mismatch_moved}"

# Expected log contract:
#
# raw.log:
# - IN_MOVE_SELF ... path=*/alfred_backend_test_identity_mismatch name=
#
# events.log:
# - WATCH_STALE ... reason=IN_MOVE_SELF
# - WATCH_RESYNC_BEGIN ... reason=IN_MOVE_SELF
# - WATCH_RESYNC_FAILED ... reason=IN_MOVE_SELF error=identity-mismatch
# - WATCH_LOST_QUEUED ... reason=IN_MOVE_SELF error=identity-mismatch
#
# Forbidden events:
# - WATCH_RESYNC_END ... result=valid
# - WATCH_RESYNC_SCAN_DONE ... reason=IN_MOVE_SELF
# - DIR_RELOCATED
# - DIR_MOVED
# - DIR_RENAMED
# - DIR_CREATED path=*/alfred_backend_test_identity_mismatch
#
# Meaning:
# The watched root is moved away, then a new directory is created at the old
# path. The path is reachable, but its st_dev/st_ino identity differs from the
# original watched object. Alfred must keep the watch STALE, skip subtree scan,
# enqueue delayed wide recovery by saved identity, and avoid inventing semantic
# move/create events.

source ../core/test_lib.sh

cleanup_identity_mismatch() {
    if [[ -n "$ALFRED_PID" ]] && kill -0 "$ALFRED_PID" 2>/dev/null; then
        kill -CONT "$ALFRED_PID" 2>/dev/null || true
    fi

    rm -rf "$MOVE_TARGET"
    cleanup
}

trap cleanup_identity_mismatch EXIT

rm -rf "$MOVE_TARGET"
start_alfred_core

mkdir "$TEST_ROOT/original-child"
sleep 1

# SIGSTOP freezes Alfred before it can drain the inotify queue. This lets the
# test move the watched root and recreate the old path while the IN_MOVE_SELF
# event is still pending. SIGCONT then resumes Alfred so the resync probe sees
# an existing path with a different st_dev/st_ino identity.
kill -STOP "$ALFRED_PID"
mv "$TEST_ROOT" "$MOVE_TARGET"
mkdir "$TEST_ROOT"

# SIGCONT resumes Alfred after the filesystem has reached the exact state the
# test needs: the old path exists again, but it is backed by a different inode.
kill -CONT "$ALFRED_PID"
sleep 1

# The raw log must contain the same kernel trigger as the positive test:
# IN_MOVE_SELF on the watched root. The difference is the filesystem identity
# Alfred observes after the path is recreated.
if ! grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_identity_mismatch name=" \
    ./raw.log; then

    echo "FAIL: missing IN_MOVE_SELF for moved watched root"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# IN_MOVE_SELF makes the old wd -> path mapping unsafe, so Alfred marks the
# watch STALE before it decides whether recovery is possible.
assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF"

# Recovery starts because the old path is reachable again. Reachability alone
# is not enough: Alfred still has to compare filesystem identity.
assert_contains "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF"

# The recreated path has a different st_dev/st_ino pair from the original
# watched directory. Alfred must refuse to trust it and keep the watch stale.
assert_contains "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF error=identity-mismatch"

# Identity mismatch still leaves Alfred with the original saved identity, so the
# backend queues delayed wide recovery instead of dropping the scope forever.
assert_contains "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF error=identity-mismatch pending=1"

# The failure and queue marker must happen after the stale marker and resync
# begin marker, which documents the guarded recovery order for this negative
# branch.
assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF"
assert_order "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF error=identity-mismatch"
assert_order "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF error=identity-mismatch" \
             "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF error=identity-mismatch pending=1"

# Identity mismatch stops recovery before subtree scan and before VALID. Alfred
# must not watch the new directory just because it reused the old path.
assert_not_contains "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF"
assert_not_contains "WATCH_RESYNC_SCAN_DONE wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF"

# IN_MOVE_SELF does not provide a destination path. Without that information the
# backend must not invent semantic relocation/move/rename events.
assert_not_contains "DIR_RELOCATED path="
assert_not_contains "DIR_MOVED path="
assert_not_contains "DIR_RENAMED path="

# The recreated root is a replacement object, not a child create event that the
# stale old watch can safely report as DIR_CREATED.
assert_not_contains "DIR_CREATED path=.*/alfred_backend_test_identity_mismatch$"

echo "PASS backend self move identity mismatch"
