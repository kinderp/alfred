#!/usr/bin/env bash

source ../lib/test_lib.sh

start_fsmon

mkdir "$TEST_ROOT/src"
sleep 0.2
mkdir "$TEST_ROOT/src/before"

mkdir "$TEST_ROOT/dst"
sleep 1

mv "$TEST_ROOT/src/before" "$TEST_ROOT/dst/after"
sleep 1

assert_contains "DIR_CREATED.*src"
assert_contains "WATCH_ADDED.*src"

assert_contains "DIR_CREATED.*src/before"
assert_contains "WATCH_ADDED.*src/before"

assert_contains "DIR_CREATED.*dst"
assert_contains "WATCH_ADDED.*dst"

assert_contains "DIR_MOVED.*src/before.*dst/after"
assert_contains "DIR_RENAMED.*src/before.*dst/after"
stop_fsmon
cleanup

echo "✔ rename dir PASS"
