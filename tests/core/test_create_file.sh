#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "hello\n" > "$TEST_ROOT/a.txt"
sleep 1

assert_contains "FILE_CREATED path=.*/a.txt"
assert_contains "FILE_MODIFIED path=.*/a.txt"
assert_contains "FILE_READY path=.*/a.txt"
assert_not_contains "core seq="

assert_order "FILE_CREATED path=.*/a.txt" "FILE_MODIFIED path=.*/a.txt"
assert_order "FILE_MODIFIED path=.*/a.txt" "FILE_READY path=.*/a.txt"

echo "PASS create file core"
