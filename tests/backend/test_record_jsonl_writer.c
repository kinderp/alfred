/*
 * test_record_jsonl_writer.c - buffered JSONL writer tests
 *
 * This test covers the first buffered JSONL writer boundary:
 *
 * alfred_record_t
 * -> alfred_record_jsonl_writer_emit()
 * -> alfred_record_format_jsonl()
 * -> writer-owned output buffer
 * -> alfred_record_jsonl_writer_flush()
 * -> caller write callback
 *
 * Expected JSONL contract:
 *
 * first record:
 * - {"schema_version":0,"layer":"semantic","category":"filesystem",
 *   "type":"FILE_CREATED","path":"/tmp/root/a.txt"}\n
 *
 * second record:
 * - {"schema_version":0,"layer":"semantic","category":"filesystem",
 *   "type":"FILE_DELETED","path":"/tmp/root/b.txt"}\n
 *
 * Meaning:
 * The writer is not the formatter and it is not the backend. It owns buffering
 * policy for already formatted JSONL lines and calls the output callback only
 * when an explicit flush is requested or when the buffer must be drained before
 * appending another complete line. This locks down the v0 rule that JSONL output
 * must not imply one flush per event.
 *
 * Ownership/lifetime contract:
 * - init requires a configured but inactive writer with used == 0
 * - init must not be used as a shortcut to discard pending buffered bytes
 * - emit/flush may operate while used > 0 because that is the normal buffered
 *   runtime state
 * - a caller that wants to reinitialize storage must flush or explicitly discard
 *   the old writer state before calling init again
 */

#include "alfred_record_jsonl_writer.h"
#include "alfred_record_sink.h"

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

static alfred_record_t make_file_record(alfred_record_type_t type,
                                        const char *path)
{
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = type;
    record.path = path;
    return record;
}

static void configure_writer(alfred_record_jsonl_writer_t *writer,
                             capture_output_t *capture,
                             char *format_buffer,
                             size_t format_buffer_size,
                             char *output_buffer,
                             size_t output_buffer_size)
{
    memset(writer, 0, sizeof(*writer));
    writer->write = capture_write;
    writer->userdata = capture;
    writer->format_buffer = format_buffer;
    writer->format_buffer_size = format_buffer_size;
    writer->buffer = output_buffer;
    writer->buffer_size = output_buffer_size;

    assert(alfred_record_jsonl_writer_init(writer) == 0);
}

static void test_writer_buffers_until_explicit_flush(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
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
    configure_writer(&writer,
                     &capture,
                     format_buffer,
                     sizeof(format_buffer),
                     output_buffer,
                     sizeof(output_buffer));

    record = make_file_record(ALFRED_RECORD_TYPE_FILE_CREATED,
                              "/tmp/root/a.txt");
    assert(alfred_record_jsonl_writer_emit(&writer, &record) == 0);

    assert(capture.count == 0u);
    assert(alfred_record_jsonl_writer_buffered_size(&writer) == strlen(expected));

    assert(alfred_record_jsonl_writer_flush(&writer) == 0);
    assert(capture.count == 1u);
    assert(capture.sizes[0] == strlen(expected));
    assert(strcmp(capture.chunks[0], expected) == 0);
    assert(alfred_record_jsonl_writer_buffered_size(&writer) == 0u);
}

static void test_writer_batches_multiple_records(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
    alfred_record_t first;
    alfred_record_t second;
    char format_buffer[256];
    char output_buffer[512];
    const char *expected =
        "{\"schema_version\":0,"
        "\"layer\":\"semantic\","
        "\"category\":\"filesystem\","
        "\"type\":\"FILE_CREATED\","
        "\"path\":\"/tmp/root/a.txt\"}\n"
        "{\"schema_version\":0,"
        "\"layer\":\"semantic\","
        "\"category\":\"filesystem\","
        "\"type\":\"FILE_DELETED\","
        "\"path\":\"/tmp/root/b.txt\"}\n";

    memset(&capture, 0, sizeof(capture));
    configure_writer(&writer,
                     &capture,
                     format_buffer,
                     sizeof(format_buffer),
                     output_buffer,
                     sizeof(output_buffer));

    first = make_file_record(ALFRED_RECORD_TYPE_FILE_CREATED,
                             "/tmp/root/a.txt");
    second = make_file_record(ALFRED_RECORD_TYPE_FILE_DELETED,
                              "/tmp/root/b.txt");

    assert(alfred_record_jsonl_writer_emit(&writer, &first) == 0);
    assert(alfred_record_jsonl_writer_emit(&writer, &second) == 0);
    assert(capture.count == 0u);

    assert(alfred_record_jsonl_writer_flush(&writer) == 0);
    assert(capture.count == 1u);
    assert(strcmp(capture.chunks[0], expected) == 0);
}

static void test_writer_auto_flushes_when_buffer_needs_space(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
    alfred_record_t first;
    alfred_record_t second;
    char format_buffer[256];
    char output_buffer[132];

    memset(&capture, 0, sizeof(capture));
    configure_writer(&writer,
                     &capture,
                     format_buffer,
                     sizeof(format_buffer),
                     output_buffer,
                     sizeof(output_buffer));

    first = make_file_record(ALFRED_RECORD_TYPE_FILE_CREATED,
                             "/tmp/root/a.txt");
    second = make_file_record(ALFRED_RECORD_TYPE_FILE_DELETED,
                              "/tmp/root/b.txt");

    assert(alfred_record_jsonl_writer_emit(&writer, &first) == 0);
    assert(capture.count == 0u);

    assert(alfred_record_jsonl_writer_emit(&writer, &second) == 0);
    assert(capture.count == 1u);
    assert(strstr(capture.chunks[0], "\"type\":\"FILE_CREATED\"") != NULL);

    assert(alfred_record_jsonl_writer_flush(&writer) == 0);
    assert(capture.count == 2u);
    assert(strstr(capture.chunks[1], "\"type\":\"FILE_DELETED\"") != NULL);
}

static void test_writer_preserves_buffer_on_flush_failure(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
    alfred_record_t record;
    char format_buffer[256];
    char output_buffer[512];
    size_t buffered;

    memset(&capture, 0, sizeof(capture));
    configure_writer(&writer,
                     &capture,
                     format_buffer,
                     sizeof(format_buffer),
                     output_buffer,
                     sizeof(output_buffer));

    record = make_file_record(ALFRED_RECORD_TYPE_FILE_CREATED,
                              "/tmp/root/a.txt");
    assert(alfred_record_jsonl_writer_emit(&writer, &record) == 0);
    buffered = alfred_record_jsonl_writer_buffered_size(&writer);

    capture.fail_writes = 1;
    assert(alfred_record_jsonl_writer_flush(&writer) == -1);
    assert(capture.count == 0u);
    assert(alfred_record_jsonl_writer_buffered_size(&writer) == buffered);

    capture.fail_writes = 0;
    assert(alfred_record_jsonl_writer_flush(&writer) == 0);
    assert(capture.count == 1u);
    assert(alfred_record_jsonl_writer_buffered_size(&writer) == 0u);
}

static void test_writer_rejects_oversized_single_line(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
    alfred_record_t record;
    char format_buffer[256];
    char output_buffer[32];

    memset(&capture, 0, sizeof(capture));
    configure_writer(&writer,
                     &capture,
                     format_buffer,
                     sizeof(format_buffer),
                     output_buffer,
                     sizeof(output_buffer));

    record = make_file_record(ALFRED_RECORD_TYPE_FILE_CREATED,
                              "/tmp/root/a.txt");
    assert(alfred_record_jsonl_writer_emit(&writer, &record) == -1);
    assert(capture.count == 0u);
    assert(alfred_record_jsonl_writer_buffered_size(&writer) == 0u);
}

static void test_writer_exposes_generic_sink(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
    alfred_record_sink_t sink;
    alfred_record_t record;
    char format_buffer[256];
    char output_buffer[512];

    memset(&capture, 0, sizeof(capture));
    configure_writer(&writer,
                     &capture,
                     format_buffer,
                     sizeof(format_buffer),
                     output_buffer,
                     sizeof(output_buffer));

    assert(alfred_record_jsonl_writer_init_sink(&writer, &sink) == 0);

    record = make_file_record(ALFRED_RECORD_TYPE_FILE_CREATED,
                              "/tmp/root/a.txt");
    assert(alfred_record_sink_emit(&sink, &record) == 0);
    assert(capture.count == 0u);

    assert(alfred_record_jsonl_writer_flush(&writer) == 0);
    assert(capture.count == 1u);
    assert(strstr(capture.chunks[0], "\"type\":\"FILE_CREATED\"") != NULL);
}

static void test_writer_rejects_invalid_configuration(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
    alfred_record_sink_t sink;
    char format_buffer[256];
    char output_buffer[512];

    memset(&capture, 0, sizeof(capture));
    memset(&writer, 0, sizeof(writer));

    assert(alfred_record_jsonl_writer_init(NULL) == -1);
    assert(alfred_record_jsonl_writer_init(&writer) == -1);

    writer.write = capture_write;
    writer.userdata = &capture;
    writer.format_buffer = format_buffer;
    writer.format_buffer_size = sizeof(format_buffer);
    writer.buffer = output_buffer;
    writer.buffer_size = sizeof(output_buffer);
    writer.used = sizeof(output_buffer) + 1u;
    assert(alfred_record_jsonl_writer_init(&writer) == -1);

    writer.used = 0u;
    assert(alfred_record_jsonl_writer_init(&writer) == 0);
    assert(alfred_record_jsonl_writer_init_sink(&writer, NULL) == -1);
    assert(alfred_record_jsonl_writer_init_sink(NULL, &sink) == -1);
}

static void test_writer_rejects_reinit_with_pending_bytes(void)
{
    capture_output_t capture;
    alfred_record_jsonl_writer_t writer;
    char format_buffer[256];
    char output_buffer[512];
    const char *pending = "{\"schema_version\":0}\n";
    size_t pending_size = strlen(pending);

    memset(&capture, 0, sizeof(capture));
    configure_writer(&writer,
                     &capture,
                     format_buffer,
                     sizeof(format_buffer),
                     output_buffer,
                     sizeof(output_buffer));

    memcpy(output_buffer, pending, pending_size);
    writer.used = pending_size;

    /*
     * Reinitializing an active writer would silently discard buffered JSONL.
     * The v0 contract makes that invalid: callers must flush or explicitly
     * discard pending bytes before init can be called again.
     */
    assert(alfred_record_jsonl_writer_init(&writer) == -1);
    assert(writer.used == pending_size);
    assert(memcmp(output_buffer, pending, pending_size) == 0);
    assert(capture.count == 0u);
}

int main(void)
{
    test_writer_buffers_until_explicit_flush();
    test_writer_batches_multiple_records();
    test_writer_auto_flushes_when_buffer_needs_space();
    test_writer_preserves_buffer_on_flush_failure();
    test_writer_rejects_oversized_single_line();
    test_writer_exposes_generic_sink();
    test_writer_rejects_invalid_configuration();
    test_writer_rejects_reinit_with_pending_bytes();

    return 0;
}
