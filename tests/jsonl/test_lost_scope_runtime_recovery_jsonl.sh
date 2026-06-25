#!/usr/bin/env bash

# Expected output.jsonl contract:
# - every line in output.jsonl is valid JSON
# - every checked record uses schema_version=0
# - root A and root B emit WATCH_ADDED diagnostics
# - the child directory under root A emits WATCH_ADDED before it is moved
# - IN_MOVE_SELF on the child emits WATCH_STALE for the old root-A path
# - local resync emits WATCH_RESYNC_FAILED with watch.error=path-unreachable
# - lost-scope handoff emits WATCH_LOST_QUEUED with recovery.pending_count >= 1
# - delayed recovery emits WATCH_LOST_SCAN_BEGIN for root A; because
#   pending_count is zero, JSONL may omit the recovery object
# - root A scan emits WATCH_LOST_NOT_FOUND for the old path
# - delayed recovery emits WATCH_LOST_SCAN_BEGIN for root B; because
#   pending_count is zero, JSONL may omit the recovery object
# - root B scan emits WATCH_LOST_FOUND with old_path=rootA/lost and
#   new_path=rootB/lost
# - successful recovery emits WATCH_LOST_RECOVERY_END with watch.state=valid
# - the parent-level move pair emits exactly one semantic DIR_MOVED with
#   old_path=rootA/lost and new_path=rootB/lost
# - creating proof.txt after recovery emits RAW_CREATE and FILE_CREATED under
#   the recovered root-B path
# - no FILE_CREATED is emitted for proof.txt under the stale root-A path
#
# Compatibility logs expected in parallel:
# - raw.log still contains the watched child IN_CREATE and child IN_MOVE_SELF
# - events.log still contains the same watch/resync/lost-scope diagnostics
# - events.log still contains DIR_MOVED for rootA/lost -> rootB/lost
# - events.log still contains FILE_CREATED for rootB/lost/proof.txt
#
# Meaning:
# This golden test fixes the structured public contract for the complete
# delayed lost-scope runtime recovery. Alfred starts with two configured roots,
# creates a watched directory under root A, then the directory is moved to root
# B. The child watch receives IN_MOVE_SELF, the old path is unreachable, and
# local resync queues wide recovery. The runtime poll loop first searches root
# A, does not find the saved identity there, then searches root B, finds the
# same filesystem object, updates the watcher-table path, and proves the
# recovered watch is active by observing a later file creation. Because both
# configured roots are watched, the ordinary parent-level move pair can also
# produce a DIR_MOVED semantic record for the same filesystem operation.

set -euo pipefail

TEST_ROOT="${TEST_ROOT:-/tmp/alfred_jsonl_lost_scope_root_a}"
SECOND_ROOT="${SECOND_ROOT:-/tmp/alfred_jsonl_lost_scope_root_b}"
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
CONFIG_FILE="./jsonl-lost-scope-runtime.conf"
OUTPUT_LOG="./output.jsonl"
ALFRED_PID=""

regex_escape() {
    printf '%s' "$1" | sed -e 's/[][\\.^$*+?{}()|]/\\&/g'
}

TEST_ROOT_RE="$(regex_escape "$TEST_ROOT")"
SECOND_ROOT_RE="$(regex_escape "$SECOND_ROOT")"
OLD_PATH_RE="${TEST_ROOT_RE}/lost"
NEW_PATH_RE="${SECOND_ROOT_RE}/lost"
PROOF_PATH_RE="${NEW_PATH_RE}/proof.txt"

source ../core/test_lib.sh

cleanup_jsonl_lost_scope_runtime() {
    stop_alfred
    rm -rf "$TEST_ROOT" "$SECOND_ROOT"

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

assert_compat_contains() {
    local pattern="$1"
    local file="$2"

    if ! grep -Eq "$pattern" "$file"; then
        fail_with_all_logs "missing pattern in $file: $pattern"
    fi
}

assert_compat_not_contains() {
    local pattern="$1"
    local file="$2"

    if grep -Eq "$pattern" "$file"; then
        fail_with_all_logs "unexpected pattern in $file: $pattern"
    fi
}

trap cleanup_jsonl_lost_scope_runtime EXIT

reset_env
rm -rf "$SECOND_ROOT"
mkdir -p "$SECOND_ROOT"
: > "$OUTPUT_LOG"

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=$OUTPUT_LOG
EOF

ALFRED_CONFIG="$CONFIG_FILE" \
ALFRED_EVENT_ENGINE=core \
    "$ALFRED_BIN" "$TEST_ROOT" "$SECOND_ROOT" >/dev/null 2>&1 &
ALFRED_PID=$!

sleep 1

mkdir "$TEST_ROOT/lost"
sleep 1

mv "$TEST_ROOT/lost" "$SECOND_ROOT/lost"
sleep 2

touch "$SECOND_ROOT/lost/proof.txt"
sleep 1

stop_alfred
ALFRED_PID=""

assert_compat_contains \
    "IN_CREATE IN_ISDIR .*path=${TEST_ROOT_RE} name=lost" \
    ./raw.log
assert_compat_contains \
    "IN_MOVE_SELF .*path=${OLD_PATH_RE} name=" \
    ./raw.log

assert_compat_contains \
    "WATCH_ADDED wd=[0-9]+ path=${TEST_ROOT_RE}$" \
    ./events.log
assert_compat_contains \
    "WATCH_ADDED wd=[0-9]+ path=${SECOND_ROOT_RE}$" \
    ./events.log
assert_compat_contains \
    "WATCH_ADDED wd=[0-9]+ path=${OLD_PATH_RE}$" \
    ./events.log
assert_compat_contains \
    "DIR_MOVED from=${OLD_PATH_RE} to=${NEW_PATH_RE}" \
    ./events.log
assert_compat_contains \
    "WATCH_STALE wd=[0-9]+ path=${OLD_PATH_RE} reason=IN_MOVE_SELF" \
    ./events.log
assert_compat_contains \
    "WATCH_RESYNC_FAILED wd=[0-9]+ path=${OLD_PATH_RE} reason=IN_MOVE_SELF error=path-unreachable" \
    ./events.log
assert_compat_contains \
    "WATCH_LOST_QUEUED wd=[0-9]+ path=${OLD_PATH_RE} reason=IN_MOVE_SELF error=path-unreachable pending=1" \
    ./events.log
assert_compat_contains \
    "WATCH_LOST_SCAN_BEGIN root=${TEST_ROOT_RE} pending=0" \
    ./events.log
assert_compat_contains \
    "WATCH_LOST_NOT_FOUND wd=[0-9]+ path=${OLD_PATH_RE} reason=IN_MOVE_SELF retry=0" \
    ./events.log
assert_compat_contains \
    "WATCH_LOST_SCAN_BEGIN root=${SECOND_ROOT_RE} pending=0" \
    ./events.log
assert_compat_contains \
    "WATCH_LOST_FOUND wd=[0-9]+ old_path=${OLD_PATH_RE} new_path=${NEW_PATH_RE} reason=IN_MOVE_SELF" \
    ./events.log
assert_compat_contains \
    "WATCH_LOST_RECOVERY_END wd=[0-9]+ path=${NEW_PATH_RE} reason=IN_MOVE_SELF result=valid" \
    ./events.log
assert_compat_contains \
    "FILE_CREATED path=${PROOF_PATH_RE}" \
    ./events.log
assert_compat_not_contains \
    "FILE_CREATED path=${OLD_PATH_RE}/proof.txt" \
    ./events.log
assert_compat_not_contains \
    "WATCH_LOST_RETRY_SCHEDULED wd=[0-9]+ path=${OLD_PATH_RE}" \
    ./events.log
assert_compat_not_contains \
    "WATCH_LOST_RECOVERY_GAVE_UP wd=[0-9]+ path=${OLD_PATH_RE}" \
    ./events.log

if [[ ! -s "$OUTPUT_LOG" ]]; then
    fail_with_all_logs "output.jsonl is empty"
fi

if ! python3 - "$OUTPUT_LOG" "$TEST_ROOT" "$SECOND_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
root_a = sys.argv[2]
root_b = sys.argv[3]
old_path = f"{root_a}/lost"
new_path = f"{root_b}/lost"
proof_path = f"{new_path}/proof.txt"
stale_proof_path = f"{old_path}/proof.txt"

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


def is_diag(record, record_type, path=None, category=None):
    if record.get("layer") != "diagnostic":
        return False
    if category is not None and record.get("category") != category:
        return False
    if record.get("type") != record_type:
        return False
    if path is not None and record.get("path") != path:
        return False
    return True


def find_diag(record_type, path=None, category=None):
    for index, record in enumerate(records):
        if is_diag(record, record_type, path=path, category=category):
            return index, record
    raise SystemExit(f"missing diagnostic record {record_type}/{path}")


def require_watch_field(record, key, value):
    watch = record.get("watch")
    if not isinstance(watch, dict):
        raise SystemExit(f"{record.get('type')} has no watch object")
    if watch.get(key) != value:
        raise SystemExit(
            f"{record.get('type')} watch.{key}={watch.get(key)!r}, "
            f"expected {value!r}"
        )


def require_recovery_field(record, key, predicate, description):
    recovery = record.get("recovery")
    if not isinstance(recovery, dict):
        raise SystemExit(f"{record.get('type')} has no recovery object")
    value = recovery.get(key)
    if not predicate(value):
        raise SystemExit(
            f"{record.get('type')} recovery.{key}={value!r}, "
            f"expected {description}"
        )


def require_order(first, second):
    if first >= second:
        raise SystemExit(
            f"wrong JSONL order: {records[first].get('type')} must precede "
            f"{records[second].get('type')}"
        )


root_a_added_idx, _ = find_diag("WATCH_ADDED", path=root_a, category="watch")
root_b_added_idx, _ = find_diag("WATCH_ADDED", path=root_b, category="watch")
child_added_idx, _ = find_diag("WATCH_ADDED", path=old_path, category="watch")

dir_moves = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("category") == "filesystem"
    and record.get("type") == "DIR_MOVED"
    and record.get("old_path") == old_path
    and record.get("new_path") == new_path
]
if len(dir_moves) != 1:
    raise SystemExit(
        "expected exactly one semantic DIR_MOVED record with "
        f"old_path={old_path!r} and new_path={new_path!r}, "
        f"found {len(dir_moves)}"
    )
dir_moved_idx = records.index(dir_moves[0])

stale_idx, stale = find_diag("WATCH_STALE", path=old_path, category="watch")
require_watch_field(stale, "reason", "IN_MOVE_SELF")

resync_failed_idx, resync_failed = find_diag(
    "WATCH_RESYNC_FAILED", path=old_path, category="recovery"
)
require_watch_field(resync_failed, "reason", "IN_MOVE_SELF")
require_watch_field(resync_failed, "error", "path-unreachable")

queued_idx, queued = find_diag(
    "WATCH_LOST_QUEUED", path=old_path, category="recovery"
)
require_watch_field(queued, "reason", "IN_MOVE_SELF")
require_watch_field(queued, "error", "path-unreachable")
require_recovery_field(
    queued,
    "pending_count",
    lambda value: isinstance(value, int) and value >= 1,
    "integer >= 1",
)

scan_begin_indexes = [
    (index, record)
    for index, record in enumerate(records)
    if is_diag(record, "WATCH_LOST_SCAN_BEGIN", category="recovery")
]
scan_a_matches = [
    (index, record)
    for index, record in scan_begin_indexes
    if record.get("path") == root_a
]
scan_b_matches = [
    (index, record)
    for index, record in scan_begin_indexes
    if record.get("path") == root_b
]
if not scan_a_matches:
    raise SystemExit("missing WATCH_LOST_SCAN_BEGIN for root A")
if not scan_b_matches:
    raise SystemExit("missing WATCH_LOST_SCAN_BEGIN for root B")
scan_a_idx, scan_a = scan_a_matches[0]
scan_b_idx, scan_b = scan_b_matches[0]

not_found_idx, not_found = find_diag(
    "WATCH_LOST_NOT_FOUND", path=old_path, category="recovery"
)
require_watch_field(not_found, "reason", "IN_MOVE_SELF")

found_matches = [
    (index, record)
    for index, record in enumerate(records)
    if is_diag(record, "WATCH_LOST_FOUND", category="recovery")
    and record.get("old_path") == old_path
    and record.get("new_path") == new_path
]
if not found_matches:
    raise SystemExit("missing WATCH_LOST_FOUND with expected old/new path")
found_idx, found = found_matches[0]
require_watch_field(found, "reason", "IN_MOVE_SELF")

end_idx, end = find_diag(
    "WATCH_LOST_RECOVERY_END", path=new_path, category="recovery"
)
require_watch_field(end, "reason", "IN_MOVE_SELF")
require_watch_field(end, "state", "valid")

raw_proof = [
    record
    for record in records
    if record.get("layer") == "normalized_raw"
    and record.get("category") == "filesystem"
    and record.get("type") == "RAW_CREATE"
    and record.get("path") == proof_path
]
if not raw_proof:
    raise SystemExit("missing RAW_CREATE for proof.txt at recovered path")

semantic_proof = [
    record
    for record in records
    if record.get("layer") == "semantic"
    and record.get("category") == "filesystem"
    and record.get("type") == "FILE_CREATED"
    and record.get("path") == proof_path
]
if not semantic_proof:
    raise SystemExit("missing FILE_CREATED for proof.txt at recovered path")

stale_path_proof = [
    record
    for record in records
    if record.get("path") == stale_proof_path
    and record.get("type") in {"RAW_CREATE", "FILE_CREATED"}
]
if stale_path_proof:
    raise SystemExit("proof.txt was incorrectly emitted under stale root-A path")

for forbidden_type in {
    "WATCH_LOST_RETRY_SCHEDULED",
    "WATCH_LOST_RECOVERY_GAVE_UP",
}:
    forbidden = [
        record
        for record in records
        if is_diag(record, forbidden_type, path=old_path, category="recovery")
    ]
    if forbidden:
        raise SystemExit(f"unexpected {forbidden_type} for recovered scope")

require_order(root_a_added_idx, child_added_idx)
require_order(root_b_added_idx, queued_idx)
require_order(child_added_idx, stale_idx)
require_order(child_added_idx, dir_moved_idx)
require_order(stale_idx, resync_failed_idx)
require_order(resync_failed_idx, queued_idx)
require_order(queued_idx, scan_a_idx)
require_order(scan_a_idx, not_found_idx)
require_order(not_found_idx, scan_b_idx)
require_order(scan_b_idx, found_idx)
require_order(found_idx, end_idx)

proof_semantic_idx = records.index(semantic_proof[0])
require_order(end_idx, proof_semantic_idx)
PY
then
    fail_with_all_logs "JSONL structural validation failed"
fi

echo "PASS jsonl golden lost scope runtime recovery"
