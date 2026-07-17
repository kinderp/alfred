#!/usr/bin/env bash

# MVP smoke test:
# - verifies the minimal CLI commands
# - runs Alfred on a temporary directory with JSONL enabled
# - creates representative filesystem activity
# - checks compatibility logs and output.jsonl
#
# This is not a stress test, benchmark, or full regression suite. It is the
# shortest user-facing proof that the current MVP can build, start, observe
# events, and write both compatibility logs and structured output.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ALFRED_BIN="${ALFRED_BIN:-$ROOT_DIR/alfred}"
RUN_DIR="$(mktemp -d "${TMPDIR:-/tmp}/alfred_mvp_smoke.XXXXXX")"
WATCH_ROOT="$RUN_DIR/watch"
CONFIG_FILE="$RUN_DIR/alfred-smoke.conf"
ALFRED_PID=""

cleanup() {
    if [[ -n "$ALFRED_PID" ]] && kill -0 "$ALFRED_PID" 2>/dev/null; then
        kill -INT "$ALFRED_PID"
        wait "$ALFRED_PID" || true
    fi

    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
        rm -rf "$RUN_DIR"
    else
        printf "Kept MVP smoke artifacts in %s\n" "$RUN_DIR"
    fi
}

fail_with_artifacts() {
    local message="$1"

    printf "FAIL: %s\n" "$message"
    printf "%s\n" "----- run dir -----"
    find "$RUN_DIR" -maxdepth 2 -type f -print 2>/dev/null || true
    printf "%s\n" "----- raw.log -----"
    cat "$RUN_DIR/raw.log" 2>/dev/null || true
    printf "%s\n" "----- events.log -----"
    cat "$RUN_DIR/events.log" 2>/dev/null || true
    printf "%s\n" "----- errors.log -----"
    cat "$RUN_DIR/errors.log" 2>/dev/null || true
    printf "%s\n" "----- output.jsonl -----"
    cat "$RUN_DIR/output.jsonl" 2>/dev/null || true
    exit 1
}

assert_file_contains() {
    local file="$1"
    local pattern="$2"
    local message="$3"

    if ! grep -Eq "$pattern" "$file"; then
        fail_with_artifacts "$message"
    fi
}

trap cleanup EXIT

mkdir -p "$WATCH_ROOT"

"$ALFRED_BIN" --help >/dev/null
"$ALFRED_BIN" --version >/dev/null
"$ALFRED_BIN" --check-config >/dev/null

cat > "$CONFIG_FILE" <<EOF
output_enabled=true
output_format=jsonl
output_buffer_size=65536
output_log=output.jsonl
event_engine=core
EOF

pushd "$RUN_DIR" >/dev/null
ALFRED_CONFIG="$CONFIG_FILE" "$ALFRED_BIN" "$WATCH_ROOT" >/dev/null 2>&1 &
ALFRED_PID="$!"
printf "%s\n" "$ALFRED_PID" > alfred.pid
popd >/dev/null
sleep 1

printf "hello smoke\n" > "$WATCH_ROOT/smoke.txt"
sleep 1
mv "$WATCH_ROOT/smoke.txt" "$WATCH_ROOT/smoke-renamed.txt"
mkdir "$WATCH_ROOT/smoke-dir"
sleep 1

if ! kill -0 "$ALFRED_PID" 2>/dev/null; then
    fail_with_artifacts "Alfred exited before the smoke workload completed"
fi

kill -INT "$ALFRED_PID"
wait "$ALFRED_PID" || true
ALFRED_PID=""

if [[ ! -s "$RUN_DIR/raw.log" ]]; then
    fail_with_artifacts "raw.log is missing or empty"
fi

if [[ ! -s "$RUN_DIR/events.log" ]]; then
    fail_with_artifacts "events.log is missing or empty"
fi

if [[ ! -s "$RUN_DIR/output.jsonl" ]]; then
    fail_with_artifacts "output.jsonl is missing or empty"
fi

assert_file_contains \
    "$RUN_DIR/raw.log" \
    "RAW_CREATE path=.*/smoke.txt" \
    "raw.log missing RAW_CREATE for smoke.txt"

assert_file_contains \
    "$RUN_DIR/events.log" \
    "FILE_CREATED path=.*/smoke.txt" \
    "events.log missing FILE_CREATED for smoke.txt"

assert_file_contains \
    "$RUN_DIR/events.log" \
    "FILE_READY path=.*/smoke.txt" \
    "events.log missing FILE_READY for smoke.txt"

assert_file_contains \
    "$RUN_DIR/events.log" \
    "FILE_RENAMED from=.*/smoke.txt to=.*/smoke-renamed.txt" \
    "events.log missing FILE_RENAMED for smoke.txt"

assert_file_contains \
    "$RUN_DIR/events.log" \
    "DIR_CREATED path=.*/smoke-dir" \
    "events.log missing DIR_CREATED for smoke-dir"

if ! python3 - "$RUN_DIR/output.jsonl" "$WATCH_ROOT" <<'PY'
import json
import sys

output_log = sys.argv[1]
watch_root = sys.argv[2]

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


def has_record(layer, category, record_type, path_suffix=None):
    expected_path = None
    if path_suffix is not None:
        expected_path = f"{watch_root}/{path_suffix}"

    for record in records:
        if (
            record.get("layer") == layer
            and record.get("category") == category
            and record.get("type") == record_type
        ):
            if expected_path is None or record.get("path") == expected_path:
                return True
    return False


def has_rename_record(old_path_suffix, new_path_suffix):
    expected_old_path = f"{watch_root}/{old_path_suffix}"
    expected_new_path = f"{watch_root}/{new_path_suffix}"

    for record in records:
        if (
            record.get("layer") == "semantic"
            and record.get("category") == "filesystem"
            and record.get("type") == "FILE_RENAMED"
            and record.get("old_path") == expected_old_path
            and record.get("new_path") == expected_new_path
        ):
            return True
    return False


required = [
    ("normalized_raw", "filesystem", "RAW_CREATE", "smoke.txt"),
    ("semantic", "filesystem", "FILE_CREATED", "smoke.txt"),
    ("semantic", "filesystem", "FILE_READY", "smoke.txt"),
    ("semantic", "filesystem", "DIR_CREATED", "smoke-dir"),
]

missing = [
    f"{layer}/{category}/{record_type}/{path_suffix}"
    for layer, category, record_type, path_suffix in required
    if not has_record(layer, category, record_type, path_suffix)
]

if missing:
    raise SystemExit("missing JSONL records: " + ", ".join(missing))

if not has_rename_record("smoke.txt", "smoke-renamed.txt"):
    raise SystemExit(
        "missing JSONL rename record: "
        "semantic/filesystem/FILE_RENAMED/smoke.txt->smoke-renamed.txt"
    )
PY
then
    fail_with_artifacts "output.jsonl structural validation failed"
fi

printf "PASS mvp smoke test\n"
