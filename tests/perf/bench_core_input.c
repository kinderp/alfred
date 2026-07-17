/*
 * bench_core_input.c - manual core input micro-benchmark
 *
 * This benchmark measures the current raw-first semantic core path:
 *
 *   synthetic alfred_raw_event_t
 *   -> alfred_process()
 *   -> semantic callback
 *   -> counter/no-op callback state
 *
 * It intentionally does not use inotify, filesystem I/O, JSONL, text writers,
 * record queues, dispatchers, sockets, threads, or flush policy. The goal is to
 * establish the baseline required by the Core Input Model / Main Loop Migration
 * v0 benchmark gate before evaluating a measured bridge or a future
 * record-first core input path.
 *
 * Output columns:
 *
 * - benchmark: benchmark row name. For this file it is
 *   core-input-raw-first.
 * - raw_events: number of raw input events passed to alfred_process() in each
 *   run.
 * - runs: number of repeated measurements.
 * - min_us, avg_us, max_us: elapsed time in microseconds.
 * - raw_events_per_sec_avg: throughput computed from raw_events and avg_us.
 * - semantic_events_last: number of semantic callback events seen in the last
 *   run. It is lower than raw_events because MOVED_FROM is stored until the
 *   matching MOVED_TO arrives.
 * - errors_last: number of alfred_process() failures or adapter conversion
 *   failures in the last run. It must be zero for a valid benchmark row.
 * - conversions_per_event: raw-first performs no record/raw bridge conversion,
 *   so this baseline reports 0. Future bridge benchmarks can use the same
 *   column to make conversion cost visible.
 * - output_mode: always counter-noop; the callback increments counters only.
 *   The raw-to-record adapter row uses adapter-only because it has no semantic
 *   callback.
 *
 * The numbers are useful for local comparisons on the same machine. They are
 * not CI thresholds and do not represent real inotify runtime throughput.
 */

#include "alfred_correlator.h"
#include "alfred_record_adapter.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    size_t semantic_events;
} bench_callback_context_t;

typedef struct {
    uint64_t elapsed_us;
    size_t semantic_events;
    size_t errors;
} bench_run_result_t;

typedef int (*bench_run_fn_t)(size_t raw_events, bench_run_result_t *result);

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static void count_semantic_event(const alfred_event_t *event, void *userdata)
{
    bench_callback_context_t *context = userdata;

    (void)event;

    if (context != NULL) {
        context->semantic_events++;
    }
}

static alfred_raw_event_t make_raw_event(size_t index)
{
    static const char *paths[] = {
        "/tmp/alfred-core-input/a.txt",
        "/tmp/alfred-core-input/b.txt",
        "/tmp/alfred-core-input/dir",
        "/tmp/alfred-core-input/orphan.txt"
    };
    alfred_raw_event_t raw;
    size_t step;

    memset(&raw, 0, sizeof(raw));
    raw.ts_ns = ((uint64_t)index + 1u) * 1000000ULL;
    raw.source = ALFRED_SRC_USER;
    raw.pid = 1000;

    step = index % 10u;

    switch (step) {
    case 0u:
        raw.mask = ALFRED_RAW_CREATE;
        raw.path = paths[0];
        break;
    case 1u:
        raw.mask = ALFRED_RAW_CLOSE_WRITE;
        raw.path = paths[0];
        break;
    case 2u:
        raw.mask = ALFRED_RAW_MODIFY;
        raw.path = paths[0];
        break;
    case 3u:
        raw.mask = ALFRED_RAW_DELETE;
        raw.path = paths[0];
        break;
    case 4u:
        raw.mask = ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR;
        raw.path = paths[2];
        break;
    case 5u:
        raw.mask = ALFRED_RAW_DELETE | ALFRED_RAW_ISDIR;
        raw.path = paths[2];
        break;
    case 6u:
        raw.mask = ALFRED_RAW_MOVED_FROM;
        raw.cookie = (uint32_t)((index / 10u) + 1u);
        raw.path = paths[0];
        break;
    case 7u:
        raw.mask = ALFRED_RAW_MOVED_TO;
        raw.cookie = (uint32_t)((index / 10u) + 1u);
        raw.path = paths[1];
        break;
    case 8u:
        raw.mask = ALFRED_RAW_MOVED_TO;
        raw.cookie = 0u;
        raw.path = paths[3];
        break;
    default:
        raw.mask = ALFRED_RAW_OVERFLOW;
        raw.path = NULL;
        break;
    }

    return raw;
}

static int run_core_raw_first_once(size_t raw_events,
                                   bench_run_result_t *result)
{
    alfred_config_t config;
    alfred_engine_t *engine;
    bench_callback_context_t context;
    uint64_t start_ns;
    uint64_t end_ns;
    size_t errors = 0u;

    if (result == NULL) {
        return -1;
    }

    memset(&context, 0, sizeof(context));
    alfred_config_default(&config);

    /*
     * Keep debounce disabled for this baseline so each MODIFY raw event has a
     * deterministic semantic effect. Future benchmarks can add a debounce-heavy
     * row if they need to isolate that cost.
     */
    config.modify_debounce_ms = 0u;

    engine = alfred_create(&config, count_semantic_event, &context);
    if (engine == NULL) {
        return -1;
    }

    start_ns = now_ns();

    for (size_t i = 0; i < raw_events; i++) {
        alfred_raw_event_t raw = make_raw_event(i);

        if (alfred_process(engine, &raw) != 0) {
            errors++;
        }
    }

    if (alfred_flush(engine) != 0) {
        errors++;
    }

    end_ns = now_ns();

    result->elapsed_us = (end_ns - start_ns) / 1000ULL;
    result->semantic_events = context.semantic_events;
    result->errors = errors;

    alfred_destroy(engine);

    return 0;
}

static int run_raw_to_record_adapter_once(size_t raw_events,
                                          bench_run_result_t *result)
{
    uint64_t start_ns;
    uint64_t end_ns;
    size_t conversion_errors = 0u;

    if (result == NULL) {
        return -1;
    }

    start_ns = now_ns();

    for (size_t i = 0; i < raw_events; i++) {
        alfred_raw_event_t raw = make_raw_event(i);
        alfred_record_t record;

        if (alfred_record_from_raw(&raw, &record) != 0) {
            conversion_errors++;
        }
    }

    end_ns = now_ns();

    result->elapsed_us = (end_ns - start_ns) / 1000ULL;
    result->semantic_events = 0u;
    result->errors = conversion_errors;

    return 0;
}

static int parse_size_arg(const char *value, const char *name, size_t *out)
{
    char *end = NULL;
    unsigned long parsed;

    if (value == NULL || name == NULL || out == NULL) {
        return -1;
    }

    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0ul) {
        fprintf(stderr, "invalid %s: %s\n", name, value);
        return -1;
    }

    *out = (size_t)parsed;
    return 0;
}

static int run_benchmark(const char *benchmark,
                         bench_run_fn_t run_once,
                         size_t raw_events,
                         size_t runs,
                         unsigned int conversions_per_event,
                         const char *output_mode)
{
    uint64_t min_us = UINT64_MAX;
    uint64_t max_us = 0u;
    uint64_t total_us = 0u;
    size_t semantic_events_last = 0u;
    size_t errors_last = 0u;

    if (benchmark == NULL || run_once == NULL || output_mode == NULL) {
        return -1;
    }

    for (size_t run = 0; run < runs; run++) {
        bench_run_result_t result;

        if (run_once(raw_events, &result) != 0) {
            return -1;
        }

        if (result.elapsed_us < min_us) {
            min_us = result.elapsed_us;
        }
        if (result.elapsed_us > max_us) {
            max_us = result.elapsed_us;
        }

        total_us += result.elapsed_us;
        semantic_events_last = result.semantic_events;
        errors_last = result.errors;
    }

    {
        double avg_us = (double)total_us / (double)runs;
        double events_per_sec = 0.0;

        if (avg_us > 0.0) {
            events_per_sec = ((double)raw_events * 1000000.0) / avg_us;
        }

        printf("%s,%zu,%zu,%llu,%.2f,%llu,%.2f,%zu,%zu,%u,%s\n",
               benchmark,
               raw_events,
               runs,
               (unsigned long long)min_us,
               avg_us,
               (unsigned long long)max_us,
               events_per_sec,
               semantic_events_last,
               errors_last,
               conversions_per_event,
               output_mode);
    }

    return 0;
}

static void print_usage(const char *program)
{
    fprintf(stderr, "usage: %s [--events N] [--runs N]\n", program);
    fprintf(stderr, "       %s [EVENTS]\n", program);
}

int main(int argc, char **argv)
{
    size_t raw_events = 100000u;
    size_t runs = 1u;

    for (int i = 1; i < argc;) {
        if (strcmp(argv[i], "--events") == 0) {
            if (i + 1 >= argc ||
                parse_size_arg(argv[i + 1], "--events", &raw_events) != 0) {
                print_usage(argv[0]);
                return 1;
            }
            i += 2;
        }
        else if (strcmp(argv[i], "--runs") == 0) {
            if (i + 1 >= argc ||
                parse_size_arg(argv[i + 1], "--runs", &runs) != 0) {
                print_usage(argv[0]);
                return 1;
            }
            i += 2;
        }
        else if (argc == 2) {
            if (parse_size_arg(argv[i], "EVENTS", &raw_events) != 0) {
                print_usage(argv[0]);
                return 1;
            }
            i++;
        }
        else {
            print_usage(argv[0]);
            return 1;
        }
    }

    printf("benchmark,raw_events,runs,min_us,avg_us,max_us,"
           "raw_events_per_sec_avg,semantic_events_last,"
           "errors_last,conversions_per_event,output_mode\n");

    if (run_benchmark("core-input-raw-first",
                      run_core_raw_first_once,
                      raw_events,
                      runs,
                      0u,
                      "counter-noop") != 0) {
        fprintf(stderr, "core input benchmark failed\n");
        return 1;
    }

    if (run_benchmark("raw-to-record-adapter",
                      run_raw_to_record_adapter_once,
                      raw_events,
                      runs,
                      1u,
                      "adapter-only") != 0) {
        fprintf(stderr, "core input benchmark failed\n");
        return 1;
    }

    return 0;
}
