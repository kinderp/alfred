/* ============================================================================
 * alfred_record_diagnostic.h - diagnostic record builders
 *
 * This header exposes builders for Event Model v0 diagnostic records. The
 * current runtime still writes WATCH_* lines with logger_event(); these helpers
 * create the structured equivalent that future text, JSONL, and binary writers
 * can consume.
 * ========================================================================== */

#ifndef ALFRED_RECORD_DIAGNOSTIC_H
#define ALFRED_RECORD_DIAGNOSTIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"

/*
 * alfred_record_build_watch_diagnostic - build one WATCH_* record
 * @type: diagnostic WATCH_* record type to build
 * @backend: borrowed backend name such as "inotify", or NULL
 * @watch_id: backend watch descriptor/id, or -1 when not applicable
 * @path: borrowed primary path associated with the diagnostic, or NULL
 * @state: borrowed watcher state string such as "stale", or NULL
 * @reason: borrowed reason string such as "IN_MOVE_SELF", or NULL
 * @error: borrowed error string such as "identity-mismatch", or NULL
 * @out: destination record written by this function
 *
 * The builder accepts only diagnostic watch/recovery types currently expressed
 * in alfred_record_type_t. It does not copy strings: every string pointer in
 * @out remains borrowed from the caller.
 *
 * Return: 0 on success, -1 on invalid input or non-diagnostic @type.
 */
int alfred_record_build_watch_diagnostic(alfred_record_type_t type,
                                         const char *backend,
                                         int watch_id,
                                         const char *path,
                                         const char *state,
                                         const char *reason,
                                         const char *error,
                                         alfred_record_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_DIAGNOSTIC_H */
