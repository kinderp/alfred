#!/usr/bin/env bash

set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=tests/exploratory/nightly/nightly_lib.sh
. "$SCRIPT_DIR/nightly_lib.sh"

NAME=recursive-fast-mkdir-p
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
mkdir -p "$ROOT/one/two/three"
sleep 2
nightly_stop_alfred "$DIR"

status=0
nightly_validate_jsonl "$DIR" || status=1
nightly_assert_grep "$DIR" events.log 'DIR_CREATED path=.*/one$' \
    'recursive one created' || status=1
nightly_assert_grep "$DIR" events.log 'DIR_CREATED path=.*/one/two$' \
    'recursive two created' || status=1
nightly_assert_grep "$DIR" events.log 'DIR_CREATED path=.*/one/two/three$' \
    'recursive three created' || status=1
nightly_assert_grep "$DIR" events.log \
    'WATCH_ADDED wd=[0-9]+ path=.*/one/two/three$' \
    'recursive deepest watch added' || status=1

python3 - "$DIR/output.jsonl" <<'PY' || status=1
import json
import sys

records = [
    json.loads(line)
    for line in open(sys.argv[1], encoding="utf-8")
    if line.strip()
]
paths = {record.get("path") for record in records if record.get("type") == "DIR_CREATED"}
needed_suffixes = ["/one", "/one/two", "/one/two/three"]
missing = [
    suffix
    for suffix in needed_suffixes
    if not any((path or "").endswith(suffix) for path in paths)
]
if missing:
    raise SystemExit(f"missing JSONL DIR_CREATED suffixes: {missing}")
PY

nightly_finish "$NAME" "$DIR" "$status"
