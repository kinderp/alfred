/* ============================================================================
 * alfred_record_jsonl_writer.c - buffered JSONL writer for Event Model records
 * ========================================================================== */

#include "alfred_record_jsonl_writer.h"

#include "alfred_record_jsonl.h"

#include <string.h>

static int writer_storage_is_configured(
    const alfred_record_jsonl_writer_t *writer)
{
    return writer != NULL &&
           writer->write != NULL &&
           writer->format_buffer != NULL &&
           writer->format_buffer_size > 0u &&
           writer->buffer != NULL &&
           writer->buffer_size > 0u;
}

static int writer_runtime_is_valid(const alfred_record_jsonl_writer_t *writer)
{
    return writer_storage_is_configured(writer) &&
           writer->used <= writer->buffer_size;
}

int alfred_record_jsonl_writer_init(alfred_record_jsonl_writer_t *writer)
{
    if (!writer_storage_is_configured(writer) || writer->used != 0u) {
        return -1;
    }

    return 0;
}

int alfred_record_jsonl_writer_init_sink(alfred_record_jsonl_writer_t *writer,
                                         alfred_record_sink_t *sink)
{
    if (!writer_runtime_is_valid(writer) || sink == NULL) {
        return -1;
    }

    sink->emit = alfred_record_jsonl_writer_sink_emit;
    sink->userdata = writer;
    return 0;
}

int alfred_record_jsonl_writer_emit(alfred_record_jsonl_writer_t *writer,
                                    const alfred_record_t *record)
{
    int formatted;
    size_t line_size;

    if (!writer_runtime_is_valid(writer) || record == NULL) {
        return -1;
    }

    formatted = alfred_record_format_jsonl(record,
                                           writer->format_buffer,
                                           writer->format_buffer_size);
    if (formatted < 0) {
        return -1;
    }

    line_size = (size_t)formatted + 1u;
    if (line_size > writer->buffer_size) {
        return -1;
    }

    if (writer->buffer_size - writer->used < line_size) {
        if (alfred_record_jsonl_writer_flush(writer) != 0) {
            return -1;
        }
    }

    memcpy(writer->buffer + writer->used,
           writer->format_buffer,
           (size_t)formatted);
    writer->used += (size_t)formatted;
    writer->buffer[writer->used++] = '\n';
    return 0;
}

int alfred_record_jsonl_writer_sink_emit(void *userdata,
                                         const alfred_record_t *record)
{
    return alfred_record_jsonl_writer_emit(userdata, record);
}

int alfred_record_jsonl_writer_flush(alfred_record_jsonl_writer_t *writer)
{
    if (!writer_runtime_is_valid(writer)) {
        return -1;
    }

    if (writer->used == 0u) {
        return 0;
    }

    if (writer->write(writer->userdata, writer->buffer, writer->used) != 0) {
        return -1;
    }

    writer->used = 0u;
    return 0;
}

size_t alfred_record_jsonl_writer_buffered_size(
    const alfred_record_jsonl_writer_t *writer)
{
    if (writer == NULL) {
        return 0u;
    }

    return writer->used;
}
