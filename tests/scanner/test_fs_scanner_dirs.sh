#!/usr/bin/env bash

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_scanner_test_dirs}"
TEST_BIN="./fs_scanner_dirs_test"

cleanup() {
    rm -rf "$TEST_ROOT"
    rm -f "$TEST_BIN"
}

trap cleanup EXIT

cleanup

mkdir -p "$TEST_ROOT/a/b"
mkdir -p "$TEST_ROOT/c"
touch "$TEST_ROOT/a/file.txt"
ln -s "$TEST_ROOT/a" "$TEST_ROOT/link_to_a"

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
    -DDEBUG \
    -fsanitize=address \
    -fsanitize=undefined \
    -I../../app/include \
    ../../app/src/fs_scanner.c \
    ../../app/src/utils.c \
    test_fs_scanner_dirs.c \
    -o "$TEST_BIN" \
    -fsanitize=address \
    -fsanitize=undefined

ASAN_OPTIONS=detect_leaks=0 "$TEST_BIN" "$TEST_ROOT"

echo "PASS fs scanner dirs"
