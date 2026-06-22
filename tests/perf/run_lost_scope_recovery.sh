#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/perf"
TEST_BIN="$BUILD_DIR/bench_lost_scope_recovery"
BENCH_ROOT="${BENCH_ROOT:-/tmp/alfred_perf_lost_scope}"

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
    -Wno-unused-function \
    -g3 \
    -O2 \
    -DDEBUG \
    -I"$ROOT_DIR/app/include" \
    -I"$ROOT_DIR/core/include" \
    -I"$ROOT_DIR/core/src" \
    -I"$ROOT_DIR/modules/inotify/include" \
    "$ROOT_DIR/tests/perf/bench_lost_scope_recovery.c" \
    "$ROOT_DIR/app/src/fs_scanner.c" \
    "$ROOT_DIR/app/src/logger.c" \
    "$ROOT_DIR/app/src/utils.c" \
    "$ROOT_DIR/core/src/alfred_record_diagnostic.c" \
    "$ROOT_DIR/core/src/alfred_record_text.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_adapter.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_config.c" \
    "$ROOT_DIR/modules/inotify/src/watch_manager.c" \
    "$ROOT_DIR/modules/inotify/src/watcher.c" \
    -o "$TEST_BIN"

if [[ "$#" -eq 0 ]]; then
    set -- 10 100 1000
fi

"$TEST_BIN" "$BENCH_ROOT" "$@"
