#!/usr/bin/env bash

# Expected raw.log contract:
# - IN_MODIFY ... name=editable.txt
# - RAW_MODIFY path=*/editable.txt mask=4
#
# This test fixes only the modify side of the write lifecycle. IN_CLOSE_WRITE
# and RAW_CLOSE_WRITE are intentionally left to a separate micro-step because
# close-write feeds FILE_READY, while modify feeds FILE_MODIFIED.

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_raw_modify_record_sink}"

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "initial\n" > "$TEST_ROOT/editable.txt"
sleep 1
printf "second\n" >> "$TEST_ROOT/editable.txt"
sleep 1

if ! grep -Eq "IN_MODIFY .*name=editable.txt" ./raw.log; then
    echo "FAIL: missing kernel IN_MODIFY for edited file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_MODIFY path=.*/editable.txt mask=4" ./raw.log; then
    echo "FAIL: missing normalized RAW_MODIFY record for edited file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

echo "PASS backend raw modify record sink"
