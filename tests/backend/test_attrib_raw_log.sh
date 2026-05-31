#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_attrib_raw}"

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

touch "$TEST_ROOT/metadata.txt"
sleep 1
chmod 600 "$TEST_ROOT/metadata.txt"
sleep 1

if ! grep -Eq "IN_ATTRIB .*path=.*/alfred_backend_test_attrib_raw name=metadata.txt" \
    ./raw.log; then

    echo "FAIL: missing IN_ATTRIB raw log"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

assert_not_contains "FILE_ATTRIB"
assert_not_contains "METADATA"
assert_not_contains "core seq="

echo "PASS backend attrib raw log"
