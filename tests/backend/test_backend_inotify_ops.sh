#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/backend"
mkdir -p "$BUILD_DIR"

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I"$ROOT_DIR/app/include" \
    -I"$ROOT_DIR/core/include" \
    -I"$ROOT_DIR/core/src" \
    -I"$ROOT_DIR/modules/inotify/include" \
    "$ROOT_DIR/tests/backend/test_backend_inotify_ops.c" \
    "$ROOT_DIR/app/src/fs_scanner.c" \
    "$ROOT_DIR/app/src/logger.c" \
    "$ROOT_DIR/app/src/utils.c" \
    "$ROOT_DIR/core/src/alfred_backend_capabilities.c" \
    "$ROOT_DIR/core/src/alfred_backend_ops.c" \
    "$ROOT_DIR/core/src/alfred_record_adapter.c" \
    "$ROOT_DIR/core/src/alfred_record_diagnostic.c" \
    "$ROOT_DIR/core/src/alfred_record_sink.c" \
    "$ROOT_DIR/core/src/alfred_record_text.c" \
    "$ROOT_DIR/core/src/alfred_record_text_sink.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_adapter.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_backend.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_backend_capabilities.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_backend_ops.c" \
    "$ROOT_DIR/modules/inotify/src/inotify_config.c" \
    "$ROOT_DIR/modules/inotify/src/watch_manager.c" \
    "$ROOT_DIR/modules/inotify/src/watcher.c" \
    -o "$BUILD_DIR/alfred_test_backend_inotify_ops"

"$BUILD_DIR/alfred_test_backend_inotify_ops"

echo "PASS backend inotify ops"
