#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/backend"
TEST_BIN="$BUILD_DIR/test_resync_reinstall_policy"

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
    -O0 \
    -DDEBUG \
    -fsanitize=address \
    -fsanitize=undefined \
    -I"$ROOT_DIR/app/include" \
    -I"$ROOT_DIR/core/include" \
    -I"$ROOT_DIR/core/src" \
    -I"$ROOT_DIR/modules/inotify/include" \
    "$ROOT_DIR/tests/backend/test_resync_reinstall_policy.c" \
    "$ROOT_DIR/app/src/fs_scanner.c" \
    "$ROOT_DIR/app/src/logger.c" \
    "$ROOT_DIR/app/src/utils.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_adapter.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_config.c" \
    "$ROOT_DIR/modules/inotify/src/watch_manager.c" \
    "$ROOT_DIR/modules/inotify/src/watcher.c" \
    -o "$TEST_BIN" \
    -fsanitize=address \
    -fsanitize=undefined

ASAN_OPTIONS=detect_leaks=0 "$TEST_BIN"

echo "PASS backend resync reinstall policy"
