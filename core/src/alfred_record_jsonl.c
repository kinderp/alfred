/* ============================================================================
 * alfred_record_jsonl.c - JSON Lines formatter for Event Model v0 records
 *
 * This file intentionally avoids external JSON dependencies. The v0 formatter
 * has a narrow contract: write one compact object into caller-owned memory,
 * escape JSON control characters, and fail on truncation. It does not yet solve
 * lossless encoding for arbitrary non-UTF-8 Linux path bytes.
 * ========================================================================== */

#include "alfred_record_jsonl.h"

#include <stdarg.h>
#include <stdio.h>

typedef struct {
    char *dst;
    size_t size;
    size_t used;
} jsonl_buffer_t;

static const char *layer_name(alfred_record_layer_t layer)
{
    switch (layer) {
    case ALFRED_RECORD_LAYER_BACKEND_OBSERVED: return "backend_observed";
    case ALFRED_RECORD_LAYER_NORMALIZED_RAW: return "normalized_raw";
    case ALFRED_RECORD_LAYER_SEMANTIC: return "semantic";
    case ALFRED_RECORD_LAYER_DIAGNOSTIC: return "diagnostic";
    default:
        return NULL;
    }
}

static const char *category_name(alfred_record_category_t category)
{
    switch (category) {
    case ALFRED_RECORD_CATEGORY_FILESYSTEM: return "filesystem";
    case ALFRED_RECORD_CATEGORY_WATCH: return "watch";
    case ALFRED_RECORD_CATEGORY_RECOVERY: return "recovery";
    case ALFRED_RECORD_CATEGORY_BACKEND: return "backend";
    case ALFRED_RECORD_CATEGORY_LIFECYCLE: return "lifecycle";
    case ALFRED_RECORD_CATEGORY_POLICY: return "policy";
    default:
        return NULL;
    }
}

static const char *type_name(alfred_record_type_t type)
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
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED: return "WATCH_RESYNC_FAILED";
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_END: return "WATCH_RESYNC_END";
    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED: return "WATCH_LOST_QUEUED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_SKIPPED:
        return "WATCH_LOST_QUEUE_SKIPPED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED:
        return "WATCH_LOST_QUEUE_FAILED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN:
        return "WATCH_LOST_SCAN_BEGIN";
    case ALFRED_RECORD_TYPE_WATCH_LOST_FOUND: return "WATCH_LOST_FOUND";
    case ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED:
        return "WATCH_LOST_PREFIX_UPDATED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE:
        return "WATCH_LOST_COVERAGE_DONE";
    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING:
        return "WATCH_LOST_COVERAGE_MISSING";
    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS:
        return "WATCH_LOST_COVERAGE_CLASS";
    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED:
        return "WATCH_LOST_REINSTALLED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED:
        return "WATCH_LOST_REINSTALL_FAILED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK:
        return "WATCH_LOST_ROLLBACK";
    case ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND:
        return "WATCH_LOST_NOT_FOUND";
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED:
        return "WATCH_LOST_RECOVERY_FAILED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END:
        return "WATCH_LOST_RECOVERY_END";
    case ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED:
        return "WATCH_LOST_RETRY_SCHEDULED";
    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP:
        return "WATCH_LOST_RECOVERY_GAVE_UP";
    case ALFRED_RECORD_TYPE_SESSION_CONTEXT: return "SESSION_CONTEXT";
    default:
        return NULL;
    }
}

static int append_raw(jsonl_buffer_t *buffer, const char *text)
{
    size_t i;

    if (buffer == NULL || text == NULL) {
        return -1;
    }

    for (i = 0u; text[i] != '\0'; ++i) {
        if (buffer->used + 1u >= buffer->size) {
            return -1;
        }
        buffer->dst[buffer->used++] = text[i];
        buffer->dst[buffer->used] = '\0';
    }

    return 0;
}

static int append_fmt(jsonl_buffer_t *buffer, const char *fmt, ...)
{
    int written;
    va_list ap;

    if (buffer == NULL ||
        buffer->dst == NULL ||
        buffer->used >= buffer->size ||
        fmt == NULL) {
        return -1;
    }

    va_start(ap, fmt);
    written = vsnprintf(buffer->dst + buffer->used,
                        buffer->size - buffer->used,
                        fmt,
                        ap);
    va_end(ap);

    if (written < 0 || (size_t)written >= buffer->size - buffer->used) {
        return -1;
    }

    buffer->used += (size_t)written;
    return 0;
}

static int append_json_string(jsonl_buffer_t *buffer, const char *value)
{
    const unsigned char *p = (const unsigned char *)value;

    if (append_raw(buffer, "\"") != 0) {
        return -1;
    }

    while (*p != '\0') {
        switch (*p) {
        case '"':
            if (append_raw(buffer, "\\\"") != 0) {
                return -1;
            }
            break;
        case '\\':
            if (append_raw(buffer, "\\\\") != 0) {
                return -1;
            }
            break;
        case '\b':
            if (append_raw(buffer, "\\b") != 0) {
                return -1;
            }
            break;
        case '\f':
            if (append_raw(buffer, "\\f") != 0) {
                return -1;
            }
            break;
        case '\n':
            if (append_raw(buffer, "\\n") != 0) {
                return -1;
            }
            break;
        case '\r':
            if (append_raw(buffer, "\\r") != 0) {
                return -1;
            }
            break;
        case '\t':
            if (append_raw(buffer, "\\t") != 0) {
                return -1;
            }
            break;
        default:
            if (*p < 0x20u) {
                if (append_fmt(buffer, "\\u%04x", (unsigned)*p) != 0) {
                    return -1;
                }
            } else {
                char one[2];

                one[0] = (char)*p;
                one[1] = '\0';
                if (append_raw(buffer, one) != 0) {
                    return -1;
                }
            }
            break;
        }
        p++;
    }

    return append_raw(buffer, "\"");
}

static int append_comma_if_needed(jsonl_buffer_t *buffer, int *needs_comma)
{
    if (needs_comma == NULL) {
        return -1;
    }

    if (*needs_comma != 0 && append_raw(buffer, ",") != 0) {
        return -1;
    }

    *needs_comma = 1;
    return 0;
}

static int append_string_field(jsonl_buffer_t *buffer,
                               int *needs_comma,
                               const char *key,
                               const char *value)
{
    if (value == NULL) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_json_string(buffer, key) != 0 ||
        append_raw(buffer, ":") != 0 ||
        append_json_string(buffer, value) != 0) {
        return -1;
    }

    return 0;
}

static int append_u64_field(jsonl_buffer_t *buffer,
                            int *needs_comma,
                            const char *key,
                            uint64_t value)
{
    if (value == 0u) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_json_string(buffer, key) != 0 ||
        append_fmt(buffer, ":%llu", (unsigned long long)value) != 0) {
        return -1;
    }

    return 0;
}

static int append_u32_field(jsonl_buffer_t *buffer,
                            int *needs_comma,
                            const char *key,
                            uint32_t value)
{
    if (value == 0u) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_json_string(buffer, key) != 0 ||
        append_fmt(buffer, ":%u", value) != 0) {
        return -1;
    }

    return 0;
}

static int append_int_field(jsonl_buffer_t *buffer,
                            int *needs_comma,
                            const char *key,
                            int value)
{
    if (value == 0) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_json_string(buffer, key) != 0 ||
        append_fmt(buffer, ":%d", value) != 0) {
        return -1;
    }

    return 0;
}

static int append_size_field(jsonl_buffer_t *buffer,
                             int *needs_comma,
                             const char *key,
                             size_t value)
{
    if (value == 0u) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_json_string(buffer, key) != 0 ||
        append_fmt(buffer, ":%zu", value) != 0) {
        return -1;
    }

    return 0;
}

static int append_required_u32_field(jsonl_buffer_t *buffer,
                                     int *needs_comma,
                                     const char *key,
                                     uint32_t value)
{
    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_json_string(buffer, key) != 0 ||
        append_fmt(buffer, ":%u", value) != 0) {
        return -1;
    }

    return 0;
}

static int append_required_string_field(jsonl_buffer_t *buffer,
                                        int *needs_comma,
                                        const char *key,
                                        const char *value)
{
    if (value == NULL ||
        append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_json_string(buffer, key) != 0 ||
        append_raw(buffer, ":") != 0 ||
        append_json_string(buffer, value) != 0) {
        return -1;
    }

    return 0;
}

static int append_identity(jsonl_buffer_t *buffer,
                           int *needs_comma,
                           const alfred_record_identity_t *identity)
{
    int inner_comma = 0;

    /*
     * Device and inode are meaningful as a pair. A single non-zero side is only
     * partial evidence, not a stable filesystem identity, so JSONL v0 omits the
     * whole object unless both values are present.
     */
    if (identity == NULL ||
        identity->device_id == 0 ||
        identity->inode_id == 0) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_raw(buffer, "\"identity\":{") != 0 ||
        append_comma_if_needed(buffer, &inner_comma) != 0 ||
        append_json_string(buffer, "device_id") != 0 ||
        append_fmt(buffer, ":%llu", (unsigned long long)identity->device_id) != 0 ||
        append_comma_if_needed(buffer, &inner_comma) != 0 ||
        append_json_string(buffer, "inode_id") != 0 ||
        append_fmt(buffer, ":%llu", (unsigned long long)identity->inode_id) != 0 ||
        append_raw(buffer, "}") != 0) {
        return -1;
    }

    return 0;
}

static int append_os_error(jsonl_buffer_t *buffer,
                           int *needs_comma,
                           const alfred_record_os_error_t *os_error)
{
    int inner_comma = 0;

    if (os_error == NULL ||
        (os_error->code == 0 &&
         os_error->name == NULL &&
         os_error->message == NULL)) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_raw(buffer, "\"os_error\":{") != 0 ||
        append_int_field(buffer, &inner_comma, "code", os_error->code) != 0 ||
        append_string_field(buffer, &inner_comma, "name", os_error->name) != 0 ||
        append_string_field(buffer,
                            &inner_comma,
                            "message",
                            os_error->message) != 0 ||
        append_raw(buffer, "}") != 0) {
        return -1;
    }

    return 0;
}

static int append_watch(jsonl_buffer_t *buffer,
                        int *needs_comma,
                        const alfred_record_watch_t *watch)
{
    int inner_comma = 0;

    if (watch == NULL ||
        (watch->watch_id == 0 &&
         watch->state == NULL &&
         watch->reason == NULL &&
         watch->error == NULL &&
         watch->event_mask == NULL &&
         watch->event_name == NULL &&
         watch->retry_after_ns == 0u &&
         watch->retry_count == 0u)) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_raw(buffer, "\"watch\":{") != 0 ||
        append_int_field(buffer, &inner_comma, "watch_id", watch->watch_id) != 0 ||
        append_string_field(buffer, &inner_comma, "state", watch->state) != 0 ||
        append_string_field(buffer, &inner_comma, "reason", watch->reason) != 0 ||
        append_string_field(buffer, &inner_comma, "error", watch->error) != 0 ||
        append_string_field(buffer,
                            &inner_comma,
                            "event_mask",
                            watch->event_mask) != 0 ||
        append_string_field(buffer,
                            &inner_comma,
                            "event_name",
                            watch->event_name) != 0 ||
        append_u64_field(buffer,
                         &inner_comma,
                         "retry_after_ns",
                         watch->retry_after_ns) != 0 ||
        append_u32_field(buffer,
                         &inner_comma,
                         "retry_count",
                         watch->retry_count) != 0 ||
        append_raw(buffer, "}") != 0) {
        return -1;
    }

    return 0;
}

static int append_recovery(jsonl_buffer_t *buffer,
                           int *needs_comma,
                           const alfred_record_recovery_t *recovery)
{
    int inner_comma = 0;

    if (recovery == NULL ||
        (recovery->directories_seen == 0u &&
         recovery->directories_watched == 0u &&
         recovery->directories_missing == 0u &&
         recovery->detail_path == NULL &&
         recovery->related_watch_id == 0 &&
         recovery->result_code == 0 &&
         recovery->pending_count == 0u &&
         recovery->children_count == 0u &&
         recovery->watches_count == 0u &&
         recovery->delay_ms == 0u)) {
        return 0;
    }

    if (append_comma_if_needed(buffer, needs_comma) != 0 ||
        append_raw(buffer, "\"recovery\":{") != 0 ||
        append_size_field(buffer,
                          &inner_comma,
                          "directories_seen",
                          recovery->directories_seen) != 0 ||
        append_size_field(buffer,
                          &inner_comma,
                          "directories_watched",
                          recovery->directories_watched) != 0 ||
        append_size_field(buffer,
                          &inner_comma,
                          "directories_missing",
                          recovery->directories_missing) != 0 ||
        append_string_field(buffer,
                            &inner_comma,
                            "detail_path",
                            recovery->detail_path) != 0 ||
        append_int_field(buffer,
                         &inner_comma,
                         "related_watch_id",
                         recovery->related_watch_id) != 0 ||
        append_int_field(buffer,
                         &inner_comma,
                         "result_code",
                         recovery->result_code) != 0 ||
        append_size_field(buffer,
                          &inner_comma,
                          "pending_count",
                          recovery->pending_count) != 0 ||
        append_size_field(buffer,
                          &inner_comma,
                          "children_count",
                          recovery->children_count) != 0 ||
        append_size_field(buffer,
                          &inner_comma,
                          "watches_count",
                          recovery->watches_count) != 0 ||
        append_u64_field(buffer,
                         &inner_comma,
                         "delay_ms",
                         recovery->delay_ms) != 0 ||
        append_raw(buffer, "}") != 0) {
        return -1;
    }

    return 0;
}

int alfred_record_format_jsonl(const alfred_record_t *record,
                               char *dst,
                               size_t dst_size)
{
    jsonl_buffer_t buffer;
    int needs_comma = 0;

    if (record == NULL || dst == NULL || dst_size == 0u) {
        return -1;
    }

    if (layer_name(record->layer) == NULL ||
        category_name(record->category) == NULL ||
        type_name(record->type) == NULL) {
        return -1;
    }

    buffer.dst = dst;
    buffer.size = dst_size;
    buffer.used = 0u;
    dst[0] = '\0';

    if (append_raw(&buffer, "{") != 0 ||
        append_required_u32_field(&buffer,
                                  &needs_comma,
                                  "schema_version",
                                  record->schema_version) != 0 ||
        append_u64_field(&buffer, &needs_comma, "seq", record->seq) != 0 ||
        append_u64_field(&buffer, &needs_comma, "ts_ns", record->ts_ns) != 0 ||
        append_required_string_field(&buffer,
                                     &needs_comma,
                                     "layer",
                                     layer_name(record->layer)) != 0 ||
        append_required_string_field(&buffer,
                                     &needs_comma,
                                     "category",
                                     category_name(record->category)) != 0 ||
        append_required_string_field(&buffer,
                                     &needs_comma,
                                     "type",
                                     type_name(record->type)) != 0 ||
        append_u32_field(&buffer, &needs_comma, "source", record->source) != 0 ||
        append_u32_field(&buffer,
                         &needs_comma,
                         "raw_mask",
                         record->raw_mask) != 0 ||
        append_u32_field(&buffer, &needs_comma, "cookie", record->cookie) != 0 ||
        append_int_field(&buffer, &needs_comma, "pid", (int)record->pid) != 0 ||
        append_string_field(&buffer, &needs_comma, "backend", record->backend) != 0 ||
        append_string_field(&buffer, &needs_comma, "path", record->path) != 0 ||
        append_string_field(&buffer,
                            &needs_comma,
                            "old_path",
                            record->old_path) != 0 ||
        append_string_field(&buffer,
                            &needs_comma,
                            "new_path",
                            record->new_path) != 0 ||
        append_identity(&buffer, &needs_comma, &record->identity) != 0 ||
        append_os_error(&buffer, &needs_comma, &record->os_error) != 0 ||
        append_watch(&buffer, &needs_comma, &record->watch) != 0 ||
        append_recovery(&buffer, &needs_comma, &record->recovery) != 0 ||
        append_raw(&buffer, "}") != 0) {
        return -1;
    }

    return (int)buffer.used;
}
