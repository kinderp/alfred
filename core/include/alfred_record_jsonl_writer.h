/* ============================================================================
 * alfred_record_jsonl_writer.h - buffered JSONL writer for Event Model records
 *
 * This writer sits above the JSONL formatter. It receives borrowed
 * alfred_record_t values, formats them as JSON objects, appends a newline, and
 * accumulates the bytes in caller-owned storage until a flush is required.
 *
 * The writer does not allocate, open files, own file descriptors, create
 * threads, or make the runtime asynchronous. It is a deterministic v0 building
 * block for moving real output work behind queues, dispatcher, and future
 * worker threads.
 * ========================================================================== */

#ifndef ALFRED_RECORD_JSONL_WRITER_H
#define ALFRED_RECORD_JSONL_WRITER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"
#include "alfred_record_sink.h"

#include <stddef.h>

/*
 * alfred_record_jsonl_writer_write_fn - consume buffered JSONL bytes
 * @userdata: caller-provided output context
 * @data: buffered JSONL bytes
 * @size: number of bytes in @data
 *
 * @data is not NUL-terminated by contract. It can contain one or more complete
 * JSONL lines, each ending with '\n'. The callback may write to a file, append
 * to a test buffer, send to a socket, or feed a future output device.
 *
 * Return: 0 on success, -1 on output failure.
 */
typedef int (*alfred_record_jsonl_writer_write_fn)(void *userdata,
                                                   const char *data,
                                                   size_t size);

/*
 * alfred_record_jsonl_writer_t - caller-owned buffered JSONL writer state
 * @write: callback that consumes flushed bytes
 * @userdata: opaque context passed to @write
 * @format_buffer: caller-owned scratch buffer for one JSON object
 * @format_buffer_size: size of @format_buffer in bytes
 * @buffer: caller-owned output buffer for one or more JSONL lines
 * @buffer_size: size of @buffer in bytes
 * @used: bytes currently buffered and not yet flushed
 *
 * Both buffers are caller-owned so the writer can be embedded in controlled
 * runtime contexts without hidden allocations. @format_buffer must be large
 * enough for the biggest single JSON object. @buffer must be large enough for
 * that JSON object plus one trailing newline.
 */
typedef struct {
    alfred_record_jsonl_writer_write_fn write;
    void *userdata;
    char *format_buffer;
    size_t format_buffer_size;
    char *buffer;
    size_t buffer_size;
    size_t used;
} alfred_record_jsonl_writer_t;

/*
 * alfred_record_jsonl_writer_init - validate an inactive buffered JSONL writer
 * @writer: writer state to initialize
 *
 * The function requires a configured but inactive writer with @used == 0. It
 * intentionally does not flush, discard, or reset existing buffered bytes. A
 * caller that wants to reuse storage must first flush pending data or
 * explicitly discard/destroy the old writer state, then call init on an
 * inactive writer.
 *
 * Calling init on a writer with @used > 0 fails and preserves the pending
 * bytes. This prevents accidental silent loss of JSONL ledger data at the
 * writer/plugin boundary.
 *
 * Return: 0 on success, -1 on invalid configuration.
 */
int alfred_record_jsonl_writer_init(alfred_record_jsonl_writer_t *writer);

/*
 * alfred_record_jsonl_writer_init_sink - expose the writer as a generic sink
 * @writer: initialized JSONL writer
 * @sink: destination generic sink handle
 *
 * The function does not copy @writer. The caller must keep it alive as long as
 * @sink can be used by a dispatcher.
 *
 * Return: 0 on success, -1 on invalid input.
 */
int alfred_record_jsonl_writer_init_sink(alfred_record_jsonl_writer_t *writer,
                                         alfred_record_sink_t *sink);

/*
 * alfred_record_jsonl_writer_emit - buffer one record as one JSONL line
 * @writer: initialized JSONL writer
 * @record: borrowed Event Model v0 record
 *
 * The function formats @record, appends '\n', and copies the line into the
 * writer buffer. If the line fits in an empty buffer but not in the remaining
 * space, the function flushes existing bytes first, then appends the new line.
 * It does not flush after every record.
 *
 * Return: 0 on success, -1 on invalid input, formatting failure, oversized
 * single record, or output callback failure while flushing existing bytes.
 */
int alfred_record_jsonl_writer_emit(alfred_record_jsonl_writer_t *writer,
                                    const alfred_record_t *record);

/*
 * alfred_record_jsonl_writer_sink_emit - generic sink callback for the writer
 * @userdata: alfred_record_jsonl_writer_t passed through alfred_record_sink_t
 * @record: borrowed Event Model v0 record
 *
 * Return: 0 on success, -1 on failure.
 */
int alfred_record_jsonl_writer_sink_emit(void *userdata,
                                         const alfred_record_t *record);

/*
 * alfred_record_jsonl_writer_flush - write buffered JSONL bytes
 * @writer: initialized JSONL writer
 *
 * Empty flushes are successful no-ops. If the output callback fails, buffered
 * bytes remain owned by @writer so a caller can inspect or retry later.
 *
 * Return: 0 on success, -1 on invalid input or output callback failure.
 */
int alfred_record_jsonl_writer_flush(alfred_record_jsonl_writer_t *writer);

/*
 * alfred_record_jsonl_writer_buffered_size - report pending bytes
 * @writer: initialized JSONL writer
 *
 * Return: number of bytes currently buffered, or 0 for NULL.
 */
size_t alfred_record_jsonl_writer_buffered_size(
    const alfred_record_jsonl_writer_t *writer);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_JSONL_WRITER_H */
