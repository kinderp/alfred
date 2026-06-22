/* ============================================================================
 * alfred_record_text_sink.h - compatibility text sink for records
 *
 * This sink bridges Event Model v0 records to the existing plain-text payload
 * contract. It does not own logger_t and does not write timestamps, levels,
 * files, or newlines. Runtime code can adapt the write callback to the current
 * logger today and to another output device later.
 * ========================================================================== */

#ifndef ALFRED_RECORD_TEXT_SINK_H
#define ALFRED_RECORD_TEXT_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record_sink.h"

#include <stddef.h>

/*
 * alfred_record_text_sink_write_fn - consume one formatted text payload
 * @userdata: caller-provided writer context
 * @payload: NUL-terminated formatted record payload
 *
 * @payload is valid only during the call. It contains no timestamp, log level,
 * trailing newline, or destination-specific framing.
 *
 * Return: 0 on success, -1 on output failure.
 */
typedef int (*alfred_record_text_sink_write_fn)(void *userdata,
                                                const char *payload);

/*
 * alfred_record_text_sink_t - state for the compatibility text sink
 * @write: callback that receives one formatted payload
 * @userdata: opaque context passed to @write
 * @buffer: caller-owned formatting buffer
 * @buffer_size: size of @buffer in bytes
 *
 * The caller owns the buffer so tests and runtime code can choose an explicit
 * allocation policy. Hot paths can provide stack or preallocated storage and
 * avoid hidden heap allocation in the sink.
 */
typedef struct {
    alfred_record_text_sink_write_fn write;
    void *userdata;
    char *buffer;
    size_t buffer_size;
} alfred_record_text_sink_t;

/*
 * alfred_record_text_sink_init - expose a text sink as a generic record sink
 * @text_sink: initialized text sink state
 * @sink: destination generic sink handle
 *
 * The function does not copy @text_sink. The caller must keep it alive as long
 * as @sink can be used.
 *
 * Return: 0 on success, -1 on invalid input.
 */
int alfred_record_text_sink_init(alfred_record_text_sink_t *text_sink,
                                 alfred_record_sink_t *sink);

/*
 * alfred_record_text_sink_emit - format one record and call the write callback
 * @userdata: alfred_record_text_sink_t passed through alfred_record_sink_t
 * @record: borrowed Event Model v0 record
 *
 * Return: 0 on success, -1 on invalid input, formatting failure, truncation, or
 * write callback failure.
 */
int alfred_record_text_sink_emit(void *userdata,
                                 const alfred_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_TEXT_SINK_H */
