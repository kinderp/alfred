#!/usr/bin/env bash

set -euo pipefail

cc -std=gnu99 \
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -Wformat=2 -Wundef -Wnull-dereference -Wdouble-promotion \
    -I../../core/include \
    ../../core/src/alfred_record_jsonl.c \
    ../../core/src/alfred_record_diagnostic.c \
    test_record_jsonl.c \
    -o /tmp/alfred_test_record_jsonl

/tmp/alfred_test_record_jsonl

echo "PASS backend record jsonl formatter"
