#!/usr/bin/env bash

source ../lib/test_lib.sh

start_alfred

mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"

touch "$TEST_ROOT/src/a.txt"
sleep 1

mv "$TEST_ROOT/src/a.txt" "$TEST_ROOT/dst/a.txt"
sleep 1

assert_contains "FILE_MOVED.*src.*dst"

stop_alfred
cleanup

echo "✔ move file PASS"
