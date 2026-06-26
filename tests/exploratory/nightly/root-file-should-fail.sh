#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/exploratory/nightly/nightly_lib.sh
. "$SCRIPT_DIR/nightly_lib.sh"

NAME=root-file-should-fail
DIR=$(nightly_new_dir "$NAME")
ROOT_FILE="$DIR/not-a-dir.txt"
printf 'x\n' > "$ROOT_FILE"
: > "$DIR/assertions.log"

cat > "$DIR/alfred.conf" <<CFG
output_enabled=false
CFG

(
    cd "$DIR" || exit 1
    timeout 3s env \
        ALFRED_CONFIG="$DIR/alfred.conf" \
        ALFRED_EVENT_ENGINE=core \
        "$NIGHTLY_ALFRED" "$ROOT_FILE" >alfred.stdout 2>alfred.stderr
    echo $? > exit.status
)

rc=$(cat "$DIR/exit.status")
status=0

# This is the expected future contract: a regular file root must be rejected
# immediately. While issue #30 is open, Alfred times out because it starts the
# event loop instead of failing startup.
if [[ "$rc" == 124 ]]; then
    printf 'FAIL regular-file root timed out instead of failing startup\n' \
        >> "$DIR/assertions.log"
    status=1
elif [[ "$rc" == 0 ]]; then
    printf 'FAIL regular-file root exited successfully\n' \
        >> "$DIR/assertions.log"
    status=1
fi

nightly_assert_not_grep "$DIR" events.log 'WATCH_ADDED' \
    'file root no watch added' || status=1

nightly_finish "$NAME" "$DIR" "$status"
