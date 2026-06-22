#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/backend"
mkdir -p "$BUILD_DIR"

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I"$ROOT_DIR/core/include" \
    "$ROOT_DIR/tests/backend/test_record_owned.c" \
    "$ROOT_DIR/core/src/alfred_record_owned.c" \
    -o "$BUILD_DIR/alfred_test_record_owned"

"$BUILD_DIR/alfred_test_record_owned"

echo "PASS backend record owned clone"
