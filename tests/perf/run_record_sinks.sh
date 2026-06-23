#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/perf"
TEST_BIN="$BUILD_DIR/bench_record_sinks"

mkdir -p "$BUILD_DIR"

gcc \
    -std=gnu99 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -Wshadow \
    -Wconversion \
    -Wsign-conversion \
    -Wformat=2 \
    -Wundef \
    -Wnull-dereference \
    -Wdouble-promotion \
    -Wimplicit-fallthrough \
    -O2 \
    -g3 \
    -I"$ROOT_DIR/core/include" \
    "$ROOT_DIR/tests/perf/bench_record_sinks.c" \
    "$ROOT_DIR/core/src/alfred_record_counter_sink.c" \
    "$ROOT_DIR/core/src/alfred_record_jsonl.c" \
    "$ROOT_DIR/core/src/alfred_record_jsonl_sink.c" \
    "$ROOT_DIR/core/src/alfred_record_sink.c" \
    "$ROOT_DIR/core/src/alfred_record_text.c" \
    "$ROOT_DIR/core/src/alfred_record_text_sink.c" \
    -o "$TEST_BIN"

"$TEST_BIN" "${1:-100000}"
