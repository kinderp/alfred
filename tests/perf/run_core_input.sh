#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/perf"
TEST_BIN="$BUILD_DIR/bench_core_input"
EVENTS=100000
RUNS=1

while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --events)
            if [[ "$#" -lt 2 ]]; then
                echo "missing value for --events" >&2
                exit 1
            fi
            EVENTS="$2"
            shift 2
            ;;
        --runs)
            if [[ "$#" -lt 2 ]]; then
                echo "missing value for --runs" >&2
                exit 1
            fi
            RUNS="$2"
            shift 2
            ;;
        *)
            if [[ "$#" -eq 1 ]]; then
                EVENTS="$1"
                shift
            else
                echo "usage: $0 [--events N] [--runs N]" >&2
                echo "       $0 [EVENTS]" >&2
                exit 1
            fi
            ;;
    esac
done

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
    -I"$ROOT_DIR/core/src" \
    "$ROOT_DIR/tests/perf/bench_core_input.c" \
    "$ROOT_DIR/core/src/alfred_correlator.c" \
    "$ROOT_DIR/core/src/alfred_tables.c" \
    "$ROOT_DIR/core/src/alfred_utils.c" \
    -o "$TEST_BIN"

"$TEST_BIN" --events "$EVENTS" --runs "$RUNS"
