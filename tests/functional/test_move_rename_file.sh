#!/usr/bin/env bash

source ../lib/test_lib.sh

start_alfred

mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
sleep 1

touch "$TEST_ROOT/src/old.txt"
sleep 1

mv "$TEST_ROOT/src/old.txt" "$TEST_ROOT/dst/new.txt"
sleep 1

assert_contains "FILE_MOVED.*src/old.txt.*dst/new.txt"
assert_contains "FILE_RENAMED.*src/old.txt.*dst/new.txt"

stop_alfred
cleanup

echo "✔ move rename file PASS"
