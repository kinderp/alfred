#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/old-dir"
sleep 1
mv "$TEST_ROOT/old-dir" "$TEST_ROOT/new-dir"
sleep 1

assert_contains "DIR_CREATED path=.*/old-dir"
assert_contains "DIR_RENAMED from=.*/old-dir to=.*/new-dir"
assert_not_contains "DIR_MOVED from=.*/old-dir to=.*/new-dir"
assert_not_contains "DIR_RELOCATED from=.*/old-dir to=.*/new-dir"
assert_not_contains "core seq="

assert_order "DIR_CREATED path=.*/old-dir" "DIR_RENAMED from=.*/old-dir to=.*/new-dir"

echo "PASS rename dir core"
