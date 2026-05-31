#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "initial\n" > "$TEST_ROOT/editable.txt"
sleep 1
printf "second\n" >> "$TEST_ROOT/editable.txt"
sleep 1

assert_count "FILE_CREATED path=.*/editable.txt" 1
assert_count "FILE_MODIFIED path=.*/editable.txt" 2
assert_count "FILE_READY path=.*/editable.txt" 2
assert_not_contains "core seq="

assert_order "FILE_CREATED path=.*/editable.txt" "FILE_MODIFIED path=.*/editable.txt"
assert_order "FILE_MODIFIED path=.*/editable.txt" "FILE_READY path=.*/editable.txt"

echo "PASS modify file core"
