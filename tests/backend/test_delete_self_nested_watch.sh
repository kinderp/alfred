#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_delete_self_nested}"

# Expected log contract:
#
# Shell scenario:
#
#   mkdir "$TEST_ROOT/child"
#   rm -rf "$TEST_ROOT/child"
#
# raw.log:
# - IN_CREATE IN_ISDIR ... path=*/alfred_backend_test_delete_self_nested name=child
# - IN_DELETE IN_ISDIR ... path=*/alfred_backend_test_delete_self_nested name=child
# - IN_DELETE_SELF ... path=*/alfred_backend_test_delete_self_nested/child name=
# - IN_IGNORED ... path=*/alfred_backend_test_delete_self_nested/child name=
#
# events.log:
# - WATCH_ADDED ... path=*/alfred_backend_test_delete_self_nested/child
# - DIR_CREATED path=*/alfred_backend_test_delete_self_nested/child
# - DIR_DELETED path=*/alfred_backend_test_delete_self_nested/child
# - WATCH_STALE ... path=*/alfred_backend_test_delete_self_nested/child
#   reason=IN_DELETE_SELF
# - WATCH_STALE_EVENT_DROPPED ... path=*/child mask=IN_IGNORED name=
# - WATCH_REMOVED ... path=*/alfred_backend_test_delete_self_nested/child
#
# Forbidden events:
# - WATCH_LOST_QUEUED ... reason=IN_DELETE_SELF
# - WATCH_RESYNC_BEGIN ... reason=IN_DELETE_SELF
# - more than one DIR_DELETED for the child path
#
# Meaning:
# The parent watch sees a real child delete fact: IN_DELETE | IN_ISDIR with
# name=child. That fact is allowed to become DIR_DELETED. The child directory also
# has its own watch, so deleting it produces IN_DELETE_SELF and then IN_IGNORED on
# the child wd. Those self events are backend diagnostics only: Alfred marks the
# child watch STALE, drops the stale IN_IGNORED from raw/core conversion, removes
# the watch, and must not enqueue lost-scope recovery or invent a second semantic
# delete from IN_DELETE_SELF. The kernel is free to deliver the child self-event
# before or after the parent delete, so this test does not require an order
# between DIR_DELETED and WATCH_STALE.

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/child"
sleep 1

rm -rf "$TEST_ROOT/child"
sleep 1

# The parent directory reports the child creation and deletion with a child name.
# These are normal child events and may be converted into core semantics.
if ! grep -Eq "IN_CREATE IN_ISDIR .*path=.*/alfred_backend_test_delete_self_nested name=child" \
    ./raw.log; then

    echo "FAIL: missing parent IN_CREATE for child directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_DELETE IN_ISDIR .*path=.*/alfred_backend_test_delete_self_nested name=child" \
    ./raw.log; then

    echo "FAIL: missing parent IN_DELETE for child directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

# The child watch reports that the watched object itself disappeared. This event
# has no child name, so it is backend state information, not another source of
# DIR_DELETED semantics.
if ! grep -Eq "IN_DELETE_SELF .*path=.*/alfred_backend_test_delete_self_nested/child name=" \
    ./raw.log; then

    echo "FAIL: missing child IN_DELETE_SELF"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_IGNORED .*path=.*/alfred_backend_test_delete_self_nested/child name=" \
    ./raw.log; then

    echo "FAIL: missing child IN_IGNORED"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child$"
assert_contains "DIR_CREATED path=.*/alfred_backend_test_delete_self_nested/child"
assert_contains "DIR_DELETED path=.*/alfred_backend_test_delete_self_nested/child"
assert_contains "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child reason=IN_DELETE_SELF"
assert_contains "WATCH_STALE_EVENT_DROPPED wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child mask=.*IN_IGNORED .* name="
assert_contains "WATCH_REMOVED wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child$"

# Exactly one semantic delete is expected. If IN_DELETE_SELF were converted into a
# second core delete, this assertion would fail and expose the duplicate.
assert_count "DIR_DELETED path=.*/alfred_backend_test_delete_self_nested/child$" "1"

assert_order "WATCH_STALE wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child reason=IN_DELETE_SELF" \
             "WATCH_STALE_EVENT_DROPPED wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child mask=.*IN_IGNORED .* name="
assert_order "WATCH_STALE_EVENT_DROPPED wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child mask=.*IN_IGNORED .* name=" \
             "WATCH_REMOVED wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child$"

# IN_DELETE_SELF is not a lost-scope candidate. A deleted watched object is
# expected to be followed by watch cleanup, not identity search.
assert_not_contains "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child reason=IN_DELETE_SELF"
assert_not_contains "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/alfred_backend_test_delete_self_nested/child reason=IN_DELETE_SELF"

echo "PASS backend delete self nested watch"
