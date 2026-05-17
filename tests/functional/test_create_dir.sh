#!/usr/bin/env bash

source ../lib/test_lib.sh

start_fsmon

mkdir "$TEST_ROOT/dir1"
sleep 1

assert_contains "DIR_CREATED\|WATCH_ADDED.*dir1"

stop_fsmon
cleanup

echo "✔ create dir PASS"
