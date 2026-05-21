#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
sleep 1

printf "move and rename\n" > "$TEST_ROOT/src/old.txt"
sleep 1
mv "$TEST_ROOT/src/old.txt" "$TEST_ROOT/dst/new.txt"
sleep 1

assert_contains "DIR_CREATED path=.*/src"
assert_contains "DIR_CREATED path=.*/dst"
assert_contains "FILE_RELOCATED from=.*/src/old.txt to=.*/dst/new.txt"
assert_not_contains "FILE_MOVED from=.*/src/old.txt to=.*/dst/new.txt"
assert_not_contains "FILE_RENAMED from=.*/src/old.txt to=.*/dst/new.txt"
assert_not_contains "core seq="

echo "PASS move rename file core"
