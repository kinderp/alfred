#!/usr/bin/env bash

source ../lib/test_lib.sh

start_fsmon

touch "$TEST_ROOT/a.txt"
sleep 1

assert_contains "FILE_CREATED.*a.txt"

stop_fsmon
cleanup

echo "✔ create file PASS"
