/*
 * test_record_jsonl_sink.c - JSONL sink tests
 *
 * This test covers the generic sink boundary for structured JSONL output:
 *
 * alfred_record_t
 * -> alfred_record_sink_emit()
 * -> alfred_record_jsonl_sink_emit()
 * -> caller write callback
 *
 * Expected payload:
 * - {"schema_version":0,"layer":"semantic","category":"filesystem",
 *   "type":"FILE_CREATED","path":"/tmp/root/a.txt"}
 *
 * Meaning:
 * The sink formats JSONL but still performs no I/O itself. It is therefore safe
 * as a small adapter layer and can later sit behind a dispatcher or writer
 * worker without changing the generic sink contract.
 */

#include "alfred_record_jsonl_sink.h"
#include "alfred_record_sink.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char payloads[4][256];
    size_t count;
    int fail_after_first;
} capture_writer_t;

static int capture_write(void *userdata, const char *payload)
{
    capture_writer_t *capture = userdata;

    assert(capture != NULL);
    assert(payload != NULL);

    if (capture->fail_after_first && capture->count > 0u) {
        return -1;
    }

    assert(capture->count < 4u);
    snprintf(capture->payloads[capture->count],
             sizeof(capture->payloads[capture->count]),
             "%s",
             payload);
    capture->count++;
    return 0;
}

static alfred_record_t make_file_created_record(void)
{
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/a.txt";
    return record;
}

static void test_jsonl_sink_emits_payload(void)
{
    capture_writer_t capture;
    alfred_record_jsonl_sink_t jsonl_sink;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char buffer[256];

    memset(&capture, 0, sizeof(capture));
    jsonl_sink.write = capture_write;
    jsonl_sink.userdata = &capture;
    jsonl_sink.buffer = buffer;
    jsonl_sink.buffer_size = sizeof(buffer);

    assert(alfred_record_jsonl_sink_init(&jsonl_sink, &sink) == 0);

    record = make_file_created_record();
    assert(alfred_record_sink_emit(&sink, &record) == 0);

    assert(capture.count == 1u);
    assert(strcmp(capture.payloads[0],
                  "{\"schema_version\":0,"
                  "\"layer\":\"semantic\","
                  "\"category\":\"filesystem\","
                  "\"type\":\"FILE_CREATED\","
                  "\"path\":\"/tmp/root/a.txt\"}") == 0);
}

static void test_jsonl_sink_rejects_invalid_configuration(void)
{
    capture_writer_t capture;
    alfred_record_jsonl_sink_t jsonl_sink;
    alfred_record_sink_t sink;

    memset(&capture, 0, sizeof(capture));
    memset(&jsonl_sink, 0, sizeof(jsonl_sink));

    assert(alfred_record_jsonl_sink_init(NULL, &sink) == -1);
    assert(alfred_record_jsonl_sink_init(&jsonl_sink, NULL) == -1);

    jsonl_sink.write = capture_write;
    jsonl_sink.userdata = &capture;
    assert(alfred_record_jsonl_sink_init(&jsonl_sink, &sink) == -1);
}

static void test_jsonl_sink_propagates_writer_failure(void)
{
    capture_writer_t capture;
    alfred_record_jsonl_sink_t jsonl_sink;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char buffer[256];

    memset(&capture, 0, sizeof(capture));
    capture.fail_after_first = 1;
    jsonl_sink.write = capture_write;
    jsonl_sink.userdata = &capture;
    jsonl_sink.buffer = buffer;
    jsonl_sink.buffer_size = sizeof(buffer);

    assert(alfred_record_jsonl_sink_init(&jsonl_sink, &sink) == 0);

    record = make_file_created_record();
    assert(alfred_record_sink_emit(&sink, &record) == 0);
    assert(alfred_record_sink_emit(&sink, &record) == -1);
    assert(capture.count == 1u);
}

static void test_jsonl_sink_rejects_truncated_payload(void)
{
    capture_writer_t capture;
    alfred_record_jsonl_sink_t jsonl_sink;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char tiny_buffer[8];

    memset(&capture, 0, sizeof(capture));
    jsonl_sink.write = capture_write;
    jsonl_sink.userdata = &capture;
    jsonl_sink.buffer = tiny_buffer;
    jsonl_sink.buffer_size = sizeof(tiny_buffer);

    assert(alfred_record_jsonl_sink_init(&jsonl_sink, &sink) == 0);

    record = make_file_created_record();
    assert(alfred_record_sink_emit(&sink, &record) == -1);
    assert(capture.count == 0u);
}

int main(void)
{
    test_jsonl_sink_emits_payload();
    test_jsonl_sink_rejects_invalid_configuration();
    test_jsonl_sink_propagates_writer_failure();
    test_jsonl_sink_rejects_truncated_payload();

    return 0;
}
