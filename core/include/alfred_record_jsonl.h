/* ============================================================================
 * alfred_record_jsonl.h - JSON Lines formatter for Event Model v0 records
 *
 * JSONL is an output serialization of alfred_record_t, not the internal API.
 * This formatter receives one borrowed record and writes one compact JSON object
 * into caller-provided storage. It does not write files, sockets, timestamps,
 * trailing newlines, or own any writer state.
 *
 * The formatter is deliberately dependency-free: it does not use an external
 * JSON library. It escapes JSON control characters itself and currently assumes
 * that record strings are valid textual data. Lossless serialization of Linux
 * paths containing arbitrary non-UTF-8 bytes is future design work.
 * ========================================================================== */

#ifndef ALFRED_RECORD_JSONL_H
#define ALFRED_RECORD_JSONL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"

#include <stddef.h>

/*
 * alfred_record_format_jsonl - format one record as one JSONL object
 * @record: structured Event Model v0 record to format
 * @dst: caller-owned destination buffer
 * @dst_size: size of @dst in bytes
 *
 * The function writes exactly one JSON object without a trailing newline. It
 * escapes JSON strings, emits stable textual names for layer/category/type, and
 * includes optional payload objects only when their fields are present.
 *
 * Return: number of bytes written on success, -1 on invalid input, unsupported
 * record shape, or truncation.
 */
int alfred_record_format_jsonl(const alfred_record_t *record,
                               char *dst,
                               size_t dst_size);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_JSONL_H */
