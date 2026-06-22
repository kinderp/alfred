#!/usr/bin/env bash

# Expected raw.log contract:
# - IN_CREATE ... name=created-file.txt
# - RAW_CREATE path=*/created-file.txt mask=1
# - IN_CREATE IN_ISDIR ... name=created-dir
# - RAW_CREATE path=*/created-dir mask=257
#
# This test fixes the first raw runtime migration to Event Model v0 sinks. The
# kernel-facing IN_CREATE lines are still produced by the inotify backend. The
# RAW_CREATE lines are normalized Alfred raw records emitted by app.c through
# alfred_record_from_raw() -> alfred_record_sink_t -> text sink -> logger_raw().

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_raw_create_record_sink}"

source ../core/test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "hello\n" > "$TEST_ROOT/created-file.txt"
mkdir "$TEST_ROOT/created-dir"
sleep 1

if ! grep -Eq "IN_CREATE .*name=created-file.txt" ./raw.log; then
    echo "FAIL: missing kernel IN_CREATE for created file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_CREATE path=.*/created-file.txt mask=1" ./raw.log; then
    echo "FAIL: missing normalized RAW_CREATE record for created file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_CREATE IN_ISDIR .*name=created-dir" ./raw.log; then
    echo "FAIL: missing kernel IN_CREATE for created directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_CREATE path=.*/created-dir mask=257" ./raw.log; then
    echo "FAIL: missing normalized RAW_CREATE record for created directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

echo "PASS backend raw create record sink"
