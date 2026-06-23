#!/usr/bin/env bash

set -euo pipefail

cc -std=gnu99 \
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -Wformat=2 -Wundef -Wnull-dereference -Wdouble-promotion \
    -I../../core/include \
    ../../core/src/alfred_record_jsonl.c \
    ../../core/src/alfred_record_jsonl_sink.c \
    ../../core/src/alfred_record_sink.c \
    test_record_jsonl_sink.c \
    -o /tmp/alfred_test_record_jsonl_sink

/tmp/alfred_test_record_jsonl_sink

echo "PASS backend record jsonl sink"
