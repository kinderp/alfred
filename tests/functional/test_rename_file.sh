#!/usr/bin/env bash

source ../lib/test_lib.sh

start_alfred

touch "$TEST_ROOT/a.txt"
sleep 1

mv "$TEST_ROOT/a.txt" "$TEST_ROOT/b.txt"
sleep 1

assert_contains "FILE_RENAMED.*a.txt.*b.txt"

stop_alfred
cleanup

echo "✔ rename file PASS"
