#!/usr/bin/env bash

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_audit_config_invalid}"
CONFIG_FILE="./invalid_audit_events.conf"

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ALFRED_BIN="${ALFRED_BIN:-$ROOT_DIR/alfred}"

source "$ROOT_DIR/tests/core/test_lib.sh"

# Expected contract:
#
# config:
# - inotify_audit_events=IN_OPEN
#
# result:
# - startup fails with invalid ALFRED_CONFIG
#
# Meaning:
# audit configuration deliberately uses policy names such as "open", not raw
# IN_* tokens. IN_OPEN remains invalid inside inotify_watch_mask and also
# invalid as an audit policy token.

cleanup_test() {
    cleanup
    rm -f "$CONFIG_FILE"
    rm -f ./invalid_audit_config.out ./invalid_audit_config.err
}

trap cleanup_test EXIT

reset_env

cat > "$CONFIG_FILE" <<'EOF'
inotify_audit_events=IN_OPEN
EOF

if ALFRED_CONFIG="$PWD/$CONFIG_FILE" timeout 3s "$ALFRED_BIN" "$TEST_ROOT" \
    > ./invalid_audit_config.out 2> ./invalid_audit_config.err; then

    echo "FAIL: invalid inotify_audit_events token should reject startup"
    echo "----- stdout -----"
    cat ./invalid_audit_config.out || true
    echo "----- stderr -----"
    cat ./invalid_audit_config.err || true
    exit 1
fi

if ! grep -Eq "invalid ALFRED_CONFIG=" ./invalid_audit_config.err; then
    echo "FAIL: missing invalid audit config diagnostic"
    echo "----- stderr -----"
    cat ./invalid_audit_config.err || true
    exit 1
fi

echo "PASS backend invalid audit config token"
