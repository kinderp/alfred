#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/watcher"

mkdir -p "$BUILD_DIR"

echo "=============================="
echo " ALFRED WATCHER TESTS"
echo "=============================="

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
    -g3 \
    -O0 \
    -fsanitize=address \
    -fsanitize=undefined \
    -I"$ROOT_DIR/modules/inotify/include" \
    "$ROOT_DIR/tests/watcher/test_watcher_state.c" \
    "$ROOT_DIR/modules/inotify/src/watcher.c" \
    -o "$BUILD_DIR/test_watcher_state" \
    -fsanitize=address \
    -fsanitize=undefined

ASAN_OPTIONS=detect_leaks=0 "$BUILD_DIR/test_watcher_state"

echo "PASS watcher state"
echo
echo "ALL WATCHER TESTS PASSED"
