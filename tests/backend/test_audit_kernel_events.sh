#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build/tests/backend"
TEST_BIN="$BUILD_DIR/test_audit_kernel_events"

mkdir -p "$BUILD_DIR"

cc -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Wall -Wextra -std=c11 \
    "$ROOT_DIR/tests/backend/test_audit_kernel_events.c" \
    -o "$TEST_BIN"

"$TEST_BIN"

echo "PASS backend audit kernel events"
