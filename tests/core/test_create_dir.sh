#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir "$TEST_ROOT/new-dir"
sleep 1

assert_contains "DIR_CREATED path=.*/new-dir"
assert_not_contains "core seq="

echo "PASS create dir core"
