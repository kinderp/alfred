#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - one diagnostic WATCH_ADDED record exists for lost-jsonl
# - one diagnostic WATCH_STALE record exists for lost-jsonl with
#   watch.reason=IN_MOVE_SELF
# - one diagnostic WATCH_RESYNC_BEGIN record exists for lost-jsonl
# - one diagnostic WATCH_RESYNC_FAILED record exists for lost-jsonl with
#   watch.error=path-unreachable
# - one diagnostic WATCH_LOST_QUEUED record exists for lost-jsonl with
#   watch.error=path-unreachable and recovery.pending_count >= 1
# - one diagnostic WATCH_STALE_EVENT_DROPPED record exists for the child event
#   named proof-after-move.txt
# - no normalized raw or semantic filesystem record exists for
#   proof-after-move.txt, because the old watched path is stale
#
# Compatibility logs expected in parallel:
# - events.log still contains WATCH_ADDED, WATCH_STALE, WATCH_RESYNC_BEGIN,
#   WATCH_RESYNC_FAILED, WATCH_LOST_QUEUED and WATCH_STALE_EVENT_DROPPED
#
# Meaning:
# This golden test fixes the structured diagnostic contract for a watched
# directory that receives IN_MOVE_SELF and can no longer be trusted at its old
# path. The core must not invent a semantic move; the backend reports stale
# state, local resync failure and delayed lost-scope recovery work.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_test_self_move_recovery}"
MOVE_TARGET="${TEST_ROOT}_moved"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-self-move.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

source ../core/test_lib.sh

cleanup_jsonl_self_move() {
    stop_alfred
    rm -rf "$TEST_ROOT" "$MOVE_TARGET"

    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
        rm -f "$CONFIG_FILE" "$OUTPUT_LOG" ./raw.log ./events.log ./errors.log
    fi
}

fail_with_all_logs() {
    local message="$1"

    echo "FAIL: $message"
    echo "----- raw.log -----"
    cat ./raw.log || true
    echo "----- events.log -----"
    cat ./events.log || true
    echo "----- errors.log -----"
    cat ./errors.log || true
    echo "----- output.jsonl -----"
    cat "$OUTPUT_LOG" || true
    exit 1
}

trap cleanup_jsonl_self_move EXIT

reset_env
rm -rf "$MOVE_TARGET"
: > "$OUTPUT_LOG"

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$OUTPUT_LOG
EOF

ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

sleep 1

mkdir "$TEST_ROOT/lost-jsonl"
sleep 1
mv "$TEST_ROOT/lost-jsonl" "$MOVE_TARGET"

# The inotify watch follows the moved directory object, but Alfred's wd -> path
# mapping is stale. Creating a file at the new path gives the backend a chance
# to log WATCH_STALE_EVENT_DROPPED without inventing old-path semantics.
touch "$MOVE_TARGET/proof-after-move.txt"
sleep 1

stop_alfred
ALFRED_PID=""

if ! grep -Eq "WATCH_ADDED wd=[0-9]+ path=.*/lost-jsonl" ./events.log; then
    fail_with_all_logs "missing compatibility WATCH_ADDED for lost-jsonl"
fi

if ! grep -Eq "WATCH_STALE wd=[0-9]+ path=.*/lost-jsonl reason=IN_MOVE_SELF" ./events.log; then
    fail_with_all_logs "missing compatibility WATCH_STALE for lost-jsonl"
fi

if ! grep -Eq "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/lost-jsonl reason=IN_MOVE_SELF" ./events.log; then
    fail_with_all_logs "missing compatibility WATCH_RESYNC_BEGIN for lost-jsonl"
fi

if ! grep -Eq "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/lost-jsonl reason=IN_MOVE_SELF error=path-unreachable" ./events.log; then
    fail_with_all_logs "missing compatibility WATCH_RESYNC_FAILED for lost-jsonl"
fi

if ! grep -Eq "WATCH_LOST_QUEUED wd=[0-9]+ path=.*/lost-jsonl reason=IN_MOVE_SELF error=path-unreachable pending=1" ./events.log; then
    fail_with_all_logs "missing compatibility WATCH_LOST_QUEUED for lost-jsonl"
fi

if ! grep -Eq "WATCH_STALE_EVENT_DROPPED wd=[0-9]+ path=.*/lost-jsonl .*name=proof-after-move.txt" ./events.log; then
    fail_with_all_logs "missing compatibility WATCH_STALE_EVENT_DROPPED for proof-after-move.txt"
fi

if grep -Eq "DIR_MOVED|DIR_RENAMED|DIR_RELOCATED" ./events.log; then
    fail_with_all_logs "self move unexpectedly produced semantic directory move"
fi

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root = sys.argv[2]
lost_path = f"{root}/lost-jsonl"

records = []
with open(output_log, "r", encoding="utf-8") as handle:
    for line_number, line in enumerate(handle, 1):
        line = line.rstrip("\n")
        if not line:
            raise SystemExit(f"empty JSONL line at {line_number}")
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            raise SystemExit(f"invalid JSONL line {line_number}: {exc}") from exc
        if record.get("schema_version") != 0:
            raise SystemExit(
                f"line {line_number} has unexpected schema_version="
                f"{record.get('schema_version')!r}"
            )
        records.append(record)


def find_record(record_type, path=lost_path):
    matches = [
        record
        for record in records
        if record.get("layer") == "diagnostic"
        and record.get("category") in {"watch", "recovery"}
        and record.get("type") == record_type
        and record.get("path") == path
    ]
    if not matches:
        raise SystemExit(f"missing JSONL diagnostic record {record_type}/{path}")
    return matches[0]


def require_watch_field(record, key, value):
    watch = record.get("watch")
    if not isinstance(watch, dict):
        raise SystemExit(f"{record.get('type')} has no watch object")
    if watch.get(key) != value:
        raise SystemExit(
            f"{record.get('type')} watch.{key}={watch.get(key)!r}, "
            f"expected {value!r}"
        )


find_record("WATCH_ADDED")

stale = find_record("WATCH_STALE")
require_watch_field(stale, "reason", "IN_MOVE_SELF")

resync_begin = find_record("WATCH_RESYNC_BEGIN")
require_watch_field(resync_begin, "reason", "IN_MOVE_SELF")

resync_failed = find_record("WATCH_RESYNC_FAILED")
require_watch_field(resync_failed, "reason", "IN_MOVE_SELF")
require_watch_field(resync_failed, "error", "path-unreachable")

lost_queued = find_record("WATCH_LOST_QUEUED")
require_watch_field(lost_queued, "reason", "IN_MOVE_SELF")
require_watch_field(lost_queued, "error", "path-unreachable")

recovery = lost_queued.get("recovery")
if not isinstance(recovery, dict):
    raise SystemExit("WATCH_LOST_QUEUED has no recovery object")
pending_count = recovery.get("pending_count")
if not isinstance(pending_count, int) or pending_count < 1:
    raise SystemExit(
        f"WATCH_LOST_QUEUED recovery.pending_count={pending_count!r}, "
        "expected an integer >= 1"
    )

stale_child_drops = [
    record
    for record in records
    if record.get("layer") == "diagnostic"
    and record.get("category") == "watch"
    and record.get("type") == "WATCH_STALE_EVENT_DROPPED"
    and record.get("path") == lost_path
    and isinstance(record.get("watch"), dict)
    and record["watch"].get("event_name") == "proof-after-move.txt"
]
if not stale_child_drops:
    raise SystemExit(
        "missing WATCH_STALE_EVENT_DROPPED for proof-after-move.txt"
    )

stale_child_filesystem_records = [
    record
    for record in records
    if record.get("layer") in {"normalized_raw", "semantic"}
    and record.get("category") == "filesystem"
    and "proof-after-move.txt" in str(record.get("path", ""))
]
if stale_child_filesystem_records:
    raise SystemExit(
        "stale child event unexpectedly produced filesystem records"
    )

semantic_moves = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("type") in {"DIR_MOVED", "DIR_RENAMED", "DIR_RELOCATED"}
]
if semantic_moves:
    raise SystemExit(
        "self move unexpectedly produced semantic directory move records"
    )
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden self move recovery"
