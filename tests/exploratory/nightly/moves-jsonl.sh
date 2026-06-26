#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/exploratory/nightly/nightly_lib.sh
. "$SCRIPT_DIR/nightly_lib.sh"

NAME=moves-jsonl
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
mkdir "$ROOT/src" "$ROOT/dst"
sleep 1
printf 'a\n' > "$ROOT/old.txt"
sleep 1
mv "$ROOT/old.txt" "$ROOT/new.txt"
sleep 1
printf 'b\n' > "$ROOT/src/item.txt"
sleep 1
mv "$ROOT/src/item.txt" "$ROOT/dst/item.txt"
sleep 1
printf 'c\n' > "$ROOT/src/before.txt"
sleep 1
mv "$ROOT/src/before.txt" "$ROOT/dst/after.txt"
sleep 1
mkdir "$ROOT/old-dir"
sleep 1
mv "$ROOT/old-dir" "$ROOT/new-dir"
sleep 1
mkdir "$ROOT/src/dir-item"
sleep 1
mv "$ROOT/src/dir-item" "$ROOT/dst/dir-item"
sleep 1
mkdir "$ROOT/src/dir-before"
sleep 1
mv "$ROOT/src/dir-before" "$ROOT/dst/dir-after"
sleep 1
nightly_stop_alfred "$DIR"

status=0
nightly_validate_jsonl "$DIR" || status=1
nightly_assert_grep "$DIR" events.log \
    'FILE_RENAMED from=.*/old\.txt to=.*/new\.txt' \
    'file renamed' || status=1
nightly_assert_grep "$DIR" events.log \
    'FILE_MOVED from=.*/src/item\.txt to=.*/dst/item\.txt' \
    'file moved' || status=1
nightly_assert_grep "$DIR" events.log \
    'FILE_RELOCATED from=.*/src/before\.txt to=.*/dst/after\.txt' \
    'file relocated' || status=1
nightly_assert_grep "$DIR" events.log \
    'DIR_RENAMED from=.*/old-dir to=.*/new-dir' \
    'dir renamed' || status=1
nightly_assert_grep "$DIR" events.log \
    'DIR_MOVED from=.*/src/dir-item to=.*/dst/dir-item' \
    'dir moved' || status=1
nightly_assert_grep "$DIR" events.log \
    'DIR_RELOCATED from=.*/src/dir-before to=.*/dst/dir-after' \
    'dir relocated' || status=1

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
    "FILE_RENAMED",
    "FILE_MOVED",
    "FILE_RELOCATED",
    "DIR_RENAMED",
    "DIR_MOVED",
    "DIR_RELOCATED",
}
missing = needed - types
if missing:
    raise SystemExit(f"missing JSONL semantic move types: {sorted(missing)}")
PY

nightly_finish "$NAME" "$DIR" "$status"
