#!/usr/bin/env bash

set -euo pipefail

cc -std=c99 -Wall -Wextra -Werror \
    -I../../core/include \
    ../../core/src/alfred_record_dispatcher.c \
    ../../core/src/alfred_record_jsonl.c \
    ../../core/src/alfred_record_jsonl_writer.c \
    ../../core/src/alfred_record_owned.c \
    ../../core/src/alfred_record_output_pipeline.c \
    ../../core/src/alfred_record_queue.c \
    ../../core/src/alfred_record_runtime.c \
    ../../core/src/alfred_record_sink.c \
    test_record_output_pipeline.c \
    -o /tmp/alfred_test_record_output_pipeline

/tmp/alfred_test_record_output_pipeline

echo "PASS backend record output pipeline"
