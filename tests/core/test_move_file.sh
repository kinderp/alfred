#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
sleep 1

printf "move\n" > "$TEST_ROOT/src/moved.txt"
sleep 1
mv "$TEST_ROOT/src/moved.txt" "$TEST_ROOT/dst/moved.txt"
sleep 1

assert_contains "DIR_CREATED path=.*/src"
assert_contains "DIR_CREATED path=.*/dst"
assert_contains "FILE_CREATED path=.*/src/moved.txt"
assert_contains "FILE_MODIFIED path=.*/src/moved.txt"
assert_contains "FILE_READY path=.*/src/moved.txt"
assert_contains "FILE_MOVED from=.*/src/moved.txt to=.*/dst/moved.txt"
assert_not_contains "FILE_RENAMED from=.*/src/moved.txt to=.*/dst/moved.txt"
assert_not_contains "FILE_RELOCATED from=.*/src/moved.txt to=.*/dst/moved.txt"
assert_not_contains "core seq="

assert_order "FILE_READY path=.*/src/moved.txt" "FILE_MOVED from=.*/src/moved.txt to=.*/dst/moved.txt"

echo "PASS move file core"
