#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
sleep 1

mkdir "$TEST_ROOT/src/before"
sleep 1
mv "$TEST_ROOT/src/before" "$TEST_ROOT/dst/after"
sleep 1

assert_contains "DIR_CREATED path=.*/src"
assert_contains "DIR_CREATED path=.*/dst"
assert_contains "DIR_CREATED path=.*/src/before"
assert_contains "DIR_RELOCATED from=.*/src/before to=.*/dst/after"
assert_not_contains "DIR_MOVED from=.*/src/before to=.*/dst/after"
assert_not_contains "DIR_RENAMED from=.*/src/before to=.*/dst/after"
assert_not_contains "core seq="

assert_order "DIR_CREATED path=.*/src/before" "DIR_RELOCATED from=.*/src/before to=.*/dst/after"

echo "PASS move rename dir core"
