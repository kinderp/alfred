#!/usr/bin/env bash

# run_runtime_output.sh - manual Alfred runtime output benchmark
#
# This benchmark launches the real Alfred binary, watches a real temporary
# directory, creates real files, and compares two runtime modes:
#
# - compat-only: output_enabled=false, so Alfred writes only raw/events/errors.
# - jsonl-output: output_enabled=true, so Alfred also routes records through
#   app_emit_output_record() -> app_enqueue_output_record(), followed by
#   app_run() -> app_drain_output_pipeline() -> dispatcher -> JSONL writer ->
#   output_log.
#   If one backend poll fills the bounded queue, the v0 enqueue helper performs
#   one pressure-relief drain and retries enqueue; future worker benchmarks
#   should make that backpressure path asynchronous.
#
# This is intentionally a manual benchmark, not a correctness test and not a CI
# threshold. It includes scheduler noise, kernel inotify delivery, compatibility
# text logs, process startup/shutdown, file creation cost, JSONL writer cost and
# real filesystem I/O. Use it to compare gross trends with synthetic rows such
# as queue-dispatcher-jsonl and output-pipeline-jsonl, not to prove exact
# throughput on every machine.
#
# LeakSanitizer is disabled for the measured Alfred process because this script
# is a performance probe, not a leak test. In restricted or traced environments
# LSAN can turn an otherwise clean shutdown into process_status=1 with a
# sanitizer diagnostic on stderr, which would pollute the benchmark result.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
ALFRED_BIN="${ALFRED_BIN:-$ROOT_DIR/alfred}"
BENCH_ROOT="${BENCH_ROOT:-/tmp/alfred_perf_runtime_output}"
FILES=1000
RUNS=1
SETTLE_TIMEOUT_MS=5000

usage() {
    echo "usage: $0 [--files N] [--runs N] [--settle-timeout-ms N]" >&2
    echo "       $0 [FILES]" >&2
}

now_ns() {
    date +%s%N
}

elapsed_us() {
    local start_ns="$1"
    local end_ns="$2"

    echo $(((end_ns - start_ns) / 1000))
}

line_count() {
    local path="$1"

    if [[ -f "$path" ]]; then
        wc -l < "$path" | tr -d ' '
    else
        echo 0
    fi
}

byte_count() {
    local path="$1"

    if [[ -f "$path" ]]; then
        wc -c < "$path" | tr -d ' '
    else
        echo 0
    fi
}

runtime_stat() {
    local event_log="$1"
    local key="$2"
    local line
    local value

    if [[ ! -f "$event_log" ]]; then
        echo 0
        return
    fi

    line="$(grep -E "output runtime stats " "$event_log" | tail -n 1 || true)"
    if [[ -z "$line" ]]; then
        echo 0
        return
    fi

    value="$(printf "%s\n" "$line" |
        sed -n "s/.*$key=\\([0-9][0-9]*\\).*/\\1/p")"
    if [[ -z "$value" ]]; then
        echo 0
    else
        echo "$value"
    fi
}

emit_files() {
    local watch_dir="$1"
    local files="$2"
    local i

    for i in $(seq 1 "$files"); do
        printf "runtime output bench %s\n" "$i" > "$watch_dir/file-$i.txt"
    done
}

wait_for_progress() {
    local event_log="$1"
    local expected_min="$2"
    local timeout_ms="$3"
    local start_ns
    local now
    local elapsed_ms

    start_ns="$(now_ns)"

    while true; do
        if [[ "$(line_count "$event_log")" -ge "$expected_min" ]]; then
            return 0
        fi

        now="$(now_ns)"
        elapsed_ms=$(((now - start_ns) / 1000000))
        if [[ "$elapsed_ms" -ge "$timeout_ms" ]]; then
            return 0
        fi

        sleep 0.01
    done
}

run_one() {
    local mode="$1"
    local run="$2"
    local output_enabled="$3"
    local run_dir="$BENCH_ROOT/$mode/run-$run"
    local watch_dir="$run_dir/watch"
    local config_file="$run_dir/alfred.conf"
    local raw_log="$run_dir/raw.log"
    local event_log="$run_dir/events.log"
    local error_log="$run_dir/errors.log"
    local output_log="$run_dir/output.jsonl"
    local start_ns
    local after_start_ns
    local after_emit_ns
    local after_settle_ns
    local after_stop_ns
    local pid
    local status
    local startup_us
    local emit_us
    local settle_us
    local total_us
    local files_per_sec
    local expected_event_lines
    local enqueue_attempts
    local enqueue_success
    local enqueue_failures
    local pressure_drains
    local drain_calls
    local drained_records
    local max_pending

    rm -rf "$run_dir"
    mkdir -p "$watch_dir"

    cat > "$config_file" <<EOF
raw_log=$raw_log
event_log=$event_log
error_log=$error_log
output_enabled=$output_enabled
output_format=jsonl
output_buffer_size=65536
output_log=$output_log
EOF

    start_ns="$(now_ns)"
    LSAN_OPTIONS="${LSAN_OPTIONS:+$LSAN_OPTIONS:}detect_leaks=0" \
    ALFRED_CONFIG="$config_file" \
    ALFRED_EVENT_ENGINE=core \
        "$ALFRED_BIN" "$watch_dir" >/dev/null 2>&1 &
    pid=$!

    sleep 1
    after_start_ns="$(now_ns)"

    emit_files "$watch_dir" "$FILES"
    after_emit_ns="$(now_ns)"

    expected_event_lines=$((FILES * 3))
    wait_for_progress "$event_log" "$expected_event_lines" "$SETTLE_TIMEOUT_MS"
    after_settle_ns="$(now_ns)"

    kill -INT "$pid" 2>/dev/null || true
    set +e
    wait "$pid"
    status=$?
    set -e
    after_stop_ns="$(now_ns)"

    startup_us="$(elapsed_us "$start_ns" "$after_start_ns")"
    emit_us="$(elapsed_us "$after_start_ns" "$after_emit_ns")"
    settle_us="$(elapsed_us "$after_emit_ns" "$after_settle_ns")"
    total_us="$(elapsed_us "$start_ns" "$after_stop_ns")"

    if [[ "$emit_us" -gt 0 ]]; then
        files_per_sec="$(awk -v files="$FILES" -v us="$emit_us" \
            'BEGIN { printf "%.2f", files * 1000000.0 / us }')"
    else
        files_per_sec="0.00"
    fi

    enqueue_attempts="$(runtime_stat "$event_log" "enqueue_attempts")"
    enqueue_success="$(runtime_stat "$event_log" "enqueue_success")"
    enqueue_failures="$(runtime_stat "$event_log" "enqueue_failures")"
    pressure_drains="$(runtime_stat "$event_log" "pressure_drains")"
    drain_calls="$(runtime_stat "$event_log" "drain_calls")"
    drained_records="$(runtime_stat "$event_log" "drained_records")"
    max_pending="$(runtime_stat "$event_log" "max_pending")"

    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
        "$mode" \
        "$run" \
        "$FILES" \
        "$status" \
        "$startup_us" \
        "$emit_us" \
        "$settle_us" \
        "$total_us" \
        "$files_per_sec" \
        "$(line_count "$raw_log")" \
        "$(line_count "$event_log")" \
        "$(line_count "$output_log")" \
        "$(byte_count "$output_log")" \
        "$enqueue_attempts" \
        "$enqueue_success" \
        "$enqueue_failures" \
        "$pressure_drains" \
        "$drain_calls" \
        "$drained_records" \
        "$max_pending" \
        "$run_dir"
}

while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --files)
            if [[ "$#" -lt 2 ]]; then
                echo "missing value for --files" >&2
                exit 1
            fi
            FILES="$2"
            shift 2
            ;;
        --runs)
            if [[ "$#" -lt 2 ]]; then
                echo "missing value for --runs" >&2
                exit 1
            fi
            RUNS="$2"
            shift 2
            ;;
        --settle-timeout-ms)
            if [[ "$#" -lt 2 ]]; then
                echo "missing value for --settle-timeout-ms" >&2
                exit 1
            fi
            SETTLE_TIMEOUT_MS="$2"
            shift 2
            ;;
        -*)
            usage
            exit 1
            ;;
        *)
            if [[ "$#" -eq 1 ]]; then
                FILES="$1"
                shift
            else
                usage
                exit 1
            fi
            ;;
    esac
done

if [[ ! -x "$ALFRED_BIN" ]]; then
    echo "missing Alfred binary: $ALFRED_BIN" >&2
    echo "run make before this benchmark" >&2
    exit 1
fi

rm -rf "$BENCH_ROOT"
mkdir -p "$BENCH_ROOT"

printf "mode,run,files,process_status,startup_us,emit_us,settle_us,total_us,files_per_sec,raw_lines,event_lines,jsonl_lines,jsonl_bytes,enqueue_attempts,enqueue_success,enqueue_failures,pressure_drains,drain_calls,drained_records,max_pending,artifact_dir\n"

for run in $(seq 1 "$RUNS"); do
    run_one "compat-only" "$run" "false"
    run_one "jsonl-output" "$run" "true"
done
