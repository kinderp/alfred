/* ============================================================================
 * alfred_record_text.c - plain-text formatter for Event Model v0 records
 * ========================================================================== */

#include "alfred_record_text.h"

#include <stdarg.h>
#include <stdio.h>

/*
 * record_type_name - return the stable textual name for a record type
 * @type: Event Model v0 record type
 *
 * These names are the payload tokens used by text logs and tests. Keeping the
 * mapping centralized prevents future writers from inventing slightly
 * different names for the same structured record.
 *
 * Return: static string on known type, NULL on unsupported type.
 */
static const char *record_type_name(alfred_record_type_t type)
{
    switch (type) {
    case ALFRED_RECORD_TYPE_RAW_CREATE: return "RAW_CREATE";
    case ALFRED_RECORD_TYPE_RAW_DELETE: return "RAW_DELETE";
    case ALFRED_RECORD_TYPE_RAW_MODIFY: return "RAW_MODIFY";
    case ALFRED_RECORD_TYPE_RAW_ATTRIB: return "RAW_ATTRIB";
    case ALFRED_RECORD_TYPE_RAW_CLOSE_WRITE: return "RAW_CLOSE_WRITE";
    case ALFRED_RECORD_TYPE_RAW_MOVED_FROM: return "RAW_MOVED_FROM";
    case ALFRED_RECORD_TYPE_RAW_MOVED_TO: return "RAW_MOVED_TO";
    case ALFRED_RECORD_TYPE_RAW_OVERFLOW: return "RAW_OVERFLOW";

    case ALFRED_RECORD_TYPE_FILE_CREATED: return "FILE_CREATED";
    case ALFRED_RECORD_TYPE_FILE_READY: return "FILE_READY";
    case ALFRED_RECORD_TYPE_FILE_MODIFIED: return "FILE_MODIFIED";
    case ALFRED_RECORD_TYPE_FILE_DELETED: return "FILE_DELETED";
    case ALFRED_RECORD_TYPE_DIR_CREATED: return "DIR_CREATED";
    case ALFRED_RECORD_TYPE_DIR_DELETED: return "DIR_DELETED";
    case ALFRED_RECORD_TYPE_FILE_RENAMED: return "FILE_RENAMED";
    case ALFRED_RECORD_TYPE_FILE_MOVED: return "FILE_MOVED";
    case ALFRED_RECORD_TYPE_FILE_RELOCATED: return "FILE_RELOCATED";
    case ALFRED_RECORD_TYPE_DIR_RENAMED: return "DIR_RENAMED";
    case ALFRED_RECORD_TYPE_DIR_MOVED: return "DIR_MOVED";
    case ALFRED_RECORD_TYPE_DIR_RELOCATED: return "DIR_RELOCATED";
    case ALFRED_RECORD_TYPE_OVERFLOW: return "OVERFLOW";

    case ALFRED_RECORD_TYPE_WATCH_ADDED: return "WATCH_ADDED";
    case ALFRED_RECORD_TYPE_WATCH_REMOVED: return "WATCH_REMOVED";
    case ALFRED_RECORD_TYPE_WATCH_STALE: return "WATCH_STALE";
    case ALFRED_RECORD_TYPE_WATCH_STALE_EVENT_DROPPED:
        return "WATCH_STALE_EVENT_DROPPED";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_BEGIN: return "WATCH_RESYNC_BEGIN";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_FAILED:
        return "WATCH_RESYNC_SCAN_FAILED";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_DONE:
        return "WATCH_RESYNC_SCAN_DONE";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_CLASS:
        return "WATCH_RESYNC_SCAN_CLASS";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_MISSING:
        return "WATCH_RESYNC_SCAN_MISSING";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALLED:
        return "WATCH_RESYNC_REINSTALLED";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALL_FAILED:
        return "WATCH_RESYNC_REINSTALL_FAILED";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_ROLLBACK:
        return "WATCH_RESYNC_ROLLBACK";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED:
        return "WATCH_RESYNC_FAILED";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_END: return "WATCH_RESYNC_END";
    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED: return "WATCH_LOST_QUEUED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_FOUND: return "WATCH_LOST_FOUND";
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END:
        return "WATCH_LOST_RECOVERY_END";
    case ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED:
        return "WATCH_LOST_RETRY_SCHEDULED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP:
        return "WATCH_LOST_RECOVERY_GAVE_UP";

    default:
        return NULL;
    }
}

/*
 * checked_snprintf - run snprintf and reject truncation
 * @dst: destination buffer
 * @dst_size: size of @dst in bytes
 * @fmt: printf-style format string
 *
 * Return: bytes written on success, -1 on formatting error or truncation.
 */
static int checked_snprintf(char *dst, size_t dst_size, const char *fmt, ...)
{
    int written;
    va_list ap;

    if (dst == NULL || dst_size == 0u || fmt == NULL) {
        return -1;
    }

    va_start(ap, fmt);
    written = vsnprintf(dst, dst_size, fmt, ap);
    va_end(ap);

    if (written < 0 || (size_t)written >= dst_size) {
        return -1;
    }

    return written;
}

/*
 * format_filesystem_text - format raw or semantic filesystem record payloads
 * @record: filesystem record to format
 * @name: textual type name
 * @dst: destination buffer
 * @dst_size: size of @dst in bytes
 *
 * Semantic move-like records use the current event-log shape:
 *
 *   TYPE from=old to=new
 *
 * Single-path records use:
 *
 *   TYPE path=path
 *
 * Normalized raw records include the raw mask because RAW_* type alone does
 * not carry flags such as ALFRED_RAW_ISDIR.
 *
 * Return: bytes written on success, -1 on invalid input or truncation.
 */
static int format_filesystem_text(const alfred_record_t *record,
                                  const char *name,
                                  char *dst,
                                  size_t dst_size)
{
    const char *path;
    const char *old_path;
    const char *new_path;

    if (record->layer == ALFRED_RECORD_LAYER_NORMALIZED_RAW) {
        path = record->path != NULL ? record->path : "";
        return checked_snprintf(dst,
                                dst_size,
                                "%s path=%s mask=%u",
                                name,
                                path,
                                record->raw_mask);
    }

    if (record->layer != ALFRED_RECORD_LAYER_SEMANTIC) {
        return -1;
    }

    if (record->new_path != NULL) {
        old_path = record->old_path != NULL
                       ? record->old_path
                       : (record->path != NULL ? record->path : "");
        new_path = record->new_path;
        return checked_snprintf(dst,
                                dst_size,
                                "%s from=%s to=%s",
                                name,
                                old_path,
                                new_path);
    }

    path = record->path != NULL ? record->path : "";
    return checked_snprintf(dst, dst_size, "%s path=%s", name, path);
}

/*
 * format_diagnostic_text - format watch/recovery diagnostic payloads
 * @record: diagnostic record to format
 * @name: textual type name
 * @dst: destination buffer
 * @dst_size: size of @dst in bytes
 *
 * The output intentionally mirrors the compact WATCH_* text contract where the
 * fields present in the record are appended in stable order. OS errors remain
 * structured in alfred_record_t, but the compatibility text writer renders them
 * as the historical errno=N or errno=N (message) suffix.
 *
 * Return: bytes written on success, -1 on invalid input or truncation.
 */
static int format_diagnostic_text(const alfred_record_t *record,
                                  const char *name,
                                  char *dst,
                                  size_t dst_size)
{
    const char *path = record->path != NULL ? record->path : "";
    const char *reason = record->watch.reason;
    const char *error = record->watch.error;
    const char *state = record->watch.state;

    if (reason != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_FAILED) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s rc=%d",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                record->recovery.result_code);
    }

    if (reason != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_DONE) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s "
                                "dirs=%zu watched=%zu missing=%zu",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                record->recovery.directories_seen,
                                record->recovery.directories_watched,
                                record->recovery.directories_missing);
    }

    if (reason != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_CLASS &&
        state != NULL) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s result=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                state);
    }

    if (reason != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_MISSING &&
        record->recovery.detail_path != NULL) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s missing_path=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                record->recovery.detail_path);
    }

    if (reason != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALLED &&
        record->recovery.detail_path != NULL) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s installed_path=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                record->recovery.detail_path);
    }

    if (reason != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALL_FAILED &&
        record->recovery.detail_path != NULL) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s missing_path=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                record->recovery.detail_path);
    }

    if (reason != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_ROLLBACK) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s removed_wd=%d",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                record->recovery.related_watch_id);
    }

    if (reason != NULL && error != NULL) {
        if (record->os_error.code != 0) {
            if (record->os_error.message != NULL) {
                return checked_snprintf(dst,
                                        dst_size,
                                        "%s wd=%d path=%s reason=%s error=%s "
                                        "errno=%d (%s)",
                                        name,
                                        record->watch.watch_id,
                                        path,
                                        reason,
                                        error,
                                        record->os_error.code,
                                        record->os_error.message);
            }

            return checked_snprintf(dst,
                                    dst_size,
                                    "%s wd=%d path=%s reason=%s error=%s "
                                    "errno=%d",
                                    name,
                                    record->watch.watch_id,
                                    path,
                                    reason,
                                    error,
                                    record->os_error.code);
        }

        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s error=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                error);
    }

    if (reason != NULL &&
        (record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_END ||
         record->type == ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END ||
         record->type == ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED ||
         record->type == ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP) &&
        state != NULL) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s result=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                reason,
                                state);
    }

    if (reason != NULL) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s reason=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                reason);
    }

    if (state != NULL) {
        return checked_snprintf(dst,
                                dst_size,
                                "%s wd=%d path=%s result=%s",
                                name,
                                record->watch.watch_id,
                                path,
                                state);
    }

    return checked_snprintf(dst,
                            dst_size,
                            "%s wd=%d path=%s",
                            name,
                            record->watch.watch_id,
                            path);
}

int alfred_record_format_text(const alfred_record_t *record,
                              char *dst,
                              size_t dst_size)
{
    const char *name;

    if (record == NULL || dst == NULL || dst_size == 0u) {
        return -1;
    }

    name = record_type_name(record->type);
    if (name == NULL) {
        return -1;
    }

    if (record->category == ALFRED_RECORD_CATEGORY_FILESYSTEM) {
        return format_filesystem_text(record, name, dst, dst_size);
    }

    if (record->layer == ALFRED_RECORD_LAYER_DIAGNOSTIC &&
        (record->category == ALFRED_RECORD_CATEGORY_WATCH ||
         record->category == ALFRED_RECORD_CATEGORY_RECOVERY)) {
        return format_diagnostic_text(record, name, dst, dst_size);
    }

    return -1;
}
