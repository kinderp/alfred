/*
 * test_record_dispatcher.c - bounded dispatcher tests for Event Model v0 records
 *
 * This test fixes the fan-out contract before Alfred connects a dispatcher to
 * the runtime queue. A future backend path should enqueue records quickly; a
 * dispatcher can later drain that queue and deliver records to text, JSONL,
 * binary, Lab, or policy sinks.
 *
 * Expected dispatcher contract:
 *
 * registration:
 * - stores sinks in a caller-provided bounded array
 * - rejects invalid sinks and capacity overflow
 *
 * dispatch:
 * - calls sinks in registration order
 * - passes the same borrowed record view to each sink during the call
 * - stops and returns failure when a sink reports failure
 *
 * Meaning:
 * This v0 dispatcher is intentionally synchronous and disconnected from the
 * backend hot path. It proves the routing contract without adding threads,
 * locks, retries, sink queues, or drop/backpressure policy.
 */

#include "alfred_record_dispatcher.h"

#include <assert.h>
#include <string.h>

typedef struct {
    int id;
    int fail;
    size_t *call_count;
    int *order;
    const alfred_record_t **seen_records;
} capture_sink_t;

static int capture_emit(void *userdata, const alfred_record_t *record)
{
    capture_sink_t *capture = userdata;
    size_t index;

    assert(capture != NULL);
    assert(record != NULL);

    index = *capture->call_count;
    capture->order[index] = capture->id;
    capture->seen_records[index] = record;
    (*capture->call_count)++;

    if (capture->fail) {
        return -1;
    }

    return 0;
}

static alfred_record_t make_record(void)
{
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/file.txt";

    return record;
}

static alfred_record_sink_t make_sink(capture_sink_t *capture)
{
    alfred_record_sink_t sink;

    sink.emit = capture_emit;
    sink.userdata = capture;
    return sink;
}

static void test_dispatcher_calls_sinks_in_registration_order(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[2];
    alfred_record_t record;
    alfred_record_sink_t first_sink;
    alfred_record_sink_t second_sink;
    const alfred_record_t *seen_records[2];
    int order[2] = {0, 0};
    size_t call_count = 0u;
    capture_sink_t first;
    capture_sink_t second;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(seen_records, 0, sizeof(seen_records));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));

    first.id = 10;
    first.call_count = &call_count;
    first.order = order;
    first.seen_records = seen_records;
    second.id = 20;
    second.call_count = &call_count;
    second.order = order;
    second.seen_records = seen_records;

    first_sink = make_sink(&first);
    second_sink = make_sink(&second);
    record = make_record();

    assert(alfred_record_dispatcher_init(&dispatcher, entries, 2u) == 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "text",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &first_sink) == 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "jsonl",
               ALFRED_RECORD_DISPATCHER_SINK_CRITICAL,
               &second_sink) == 0);

    assert(alfred_record_dispatcher_dispatch_one(&dispatcher, &record) == 0);
    assert(call_count == 2u);
    assert(order[0] == 10);
    assert(order[1] == 20);
    assert(seen_records[0] == &record);
    assert(seen_records[1] == &record);
}

static void test_dispatcher_propagates_first_sink_failure(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[3];
    alfred_record_t record;
    alfred_record_sink_t first_sink;
    alfred_record_sink_t second_sink;
    alfred_record_sink_t third_sink;
    const alfred_record_t *seen_records[3];
    int order[3] = {0, 0, 0};
    size_t call_count = 0u;
    capture_sink_t first;
    capture_sink_t second;
    capture_sink_t third;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(seen_records, 0, sizeof(seen_records));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&third, 0, sizeof(third));

    first.id = 1;
    first.call_count = &call_count;
    first.order = order;
    first.seen_records = seen_records;
    second.id = 2;
    second.fail = 1;
    second.call_count = &call_count;
    second.order = order;
    second.seen_records = seen_records;
    third.id = 3;
    third.call_count = &call_count;
    third.order = order;
    third.seen_records = seen_records;

    first_sink = make_sink(&first);
    second_sink = make_sink(&second);
    third_sink = make_sink(&third);
    record = make_record();

    assert(alfred_record_dispatcher_init(&dispatcher, entries, 3u) == 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "first",
               ALFRED_RECORD_DISPATCHER_SINK_CRITICAL,
               &first_sink) == 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "second",
               ALFRED_RECORD_DISPATCHER_SINK_CRITICAL,
               &second_sink) == 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "third",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &third_sink) == 0);

    assert(alfred_record_dispatcher_dispatch_one(&dispatcher, &record) != 0);
    assert(call_count == 2u);
    assert(order[0] == 1);
    assert(order[1] == 2);
    assert(order[2] == 0);
}

static void test_dispatcher_rejects_invalid_inputs_and_overflow(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_sink_t sink;
    alfred_record_sink_t invalid_sink;
    alfred_record_t record;
    const alfred_record_t *seen_records[1];
    int order[1] = {0};
    size_t call_count = 0u;
    capture_sink_t capture;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(seen_records, 0, sizeof(seen_records));
    memset(&capture, 0, sizeof(capture));
    memset(&invalid_sink, 0, sizeof(invalid_sink));

    capture.id = 1;
    capture.call_count = &call_count;
    capture.order = order;
    capture.seen_records = seen_records;
    sink = make_sink(&capture);
    record = make_record();

    assert(alfred_record_dispatcher_init(NULL, entries, 1u) != 0);
    assert(alfred_record_dispatcher_init(&dispatcher, NULL, 1u) != 0);
    assert(alfred_record_dispatcher_init(&dispatcher, entries, 0u) != 0);
    assert(alfred_record_dispatcher_init(&dispatcher, entries, 1u) == 0);

    assert(alfred_record_dispatcher_add_sink(
               NULL,
               "sink",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &sink) != 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "invalid",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &invalid_sink) != 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "sink",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &sink) == 0);
    assert(alfred_record_dispatcher_is_full(&dispatcher));
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "overflow",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &sink) != 0);

    assert(alfred_record_dispatcher_dispatch_one(NULL, &record) != 0);
    assert(alfred_record_dispatcher_dispatch_one(&dispatcher, NULL) != 0);
}

static void test_dispatcher_clear_reuses_bounded_storage(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_sink_t sink;
    const alfred_record_t *seen_records[1];
    int order[1] = {0};
    size_t call_count = 0u;
    capture_sink_t capture;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(seen_records, 0, sizeof(seen_records));
    memset(&capture, 0, sizeof(capture));

    capture.id = 1;
    capture.call_count = &call_count;
    capture.order = order;
    capture.seen_records = seen_records;
    sink = make_sink(&capture);

    assert(alfred_record_dispatcher_init(&dispatcher, entries, 1u) == 0);
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "sink",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &sink) == 0);
    assert(alfred_record_dispatcher_count(&dispatcher) == 1u);
    assert(alfred_record_dispatcher_capacity(&dispatcher) == 1u);

    alfred_record_dispatcher_clear(&dispatcher);
    assert(alfred_record_dispatcher_count(&dispatcher) == 0u);
    assert(alfred_record_dispatcher_capacity(&dispatcher) == 1u);
    assert(!alfred_record_dispatcher_is_full(&dispatcher));
    assert(alfred_record_dispatcher_add_sink(
               &dispatcher,
               "sink-again",
               ALFRED_RECORD_DISPATCHER_SINK_BEST_EFFORT,
               &sink) == 0);
}

int main(void)
{
    test_dispatcher_calls_sinks_in_registration_order();
    test_dispatcher_propagates_first_sink_failure();
    test_dispatcher_rejects_invalid_inputs_and_overflow();
    test_dispatcher_clear_reuses_bounded_storage();

    return 0;
}
