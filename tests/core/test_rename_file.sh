#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "rename\n" > "$TEST_ROOT/old.txt"
sleep 1
mv "$TEST_ROOT/old.txt" "$TEST_ROOT/new.txt"
sleep 1

assert_contains "FILE_CREATED path=.*/old.txt"
assert_contains "FILE_MODIFIED path=.*/old.txt"
assert_contains "FILE_READY path=.*/old.txt"
assert_contains "FILE_RENAMED from=.*/old.txt to=.*/new.txt"
assert_not_contains "core seq="

assert_order "FILE_CREATED path=.*/old.txt" "FILE_MODIFIED path=.*/old.txt"
assert_order "FILE_MODIFIED path=.*/old.txt" "FILE_READY path=.*/old.txt"
assert_order "FILE_READY path=.*/old.txt" "FILE_RENAMED from=.*/old.txt to=.*/new.txt"

echo "PASS rename file core"
