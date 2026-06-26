#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/exploratory/nightly/nightly_lib.sh
. "$SCRIPT_DIR/nightly_lib.sh"

NAME=lost-scope-jsonl
DIR=$(nightly_new_dir "$NAME")
ROOT_A="$DIR/root-a"
ROOT_B="$DIR/root-b"
mkdir -p "$ROOT_A" "$ROOT_B"
: > "$DIR/assertions.log"

cat > "$DIR/alfred.conf" <<CFG
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$DIR/output.jsonl
CFG

nightly_start_alfred "$DIR" "$DIR/alfred.conf" "$ROOT_A" "$ROOT_B"
mkdir "$ROOT_A/lost"
sleep 1
mv "$ROOT_A/lost" "$ROOT_B/lost"
sleep 2
touch "$ROOT_B/lost/proof.txt"
sleep 1
nightly_stop_alfred "$DIR"

status=0
nightly_validate_jsonl "$DIR" || status=1
nightly_assert_grep "$DIR" events.log \
    'DIR_MOVED from=.*/root-a/lost to=.*/root-b/lost' \
    'parent dir moved' || status=1
nightly_assert_grep "$DIR" events.log \
    'WATCH_STALE wd=[0-9]+ path=.*/root-a/lost reason=IN_MOVE_SELF' \
    'watch stale' || status=1
nightly_assert_grep "$DIR" events.log \
    'WATCH_LOST_FOUND wd=[0-9]+ old_path=.*/root-a/lost new_path=.*/root-b/lost reason=IN_MOVE_SELF' \
    'lost found' || status=1
nightly_assert_grep "$DIR" events.log \
    'WATCH_LOST_RECOVERY_END wd=[0-9]+ path=.*/root-b/lost reason=IN_MOVE_SELF result=valid' \
    'lost recovery end' || status=1
nightly_assert_grep "$DIR" events.log \
    'FILE_CREATED path=.*/root-b/lost/proof\.txt' \
    'proof at recovered path' || status=1
nightly_assert_not_grep "$DIR" events.log \
    'FILE_CREATED path=.*/root-a/lost/proof\.txt' \
    'no stale proof event' || status=1

python3 - "$DIR/output.jsonl" <<'PY' || status=1
import json
import sys

records = [
    json.loads(line)
    for line in open(sys.argv[1], encoding="utf-8")
    if line.strip()
]
types = {record.get("type") for record in records}
needed = {
    "DIR_MOVED",
    "WATCH_STALE",
    "WATCH_RESYNC_FAILED",
    "WATCH_LOST_QUEUED",
    "WATCH_LOST_SCAN_BEGIN",
    "WATCH_LOST_NOT_FOUND",
    "WATCH_LOST_FOUND",
    "WATCH_LOST_RECOVERY_END",
    "RAW_CREATE",
    "FILE_CREATED",
}
missing = needed - types
if missing:
    raise SystemExit(f"missing JSONL lost-scope types: {sorted(missing)}")
PY

nightly_finish "$NAME" "$DIR" "$status"
