#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ALFRED_BIN="${ALFRED_BIN:-$ROOT_DIR/alfred}"
TEST_DIR="${TEST_DIR:-/tmp/alfred_cli_help_version}"

cleanup() {
    rm -rf "$TEST_DIR"
}

fail_with_output() {
    local message="$1"

    echo "FAIL: $message"
    echo "----- help stdout -----"
    cat "$TEST_DIR/help.out" 2>/dev/null || true
    echo "----- help stderr -----"
    cat "$TEST_DIR/help.err" 2>/dev/null || true
    echo "----- version stdout -----"
    cat "$TEST_DIR/version.out" 2>/dev/null || true
    echo "----- version stderr -----"
    cat "$TEST_DIR/version.err" 2>/dev/null || true
    exit 1
}

assert_no_runtime_logs() {
    local command_name="$1"

    for log_file in raw.log events.log errors.log output.jsonl; do
        if [ -e "$TEST_DIR/$log_file" ]; then
            fail_with_output "$command_name must not create $log_file"
        fi
    done
}

trap cleanup EXIT

cleanup
mkdir -p "$TEST_DIR"

(
    cd "$TEST_DIR"
    "$ALFRED_BIN" --help > help.out 2> help.err
)

if [ -s "$TEST_DIR/help.err" ]; then
    fail_with_output "--help must not write stderr"
fi

if ! grep -Eq '^Usage: alfred \[OPTIONS\] PATH\.\.\.$' "$TEST_DIR/help.out"; then
    fail_with_output "--help output missing usage line"
fi

if ! grep -Eq '^  --version  Show version information and exit\.$' \
    "$TEST_DIR/help.out"; then
    fail_with_output "--help output missing --version option"
fi

assert_no_runtime_logs "--help"

rm -f "$TEST_DIR/help.out" "$TEST_DIR/help.err"

(
    cd "$TEST_DIR"
    "$ALFRED_BIN" --version > version.out 2> version.err
)

if [ -s "$TEST_DIR/version.err" ]; then
    fail_with_output "--version must not write stderr"
fi

if ! grep -Eq '^alfred [0-9]+\.[0-9]+\.[0-9]+$' "$TEST_DIR/version.out"; then
    fail_with_output "--version output must be 'alfred MAJOR.MINOR.PATCH'"
fi

assert_no_runtime_logs "--version"

echo "PASS cli help version"
