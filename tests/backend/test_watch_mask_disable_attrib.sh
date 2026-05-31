#!/usr/bin/env bash

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_backend_test_watch_mask_disable_attrib}"
CONFIG_FILE="./disable_attrib.conf"

source ../core/test_lib.sh

cleanup_test() {
    cleanup
    rm -f "$CONFIG_FILE"
}

trap cleanup_test EXIT

cat > "$CONFIG_FILE" <<'EOF'
inotify_watch_mask=default,-IN_ATTRIB
EOF

export ALFRED_CONFIG="$PWD/$CONFIG_FILE"
start_alfred_core
unset ALFRED_CONFIG

touch "$TEST_ROOT/metadata.txt"
sleep 1

chmod 600 "$TEST_ROOT/metadata.txt"
sleep 1

if grep -Eq "IN_ATTRIB .*name=metadata.txt" ./raw.log; then
    echo "FAIL: IN_ATTRIB raw log should be disabled by config"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

if ! grep -Eq "IN_CREATE .*name=metadata.txt" ./raw.log; then
    echo "FAIL: configured mask should still include IN_CREATE"
    echo "----- raw.log -----"
    cat ./raw.log || true
    exit 1
fi

echo "PASS backend watch mask disables attrib"
