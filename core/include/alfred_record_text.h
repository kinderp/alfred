/* ============================================================================
 * alfred_record_text.h - plain-text formatter for Event Model v0 records
 *
 * The current logger writes timestamped lines, but the payload after the log
 * level is still plain text such as:
 *
 *   FILE_CREATED path=/tmp/a.txt
 *   WATCH_STALE wd=7 path=/tmp/root reason=IN_MOVE_SELF
 *
 * This formatter produces that payload from alfred_record_t. It deliberately
 * does not own files, timestamps, FILE streams, or logger_t.
 * ========================================================================== */

#ifndef ALFRED_RECORD_TEXT_H
#define ALFRED_RECORD_TEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"

#include <stddef.h>

/*
 * alfred_record_format_text - format one record as a log payload
 * @record: structured Event Model v0 record to format
 * @dst: caller-owned destination buffer
 * @dst_size: size of @dst in bytes
 *
 * The function writes the plain payload only. It does not add timestamps,
 * levels, or trailing newlines because those belong to the logger or future
 * output device. The output is always NUL-terminated on success.
 *
 * Return: number of bytes written on success, -1 on invalid input,
 * unsupported record shape, or truncation.
 */
int alfred_record_format_text(const alfred_record_t *record,
                              char *dst,
                              size_t dst_size);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_TEXT_H */
