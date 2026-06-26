#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/exploratory/nightly/nightly_lib.sh
. "$SCRIPT_DIR/nightly_lib.sh"

NAME=audit-raw-events
DIR=$(nightly_new_dir "$NAME")
ROOT="$DIR/root"
mkdir -p "$ROOT"
printf 'audit\n' > "$ROOT/readme.txt"
: > "$DIR/assertions.log"

cat > "$DIR/alfred.conf" <<CFG
inotify_audit_events=open,access,close-nowrite
output_enabled=false
CFG

nightly_start_alfred "$DIR" "$DIR/alfred.conf" "$ROOT"
cat "$ROOT/readme.txt" >/dev/null
sleep 1
nightly_stop_alfred "$DIR"

status=0
nightly_assert_grep "$DIR" raw.log 'IN_OPEN .*readme\.txt' \
    'audit open raw' || status=1
nightly_assert_grep "$DIR" raw.log 'IN_ACCESS .*readme\.txt' \
    'audit access raw' || status=1
nightly_assert_grep "$DIR" raw.log 'IN_CLOSE_NOWRITE .*readme\.txt' \
    'audit close-nowrite raw' || status=1
nightly_assert_not_grep "$DIR" events.log 'FILE_READY path=.*/readme\.txt' \
    'audit close no semantic ready' || status=1

nightly_finish "$NAME" "$DIR" "$status"
