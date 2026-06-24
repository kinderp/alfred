/*
 * bench_record_sinks.c - manual record sink micro-benchmark
 *
 * This benchmark measures the isolated cost of delivering synthetic
 * alfred_record_t values to sink implementations and queue boundaries:
 *
 * - counter: no formatting, no I/O, counters only
 * - text: compatibility text formatter plus caller callback
 * - jsonl: JSONL formatter plus caller callback
 * - queue-counter: clone borrowed records into the owned-record queue, pop
 *   them back out, deliver them to the counter sink, then destroy them
 * - dispatcher-*: route borrowed records through the bounded dispatcher before
 *   they reach one or more registered sinks
 *
 * It intentionally does not use inotify, filesystem events, dispatcher threads,
 * file writes, socket sends, flush policy, or backpressure. The goal is to get
 * a first local baseline for record -> sink, record -> dispatcher -> sink, and
 * record -> queue -> sink overhead before connecting writers to the runtime
 * path.
 *
 * Output columns:
 *
 * - sink: counter, text, jsonl, queue-counter, or dispatcher-*.
 *   This tells which sink implementation was measured. Counter is the no-op
 *   baseline, text is the compatibility formatter, jsonl is the structured
 *   JSONL formatter, queue-counter is the first queue-boundary baseline, and
 *   dispatcher-* rows measure bounded fan-out without a queue.
 *
 * - records: number of synthetic records emitted in each run.
 *   If records is 100000 and runs is 5, each sink receives 5 independent runs
 *   of 100000 records.
 *
 * - runs: number of repeated measurements for the same sink.
 *   More than one run helps expose scheduler noise and cache effects. It is
 *   still not a full statistical benchmark, but it is better than trusting one
 *   timing sample.
 *
 * - min_us: fastest run in microseconds.
 *   This is the best observed case for that sink on this machine.
 *
 * - avg_us: arithmetic mean of all run times in microseconds.
 *   This is the main value to compare at this stage.
 *
 * - max_us: slowest run in microseconds.
 *   If max_us is far from min_us, the machine or benchmark run was noisy.
 *
 * - records_per_sec_avg: throughput computed from records and avg_us.
 *   Formula: records * 1,000,000 / avg_us.
 *
 * - bytes_last: payload bytes observed in the last run.
 *   Text and JSONL sinks format strings, so this is the sum of the payload
 *   lengths. Counter produces no payload and therefore reports 0.
 *
 * - counter_total_last: records observed by the counter sink in the last run.
 *   It should match records for the counter and queue-counter rows. It is 0
 *   for text and jsonl.
 *
 * Interpretation examples:
 *
 * - counter fast, text slower, jsonl much slower is expected: counter only
 *   increments integers, text formats compact human strings, and JSONL writes
 *   field names, escaping, and nested objects.
 * - counter slow means the problem is close to record creation, sink dispatch,
 *   the benchmark loop, or the machine. It is not caused by JSON formatting.
 * - queue-counter slower than counter is expected: it adds owned-string clone,
 *   queue push, queue pop, and owned-record destroy. This is the cost we need
 *   to know before moving writers out of the hot path.
 * - dispatcher-* rows measure routing through the bounded dispatcher. A single
 *   dispatcher row should stay close to the corresponding direct sink row,
 *   while dispatcher-counter-text-jsonl measures synchronous fan-out to three
 *   registered sinks.
 * - jsonl slow while counter is fast means the cost is likely in serialization,
 *   escaping, and produced byte volume.
 *
 * The numbers are useful for comparing implementations on the same machine.
 * They are not stable correctness thresholds and should not gate CI.
 */

#include "alfred_record_counter_sink.h"
#include "alfred_record_dispatcher.h"
#include "alfred_record_jsonl_sink.h"
#include "alfred_record_owned.h"
#include "alfred_record_queue.h"
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
    BENCH_SINK_JSONL,
    BENCH_SINK_QUEUE_COUNTER,
    BENCH_SINK_DISPATCHER_COUNTER,
    BENCH_SINK_DISPATCHER_TEXT,
    BENCH_SINK_DISPATCHER_JSONL,
    BENCH_SINK_DISPATCHER_ALL
} bench_sink_kind_t;

typedef struct {
    size_t bytes;
} byte_count_writer_t;

typedef struct {
    uint64_t elapsed_us;
    size_t bytes;
    size_t counter_total;
} bench_run_result_t;

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
    case BENCH_SINK_QUEUE_COUNTER: return "queue-counter";
    case BENCH_SINK_DISPATCHER_COUNTER: return "dispatcher-counter";
    case BENCH_SINK_DISPATCHER_TEXT: return "dispatcher-text";
    case BENCH_SINK_DISPATCHER_JSONL: return "dispatcher-jsonl";
    case BENCH_SINK_DISPATCHER_ALL: return "dispatcher-counter-text-jsonl";
    default:
        return "unknown";
    }
}

static int add_sink_to_dispatcher(alfred_record_dispatcher_t *dispatcher,
                                  const char *name,
                                  const alfred_record_sink_t *sink)
{
    return alfred_record_dispatcher_add_sink(
        dispatcher,
        name,
        ALFRED_RECORD_DISPATCHER_SINK_CRITICAL,
        sink);
}

static int run_dispatcher_once(bench_sink_kind_t kind,
                               size_t records,
                               bench_run_result_t *result)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t dispatcher_storage[3];
    alfred_record_counter_sink_t counter_sink;
    alfred_record_text_sink_t text_sink;
    alfred_record_jsonl_sink_t jsonl_sink;
    alfred_record_sink_t counter_generic;
    alfred_record_sink_t text_generic;
    alfred_record_sink_t jsonl_generic;
    byte_count_writer_t writer;
    char text_buffer[512];
    char jsonl_buffer[1024];
    uint64_t start_ns;
    uint64_t end_ns;

    if (result == NULL) {
        return -1;
    }

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(dispatcher_storage, 0, sizeof(dispatcher_storage));
    memset(&counter_generic, 0, sizeof(counter_generic));
    memset(&text_generic, 0, sizeof(text_generic));
    memset(&jsonl_generic, 0, sizeof(jsonl_generic));
    memset(&counter_sink, 0, sizeof(counter_sink));
    memset(&writer, 0, sizeof(writer));
    memset(result, 0, sizeof(*result));

    if (alfred_record_dispatcher_init(
            &dispatcher,
            dispatcher_storage,
            sizeof(dispatcher_storage) / sizeof(dispatcher_storage[0])) != 0) {
        return -1;
    }

    switch (kind) {
    case BENCH_SINK_DISPATCHER_COUNTER:
    case BENCH_SINK_DISPATCHER_ALL:
        if (alfred_record_counter_sink_init(&counter_sink,
                                            &counter_generic) != 0) {
            return -1;
        }
        if (add_sink_to_dispatcher(&dispatcher,
                                   "counter",
                                   &counter_generic) != 0) {
            return -1;
        }
        if (kind == BENCH_SINK_DISPATCHER_COUNTER) {
            break;
        }
        /* fall through */
    case BENCH_SINK_DISPATCHER_TEXT:
        text_sink.write = count_payload_bytes;
        text_sink.userdata = &writer;
        text_sink.buffer = text_buffer;
        text_sink.buffer_size = sizeof(text_buffer);
        if (alfred_record_text_sink_init(&text_sink, &text_generic) != 0) {
            return -1;
        }
        if (add_sink_to_dispatcher(&dispatcher, "text", &text_generic) != 0) {
            return -1;
        }
        if (kind == BENCH_SINK_DISPATCHER_TEXT) {
            break;
        }
        /* fall through */
    case BENCH_SINK_DISPATCHER_JSONL:
        jsonl_sink.write = count_payload_bytes;
        jsonl_sink.userdata = &writer;
        jsonl_sink.buffer = jsonl_buffer;
        jsonl_sink.buffer_size = sizeof(jsonl_buffer);
        if (alfred_record_jsonl_sink_init(&jsonl_sink, &jsonl_generic) != 0) {
            return -1;
        }
        if (add_sink_to_dispatcher(&dispatcher, "jsonl", &jsonl_generic) != 0) {
            return -1;
        }
        break;
    default:
        return -1;
    }

    start_ns = now_ns();
    for (size_t i = 0u; i < records; i++) {
        alfred_record_t record = make_record(i);

        if (alfred_record_dispatcher_dispatch_one(&dispatcher, &record) != 0) {
            return -1;
        }
    }
    end_ns = now_ns();

    result->elapsed_us = (end_ns - start_ns) / 1000ULL;
    result->bytes = writer.bytes;
    result->counter_total = counter_sink.total_records;

    return 0;
}

static int run_queue_counter_once(size_t records, bench_run_result_t *result)
{
    alfred_record_queue_t queue;
    alfred_record_counter_sink_t counter_sink;
    alfred_record_sink_t sink;
    uint64_t start_ns;
    uint64_t end_ns;

    if (result == NULL) {
        return -1;
    }

    memset(&queue, 0, sizeof(queue));
    memset(&sink, 0, sizeof(sink));
    memset(result, 0, sizeof(*result));

    if (alfred_record_queue_init(&queue, records) != 0) {
        return -1;
    }

    if (alfred_record_counter_sink_init(&counter_sink, &sink) != 0) {
        alfred_record_queue_destroy(&queue);
        return -1;
    }

    /*
     * The allocation above is intentionally outside the timed region. This row
     * measures the per-record boundary cost: clone owned, enqueue, dequeue,
     * counter emit, and destroy owned.
     */
    start_ns = now_ns();
    for (size_t i = 0u; i < records; i++) {
        alfred_record_t record = make_record(i);

        if (alfred_record_queue_push(&queue, &record) != 0) {
            alfred_record_queue_destroy(&queue);
            return -1;
        }
    }

    for (size_t i = 0u; i < records; i++) {
        alfred_record_t record;

        memset(&record, 0, sizeof(record));
        if (alfred_record_queue_pop(&queue, &record) != 0) {
            alfred_record_queue_destroy(&queue);
            return -1;
        }
        if (alfred_record_sink_emit(&sink, &record) != 0) {
            alfred_record_destroy_owned(&record);
            alfred_record_queue_destroy(&queue);
            return -1;
        }
        alfred_record_destroy_owned(&record);
    }
    end_ns = now_ns();

    result->elapsed_us = (end_ns - start_ns) / 1000ULL;
    result->bytes = 0u;
    result->counter_total = counter_sink.total_records;

    alfred_record_queue_destroy(&queue);
    return 0;
}

static int run_once(bench_sink_kind_t kind,
                    size_t records,
                    bench_run_result_t *result)
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
    size_t counter_total = 0u;

    if (result == NULL) {
        return -1;
    }

    memset(&sink, 0, sizeof(sink));
    memset(&writer, 0, sizeof(writer));
    memset(result, 0, sizeof(*result));

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
    case BENCH_SINK_QUEUE_COUNTER:
        return run_queue_counter_once(records, result);
    case BENCH_SINK_DISPATCHER_COUNTER:
    case BENCH_SINK_DISPATCHER_TEXT:
    case BENCH_SINK_DISPATCHER_JSONL:
    case BENCH_SINK_DISPATCHER_ALL:
        return run_dispatcher_once(kind, records, result);
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

    result->elapsed_us = (end_ns - start_ns) / 1000ULL;
    result->bytes = writer.bytes;
    result->counter_total = counter_total;

    return 0;
}

static int run_benchmark(bench_sink_kind_t kind, size_t records, size_t runs)
{
    bench_run_result_t result;
    uint64_t min_us = UINT64_MAX;
    uint64_t max_us = 0u;
    unsigned long long total_us = 0u;
    double avg_us;
    double records_per_sec_avg;
    size_t bytes_last = 0u;
    size_t counter_total_last = 0u;

    for (size_t i = 0u; i < runs; i++) {
        if (run_once(kind, records, &result) != 0) {
            return -1;
        }

        if (result.elapsed_us < min_us) {
            min_us = result.elapsed_us;
        }
        if (result.elapsed_us > max_us) {
            max_us = result.elapsed_us;
        }

        total_us += (unsigned long long)result.elapsed_us;
        bytes_last = result.bytes;
        counter_total_last = result.counter_total;
    }

    avg_us = (double)total_us / (double)runs;
    records_per_sec_avg = avg_us > 0.0
        ? ((double)records * 1000000.0) / avg_us
        : 0.0;

    printf("%s,%zu,%zu,%llu,%.2f,%llu,%.2f,%zu,%zu\n",
           sink_name(kind),
           records,
           runs,
           (unsigned long long)min_us,
           avg_us,
           (unsigned long long)max_us,
           records_per_sec_avg,
           bytes_last,
           counter_total_last);

    return 0;
}

static int parse_size(const char *text, size_t *value)
{
    char *end = NULL;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed == 0u) {
        return -1;
    }

    *value = (size_t)parsed;
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--records N] [--runs N]\n"
            "       %s [RECORDS]\n"
            "example: %s --records 100000 --runs 5\n",
            argv0,
            argv0,
            argv0);
}

int main(int argc, char **argv)
{
    size_t records = 100000u;
    size_t runs = 1u;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--records") == 0) {
            if (i + 1 >= argc || parse_size(argv[i + 1], &records) != 0) {
                fprintf(stderr, "invalid --records value\n");
                return 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "--runs") == 0) {
            if (i + 1 >= argc || parse_size(argv[i + 1], &runs) != 0) {
                fprintf(stderr, "invalid --runs value\n");
                return 1;
            }
            i++;
        }
        else if (argc == 2 && parse_size(argv[i], &records) == 0) {
            continue;
        }
        else {
            usage(argv[0]);
            return 1;
        }
    }

    printf("sink,records,runs,min_us,avg_us,max_us,"
           "records_per_sec_avg,bytes_last,counter_total_last\n");

    if (run_benchmark(BENCH_SINK_COUNTER, records, runs) != 0 ||
        run_benchmark(BENCH_SINK_TEXT, records, runs) != 0 ||
        run_benchmark(BENCH_SINK_JSONL, records, runs) != 0 ||
        run_benchmark(BENCH_SINK_QUEUE_COUNTER, records, runs) != 0 ||
        run_benchmark(BENCH_SINK_DISPATCHER_COUNTER, records, runs) != 0 ||
        run_benchmark(BENCH_SINK_DISPATCHER_TEXT, records, runs) != 0 ||
        run_benchmark(BENCH_SINK_DISPATCHER_JSONL, records, runs) != 0 ||
        run_benchmark(BENCH_SINK_DISPATCHER_ALL, records, runs) != 0) {
        return 1;
    }

    return 0;
}
