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
    echo "----- check-config stdout -----"
    cat "$TEST_DIR/check_config.out" 2>/dev/null || true
    echo "----- check-config stderr -----"
    cat "$TEST_DIR/check_config.err" 2>/dev/null || true
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

if ! grep -Eq '^  --version[[:space:]]+Show version information and exit\.$' \
    "$TEST_DIR/help.out"; then
    fail_with_output "--help output missing --version option"
fi

if ! grep -Eq '^  --check-config[[:space:]]+Validate configuration and exit\.$' \
    "$TEST_DIR/help.out"; then
    fail_with_output "--help output missing --check-config option"
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

rm -f "$TEST_DIR/version.out" "$TEST_DIR/version.err"

(
    cd "$TEST_DIR"
    "$ALFRED_BIN" --check-config > check_config.out 2> check_config.err
)

if [ -s "$TEST_DIR/check_config.err" ]; then
    fail_with_output "--check-config with defaults must not write stderr"
fi

if ! grep -Eq '^configuration OK$' "$TEST_DIR/check_config.out"; then
    fail_with_output "--check-config with defaults must report success"
fi

assert_no_runtime_logs "--check-config defaults"

rm -f "$TEST_DIR/check_config.out" "$TEST_DIR/check_config.err"

cat > "$TEST_DIR/valid.conf" <<'EOF'
output_enabled=true
output_format=jsonl
output_buffer_size=65536
event_engine=core
workspace_id=cli-test-workspace
ledger_session_id=cli-test-ledger
EOF

(
    cd "$TEST_DIR"
    ALFRED_CONFIG="$TEST_DIR/valid.conf" \
        "$ALFRED_BIN" --check-config > check_config.out 2> check_config.err
)

if [ -s "$TEST_DIR/check_config.err" ]; then
    fail_with_output "--check-config with valid ALFRED_CONFIG must not write stderr"
fi

if ! grep -Eq '^configuration OK$' "$TEST_DIR/check_config.out"; then
    fail_with_output "--check-config with valid ALFRED_CONFIG must report success"
fi

assert_no_runtime_logs "--check-config valid config"

rm -f "$TEST_DIR/check_config.out" "$TEST_DIR/check_config.err"

cat > "$TEST_DIR/invalid.conf" <<'EOF'
output_format=protobuf
EOF

if (
    cd "$TEST_DIR"
    ALFRED_CONFIG="$TEST_DIR/invalid.conf" \
        "$ALFRED_BIN" --check-config > check_config.out 2> check_config.err
); then
    fail_with_output "--check-config with invalid ALFRED_CONFIG must fail"
fi

if [ -s "$TEST_DIR/check_config.out" ]; then
    fail_with_output "--check-config invalid ALFRED_CONFIG must not write stdout"
fi

if ! grep -Eq '^invalid ALFRED_CONFIG=.*/invalid\.conf$' \
    "$TEST_DIR/check_config.err"; then
    fail_with_output "--check-config invalid ALFRED_CONFIG missing stderr"
fi

assert_no_runtime_logs "--check-config invalid config"

rm -f "$TEST_DIR/check_config.out" "$TEST_DIR/check_config.err"

if (
    cd "$TEST_DIR"
    ALFRED_EVENT_ENGINE=shadow \
        "$ALFRED_BIN" --check-config > check_config.out 2> check_config.err
); then
    fail_with_output "--check-config with invalid ALFRED_EVENT_ENGINE must fail"
fi

if [ -s "$TEST_DIR/check_config.out" ]; then
    fail_with_output "--check-config invalid ALFRED_EVENT_ENGINE must not write stdout"
fi

if ! grep -Eq '^invalid ALFRED_EVENT_ENGINE=shadow \(expected core\)$' \
    "$TEST_DIR/check_config.err"; then
    fail_with_output "--check-config invalid ALFRED_EVENT_ENGINE missing stderr"
fi

assert_no_runtime_logs "--check-config invalid event engine"

echo "PASS cli help version check-config"
