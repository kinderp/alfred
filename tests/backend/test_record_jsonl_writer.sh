#!/usr/bin/env bash

set -euo pipefail

cc -std=c99 -Wall -Wextra -Werror \
    -I../../core/include \
    ../../core/src/alfred_record_jsonl.c \
    ../../core/src/alfred_record_jsonl_writer.c \
    ../../core/src/alfred_record_sink.c \
    test_record_jsonl_writer.c \
    -o /tmp/alfred_test_record_jsonl_writer

/tmp/alfred_test_record_jsonl_writer

echo "PASS backend record jsonl writer"
