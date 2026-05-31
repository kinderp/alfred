#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_recursive_slow}"

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/a"
sleep 1
mkdir "$TEST_ROOT/a/b"
sleep 1
mkdir "$TEST_ROOT/a/b/c"
sleep 1

assert_contains "DIR_CREATED path=.*/a$"
assert_contains "DIR_CREATED path=.*/a/b$"
assert_contains "DIR_CREATED path=.*/a/b/c$"

assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/a$"
assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/a/b$"
assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/a/b/c$"
assert_not_contains "core seq="

assert_order "WATCH_ADDED wd=[0-9]+ path=.*/a$" \
             "WATCH_ADDED wd=[0-9]+ path=.*/a/b$"
assert_order "WATCH_ADDED wd=[0-9]+ path=.*/a/b$" \
             "WATCH_ADDED wd=[0-9]+ path=.*/a/b/c$"

echo "PASS backend recursive slow watch tree"
