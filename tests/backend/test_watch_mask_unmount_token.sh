#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_watch_mask_unmount_token}"
CONFIG_FILE="./unmount_watch_mask.conf"

# Expected contract:
#
# config:
# - inotify_watch_mask=IN_UNMOUNT,IN_IGNORED
#
# process:
# - Alfred must accept IN_UNMOUNT as a supported backend diagnostic token.
# - The test does not try to generate IN_UNMOUNT because mounting/unmounting a
#   filesystem would require privileges and would make CI/environment behavior
#   fragile.
#
# Meaning:
# IN_UNMOUNT is part of the inotify backend observability contract, but it is
# not a core semantic event. Runtime tests for the actual kernel event belong to
# an integration environment that can safely create and unmount a test mount.

source ../core/test_lib.sh

cleanup_test() {
    cleanup
    rm -f "$CONFIG_FILE"
}

trap cleanup_test EXIT

cat > "$CONFIG_FILE" <<'EOF'
inotify_watch_mask=IN_UNMOUNT,IN_IGNORED
EOF

export ALFRED_CONFIG="$PWD/$CONFIG_FILE"
start_alfred_core
unset ALFRED_CONFIG

if ! kill -0 "$ALFRED_PID" 2>/dev/null; then
    echo "FAIL: Alfred exited after accepting IN_UNMOUNT watch mask"
    echo "----- errors.log -----"
    cat ./errors.log || true
    exit 1
fi

echo "PASS backend watch mask accepts unmount token"
