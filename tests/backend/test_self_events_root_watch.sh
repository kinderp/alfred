#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_self_events}"
MOVE_TARGET="${MOVE_TARGET:-/tmp/alfred_backend_test_self_events_moved}"

# Expected log contract, delete half:
#
# raw.log:
# - IN_DELETE_SELF ... path=*/alfred_backend_test_self_events name=
# - IN_IGNORED ... path=*/alfred_backend_test_self_events name=
#
# events.log:
# - WATCH_STALE ... reason=IN_DELETE_SELF
# - WATCH_REMOVED ... path=*/alfred_backend_test_self_events
# - FILE_DELETED path=*/file-before-delete.txt
# - DIR_DELETED path=*/dir-before-delete
#
# Meaning:
# Removing the watched root marks the root watch STALE and then the kernel
# removes it with IN_IGNORED. Child FILE_DELETED/DIR_DELETED events are accepted
# only when they come from real child delete facts observed before watch removal.
#
# Expected log contract, move half:
#
# raw.log:
# - IN_MOVE_SELF ... path=*/alfred_backend_test_self_events name=
#
# events.log:
# - WATCH_STALE ... reason=IN_MOVE_SELF
# - WATCH_RESYNC_BEGIN ... reason=IN_MOVE_SELF
# - WATCH_RESYNC_FAILED ... reason=IN_MOVE_SELF
# - WATCH_STALE_EVENT_DROPPED ... name=file-after-move.txt
#
# Forbidden events:
# - DIR_RELOCATED
# - DIR_MOVED
# - DIR_RENAMED
# - FILE_DELETED path=*/file-before-move.txt
# - DIR_DELETED path=*/dir-before-move
# - FILE_CREATED path=*/file-after-move.txt
# - core seq=
#
# Meaning:
# Moving the watched root away gives Alfred no destination path. Alfred may
# mark the watch stale and attempt resync, but it must not invent relocation,
# move, rename, or child delete semantics. If the kernel later reports child
# events on the stale wd, Alfred logs that it dropped them instead of forwarding
# raw/core events with the old untrusted path.

source ../core/test_lib.sh
trap 'rm -rf "$MOVE_TARGET"; cleanup' EXIT

rm -rf "$MOVE_TARGET"
start_alfred_core

touch "$TEST_ROOT/file-before-delete.txt"
mkdir "$TEST_ROOT/dir-before-delete"
sleep 1

# Deleting the watched root can produce normal child delete events before the
# watch itself is invalidated. The test fixes both layers: child semantics and
# backend watch diagnostics.
rm -rf "$TEST_ROOT"
sleep 1

# IN_DELETE_SELF is the kernel fact for "the watched object itself was deleted".
# It is different from IN_DELETE for a child name inside the watched directory.
if ! grep -Eq "IN_DELETE_SELF .*path=.*/alfred_backend_test_self_events name=" \
    ./raw.log; then

    echo "FAIL: missing IN_DELETE_SELF for watched root"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# IN_IGNORED follows when the kernel removes the watch. Alfred should then
# clear the watcher-table entry and log WATCH_REMOVED.
if ! grep -Eq "IN_IGNORED .*path=.*/alfred_backend_test_self_events name=" \
    ./raw.log; then

    echo "FAIL: missing IN_IGNORED after watched root delete"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# Deleting the watched root first makes the mapping stale, then IN_IGNORED
# removes the kernel watch. The order documents that STALE is a diagnostic
# transition before final watch cleanup.
assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_DELETE_SELF"
assert_contains "WATCH_REMOVED wd=[0-9]+ path=.*/alfred_backend_test_self_events$"
assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_DELETE_SELF" \
             "WATCH_REMOVED wd=[0-9]+ path=.*/alfred_backend_test_self_events$"

# These semantic events come from real child delete facts observed before the
# root watch is removed. They are not invented from IN_DELETE_SELF.
assert_contains "FILE_DELETED path=.*/file-before-delete.txt"
assert_contains "DIR_DELETED path=.*/dir-before-delete"

# The current observed order for rm -rf in this scenario is directory delete
# before file delete. This assertion documents the test fixture, not a general
# filesystem guarantee for every possible tree.
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

# The inotify watch can keep observing the moved directory object. This create
# happens at the real new path, but Alfred only knows the old wd -> path mapping,
# which is already STALE. The backend should log a diagnostic drop instead of
# forwarding a raw/core event with the old path.
touch "$MOVE_TARGET/file-after-move.txt"
sleep 1

# Moving the watched root emits IN_MOVE_SELF. Unlike child IN_MOVED_FROM/TO,
# this self-event does not include the destination path.
if ! grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_self_events name=" \
    ./raw.log; then

    echo "FAIL: missing IN_MOVE_SELF for moved watched root"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# The root path is gone, so Alfred can start resync but cannot prove identity or
# scan a trusted root. The watch remains stale and recovery fails explicitly.
assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF"
assert_contains "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF"
assert_contains "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF"
assert_contains "WATCH_STALE_EVENT_DROPPED wd=[0-9]+ path=.*/alfred_backend_test_self_events mask=.*IN_CREATE .* name=file-after-move.txt"
assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF"
assert_order "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF"
assert_order "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_self_events reason=IN_MOVE_SELF" \
             "WATCH_STALE_EVENT_DROPPED wd=[0-9]+ path=.*/alfred_backend_test_self_events mask=.*IN_CREATE .* name=file-after-move.txt"

# IN_MOVE_SELF has no new path, so Alfred must not synthesize semantic
# relocation/move/rename events for the watched root.
assert_not_contains "DIR_RELOCATED path="
assert_not_contains "DIR_MOVED path="
assert_not_contains "DIR_RENAMED path="

# Moving the root should not be reported as deletion of children. The kernel did
# not tell Alfred that the children were deleted; it only said the watched root
# moved away.
if grep -Eq "FILE_DELETED path=.*/file-before-move.txt" "$LOG_FILE"; then
    echo "FAIL: root move unexpectedly produced child file delete"
    echo "----- events.log -----"
    cat "$LOG_FILE" || true
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# Same rule for directories: a root move without destination is backend stale
# state, not a semantic delete of every child.
if grep -Eq "DIR_DELETED path=.*/dir-before-move" "$LOG_FILE"; then
    echo "FAIL: root move unexpectedly produced child dir delete"
    echo "----- events.log -----"
    cat "$LOG_FILE" || true
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# The kernel may report this child create on the stale wd, but Alfred cannot
# reconstruct the new path. Reporting FILE_CREATED with the old path would be a
# false semantic event.
if grep -Eq "FILE_CREATED path=.*/file-after-move.txt" "$LOG_FILE"; then
    echo "FAIL: stale moved root unexpectedly produced child file create"
    echo "----- events.log -----"
    cat "$LOG_FILE" || true
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# This backend diagnostic suite asserts backend/core behavior, not shadow-mode
# comparison output.
assert_not_contains "core seq="

echo "PASS backend self events root watch"
