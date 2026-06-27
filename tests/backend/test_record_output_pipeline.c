/*
 * test_record_output_pipeline.c - single-writer output pipeline tests
 *
 * This test covers the first composed output pipeline:
 *
 * alfred_record_t
 * -> alfred_record_output_pipeline_enqueue()
 * -> alfred_record_queue_push()
 * -> alfred_record_output_pipeline_drain_once()
 * -> alfred_record_runtime_drain_once()
 * -> alfred_record_dispatcher_dispatch_one()
 * -> alfred_record_jsonl_writer_emit()
 * -> alfred_record_output_pipeline_flush()
 * -> caller write(data, size)
 *
 * Expected JSONL contract after flush:
 *
 * - {"schema_version":0,"layer":"semantic","category":"filesystem",
 *   "type":"FILE_CREATED","path":"/tmp/root/a.txt"}\n
 *
 * Meaning:
 * The pipeline is still single-threaded and intentionally free of worker
 * threads, sockets, or real backpressure. app.c can now wire it behind
 * output_enabled=true for additive JSONL output, while this unit test keeps the
 * helper isolated from runtime file ownership. Disabled mode is a no-op and
 * represents output_enabled=false: Alfred keeps the compatibility logs outside
 * this pipeline. Counter mode is a benchmark/no-op variant of the same
 * queue/dispatcher path: it drains records into alfred_record_counter_sink_t
 * without JSONL formatting, buffering, or output writes.
 */

#include "alfred_record_output_pipeline.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char chunks[4][512];
    size_t sizes[4];
    size_t count;
    int fail_writes;
} capture_output_t;

static int capture_write(void *userdata, const char *data, size_t size)
{
    capture_output_t *capture = userdata;

    assert(capture != NULL);
    assert(data != NULL);

    if (capture->fail_writes) {
        return -1;
    }

    assert(capture->count < 4u);
    assert(size < sizeof(capture->chunks[capture->count]));
    memcpy(capture->chunks[capture->count], data, size);
    capture->chunks[capture->count][size] = '\0';
    capture->sizes[capture->count] = size;
    capture->count++;
    return 0;
}

static alfred_record_t make_file_created_record(const char *path)
{
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = path;
    return record;
}

static alfred_record_output_pipeline_config_t make_enabled_config(
    capture_output_t *capture,
    char *format_buffer,
    size_t format_buffer_size,
    char *output_buffer,
    size_t output_buffer_size)
{
    alfred_record_output_pipeline_config_t config;

    memset(&config, 0, sizeof(config));
    config.enabled = 1;
    config.format = ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_JSONL;
    config.queue_capacity = 4u;
    config.drain_batch_size = 8u;
    config.write = capture_write;
    config.userdata = capture;
    config.format_buffer = format_buffer;
    config.format_buffer_size = format_buffer_size;
    config.output_buffer = output_buffer;
    config.output_buffer_size = output_buffer_size;
    return config;
}

static alfred_record_output_pipeline_config_t make_counter_config(void)
{
    alfred_record_output_pipeline_config_t config;

    memset(&config, 0, sizeof(config));
    config.enabled = 1;
    config.format = ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_COUNTER;
    config.queue_capacity = 4u;
    config.drain_batch_size = 8u;
    return config;
}

static void test_disabled_pipeline_is_noop(void)
{
    alfred_record_output_pipeline_t pipeline;
    alfred_record_output_pipeline_config_t config;
    alfred_record_runtime_drain_result_t result;
    alfred_record_t record;

    memset(&pipeline, 0, sizeof(pipeline));
    memset(&config, 0, sizeof(config));
    config.enabled = 0;

    assert(alfred_record_output_pipeline_init(&pipeline, &config) == 0);

    record = make_file_created_record("/tmp/root/a.txt");
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &record) == 0);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 0u);

    assert(alfred_record_output_pipeline_drain_once(&pipeline, &result) == 0);
    assert(result.max_records == 0u);
    assert(result.dispatched == 0u);
    assert(result.remaining == 0u);
    assert(result.status == 0);

    assert(alfred_record_output_pipeline_flush(&pipeline) == 0);
    assert(alfred_record_output_pipeline_buffered_bytes(&pipeline) == 0u);

    alfred_record_output_pipeline_destroy(&pipeline);
}

static void test_enabled_jsonl_pipeline_enqueues_drains_and_flushes(void)
{
    capture_output_t capture;
    alfred_record_output_pipeline_t pipeline;
    alfred_record_output_pipeline_config_t config;
    alfred_record_runtime_drain_result_t result;
    alfred_record_t record;
    char format_buffer[256];
    char output_buffer[512];
    const char *expected =
        "{\"schema_version\":0,"
        "\"layer\":\"semantic\","
        "\"category\":\"filesystem\","
        "\"type\":\"FILE_CREATED\","
        "\"path\":\"/tmp/root/a.txt\"}\n";

    memset(&capture, 0, sizeof(capture));
    memset(&pipeline, 0, sizeof(pipeline));
    config = make_enabled_config(&capture,
                                 format_buffer,
                                 sizeof(format_buffer),
                                 output_buffer,
                                 sizeof(output_buffer));

    assert(alfred_record_output_pipeline_init(&pipeline, &config) == 0);

    record = make_file_created_record("/tmp/root/a.txt");
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &record) == 0);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 1u);
    assert(capture.count == 0u);

    assert(alfred_record_output_pipeline_drain_once(&pipeline, &result) == 0);
    assert(result.max_records == 8u);
    assert(result.dispatched == 1u);
    assert(result.remaining == 0u);
    assert(result.status == 0);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 0u);

    assert(capture.count == 0u);
    assert(alfred_record_output_pipeline_buffered_bytes(&pipeline) ==
           strlen(expected));

    assert(alfred_record_output_pipeline_flush(&pipeline) == 0);
    assert(capture.count == 1u);
    assert(capture.sizes[0] == strlen(expected));
    assert(strcmp(capture.chunks[0], expected) == 0);
    assert(alfred_record_output_pipeline_buffered_bytes(&pipeline) == 0u);

    alfred_record_output_pipeline_destroy(&pipeline);
}

static void test_enabled_counter_pipeline_drains_without_output(void)
{
    alfred_record_output_pipeline_t pipeline;
    alfred_record_output_pipeline_config_t config;
    alfred_record_runtime_drain_result_t result;
    alfred_record_t record;

    memset(&pipeline, 0, sizeof(pipeline));
    config = make_counter_config();

    assert(alfred_record_output_pipeline_init(&pipeline, &config) == 0);

    record = make_file_created_record("/tmp/root/a.txt");
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &record) == 0);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 1u);

    assert(alfred_record_output_pipeline_drain_once(&pipeline, &result) == 0);
    assert(result.max_records == 8u);
    assert(result.dispatched == 1u);
    assert(result.remaining == 0u);
    assert(result.status == 0);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 0u);
    assert(alfred_record_output_pipeline_buffered_bytes(&pipeline) == 0u);
    assert(alfred_record_output_pipeline_flush(&pipeline) == 0);

    alfred_record_output_pipeline_destroy(&pipeline);
}

static void test_pipeline_respects_drain_batch_size(void)
{
    capture_output_t capture;
    alfred_record_output_pipeline_t pipeline;
    alfred_record_output_pipeline_config_t config;
    alfred_record_runtime_drain_result_t result;
    alfred_record_t first;
    alfred_record_t second;
    char format_buffer[256];
    char output_buffer[512];

    memset(&capture, 0, sizeof(capture));
    memset(&pipeline, 0, sizeof(pipeline));
    config = make_enabled_config(&capture,
                                 format_buffer,
                                 sizeof(format_buffer),
                                 output_buffer,
                                 sizeof(output_buffer));
    config.drain_batch_size = 1u;

    assert(alfred_record_output_pipeline_init(&pipeline, &config) == 0);

    first = make_file_created_record("/tmp/root/a.txt");
    second = make_file_created_record("/tmp/root/b.txt");
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &first) == 0);
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &second) == 0);

    assert(alfred_record_output_pipeline_drain_once(&pipeline, &result) == 0);
    assert(result.max_records == 1u);
    assert(result.dispatched == 1u);
    assert(result.remaining == 1u);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 1u);

    assert(alfred_record_output_pipeline_drain_once(&pipeline, &result) == 0);
    assert(result.dispatched == 1u);
    assert(result.remaining == 0u);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 0u);

    assert(alfred_record_output_pipeline_flush(&pipeline) == 0);
    assert(capture.count == 1u);
    assert(strstr(capture.chunks[0], "\"path\":\"/tmp/root/a.txt\"") != NULL);
    assert(strstr(capture.chunks[0], "\"path\":\"/tmp/root/b.txt\"") != NULL);

    alfred_record_output_pipeline_destroy(&pipeline);
}

static void test_pipeline_reports_queue_full(void)
{
    capture_output_t capture;
    alfred_record_output_pipeline_t pipeline;
    alfred_record_output_pipeline_config_t config;
    alfred_record_t first;
    alfred_record_t second;
    char format_buffer[256];
    char output_buffer[512];

    memset(&capture, 0, sizeof(capture));
    memset(&pipeline, 0, sizeof(pipeline));
    config = make_enabled_config(&capture,
                                 format_buffer,
                                 sizeof(format_buffer),
                                 output_buffer,
                                 sizeof(output_buffer));
    config.queue_capacity = 1u;

    assert(alfred_record_output_pipeline_init(&pipeline, &config) == 0);

    first = make_file_created_record("/tmp/root/a.txt");
    second = make_file_created_record("/tmp/root/b.txt");
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &first) == 0);
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &second) == -1);
    assert(alfred_record_output_pipeline_pending(&pipeline) == 1u);

    alfred_record_output_pipeline_destroy(&pipeline);
}

static void test_pipeline_propagates_flush_failure(void)
{
    capture_output_t capture;
    alfred_record_output_pipeline_t pipeline;
    alfred_record_output_pipeline_config_t config;
    alfred_record_runtime_drain_result_t result;
    alfred_record_t record;
    char format_buffer[256];
    char output_buffer[512];
    size_t buffered;

    memset(&capture, 0, sizeof(capture));
    memset(&pipeline, 0, sizeof(pipeline));
    config = make_enabled_config(&capture,
                                 format_buffer,
                                 sizeof(format_buffer),
                                 output_buffer,
                                 sizeof(output_buffer));

    assert(alfred_record_output_pipeline_init(&pipeline, &config) == 0);

    record = make_file_created_record("/tmp/root/a.txt");
    assert(alfred_record_output_pipeline_enqueue(&pipeline, &record) == 0);
    assert(alfred_record_output_pipeline_drain_once(&pipeline, &result) == 0);
    buffered = alfred_record_output_pipeline_buffered_bytes(&pipeline);

    capture.fail_writes = 1;
    assert(alfred_record_output_pipeline_flush(&pipeline) == -1);
    assert(capture.count == 0u);
    assert(alfred_record_output_pipeline_buffered_bytes(&pipeline) == buffered);

    capture.fail_writes = 0;
    assert(alfred_record_output_pipeline_flush(&pipeline) == 0);
    assert(capture.count == 1u);

    alfred_record_output_pipeline_destroy(&pipeline);
}

static void test_pipeline_rejects_invalid_enabled_config(void)
{
    capture_output_t capture;
    alfred_record_output_pipeline_t pipeline;
    alfred_record_output_pipeline_config_t config;
    char format_buffer[256];
    char output_buffer[512];

    memset(&capture, 0, sizeof(capture));
    memset(&pipeline, 0, sizeof(pipeline));
    config = make_enabled_config(&capture,
                                 format_buffer,
                                 sizeof(format_buffer),
                                 output_buffer,
                                 sizeof(output_buffer));

    config.queue_capacity = 0u;
    assert(alfred_record_output_pipeline_init(&pipeline, &config) == -1);

    memset(&pipeline, 0, sizeof(pipeline));
    config = make_enabled_config(&capture,
                                 format_buffer,
                                 sizeof(format_buffer),
                                 output_buffer,
                                 sizeof(output_buffer));
    config.write = NULL;
    assert(alfred_record_output_pipeline_init(&pipeline, &config) == -1);

    memset(&pipeline, 0, sizeof(pipeline));
    config = make_counter_config();
    config.queue_capacity = 0u;
    assert(alfred_record_output_pipeline_init(&pipeline, &config) == -1);
}

int main(void)
{
    test_disabled_pipeline_is_noop();
    test_enabled_jsonl_pipeline_enqueues_drains_and_flushes();
    test_enabled_counter_pipeline_drains_without_output();
    test_pipeline_respects_drain_batch_size();
    test_pipeline_reports_queue_full();
    test_pipeline_propagates_flush_failure();
    test_pipeline_rejects_invalid_enabled_config();

    return 0;
}
