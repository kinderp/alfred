#!/usr/bin/env bash

# Expected raw.log contract:
# - IN_MOVED_FROM ... name=move-source.txt
# - IN_MOVED_TO ... name=move-target.txt
# - RAW_MOVED_FROM path=*/move-source.txt mask=32 cookie=<same cookie>
# - RAW_MOVED_TO path=*/move-target.txt mask=64 cookie=<same cookie>
#
# This test fixes the raw move side of the record sink migration. MOVED_FROM
# and MOVED_TO are still raw facts: the backend/app path only preserves their
# source path, destination path, mask, and correlation cookie. The core remains
# responsible for turning the pair into a semantic rename/move/relocate event.

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_raw_move_record_sink}"

source ../core/test_lib.sh
trap cleanup EXIT

extract_raw_cookie() {
    local pattern="$1"

    grep -E "$pattern" ./raw.log |
        sed -E 's/.* cookie=([0-9]+).*/\1/' |
        head -n 1
}

start_alfred_core

printf "content\n" > "$TEST_ROOT/move-source.txt"
sleep 1
mv "$TEST_ROOT/move-source.txt" "$TEST_ROOT/move-target.txt"
sleep 1

if ! grep -Eq "IN_MOVED_FROM .*name=move-source.txt" ./raw.log; then
    echo "FAIL: missing kernel IN_MOVED_FROM for renamed file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_MOVED_TO .*name=move-target.txt" ./raw.log; then
    echo "FAIL: missing kernel IN_MOVED_TO for renamed file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_MOVED_FROM path=.*/move-source.txt mask=32 cookie=[1-9][0-9]*" ./raw.log; then
    echo "FAIL: missing normalized RAW_MOVED_FROM record for renamed file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_MOVED_TO path=.*/move-target.txt mask=64 cookie=[1-9][0-9]*" ./raw.log; then
    echo "FAIL: missing normalized RAW_MOVED_TO record for renamed file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

from_cookie=$(extract_raw_cookie "RAW_MOVED_FROM path=.*/move-source.txt")
to_cookie=$(extract_raw_cookie "RAW_MOVED_TO path=.*/move-target.txt")

if [[ -z "$from_cookie" || -z "$to_cookie" || "$from_cookie" != "$to_cookie" ]]; then
    echo "FAIL: normalized RAW_MOVED_FROM/TO records do not share a cookie"
    echo "from_cookie=$from_cookie to_cookie=$to_cookie"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

echo "PASS backend raw move record sink"
