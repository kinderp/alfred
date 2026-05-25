#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_watch_removed}"

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/removed-dir"
sleep 1
rmdir "$TEST_ROOT/removed-dir"
sleep 1

assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/removed-dir"
assert_contains "DIR_CREATED path=.*/removed-dir"
assert_contains "DIR_DELETED path=.*/removed-dir"
assert_contains "WATCH_REMOVED wd=[0-9]+ path=.*/removed-dir"
assert_not_contains "core seq="

assert_order "DIR_CREATED path=.*/removed-dir" "DIR_DELETED path=.*/removed-dir"

echo "PASS backend watch removed delete dir"
