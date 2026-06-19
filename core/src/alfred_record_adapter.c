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

/*
 * event_type_to_record_type - choose the semantic record type
 * @type: ALFRED_EV_* value from alfred_event_t
 *
 * The mapping is one-to-one because alfred_event_t is already the semantic core
 * output. Unlike raw conversion, this adapter is allowed to produce FILE_*,
 * DIR_*, and OVERFLOW record types.
 *
 * Return: semantic record type on supported input, UNKNOWN otherwise.
 */
static alfred_record_type_t event_type_to_record_type(uint32_t type)
{
    switch (type) {
    case ALFRED_EV_FILE_CREATED:
        return ALFRED_RECORD_TYPE_FILE_CREATED;
    case ALFRED_EV_FILE_READY:
        return ALFRED_RECORD_TYPE_FILE_READY;
    case ALFRED_EV_FILE_MODIFIED:
        return ALFRED_RECORD_TYPE_FILE_MODIFIED;
    case ALFRED_EV_FILE_DELETED:
        return ALFRED_RECORD_TYPE_FILE_DELETED;
    case ALFRED_EV_DIR_CREATED:
        return ALFRED_RECORD_TYPE_DIR_CREATED;
    case ALFRED_EV_DIR_DELETED:
        return ALFRED_RECORD_TYPE_DIR_DELETED;
    case ALFRED_EV_FILE_RENAMED:
        return ALFRED_RECORD_TYPE_FILE_RENAMED;
    case ALFRED_EV_FILE_MOVED:
        return ALFRED_RECORD_TYPE_FILE_MOVED;
    case ALFRED_EV_FILE_RELOCATED:
        return ALFRED_RECORD_TYPE_FILE_RELOCATED;
    case ALFRED_EV_DIR_RENAMED:
        return ALFRED_RECORD_TYPE_DIR_RENAMED;
    case ALFRED_EV_DIR_MOVED:
        return ALFRED_RECORD_TYPE_DIR_MOVED;
    case ALFRED_EV_DIR_RELOCATED:
        return ALFRED_RECORD_TYPE_DIR_RELOCATED;
    case ALFRED_EV_OVERFLOW:
        return ALFRED_RECORD_TYPE_OVERFLOW;
    default:
        return ALFRED_RECORD_TYPE_UNKNOWN;
    }
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

int alfred_record_from_event(const alfred_event_t *event,
                             alfred_record_t *out)
{
    alfred_record_type_t type;

    if (event == NULL || out == NULL) {
        return -1;
    }

    type = event_type_to_record_type(event->type);
    if (type == ALFRED_RECORD_TYPE_UNKNOWN) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    out->schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    out->seq = event->seq;
    out->ts_ns = event->ts_ns;
    out->layer = ALFRED_RECORD_LAYER_SEMANTIC;
    out->category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    out->type = type;
    out->pid = event->pid;
    out->path = event->src_path;
    out->old_path = event->src_path;
    out->new_path = event->dst_path;

    return 0;
}
