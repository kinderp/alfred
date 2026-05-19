#!/usr/bin/env bash

source ../lib/test_lib.sh

start_alfred

mkdir "$TEST_ROOT/dir1"
sleep 1

assert_contains "DIR_CREATED\|WATCH_ADDED.*dir1"

stop_alfred
cleanup

echo "✔ create dir PASS"
