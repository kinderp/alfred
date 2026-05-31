#!/usr/bin/env bash

source ../lib/test_lib.sh

start_alfred

mkdir "$TEST_ROOT/a"
sleep 0.2
mkdir "$TEST_ROOT/a/b"
sleep 0.2
mkdir "$TEST_ROOT/a/b/c"
sleep 1

touch "$TEST_ROOT/a/b/c/file.txt"
sleep 1

assert_contains "DIR_CREATED.*a"
assert_contains "WATCH_ADDED.*a"

assert_contains "DIR_CREATED.*a/b"
assert_contains "WATCH_ADDED.*a/b"

assert_contains "DIR_CREATED.*a/b/c"
assert_contains "WATCH_ADDED.*a/b/c"

assert_contains "FILE_CREATED.*file.txt"

stop_alfred
cleanup

echo "✔ recursive PASS"
