#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_watch_mask_invalid_token}"
CONFIG_FILE="./invalid_watch_mask.conf"
UNSUPPORTED_CONFIG_FILE="./unsupported_watch_mask.conf"

source ../core/test_lib.sh

cleanup_test() {
    cleanup
    rm -f "$CONFIG_FILE" "$UNSUPPORTED_CONFIG_FILE"
    rm -f ./invalid_config.out ./invalid_config.err
    rm -f ./unsupported_config.out ./unsupported_config.err
}

trap cleanup_test EXIT

reset_env

cat > "$CONFIG_FILE" <<'EOF'
inotify_watch_mask=default,-IN_ATRIB
EOF

if ALFRED_CONFIG="$PWD/$CONFIG_FILE" timeout 3s "$ALFRED_BIN" "$TEST_ROOT" \
    > ./invalid_config.out 2> ./invalid_config.err; then

    echo "FAIL: invalid inotify_watch_mask token should reject startup"
    echo "----- stdout -----"
    cat ./invalid_config.out || true
    echo "----- stderr -----"
    cat ./invalid_config.err || true
    exit 1
fi

if ! grep -Eq "invalid ALFRED_CONFIG=" ./invalid_config.err; then
    echo "FAIL: missing invalid config diagnostic"
    echo "----- stderr -----"
    cat ./invalid_config.err || true
    exit 1
fi

cat > "$UNSUPPORTED_CONFIG_FILE" <<'EOF'
inotify_watch_mask=default,+IN_OPEN
EOF

if ALFRED_CONFIG="$PWD/$UNSUPPORTED_CONFIG_FILE" timeout 3s \
    "$ALFRED_BIN" "$TEST_ROOT" \
    > ./unsupported_config.out 2> ./unsupported_config.err; then

    echo "FAIL: unsupported inotify_watch_mask token should reject startup"
    echo "----- stdout -----"
    cat ./unsupported_config.out || true
    echo "----- stderr -----"
    cat ./unsupported_config.err || true
    exit 1
fi

if ! grep -Eq "invalid ALFRED_CONFIG=" ./unsupported_config.err; then
    echo "FAIL: missing unsupported config diagnostic"
    echo "----- stderr -----"
    cat ./unsupported_config.err || true
    exit 1
fi

echo "PASS backend invalid watch mask token"
