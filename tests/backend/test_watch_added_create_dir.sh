#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_watch_added}"

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/observed-dir"
sleep 1

assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/observed-dir"
assert_contains "DIR_CREATED path=.*/observed-dir"
assert_not_contains "core seq="

echo "PASS backend watch added create dir"
