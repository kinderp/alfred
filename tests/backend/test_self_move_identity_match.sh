#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_identity_match}"
MOVE_TARGET="${MOVE_TARGET:-/tmp/alfred_backend_test_identity_match_moved}"

# Expected log contract:
#
# raw.log:
# - IN_MOVE_SELF ... path=*/alfred_backend_test_identity_match name=
#
# events.log:
# - WATCH_STALE ... reason=IN_MOVE_SELF
# - WATCH_RESYNC_BEGIN ... reason=IN_MOVE_SELF
# - WATCH_RESYNC_SCAN_DONE ... dirs=3 watched=1 missing=2
# - WATCH_RESYNC_SCAN_CLASS ... result=needs-reinstall
# - WATCH_RESYNC_SCAN_MISSING ... missing_path=*/unwatched-one
# - WATCH_RESYNC_SCAN_MISSING ... missing_path=*/unwatched-two
# - WATCH_RESYNC_REINSTALLED ... installed_path=*/unwatched-one
# - WATCH_RESYNC_REINSTALLED ... installed_path=*/unwatched-two
# - WATCH_RESYNC_END ... result=valid
# - FILE_CREATED path=*/unwatched-one/proof.txt
# - FILE_CREATED path=*/unwatched-two/proof.txt
#
# Forbidden events:
# - WATCH_RESYNC_FAILED ... reason=IN_MOVE_SELF
# - WATCH_LOST_QUEUED ... reason=IN_MOVE_SELF
# - DIR_RELOCATED
# - DIR_MOVED
# - DIR_RENAMED
#
# Meaning:
# The watched root is moved away and restored with the same filesystem
# identity. Alfred may trust the old path again, scan the subtree, reinstall
# every missing child watch, and return the parent watch to VALID. The final
# FILE_CREATED lines prove that both repaired watches are active.

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
# the IN_MOVE_SELF event is still pending. The extra children are created while
# Alfred is stopped, so the resync scan can observe multiple directories that
# do not yet have watcher-table entries.
kill -STOP "$ALFRED_PID"
mv "$TEST_ROOT" "$MOVE_TARGET"
mv "$MOVE_TARGET" "$TEST_ROOT"
mkdir "$TEST_ROOT/unwatched-one"
mkdir "$TEST_ROOT/unwatched-two"

# SIGCONT resumes Alfred after the old path is reachable again and still points
# to the original inode captured when the watch was installed.
kill -CONT "$ALFRED_PID"
sleep 1

touch "$TEST_ROOT/unwatched-one/proof.txt"
touch "$TEST_ROOT/unwatched-two/proof.txt"
sleep 1

# The raw inotify log must contain the kernel fact that starts this scenario:
# the watched root itself moved while the original watch descriptor was still
# alive. Everything below explains how Alfred recovers from that self-event.
if ! grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_identity_match name=" \
    ./raw.log; then

    echo "FAIL: missing IN_MOVE_SELF for temporarily moved watched root"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# IN_MOVE_SELF first makes the old wd -> path mapping unsafe. Alfred records
# that with WATCH_STALE before trying any recovery.
assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"

# WATCH_RESYNC_BEGIN means Alfred entered the guarded recovery path for that
# stale watch. At this point no semantic move/rename has been invented.
assert_contains "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"

# The old path exists again and still has the original identity, so Alfred scans
# its subtree. original-child was watched before SIGSTOP; unwatched-one and
# unwatched-two were created while Alfred was stopped, so the expected coverage
# is dirs=3, watched=1, missing=2.
assert_contains "WATCH_RESYNC_SCAN_DONE wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF dirs=3 watched=1 missing=2"

# missing>0 is classified as needs-reinstall. This is still backend diagnostic
# state, not a core filesystem event.
assert_contains "WATCH_RESYNC_SCAN_CLASS wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=needs-reinstall"

# The scan reports both missing watch candidates. These lines correspond to the
# two mkdir operations performed while Alfred was stopped.
assert_contains "WATCH_RESYNC_SCAN_MISSING wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF missing_path=.*/alfred_backend_test_identity_match/unwatched-one"
assert_contains "WATCH_RESYNC_SCAN_MISSING wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF missing_path=.*/alfred_backend_test_identity_match/unwatched-two"

# Each missing candidate must be repaired with a real inotify watch before the
# parent can return VALID.
assert_contains "WATCH_RESYNC_REINSTALLED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF installed_path=.*/alfred_backend_test_identity_match/unwatched-one"
assert_contains "WATCH_RESYNC_REINSTALLED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF installed_path=.*/alfred_backend_test_identity_match/unwatched-two"

# VALID is allowed only after every missing watch in the trusted scope has been
# restored. This line is the successful end of the resync transaction.
assert_contains "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=valid"

# These FILE_CREATED events prove the repaired watches are operational: the
# files are created after resync, inside directories that were missing watches.
assert_contains "FILE_CREATED path=.*/alfred_backend_test_identity_match/unwatched-one/proof.txt"
assert_contains "FILE_CREATED path=.*/alfred_backend_test_identity_match/unwatched-two/proof.txt"

# Ordering assertions describe the recovery state machine: stale first, then
# begin, then scan, then classification, then reinstall, then VALID.
assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"
assert_order "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_SCAN_DONE wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF dirs=3 watched=1 missing=2"
assert_order "WATCH_RESYNC_SCAN_DONE wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF dirs=3 watched=1 missing=2" \
             "WATCH_RESYNC_SCAN_CLASS wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=needs-reinstall"
assert_order "WATCH_RESYNC_SCAN_CLASS wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=needs-reinstall" \
             "WATCH_RESYNC_SCAN_MISSING wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF missing_path=.*/alfred_backend_test_identity_match/unwatched-one"
assert_order "WATCH_RESYNC_SCAN_CLASS wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=needs-reinstall" \
             "WATCH_RESYNC_SCAN_MISSING wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF missing_path=.*/alfred_backend_test_identity_match/unwatched-two"
assert_order "WATCH_RESYNC_SCAN_MISSING wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF missing_path=.*/alfred_backend_test_identity_match/unwatched-one" \
             "WATCH_RESYNC_REINSTALLED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF installed_path=.*/alfred_backend_test_identity_match/unwatched-one"
assert_order "WATCH_RESYNC_SCAN_MISSING wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF missing_path=.*/alfred_backend_test_identity_match/unwatched-two" \
             "WATCH_RESYNC_REINSTALLED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF installed_path=.*/alfred_backend_test_identity_match/unwatched-two"
assert_order "WATCH_RESYNC_REINSTALLED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF installed_path=.*/alfred_backend_test_identity_match/unwatched-one" \
             "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=valid"
assert_order "WATCH_RESYNC_REINSTALLED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF installed_path=.*/alfred_backend_test_identity_match/unwatched-two" \
             "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=valid"
assert_order "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=valid" \
             "FILE_CREATED path=.*/alfred_backend_test_identity_match/unwatched-one/proof.txt"
assert_order "WATCH_RESYNC_END wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF result=valid" \
             "FILE_CREATED path=.*/alfred_backend_test_identity_match/unwatched-two/proof.txt"

# A successful identity-match resync must not report recovery failure and must
# not enqueue delayed lost-scope recovery. The old path has already been proven
# trustworthy and the missing child watches have been reinstalled.
assert_not_contains "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"
assert_not_contains "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/alfred_backend_test_identity_match reason=IN_MOVE_SELF"

# IN_MOVE_SELF has no destination path, so even a successful local recovery must
# not invent semantic relocation/move/rename events for the watched root.
assert_not_contains "DIR_RELOCATED path="
assert_not_contains "DIR_MOVED path="
assert_not_contains "DIR_RENAMED path="

echo "PASS backend self move identity match"
