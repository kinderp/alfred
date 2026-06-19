#!/usr/bin/env bash

set -euo pipefail

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I../../app/include \
    -I../../core/include \
    -I../../modules/inotify/include \
    -I../../core/src \
    test_overflow_raw_bridge.c \
    ../../app/src/fs_scanner.c \
    ../../app/src/logger.c \
    ../../app/src/utils.c \
    ../../core/src/alfred_correlator.c \
    ../../core/src/alfred_record_diagnostic.c \
    ../../core/src/alfred_record_text.c \
    ../../core/src/alfred_tables.c \
    ../../core/src/alfred_utils.c \
    ../../modules/inotify/src/inotify_adapter.c \
    ../../modules/inotify/src/inotify_config.c \
    ../../modules/inotify/src/watch_manager.c \
    ../../modules/inotify/src/watcher.c \
    -o /tmp/alfred_test_overflow_raw_bridge

/tmp/alfred_test_overflow_raw_bridge

echo "PASS backend overflow raw bridge"
