#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "temporary\n" > "$TEST_ROOT/delete-me.txt"
sleep 1
rm "$TEST_ROOT/delete-me.txt"
sleep 1

assert_contains "FILE_CREATED path=.*/delete-me.txt"
assert_contains "FILE_MODIFIED path=.*/delete-me.txt"
assert_contains "FILE_READY path=.*/delete-me.txt"
assert_contains "FILE_DELETED path=.*/delete-me.txt"
assert_not_contains "core seq="

assert_order "FILE_CREATED path=.*/delete-me.txt" "FILE_MODIFIED path=.*/delete-me.txt"
assert_order "FILE_MODIFIED path=.*/delete-me.txt" "FILE_READY path=.*/delete-me.txt"
assert_order "FILE_READY path=.*/delete-me.txt" "FILE_DELETED path=.*/delete-me.txt"

echo "PASS delete file core"
