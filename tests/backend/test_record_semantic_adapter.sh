#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/backend"
mkdir -p "$BUILD_DIR"

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    -I"$ROOT_DIR/core/include" \
    "$ROOT_DIR/tests/backend/test_record_semantic_adapter.c" \
    "$ROOT_DIR/core/src/alfred_record_adapter.c" \
    -o "$BUILD_DIR/alfred_test_record_semantic_adapter"

"$BUILD_DIR/alfred_test_record_semantic_adapter"

echo "PASS backend record semantic adapter"
