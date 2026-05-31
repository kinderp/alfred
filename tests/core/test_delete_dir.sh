#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/delete-dir"
sleep 1
rmdir "$TEST_ROOT/delete-dir"
sleep 1

assert_contains "DIR_CREATED path=.*/delete-dir"
assert_contains "DIR_DELETED path=.*/delete-dir"
assert_not_contains "WATCH_REMOVED path=.*/delete-dir"
assert_not_contains "core seq="

assert_order "DIR_CREATED path=.*/delete-dir" "DIR_DELETED path=.*/delete-dir"

echo "PASS delete dir core"
