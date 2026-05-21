#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
sleep 1

mkdir "$TEST_ROOT/src/item"
sleep 1
mv "$TEST_ROOT/src/item" "$TEST_ROOT/dst/item"
sleep 1

assert_contains "DIR_CREATED path=.*/src"
assert_contains "DIR_CREATED path=.*/dst"
assert_contains "DIR_CREATED path=.*/src/item"
assert_contains "DIR_MOVED from=.*/src/item to=.*/dst/item"
assert_not_contains "DIR_RENAMED from=.*/src/item to=.*/dst/item"
assert_not_contains "DIR_RELOCATED from=.*/src/item to=.*/dst/item"
assert_not_contains "core seq="

assert_order "DIR_CREATED path=.*/src/item" "DIR_MOVED from=.*/src/item to=.*/dst/item"

echo "PASS move dir core"
