/* ============================================================================
 * alfred_record_jsonl_sink.h - JSONL sink for Event Model v0 records
 *
 * This sink adapts alfred_record_t to a caller-provided JSONL write callback.
 * It mirrors the text sink shape intentionally: formatting is separated from
 * I/O, and the caller controls storage so runtime code can avoid hidden
 * allocations in hot paths.
 * ========================================================================== */

#ifndef ALFRED_RECORD_JSONL_SINK_H
#define ALFRED_RECORD_JSONL_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record_sink.h"

#include <stddef.h>

/*
 * alfred_record_jsonl_sink_write_fn - consume one formatted JSONL object
 * @userdata: caller-provided writer context
 * @payload: NUL-terminated JSON object without trailing newline
 *
 * @payload is valid only during the call. The callback decides whether to append
 * a newline, write to a file, send on a socket, or store it for tests.
 *
 * Return: 0 on success, -1 on output failure.
 */
typedef int (*alfred_record_jsonl_sink_write_fn)(void *userdata,
                                                 const char *payload);

/*
 * alfred_record_jsonl_sink_t - state for the JSONL sink
 * @write: callback that receives one JSONL object
 * @userdata: opaque context passed to @write
 * @buffer: caller-owned formatting buffer
 * @buffer_size: size of @buffer in bytes
 *
 * The sink does not allocate internally. This is deliberate: JSONL can be a
 * relatively expensive writer, so allocation and buffering policy must remain
 * visible at the writer/dispatcher layer.
 */
typedef struct {
    alfred_record_jsonl_sink_write_fn write;
    void *userdata;
    char *buffer;
    size_t buffer_size;
} alfred_record_jsonl_sink_t;

/*
 * alfred_record_jsonl_sink_init - expose a JSONL sink as a generic record sink
 * @jsonl_sink: initialized JSONL sink state
 * @sink: destination generic sink handle
 *
 * The function does not copy @jsonl_sink. The caller must keep it alive as long
 * as @sink can be used.
 *
 * Return: 0 on success, -1 on invalid input.
 */
int alfred_record_jsonl_sink_init(alfred_record_jsonl_sink_t *jsonl_sink,
                                  alfred_record_sink_t *sink);

/*
 * alfred_record_jsonl_sink_emit - format one record and call the write callback
 * @userdata: alfred_record_jsonl_sink_t passed through alfred_record_sink_t
 * @record: borrowed Event Model v0 record
 *
 * Return: 0 on success, -1 on invalid input, formatting failure, truncation, or
 * write callback failure.
 */
int alfred_record_jsonl_sink_emit(void *userdata,
                                  const alfred_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_JSONL_SINK_H */
