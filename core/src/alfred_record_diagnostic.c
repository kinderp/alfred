/* ============================================================================
 * alfred_record_diagnostic.c - diagnostic record builders
 * ========================================================================== */

#include "alfred_record_diagnostic.h"

#include <string.h>

/*
 * diagnostic_category_for_type - classify a diagnostic record type
 * @type: candidate diagnostic record type
 *
 * Watch lifecycle diagnostics describe watch table state. Recovery diagnostics
 * describe resync or lost-scope processing. The split lets writers and future
 * consumers filter WATCH_STALE differently from WATCH_RESYNC_FAILED even though
 * both are diagnostic records.
 *
 * Return: WATCH or RECOVERY category on supported input, UNKNOWN otherwise.
 */
static alfred_record_category_t
diagnostic_category_for_type(alfred_record_type_t type)
{
    switch (type) {
    case ALFRED_RECORD_TYPE_WATCH_ADDED:
    case ALFRED_RECORD_TYPE_WATCH_REMOVED:
    case ALFRED_RECORD_TYPE_WATCH_STALE:
    case ALFRED_RECORD_TYPE_WATCH_STALE_EVENT_DROPPED:
        return ALFRED_RECORD_CATEGORY_WATCH;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_BEGIN:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_FAILED:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_DONE:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_CLASS:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_MISSING:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALLED:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALL_FAILED:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_ROLLBACK:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED:
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_END:
    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED:
    case ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN:
    case ALFRED_RECORD_TYPE_WATCH_LOST_FOUND:
    case ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED:
    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE:
    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING:
    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS:
    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED:
    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED:
    case ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK:
    case ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND:
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED:
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END:
    case ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED:
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP:
        return ALFRED_RECORD_CATEGORY_RECOVERY;

    default:
        return ALFRED_RECORD_CATEGORY_UNKNOWN;
    }
}

int alfred_record_build_watch_diagnostic(alfred_record_type_t type,
                                         const char *backend,
                                         int watch_id,
                                         const char *path,
                                         const char *state,
                                         const char *reason,
                                         const char *error,
                                         alfred_record_t *out)
{
    return alfred_record_build_watch_diagnostic_with_os_error(type,
                                                             backend,
                                                             watch_id,
                                                             path,
                                                             state,
                                                             reason,
                                                             error,
                                                             0,
                                                             NULL,
                                                             NULL,
                                                             out);
}

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
    alfred_record_t *out)
{
    alfred_record_category_t category;

    if (out == NULL) {
        return -1;
    }

    category = diagnostic_category_for_type(type);
    if (category == ALFRED_RECORD_CATEGORY_UNKNOWN) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    out->schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    out->layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    out->category = category;
    out->type = type;
    out->backend = backend;
    out->path = path;
    out->watch.watch_id = watch_id;
    out->watch.state = state;
    out->watch.reason = reason;
    out->watch.error = error;
    out->os_error.code = os_error_code;
    out->os_error.name = os_error_name;
    out->os_error.message = os_error_message;

    return 0;
}
