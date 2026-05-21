#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

mkdir -p "$TEST_ROOT/one/two/three"
sleep 1

assert_contains "DIR_CREATED path=.*/one$"
assert_contains "DIR_CREATED path=.*/one/two$"
assert_contains "DIR_CREATED path=.*/one/two/three$"
assert_not_contains "core seq="

assert_order "DIR_CREATED path=.*/one$" "DIR_CREATED path=.*/one/two$"
assert_order "DIR_CREATED path=.*/one/two$" "DIR_CREATED path=.*/one/two/three$"

echo "PASS recursive create nested dir core"
