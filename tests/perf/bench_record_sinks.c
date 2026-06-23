/*
 * bench_record_sinks.c - manual record sink micro-benchmark
 *
 * This benchmark measures the isolated cost of delivering synthetic
 * alfred_record_t values to three sink implementations:
 *
 * - counter: no formatting, no I/O, counters only
 * - text: compatibility text formatter plus caller callback
 * - jsonl: JSONL formatter plus caller callback
 *
 * It intentionally does not use inotify, filesystem events, dispatcher threads,
 * file writes, socket sends, flush policy, or backpressure. The goal is to get
 * a first local baseline for record -> sink overhead before connecting writers
 * to the runtime path.
 *
 * Output columns:
 *
 * - sink: counter, text, or jsonl
 * - records: number of synthetic records emitted
 * - elapsed_us: monotonic wall-clock time spent emitting records
 * - records_per_sec: simple throughput for this run
 * - bytes: payload bytes observed by formatting sinks, 0 for counter
 * - counter_total: total records observed by the counter sink, 0 otherwise
 *
 * The numbers are useful for comparing implementations on the same machine.
 * They are not stable correctness thresholds and should not gate CI.
 */

#include "alfred_record_counter_sink.h"
#include "alfred_record_jsonl_sink.h"
#include "alfred_record_sink.h"
#include "alfred_record_text_sink.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum {
    BENCH_SINK_COUNTER = 0,
    BENCH_SINK_TEXT,
    BENCH_SINK_JSONL
} bench_sink_kind_t;

typedef struct {
    size_t bytes;
} byte_count_writer_t;

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static int count_payload_bytes(void *userdata, const char *payload)
{
    byte_count_writer_t *writer = userdata;

    if (writer == NULL || payload == NULL) {
        return -1;
    }

    writer->bytes += strlen(payload);
    return 0;
}

static alfred_record_t make_record(size_t index)
{
    static const char *paths[] = {
        "/tmp/alfred-bench/a.txt",
        "/tmp/alfred-bench/b.txt",
        "/tmp/alfred-bench/c.txt"
    };
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;

    switch (index % 3u) {
    case 0u:
        record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
        record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
        record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
        record.path = paths[0];
        break;
    case 1u:
        record.layer = ALFRED_RECORD_LAYER_NORMALIZED_RAW;
        record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
        record.type = ALFRED_RECORD_TYPE_RAW_MODIFY;
        record.source = 1u;
        record.raw_mask = 2u;
        record.path = paths[1];
        break;
    default:
        record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
        record.category = ALFRED_RECORD_CATEGORY_RECOVERY;
        record.type = ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED;
        record.backend = "inotify";
        record.path = paths[2];
        record.watch.watch_id = 7;
        record.watch.state = "stale";
        record.watch.reason = "IN_MOVE_SELF";
        record.watch.error = "identity-mismatch";
        record.recovery.result_code = -1;
        record.recovery.pending_count = 1u;
        break;
    }

    return record;
}

static const char *sink_name(bench_sink_kind_t kind)
{
    switch (kind) {
    case BENCH_SINK_COUNTER: return "counter";
    case BENCH_SINK_TEXT: return "text";
    case BENCH_SINK_JSONL: return "jsonl";
    default:
        return "unknown";
    }
}

static int run_benchmark(bench_sink_kind_t kind, size_t records)
{
    alfred_record_counter_sink_t counter_sink;
    alfred_record_text_sink_t text_sink;
    alfred_record_jsonl_sink_t jsonl_sink;
    alfred_record_sink_t sink;
    byte_count_writer_t writer;
    char text_buffer[512];
    char jsonl_buffer[1024];
    uint64_t start_ns;
    uint64_t end_ns;
    double elapsed_seconds;
    double records_per_sec;
    size_t counter_total = 0u;

    memset(&sink, 0, sizeof(sink));
    memset(&writer, 0, sizeof(writer));

    switch (kind) {
    case BENCH_SINK_COUNTER:
        if (alfred_record_counter_sink_init(&counter_sink, &sink) != 0) {
            return -1;
        }
        break;
    case BENCH_SINK_TEXT:
        text_sink.write = count_payload_bytes;
        text_sink.userdata = &writer;
        text_sink.buffer = text_buffer;
        text_sink.buffer_size = sizeof(text_buffer);
        if (alfred_record_text_sink_init(&text_sink, &sink) != 0) {
            return -1;
        }
        break;
    case BENCH_SINK_JSONL:
        jsonl_sink.write = count_payload_bytes;
        jsonl_sink.userdata = &writer;
        jsonl_sink.buffer = jsonl_buffer;
        jsonl_sink.buffer_size = sizeof(jsonl_buffer);
        if (alfred_record_jsonl_sink_init(&jsonl_sink, &sink) != 0) {
            return -1;
        }
        break;
    default:
        return -1;
    }

    start_ns = now_ns();
    for (size_t i = 0u; i < records; i++) {
        alfred_record_t record = make_record(i);

        if (alfred_record_sink_emit(&sink, &record) != 0) {
            return -1;
        }
    }
    end_ns = now_ns();

    if (kind == BENCH_SINK_COUNTER) {
        counter_total = counter_sink.total_records;
    }

    elapsed_seconds = (double)(end_ns - start_ns) / 1000000000.0;
    records_per_sec =
        elapsed_seconds > 0.0 ? (double)records / elapsed_seconds : 0.0;

    printf("%s,%zu,%llu,%.2f,%zu,%zu\n",
           sink_name(kind),
           records,
           (unsigned long long)((end_ns - start_ns) / 1000ULL),
           records_per_sec,
           writer.bytes,
           counter_total);

    return 0;
}

static int parse_records(const char *text, size_t *records)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0u) {
        return -1;
    }

    *records = (size_t)parsed;
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [RECORDS]\n"
            "example: %s 100000\n",
            argv0,
            argv0);
}

int main(int argc, char **argv)
{
    size_t records = 100000u;

    if (argc > 2) {
        usage(argv[0]);
        return 1;
    }

    if (argc == 2 && parse_records(argv[1], &records) != 0) {
        fprintf(stderr, "invalid record count: %s\n", argv[1]);
        return 1;
    }

    printf("sink,records,elapsed_us,records_per_sec,bytes,counter_total\n");

    if (run_benchmark(BENCH_SINK_COUNTER, records) != 0 ||
        run_benchmark(BENCH_SINK_TEXT, records) != 0 ||
        run_benchmark(BENCH_SINK_JSONL, records) != 0) {
        return 1;
    }

    return 0;
}
