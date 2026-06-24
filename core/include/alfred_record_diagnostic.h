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
 * This compatibility builder leaves @out->os_error empty. Use
 * alfred_record_build_watch_diagnostic_with_os_error() when the diagnostic also
 * needs to preserve errno-like operating-system evidence.
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

/*
 * alfred_record_build_watch_diagnostic_with_os_error - build WATCH_* record
 * @type: diagnostic WATCH_* record type to build
 * @backend: borrowed backend name such as "inotify", or NULL
 * @watch_id: backend watch descriptor/id, or -1 when not applicable
 * @path: borrowed primary path associated with the diagnostic, or NULL
 * @state: borrowed watcher state string such as "stale", or NULL
 * @reason: borrowed reason string such as "IN_MOVE_SELF", or NULL
 * @error: borrowed Alfred error token such as "path-unreachable", or NULL
 * @os_error_code: numeric operating-system error code, or 0 when unavailable
 * @os_error_name: borrowed symbolic OS error name such as "ENOENT", or NULL
 * @os_error_message: borrowed human-readable OS error message, or NULL
 * @out: destination record written by this function
 *
 * @error and the OS error fields intentionally describe different layers of
 * failure. @error is Alfred's stable diagnostic classification; os_error_*
 * preserves optional platform evidence for debug and structured writers. The
 * builder does not copy strings: backend, path, state, reason, error,
 * os_error_name, and os_error_message remain borrowed.
 *
 * Return: 0 on success, -1 on invalid input or non-diagnostic @type.
 */
int alfred_record_build_watch_diagnostic_with_os_error(
    alfred_record_type_t type,
    const char *backend,
    int watch_id,
    const char *path,
    const char *state,
    const char *reason,
    const char *error,
    int os_error_code,
    const char *os_error_name,
    const char *os_error_message,
    alfred_record_t *out);

/*
 * alfred_record_build_stale_event_dropped - build dropped stale-watch record
 * @backend: borrowed backend name such as "inotify", or NULL
 * @watch_id: backend watch descriptor/id that received the dropped event
 * @path: borrowed stale path associated with @watch_id, or NULL
 * @event_mask: borrowed backend mask text for the dropped event
 * @event_name: borrowed child name carried by the backend event, or NULL
 * @out: destination record written by this function
 *
 * WATCH_STALE_EVENT_DROPPED needs to preserve a different shape from normal
 * WATCH_STALE: it is not just a state transition, but evidence that Alfred saw
 * one backend event and deliberately refused to forward it to raw/core because
 * the watch path was stale. The builder stores the mask/name details inside the
 * watch diagnostic payload. It does not copy strings: every string pointer in
 * @out remains borrowed from the caller.
 *
 * Return: 0 on success, -1 on invalid input.
 */
int alfred_record_build_stale_event_dropped(const char *backend,
                                            int watch_id,
                                            const char *path,
                                            const char *event_mask,
                                            const char *event_name,
                                            alfred_record_t *out);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_DIAGNOSTIC_H */
