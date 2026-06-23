#!/usr/bin/env bash

set -euo pipefail

cc -std=gnu99 \
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -Wformat=2 -Wundef -Wnull-dereference -Wdouble-promotion \
    -I../../core/include \
    ../../core/src/alfred_record_counter_sink.c \
    ../../core/src/alfred_record_sink.c \
    test_record_counter_sink.c \
    -o /tmp/alfred_test_record_counter_sink

/tmp/alfred_test_record_counter_sink

echo "PASS backend record counter sink"
