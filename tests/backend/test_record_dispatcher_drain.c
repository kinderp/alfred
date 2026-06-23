/*
 * test_record_dispatcher_drain.c - queue drain tests for Event Model v0
 *
 * This test covers the first place where the queue and dispatcher are used
 * together. A drain is not just "reading" a queue: it consumes queued records by
 * popping an owned record, dispatching it to registered sinks, and destroying the
 * owned record after delivery.
 *
 * Expected drain contract:
 *
 * empty queue:
 * - dispatches zero records
 * - returns success
 *
 * bounded batch:
 * - dispatches at most max_records
 * - leaves later records in the queue for a future drain call
 *
 * normal drain:
 * - dispatches records in FIFO order
 * - destroys each owned record after dispatch
 *
 * sink failure:
 * - stops at the first failing record
 * - reports how many records were successfully dispatched
 * - destroys the popped failing record in v0
 *
 * Meaning:
 * This helper is still disconnected from the runtime. It fixes the lifecycle
 * between queue, dispatcher, and sinks before Alfred chooses threaded workers,
 * retry/requeue policy, dead-letter queues, or per-sink backpressure behavior.
 */

#include "alfred_record_dispatcher.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char paths[8][64];
    size_t count;
    size_t fail_on_call;
} capture_sink_t;

static int capture_emit(void *userdata, const alfred_record_t *record)
{
    capture_sink_t *capture = userdata;

    assert(capture != NULL);
    assert(record != NULL);
    assert(record->path != NULL);
    assert(capture->count < 8u);

    capture->count++;
    if (capture->fail_on_call != 0u &&
        capture->count == capture->fail_on_call) {
        return -1;
    }

    snprintf(capture->paths[capture->count - 1u],
             sizeof(capture->paths[capture->count - 1u]),
             "%s",
             record->path);
    return 0;
}

static alfred_record_t make_record(const char *path)
{
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_NORMALIZED_RAW;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_RAW_CREATE;
    record.path = path;

    return record;
}

static void init_dispatcher(alfred_record_dispatcher_t *dispatcher,
                            alfred_record_dispatcher_sink_t *entries,
                            capture_sink_t *capture)
{
    alfred_record_sink_t sink;

    sink.emit = capture_emit;
    sink.userdata = capture;

    assert(alfred_record_dispatcher_init(dispatcher, entries, 1u) == 0);
    assert(alfred_record_dispatcher_add_sink(
               dispatcher,
               "capture",
               ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
               &sink) == 0);
}

static void test_drain_empty_queue_dispatches_zero(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    capture_sink_t capture;
    size_t dispatched = 99u;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&capture, 0, sizeof(capture));

    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 2u) == 0);

    assert(alfred_record_dispatcher_drain_queue(
               &dispatcher,
               &queue,
               4u,
               &dispatched) == 0);
    assert(dispatched == 0u);
    assert(capture.count == 0u);

    alfred_record_queue_destroy(&queue);
}

static void test_drain_respects_max_records_and_fifo_order(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    capture_sink_t capture;
    alfred_record_t first;
    alfred_record_t second;
    alfred_record_t third;
    size_t dispatched = 0u;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&capture, 0, sizeof(capture));

    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 4u) == 0);

    first = make_record("/tmp/root/one");
    second = make_record("/tmp/root/two");
    third = make_record("/tmp/root/three");

    assert(alfred_record_queue_push(&queue, &first) == 0);
    assert(alfred_record_queue_push(&queue, &second) == 0);
    assert(alfred_record_queue_push(&queue, &third) == 0);

    assert(alfred_record_dispatcher_drain_queue(
               &dispatcher,
               &queue,
               2u,
               &dispatched) == 0);
    assert(dispatched == 2u);
    assert(alfred_record_queue_count(&queue) == 1u);
    assert(capture.count == 2u);
    assert(strcmp(capture.paths[0], "/tmp/root/one") == 0);
    assert(strcmp(capture.paths[1], "/tmp/root/two") == 0);

    assert(alfred_record_dispatcher_drain_queue(
               &dispatcher,
               &queue,
               2u,
               &dispatched) == 0);
    assert(dispatched == 1u);
    assert(alfred_record_queue_is_empty(&queue));
    assert(capture.count == 3u);
    assert(strcmp(capture.paths[2], "/tmp/root/three") == 0);

    alfred_record_queue_destroy(&queue);
}

static void test_drain_zero_max_records_does_not_dispatch(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    capture_sink_t capture;
    alfred_record_t record;
    size_t dispatched = 99u;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&capture, 0, sizeof(capture));

    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 1u) == 0);
    record = make_record("/tmp/root/one");
    assert(alfred_record_queue_push(&queue, &record) == 0);

    assert(alfred_record_dispatcher_drain_queue(
               &dispatcher,
               &queue,
               0u,
               &dispatched) == 0);
    assert(dispatched == 0u);
    assert(capture.count == 0u);
    assert(alfred_record_queue_count(&queue) == 1u);

    alfred_record_queue_destroy(&queue);
}

static void test_drain_stops_on_sink_failure(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    capture_sink_t capture;
    alfred_record_t first;
    alfred_record_t second;
    alfred_record_t third;
    size_t dispatched = 99u;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&capture, 0, sizeof(capture));

    capture.fail_on_call = 2u;
    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 4u) == 0);

    first = make_record("/tmp/root/one");
    second = make_record("/tmp/root/two");
    third = make_record("/tmp/root/three");

    assert(alfred_record_queue_push(&queue, &first) == 0);
    assert(alfred_record_queue_push(&queue, &second) == 0);
    assert(alfred_record_queue_push(&queue, &third) == 0);

    assert(alfred_record_dispatcher_drain_queue(
               &dispatcher,
               &queue,
               4u,
               &dispatched) != 0);
    assert(dispatched == 1u);
    assert(capture.count == 2u);
    assert(strcmp(capture.paths[0], "/tmp/root/one") == 0);
    assert(alfred_record_queue_count(&queue) == 1u);

    alfred_record_queue_destroy(&queue);
}

static void test_drain_rejects_invalid_inputs(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    capture_sink_t capture;
    size_t dispatched = 99u;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&capture, 0, sizeof(capture));

    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 1u) == 0);

    assert(alfred_record_dispatcher_drain_queue(
               NULL,
               &queue,
               1u,
               &dispatched) != 0);
    assert(dispatched == 0u);

    dispatched = 99u;
    assert(alfred_record_dispatcher_drain_queue(
               &dispatcher,
               NULL,
               1u,
               &dispatched) != 0);
    assert(dispatched == 0u);

    alfred_record_queue_destroy(&queue);
}

int main(void)
{
    test_drain_empty_queue_dispatches_zero();
    test_drain_respects_max_records_and_fifo_order();
    test_drain_zero_max_records_does_not_dispatch();
    test_drain_stops_on_sink_failure();
    test_drain_rejects_invalid_inputs();

    return 0;
}
