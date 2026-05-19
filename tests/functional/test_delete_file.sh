#!/usr/bin/env bash

source ../lib/test_lib.sh

start_alfred

touch "$TEST_ROOT/a.txt"
sleep 1

rm "$TEST_ROOT/a.txt"
sleep 1

assert_contains "FILE_CREATED.*a.txt"

stop_alfred
cleanup

echo "✔ delete file PASS"
