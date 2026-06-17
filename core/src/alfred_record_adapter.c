/* ============================================================================
 * alfred_record_adapter.c - current type adapters for Event Model v0 records
 * ========================================================================== */

#include "alfred_record_adapter.h"

#include <string.h>

/*
 * raw_mask_to_record_type - choose the normalized raw record type
 * @mask: ALFRED_RAW_* bitmask from alfred_raw_event_t
 *
 * The order is intentional. MOVED_FROM, MOVED_TO, and OVERFLOW identify
 * distinct raw facts that should not be collapsed into create/delete/modify
 * semantics. Directory information stays in raw_mask through ALFRED_RAW_ISDIR.
 *
 * Return: ALFRED_RECORD_TYPE_RAW_* on supported input, UNKNOWN otherwise.
 */
static alfred_record_type_t raw_mask_to_record_type(uint32_t mask)
{
    if ((mask & ALFRED_RAW_MOVED_FROM) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_MOVED_FROM;
    }
    if ((mask & ALFRED_RAW_MOVED_TO) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_MOVED_TO;
    }
    if ((mask & ALFRED_RAW_OVERFLOW) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_OVERFLOW;
    }
    if ((mask & ALFRED_RAW_CLOSE_WRITE) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_CLOSE_WRITE;
    }
    if ((mask & ALFRED_RAW_ATTRIB) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_ATTRIB;
    }
    if ((mask & ALFRED_RAW_MODIFY) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_MODIFY;
    }
    if ((mask & ALFRED_RAW_CREATE) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_CREATE;
    }
    if ((mask & ALFRED_RAW_DELETE) != 0u) {
        return ALFRED_RECORD_TYPE_RAW_DELETE;
    }

    return ALFRED_RECORD_TYPE_UNKNOWN;
}

int alfred_record_from_raw(const alfred_raw_event_t *raw,
                           alfred_record_t *out)
{
    alfred_record_type_t type;

    if (raw == NULL || out == NULL) {
        return -1;
    }

    type = raw_mask_to_record_type(raw->mask);
    if (type == ALFRED_RECORD_TYPE_UNKNOWN) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    out->schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    out->ts_ns = raw->ts_ns;
    out->layer = ALFRED_RECORD_LAYER_NORMALIZED_RAW;
    out->category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    out->type = type;
    out->source = raw->source;
    out->raw_mask = raw->mask;
    out->cookie = raw->cookie;
    out->pid = raw->pid;
    out->path = raw->path;

    return 0;
}
