#!/usr/bin/env bash

source ../lib/test_lib.sh

start_fsmon

mkdir "$TEST_ROOT/dir"
sleep 1

rm -rf "$TEST_ROOT/dir"
sleep 1

assert_contains "WATCH_REMOVED\|DIR_DELETED"

stop_fsmon
cleanup

echo "✔ delete dir PASS"
