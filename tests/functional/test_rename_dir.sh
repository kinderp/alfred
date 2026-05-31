#!/usr/bin/env bash

source ../lib/test_lib.sh

start_alfred

mkdir "$TEST_ROOT/one"
sleep 1

mv "$TEST_ROOT/one" "$TEST_ROOT/two"
sleep 1

assert_contains "DIR_RENAMED.*one.*two"

stop_alfred
cleanup

echo "✔ rename dir PASS"
