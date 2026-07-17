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
    echo "----- parser stdout -----"
    cat "$TEST_DIR/parser.out" 2>/dev/null || true
    echo "----- parser stderr -----"
    cat "$TEST_DIR/parser.err" 2>/dev/null || true
    echo "----- runtime stdout -----"
    cat "$TEST_DIR/runtime.out" 2>/dev/null || true
    echo "----- runtime stderr -----"
    cat "$TEST_DIR/runtime.err" 2>/dev/null || true
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

reset_outputs() {
    rm -f "$TEST_DIR"/help.out \
          "$TEST_DIR"/help.err \
          "$TEST_DIR"/version.out \
          "$TEST_DIR"/version.err \
          "$TEST_DIR"/check_config.out \
          "$TEST_DIR"/check_config.err \
          "$TEST_DIR"/parser.out \
          "$TEST_DIR"/parser.err \
          "$TEST_DIR"/runtime.out \
          "$TEST_DIR"/runtime.err \
          "$TEST_DIR"/raw.log \
          "$TEST_DIR"/events.log \
          "$TEST_DIR"/errors.log \
          "$TEST_DIR"/output.jsonl
}

assert_parser_rejects() {
    local name="$1"
    local expected="$2"
    shift 2

    reset_outputs

    if (
        cd "$TEST_DIR"
        "$ALFRED_BIN" "$@" > parser.out 2> parser.err
    ); then
        fail_with_output "$name must fail"
    fi

    if [ -s "$TEST_DIR/parser.out" ]; then
        fail_with_output "$name must not write stdout"
    fi

    if ! grep -Eq "$expected" "$TEST_DIR/parser.err"; then
        fail_with_output "$name missing expected parser error"
    fi

    assert_no_runtime_logs "$name"
}

assert_runtime_starts() {
    local name="$1"
    shift

    reset_outputs
    rm -rf "$TEST_DIR/watch-root"
    mkdir -p "$TEST_DIR/watch-root"

    (
        cd "$TEST_DIR"
        "$ALFRED_BIN" "$@" > runtime.out 2> runtime.err &
        pid=$!
        sleep 0.4
        kill -INT "$pid"
        wait "$pid"
    )

    if [ -s "$TEST_DIR/runtime.out" ]; then
        fail_with_output "$name must not write stdout"
    fi

    if [ -s "$TEST_DIR/runtime.err" ]; then
        fail_with_output "$name must not write stderr"
    fi

    if ! grep -Eq 'application startup complete' "$TEST_DIR/events.log"; then
        fail_with_output "$name must start the runtime"
    fi
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

reset_outputs

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

reset_outputs

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

reset_outputs

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

reset_outputs

(
    cd "$TEST_DIR"
    "$ALFRED_BIN" -c "$TEST_DIR/valid.conf" --check-config \
        > check_config.out 2> check_config.err
)

if [ -s "$TEST_DIR/check_config.err" ]; then
    fail_with_output "-c valid.conf --check-config must not write stderr"
fi

if ! grep -Eq '^configuration OK$' "$TEST_DIR/check_config.out"; then
    fail_with_output "-c valid.conf --check-config must report success"
fi

assert_no_runtime_logs "-c valid.conf --check-config"

reset_outputs

(
    cd "$TEST_DIR"
    "$ALFRED_BIN" --config "$TEST_DIR/valid.conf" --check-config \
        > check_config.out 2> check_config.err
)

if [ -s "$TEST_DIR/check_config.err" ]; then
    fail_with_output "--config valid.conf --check-config must not write stderr"
fi

if ! grep -Eq '^configuration OK$' "$TEST_DIR/check_config.out"; then
    fail_with_output "--config valid.conf --check-config must report success"
fi

assert_no_runtime_logs "--config valid.conf --check-config"

reset_outputs

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

reset_outputs

if (
    cd "$TEST_DIR"
    "$ALFRED_BIN" -c "$TEST_DIR/invalid.conf" --check-config \
        > check_config.out 2> check_config.err
); then
    fail_with_output "-c invalid.conf --check-config must fail"
fi

if [ -s "$TEST_DIR/check_config.out" ]; then
    fail_with_output "-c invalid.conf --check-config must not write stdout"
fi

if ! grep -Eq '^invalid --config=.*/invalid\.conf$' \
    "$TEST_DIR/check_config.err"; then
    fail_with_output "-c invalid.conf --check-config missing stderr"
fi

assert_no_runtime_logs "-c invalid.conf --check-config"

reset_outputs

(
    cd "$TEST_DIR"
    ALFRED_CONFIG="$TEST_DIR/invalid.conf" \
        "$ALFRED_BIN" -c "$TEST_DIR/valid.conf" --check-config \
        > check_config.out 2> check_config.err
)

if [ -s "$TEST_DIR/check_config.err" ]; then
    fail_with_output "explicit -c must win over invalid ALFRED_CONFIG"
fi

if ! grep -Eq '^configuration OK$' "$TEST_DIR/check_config.out"; then
    fail_with_output "explicit -c with invalid ALFRED_CONFIG must report success"
fi

assert_no_runtime_logs "explicit -c wins over ALFRED_CONFIG"

reset_outputs

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

assert_parser_rejects \
    "unknown option" \
    "^alfred: unknown option '--unknown'$" \
    --unknown

assert_parser_rejects \
    "missing -c value" \
    "^alfred: option requires a value '-c'$" \
    -c

assert_parser_rejects \
    "missing --config value" \
    "^alfred: option requires a value '--config'$" \
    --config

assert_parser_rejects \
    "duplicate -c" \
    "^alfred: duplicate configuration option '-c'$" \
    -c "$TEST_DIR/valid.conf" -c "$TEST_DIR/valid.conf" "$TEST_DIR/watch-root"

assert_parser_rejects \
    "duplicate --config" \
    "^alfred: duplicate configuration option '--config'$" \
    -c "$TEST_DIR/valid.conf" --config "$TEST_DIR/valid.conf" "$TEST_DIR/watch-root"

assert_parser_rejects \
    "--help with path" \
    "^alfred: paths are not allowed for '.*/watch-root'$" \
    --help "$TEST_DIR/watch-root"

assert_parser_rejects \
    "-c with --help" \
    "^alfred: option cannot be combined with '--help'$" \
    -c "$TEST_DIR/valid.conf" --help

assert_parser_rejects \
    "--version with path" \
    "^alfred: paths are not allowed for '.*/watch-root'$" \
    --version "$TEST_DIR/watch-root"

assert_parser_rejects \
    "-c with --version" \
    "^alfred: option cannot be combined with '--version'$" \
    -c "$TEST_DIR/valid.conf" --version

assert_parser_rejects \
    "--check-config with path" \
    "^alfred: paths are not allowed for '.*/watch-root'$" \
    --check-config "$TEST_DIR/watch-root"

assert_parser_rejects \
    "--check-config before -c" \
    "^alfred: option cannot follow no-runtime command '-c'$" \
    --check-config -c "$TEST_DIR/valid.conf"

assert_parser_rejects \
    "option after path" \
    "^alfred: unexpected option after path '--config'$" \
    "$TEST_DIR/watch-root" --config "$TEST_DIR/valid.conf"

assert_parser_rejects \
    "dash path without terminator" \
    "^alfred: unknown option '-tmp/root'$" \
    -tmp/root

assert_parser_rejects \
    "bare option terminator" \
    "^alfred: expected at least one path after '--'$" \
    --

assert_parser_rejects \
    "unsupported print config" \
    "^alfred: unsupported option '--print-config'$" \
    --print-config

assert_runtime_starts "-- path terminator runtime" -- "$TEST_DIR/watch-root"

echo "PASS cli help version check-config"
