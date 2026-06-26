#!/usr/bin/env bash

set -u

NIGHTLY_REPO=${NIGHTLY_REPO:-/lab/progetto_3a_inf}
NIGHTLY_ALFRED=${NIGHTLY_ALFRED:-"$NIGHTLY_REPO/alfred"}

nightly_new_dir() {
    local name="$1"
    mktemp -d "/tmp/alfred-nightly-${name}.XXXXXX"
}

nightly_start_alfred() {
    local dir="$1"
    local config="$2"
    shift 2

    (
        cd "$dir" || exit 1
        ALFRED_CONFIG="$config" \
            ALFRED_EVENT_ENGINE=core \
            "$NIGHTLY_ALFRED" "$@" >alfred.stdout 2>alfred.stderr &
        echo $! > alfred.pid
    )
    sleep 1
}

nightly_stop_alfred() {
    local dir="$1"

    [[ -f "$dir/alfred.pid" ]] || return 0

    local pid
    pid=$(cat "$dir/alfred.pid")

    if kill -0 "$pid" 2>/dev/null; then
        kill -INT "$pid" 2>/dev/null || true
        for _ in 1 2 3 4 5 6 7 8 9 10; do
            kill -0 "$pid" 2>/dev/null || return 0
            sleep 0.2
        done
        kill -TERM "$pid" 2>/dev/null || true
    fi
}

nightly_assert_grep() {
    local dir="$1"
    local file="$2"
    local pattern="$3"
    local message="$4"

    if ! grep -Eq "$pattern" "$dir/$file"; then
        printf 'FAIL %s: missing %s in %s\n' \
            "$message" "$pattern" "$file" >> "$dir/assertions.log"
        return 1
    fi

    return 0
}

nightly_assert_not_grep() {
    local dir="$1"
    local file="$2"
    local pattern="$3"
    local message="$4"

    if grep -Eq "$pattern" "$dir/$file"; then
        printf 'FAIL %s: unexpected %s in %s\n' \
            "$message" "$pattern" "$file" >> "$dir/assertions.log"
        return 1
    fi

    return 0
}

nightly_validate_jsonl() {
    local dir="$1"

    if [[ ! -s "$dir/output.jsonl" ]]; then
        printf 'FAIL output.jsonl is empty\n' >> "$dir/assertions.log"
        return 1
    fi

    python3 - "$dir/output.jsonl" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    for line_no, line in enumerate(handle, 1):
        if not line.strip():
            raise SystemExit(f"empty JSONL line {line_no}")
        record = json.loads(line)
        if record.get("schema_version") != 0:
            raise SystemExit(
                f"schema_version mismatch at {line_no}: "
                f"{record.get('schema_version')!r}"
            )
PY
}

nightly_finish() {
    local name="$1"
    local dir="$2"
    local status="$3"

    printf '%s artifacts: %s\n' "$name" "$dir"

    if [[ "$status" == 0 ]]; then
        printf 'PASS %s\n' "$name"
    else
        printf 'FAIL %s\n' "$name"
        printf 'Inspect: %s/assertions.log\n' "$dir"
    fi

    return "$status"
}
