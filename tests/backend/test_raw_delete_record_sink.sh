#!/usr/bin/env bash

# Expected raw.log contract:
# - IN_DELETE ... name=delete-file.txt
# - RAW_DELETE path=*/delete-file.txt mask=2
# - IN_DELETE IN_ISDIR ... name=delete-dir
# - RAW_DELETE path=*/delete-dir mask=258
#
# The files are created before Alfred starts so this test focuses on delete
# facts. The kernel-facing IN_DELETE lines are still produced by the inotify
# backend. The RAW_DELETE lines are normalized Alfred raw records emitted by
# app.c through alfred_record_from_raw() -> alfred_record_sink_t -> text sink
# -> logger_raw().

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_raw_delete_record_sink}"

source ../core/test_lib.sh
trap cleanup EXIT

reset_env

printf "temporary\n" > "$TEST_ROOT/delete-file.txt"
mkdir "$TEST_ROOT/delete-dir"

ALFRED_EVENT_ENGINE=core "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!
sleep 1

rm "$TEST_ROOT/delete-file.txt"
rmdir "$TEST_ROOT/delete-dir"
sleep 1

if ! grep -Eq "IN_DELETE .*name=delete-file.txt" ./raw.log; then
    echo "FAIL: missing kernel IN_DELETE for deleted file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_DELETE path=.*/delete-file.txt mask=2" ./raw.log; then
    echo "FAIL: missing normalized RAW_DELETE record for deleted file"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_DELETE IN_ISDIR .*name=delete-dir" ./raw.log; then
    echo "FAIL: missing kernel IN_DELETE for deleted directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "RAW_DELETE path=.*/delete-dir mask=258" ./raw.log; then
    echo "FAIL: missing normalized RAW_DELETE record for deleted directory"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

echo "PASS backend raw delete record sink"
