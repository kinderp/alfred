#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/exploratory/nightly/nightly_lib.sh
. "$SCRIPT_DIR/nightly_lib.sh"

NAME=base-jsonl-lifecycle
DIR=$(nightly_new_dir "$NAME")
ROOT="$DIR/root"
mkdir -p "$ROOT"
: > "$DIR/assertions.log"

cat > "$DIR/alfred.conf" <<CFG
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$DIR/output.jsonl
CFG

nightly_start_alfred "$DIR" "$DIR/alfred.conf" "$ROOT"
printf 'hello\n' > "$ROOT/file.txt"
sleep 1
printf 'world\n' >> "$ROOT/file.txt"
sleep 1
chmod 600 "$ROOT/file.txt"
sleep 1
mkdir "$ROOT/dir"
sleep 1
rm "$ROOT/file.txt"
sleep 1
rmdir "$ROOT/dir"
sleep 1
nightly_stop_alfred "$DIR"

status=0
nightly_validate_jsonl "$DIR" || status=1
nightly_assert_grep "$DIR" events.log 'FILE_CREATED path=.*/file\.txt' \
    'file created semantic' || status=1
nightly_assert_grep "$DIR" events.log 'FILE_MODIFIED path=.*/file\.txt' \
    'file modified semantic' || status=1
nightly_assert_grep "$DIR" events.log 'FILE_READY path=.*/file\.txt' \
    'file ready semantic' || status=1
nightly_assert_grep "$DIR" raw.log 'IN_ATTRIB .*file\.txt' \
    'attrib raw log' || status=1
nightly_assert_grep "$DIR" events.log 'DIR_CREATED path=.*/dir' \
    'dir created semantic' || status=1
nightly_assert_grep "$DIR" events.log 'FILE_DELETED path=.*/file\.txt' \
    'file deleted semantic' || status=1
nightly_assert_grep "$DIR" events.log 'DIR_DELETED path=.*/dir' \
    'dir deleted semantic' || status=1

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
    "RAW_CREATE",
    "RAW_MODIFY",
    "RAW_CLOSE_WRITE",
    "RAW_ATTRIB",
    "RAW_DELETE",
    "FILE_CREATED",
    "FILE_MODIFIED",
    "FILE_READY",
    "FILE_DELETED",
    "DIR_CREATED",
    "DIR_DELETED",
}
missing = needed - types
if missing:
    raise SystemExit(f"missing JSONL types: {sorted(missing)}")
PY

nightly_finish "$NAME" "$DIR" "$status"
