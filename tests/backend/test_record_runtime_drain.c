/*
 * test_record_runtime_drain.c - runtime drain helper contract tests
 *
 * The runtime drain helper is the named single-threaded step that a future
 * writer worker will repeat. It sits above the lower-level dispatcher drain
 * helper and returns a small result summary:
 *
 * - max_records: batch limit requested by the caller
 * - dispatched: records delivered successfully to all registered sinks
 * - remaining: records left in the queue after the attempt
 * - status: 0 on success, -1 on invalid input or first dispatch failure
 *
 * Expected contract:
 *
 * empty queue:
 * - success
 * - zero dispatched
 * - zero remaining
 *
 * bounded batch:
 * - success
 * - dispatches at most max_records
 * - leaves later records queued
 *
 * complete drain:
 * - success
 * - dispatches every queued record
 * - leaves the queue empty
 *
 * sink failure:
 * - failure
 * - reports only successful records in dispatched
 * - reports records still queued after the failing popped record is destroyed
 */

#include "alfred_record_runtime.h"

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

static void test_runtime_drain_empty_queue_reports_zero(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    alfred_record_runtime_drain_result_t result;
    capture_sink_t capture;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&result, 0, sizeof(result));
    memset(&capture, 0, sizeof(capture));

    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 2u) == 0);

    assert(alfred_record_runtime_drain_once(
               &dispatcher,
               &queue,
               4u,
               &result) == 0);
    assert(result.max_records == 4u);
    assert(result.dispatched == 0u);
    assert(result.remaining == 0u);
    assert(result.status == 0);
    assert(capture.count == 0u);

    alfred_record_queue_destroy(&queue);
}

static void test_runtime_drain_respects_batch_limit(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    alfred_record_runtime_drain_result_t result;
    capture_sink_t capture;
    alfred_record_t first;
    alfred_record_t second;
    alfred_record_t third;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&result, 0, sizeof(result));
    memset(&capture, 0, sizeof(capture));

    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 4u) == 0);

    first = make_record("/tmp/root/one");
    second = make_record("/tmp/root/two");
    third = make_record("/tmp/root/three");

    assert(alfred_record_queue_push(&queue, &first) == 0);
    assert(alfred_record_queue_push(&queue, &second) == 0);
    assert(alfred_record_queue_push(&queue, &third) == 0);

    assert(alfred_record_runtime_drain_once(
               &dispatcher,
               &queue,
               2u,
               &result) == 0);
    assert(result.max_records == 2u);
    assert(result.dispatched == 2u);
    assert(result.remaining == 1u);
    assert(result.status == 0);
    assert(capture.count == 2u);
    assert(strcmp(capture.paths[0], "/tmp/root/one") == 0);
    assert(strcmp(capture.paths[1], "/tmp/root/two") == 0);

    alfred_record_queue_destroy(&queue);
}

static void test_runtime_drain_complete_queue(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    alfred_record_runtime_drain_result_t result;
    capture_sink_t capture;
    alfred_record_t first;
    alfred_record_t second;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&result, 0, sizeof(result));
    memset(&capture, 0, sizeof(capture));

    init_dispatcher(&dispatcher, entries, &capture);
    assert(alfred_record_queue_init(&queue, 2u) == 0);

    first = make_record("/tmp/root/one");
    second = make_record("/tmp/root/two");

    assert(alfred_record_queue_push(&queue, &first) == 0);
    assert(alfred_record_queue_push(&queue, &second) == 0);

    assert(alfred_record_runtime_drain_once(
               &dispatcher,
               &queue,
               8u,
               &result) == 0);
    assert(result.max_records == 8u);
    assert(result.dispatched == 2u);
    assert(result.remaining == 0u);
    assert(result.status == 0);
    assert(capture.count == 2u);
    assert(alfred_record_queue_is_empty(&queue));

    alfred_record_queue_destroy(&queue);
}

static void test_runtime_drain_reports_sink_failure(void)
{
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t entries[1];
    alfred_record_queue_t queue;
    alfred_record_runtime_drain_result_t result;
    capture_sink_t capture;
    alfred_record_t first;
    alfred_record_t second;
    alfred_record_t third;

    memset(&dispatcher, 0, sizeof(dispatcher));
    memset(entries, 0, sizeof(entries));
    memset(&queue, 0, sizeof(queue));
    memset(&result, 0, sizeof(result));
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

    assert(alfred_record_runtime_drain_once(
               &dispatcher,
               &queue,
               4u,
               &result) != 0);
    assert(result.max_records == 4u);
    assert(result.dispatched == 1u);
    assert(result.remaining == 1u);
    assert(result.status == -1);
    assert(capture.count == 2u);
    assert(strcmp(capture.paths[0], "/tmp/root/one") == 0);

    alfred_record_queue_destroy(&queue);
}

static void test_runtime_drain_rejects_invalid_inputs(void)
{
    alfred_record_runtime_drain_result_t result;

    memset(&result, 0, sizeof(result));

    assert(alfred_record_runtime_drain_once(NULL, NULL, 3u, &result) != 0);
    assert(result.max_records == 3u);
    assert(result.dispatched == 0u);
    assert(result.remaining == 0u);
    assert(result.status == -1);
}

int main(void)
{
    test_runtime_drain_empty_queue_reports_zero();
    test_runtime_drain_respects_batch_limit();
    test_runtime_drain_complete_queue();
    test_runtime_drain_reports_sink_failure();
    test_runtime_drain_rejects_invalid_inputs();

    return 0;
}
