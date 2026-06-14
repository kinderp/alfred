#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_lost_scope_root_a}"
SECOND_ROOT="${SECOND_ROOT:-/tmp/alfred_backend_test_lost_scope_root_b}"

# Expected log contract:
#
# raw.log:
# - IN_CREATE IN_ISDIR ... path=*/alfred_backend_test_lost_scope_root_a name=lost
# - IN_MOVE_SELF ... path=*/alfred_backend_test_lost_scope_root_a/lost name=
#
# events.log:
# - WATCH_ADDED ... path=*/alfred_backend_test_lost_scope_root_a
# - WATCH_ADDED ... path=*/alfred_backend_test_lost_scope_root_b
# - WATCH_ADDED ... path=*/alfred_backend_test_lost_scope_root_a/lost
# - WATCH_STALE ... path=*/alfred_backend_test_lost_scope_root_a/lost
# - WATCH_RESYNC_FAILED ... error=path-unreachable
# - WATCH_LOST_QUEUED ... path=*/alfred_backend_test_lost_scope_root_a/lost
# - WATCH_LOST_SCAN_BEGIN root=*/alfred_backend_test_lost_scope_root_a
# - WATCH_LOST_NOT_FOUND ... path=*/alfred_backend_test_lost_scope_root_a/lost
# - WATCH_LOST_SCAN_BEGIN root=*/alfred_backend_test_lost_scope_root_b
# - WATCH_LOST_FOUND ... old_path=*/root_a/lost new_path=*/root_b/lost
# - WATCH_LOST_RECOVERY_END ... path=*/root_b/lost result=valid
# - FILE_CREATED path=*/alfred_backend_test_lost_scope_root_b/lost/proof.txt
#
# Forbidden events:
# - WATCH_LOST_RETRY_SCHEDULED ... path=*/alfred_backend_test_lost_scope_root_a/lost
# - WATCH_LOST_RECOVERY_GAVE_UP ... path=*/alfred_backend_test_lost_scope_root_a/lost
#
# Meaning:
# This is the runtime version of the delayed lost-scope recovery. Alfred starts
# with two configured roots, creates a watched directory under root A, then the
# directory is moved to root B. The watched directory's own wd receives
# IN_MOVE_SELF, the old path is unreachable, and the backend queues wide
# recovery. The poll loop later processes that queue, fails to find the saved
# identity under root A, falls back to root B, updates the watcher path, and
# proves the recovered watch is operational with a later FILE_CREATED event.

source ../core/test_lib.sh

cleanup_lost_scope_runtime() {
    cleanup
    rm -rf "$SECOND_ROOT"
}

trap cleanup_lost_scope_runtime EXIT

reset_env
rm -rf "$SECOND_ROOT"
mkdir -p "$SECOND_ROOT"

ALFRED_EVENT_ENGINE=core "$ALFRED_BIN" "$TEST_ROOT" "$SECOND_ROOT" \
    >/dev/null 2>&1 &
ALFRED_PID=$!
sleep 1

mkdir "$TEST_ROOT/lost"
sleep 1

mv "$TEST_ROOT/lost" "$SECOND_ROOT/lost"
sleep 2

touch "$SECOND_ROOT/lost/proof.txt"
sleep 1

# The first raw fact proves that the moved directory had its own watch before
# the move. Without this WATCH_ADDED-producing create, IN_MOVE_SELF on the child
# directory would not be available for delayed lost-scope recovery.
if ! grep -Eq "IN_CREATE IN_ISDIR .*path=.*/alfred_backend_test_lost_scope_root_a name=lost" \
    ./raw.log; then

    echo "FAIL: missing watched child directory create"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# IN_MOVE_SELF is the kernel trigger for the watched object itself moving. It
# does not contain the destination path, so the backend must recover by saved
# identity rather than by trusting event payload alone.
if ! grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_lost_scope_root_a/lost name=" \
    ./raw.log; then

    echo "FAIL: missing IN_MOVE_SELF for moved watched directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# Both startup roots must be registered. The fallback root is not an arbitrary
# filesystem scan target; it is one of the explicit roots passed to Alfred.
assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a$"
assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_b$"

# Creating the directory under root A installs the child watch that later
# becomes stale. This is the watch whose path must be rewritten to root B.
assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost$"

# After IN_MOVE_SELF, Alfred marks the child watch stale before attempting any
# recovery. The old textual path is no longer trustworthy.
assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF"

# The immediate local probe cannot use the old path because root A no longer
# contains the watched directory. This is why the delayed lost-scope queue is
# needed.
assert_contains "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF error=path-unreachable"
assert_contains "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF error=path-unreachable pending=1"

# Runtime polling processes the queued entry. It tries the saved scan_root
# first, which is root A, and correctly does not find the identity there.
assert_contains "WATCH_LOST_SCAN_BEGIN root=.*/alfred_backend_test_lost_scope_root_a pending=0"
assert_contains "WATCH_LOST_NOT_FOUND wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF retry=0"

# The clean NOT_FOUND under root A is not charged as a retry yet. Alfred tries
# the other configured root, finds the saved st_dev/st_ino identity, and repairs
# the watcher-table path.
assert_contains "WATCH_LOST_SCAN_BEGIN root=.*/alfred_backend_test_lost_scope_root_b pending=0"
assert_contains "WATCH_LOST_FOUND wd=[0-9]+ old_path=.*/alfred_backend_test_lost_scope_root_a/lost new_path=.*/alfred_backend_test_lost_scope_root_b/lost reason=IN_MOVE_SELF"
assert_contains "WATCH_LOST_RECOVERY_END wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_b/lost reason=IN_MOVE_SELF result=valid"

# The file is created after recovery. Seeing it under the new root proves the
# recovered watch is active and that Alfred no longer reports through the stale
# root-A path.
assert_contains "FILE_CREATED path=.*/alfred_backend_test_lost_scope_root_b/lost/proof.txt"
assert_not_contains "FILE_CREATED path=.*/alfred_backend_test_lost_scope_root_a/lost/proof.txt"

# Ordering assertions document the delayed recovery state machine.
assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF error=path-unreachable"
assert_order "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF error=path-unreachable" \
             "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF error=path-unreachable pending=1"
assert_order "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF error=path-unreachable pending=1" \
             "WATCH_LOST_SCAN_BEGIN root=.*/alfred_backend_test_lost_scope_root_a pending=0"
assert_order "WATCH_LOST_SCAN_BEGIN root=.*/alfred_backend_test_lost_scope_root_a pending=0" \
             "WATCH_LOST_NOT_FOUND wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF retry=0"
assert_order "WATCH_LOST_NOT_FOUND wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost reason=IN_MOVE_SELF retry=0" \
             "WATCH_LOST_SCAN_BEGIN root=.*/alfred_backend_test_lost_scope_root_b pending=0"
assert_order "WATCH_LOST_SCAN_BEGIN root=.*/alfred_backend_test_lost_scope_root_b pending=0" \
             "WATCH_LOST_FOUND wd=[0-9]+ old_path=.*/alfred_backend_test_lost_scope_root_a/lost new_path=.*/alfred_backend_test_lost_scope_root_b/lost reason=IN_MOVE_SELF"
assert_order "WATCH_LOST_FOUND wd=[0-9]+ old_path=.*/alfred_backend_test_lost_scope_root_a/lost new_path=.*/alfred_backend_test_lost_scope_root_b/lost reason=IN_MOVE_SELF" \
             "WATCH_LOST_RECOVERY_END wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_b/lost reason=IN_MOVE_SELF result=valid"
assert_order "WATCH_LOST_RECOVERY_END wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_b/lost reason=IN_MOVE_SELF result=valid" \
             "FILE_CREATED path=.*/alfred_backend_test_lost_scope_root_b/lost/proof.txt"

# The multi-root recovery should find the directory before spending retry
# budget or giving up.
assert_not_contains "WATCH_LOST_RETRY_SCHEDULED wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost"
assert_not_contains "WATCH_LOST_RECOVERY_GAVE_UP wd=[0-9]+ path=.*/alfred_backend_test_lost_scope_root_a/lost"

echo "PASS backend lost scope runtime recovery"
