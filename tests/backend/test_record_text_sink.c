/*
 * test_record_text_sink.c - compatibility text sink test
 *
 * This test covers the first emit(record) boundary. The sink receives an
 * alfred_record_t, formats it through alfred_record_format_text(), and writes
 * the resulting payload to a caller-provided callback.
 *
 * Expected log contract:
 *
 * semantic single-path event:
 * - FILE_CREATED path=/tmp/root/a.txt
 *
 * diagnostic watch event:
 * - WATCH_STALE wd=7 path=/tmp/root reason=IN_MOVE_SELF
 *
 * Meaning:
 * The text sink must preserve the existing payload contract while moving the
 * runtime shape toward record -> sink -> writer. It must not add timestamps,
 * levels, newlines, or logger-specific behavior.
 */

#include "alfred_record_diagnostic.h"
#include "alfred_record_sink.h"
#include "alfred_record_text_sink.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char payloads[4][160];
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

static void test_text_sink_emits_payloads(void)
{
    capture_writer_t capture;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char buffer[192];

    memset(&capture, 0, sizeof(capture));
    text_sink.write = capture_write;
    text_sink.userdata = &capture;
    text_sink.buffer = buffer;
    text_sink.buffer_size = sizeof(buffer);

    assert(alfred_record_text_sink_init(&text_sink, &sink) == 0);

    record = make_file_created_record();
    assert(alfred_record_sink_emit(&sink, &record) == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_STALE,
               "inotify",
               7,
               "/tmp/root",
               "stale",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    assert(alfred_record_sink_emit(&sink, &record) == 0);

    assert(capture.count == 2u);
    assert(strcmp(capture.payloads[0],
                  "FILE_CREATED path=/tmp/root/a.txt") == 0);
    assert(strcmp(capture.payloads[1],
                  "WATCH_STALE wd=7 path=/tmp/root reason=IN_MOVE_SELF") == 0);
}

static void test_text_sink_rejects_invalid_configuration(void)
{
    capture_writer_t capture;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char buffer[64];

    memset(&capture, 0, sizeof(capture));
    memset(&text_sink, 0, sizeof(text_sink));

    assert(alfred_record_text_sink_init(NULL, &sink) == -1);
    assert(alfred_record_text_sink_init(&text_sink, NULL) == -1);

    text_sink.write = capture_write;
    text_sink.userdata = &capture;
    text_sink.buffer = buffer;
    text_sink.buffer_size = sizeof(buffer);
    assert(alfred_record_text_sink_init(&text_sink, &sink) == 0);

    record = make_file_created_record();
    assert(alfred_record_sink_emit(NULL, &record) == -1);
}

static void test_text_sink_propagates_writer_failure(void)
{
    capture_writer_t capture;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char buffer[192];

    memset(&capture, 0, sizeof(capture));
    capture.fail_after_first = 1;
    text_sink.write = capture_write;
    text_sink.userdata = &capture;
    text_sink.buffer = buffer;
    text_sink.buffer_size = sizeof(buffer);

    assert(alfred_record_text_sink_init(&text_sink, &sink) == 0);

    record = make_file_created_record();
    assert(alfred_record_sink_emit(&sink, &record) == 0);
    assert(alfred_record_sink_emit(&sink, &record) == -1);
    assert(capture.count == 1u);
}

static void test_text_sink_rejects_truncated_payload(void)
{
    capture_writer_t capture;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char tiny_buffer[8];

    memset(&capture, 0, sizeof(capture));
    text_sink.write = capture_write;
    text_sink.userdata = &capture;
    text_sink.buffer = tiny_buffer;
    text_sink.buffer_size = sizeof(tiny_buffer);

    assert(alfred_record_text_sink_init(&text_sink, &sink) == 0);

    record = make_file_created_record();
    assert(alfred_record_sink_emit(&sink, &record) == -1);
    assert(capture.count == 0u);
}

int main(void)
{
    test_text_sink_emits_payloads();
    test_text_sink_rejects_invalid_configuration();
    test_text_sink_propagates_writer_failure();
    test_text_sink_rejects_truncated_payload();

    return 0;
}
