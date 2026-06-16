#!/usr/bin/env bash

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_audit_config}"
CONFIG_FILE="./audit_events.conf"

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

ALFRED_BIN="${ALFRED_BIN:-$ROOT_DIR/alfred}"

source "$ROOT_DIR/tests/core/test_lib.sh"

# Expected log contract:
#
# raw.log:
# - IN_OPEN ... name=read-only.txt
# - IN_ACCESS ... name=read-only.txt
# - IN_CLOSE_NOWRITE ... name=read-only.txt
#
# events.log:
# - no FILE_READY for read-only.txt
# - no FILE_MODIFIED for read-only.txt
#
# Meaning:
# inotify_audit_events is an explicit opt-in raw inotify subscription. It makes
# audit-style kernel facts visible in raw.log, but they still do not become
# Alfred raw/core semantic events.

cleanup_test() {
    cleanup
    rm -f "$CONFIG_FILE"
}

trap cleanup_test EXIT

reset_env

printf "audit\n" > "$TEST_ROOT/read-only.txt"

cat > "$CONFIG_FILE" <<'EOF'
inotify_audit_events=open,access,close-nowrite
EOF

ALFRED_CONFIG="$PWD/$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

sleep 1

exec 3< "$TEST_ROOT/read-only.txt"
read -r _line <&3
exec 3<&-

sleep 1

if ! grep -Eq "IN_OPEN .*name=read-only.txt" ./raw.log; then
    echo "FAIL: missing IN_OPEN raw log"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_ACCESS .*name=read-only.txt" ./raw.log; then
    echo "FAIL: missing IN_ACCESS raw log"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_CLOSE_NOWRITE .*name=read-only.txt" ./raw.log; then
    echo "FAIL: missing IN_CLOSE_NOWRITE raw log"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

assert_not_contains "FILE_READY path=.*/read-only.txt"
assert_not_contains "FILE_MODIFIED path=.*/read-only.txt"

echo "PASS backend audit config raw log"
