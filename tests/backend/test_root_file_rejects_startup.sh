#!/usr/bin/env bash

# Regression for issue #30.
#
# The inotify backend is directory-scoped: configured roots are directories and
# file events are observed through parent-directory watches. Passing a regular
# file as the startup root must fail during app_init(). It must not enter the
# event loop with zero installed watches, because that looks like a healthy
# Alfred process while observing nothing.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_root_file_rejects_startup}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./root-file-rejects-startup.conf"
REGULAR_FILE="$TEST_ROOT/not-a-dir.txt"

source ../core/test_lib.sh

cleanup_root_file_rejects_startup() {
    rm -rf "$TEST_ROOT"

    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
        rm -f "$CONFIG_FILE" ./root_file.out ./root_file.err
        rm -f ./raw.log ./events.log ./errors.log
    fi
}

trap cleanup_root_file_rejects_startup EXIT

reset_env
mkdir -p "$TEST_ROOT"
printf 'x\n' > "$REGULAR_FILE"

cat > "$CONFIG_FILE" <<'EOF'
output_enabled=false
EOF

set +e
ALFRED_CONFIG="$PWD/$CONFIG_FILE" \
    ALFRED_EVENT_ENGINE=core \
    timeout 3s "$ALFRED_BIN" "$REGULAR_FILE" \
    > ./root_file.out 2> ./root_file.err
status=$?
set -e

if [[ "$status" -eq 0 ]]; then
    echo "FAIL: regular-file root should reject startup"
    cat ./root_file.out || true
    cat ./root_file.err || true
    exit 1
fi

if [[ "$status" -eq 124 ]]; then
    echo "FAIL: regular-file root entered the event loop until timeout"
    echo "----- events.log -----"
    cat ./events.log || true
    echo "----- errors.log -----"
    cat ./errors.log || true
    echo "----- stderr -----"
    cat ./root_file.err || true
    exit 1
fi

if grep -Eq "event loop started" ./events.log 2>/dev/null; then
    echo "FAIL: regular-file root must fail before event loop startup"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

if grep -Eq "application startup complete" ./events.log 2>/dev/null; then
    echo "FAIL: regular-file root must not report successful startup"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

if ! grep -Eq "configured root is not a directory" ./errors.log; then
    echo "FAIL: missing regular-file root diagnostic"
    echo "----- errors.log -----"
    cat ./errors.log || true
    exit 1
fi

if grep -Eq "WATCH_ADDED" ./events.log 2>/dev/null; then
    echo "FAIL: regular-file root must not emit WATCH_ADDED"
    echo "----- events.log -----"
    cat ./events.log || true
    exit 1
fi

echo "PASS backend regular-file root rejects startup"
