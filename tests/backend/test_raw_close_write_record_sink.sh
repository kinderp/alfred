#!/usr/bin/env bash

# Expected raw.log contract:
# - IN_CLOSE_WRITE ... name=ready.txt
# - RAW_CLOSE_WRITE path=*/ready.txt mask=16
#
# This test fixes the close-write side of the write lifecycle. RAW_CLOSE_WRITE
# is tested separately from RAW_MODIFY because close-write feeds FILE_READY,
# while modify feeds FILE_MODIFIED.

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_raw_close_write_record_sink}"

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "ready\n" > "$TEST_ROOT/ready.txt"
sleep 1

if ! grep -Eq "IN_CLOSE_WRITE .*name=ready.txt" ./raw.log; then
    echo "FAIL: missing kernel IN_CLOSE_WRITE for written file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_CLOSE_WRITE path=.*/ready.txt mask=16" ./raw.log; then
    echo "FAIL: missing normalized RAW_CLOSE_WRITE record for written file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

echo "PASS backend raw close-write record sink"
