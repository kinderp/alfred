/*
 * test_record_raw_adapter.c - focused test for raw event record adaptation
 *
 * This test verifies the first bridge from the current runtime contract to
 * Event Model v0. The runtime still emits alfred_raw_event_t, but structured
 * writers and Backend API v0 will consume alfred_record_t. The adapter must
 * preserve raw facts without promoting them to semantic core outcomes.
 *
 * Expected record contract:
 *
 * create directory raw event:
 * - layer=ALFRED_RECORD_LAYER_NORMALIZED_RAW
 * - category=ALFRED_RECORD_CATEGORY_FILESYSTEM
 * - type=ALFRED_RECORD_TYPE_RAW_CREATE
 * - raw_mask=ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR
 * - path=/tmp/root/dir
 *
 * moved-from raw event:
 * - type=ALFRED_RECORD_TYPE_RAW_MOVED_FROM
 * - cookie=<raw cookie>
 *
 * close-write raw event:
 * - type=ALFRED_RECORD_TYPE_RAW_CLOSE_WRITE
 *
 * attrib raw event:
 * - type=ALFRED_RECORD_TYPE_RAW_ATTRIB
 *
 * overflow raw event:
 * - type=ALFRED_RECORD_TYPE_RAW_OVERFLOW
 *
 * invalid raw event:
 * - adapter returns failure
 * - masks with more than one primary action bit return failure
 * - overflow with a directory qualifier returns failure
 *
 * Meaning:
 * RAW_* record types are deliberately separate from FILE_* and DIR_* semantic
 * types. A raw MOVED_FROM is only one kernel/backend fact; the core must still
 * correlate it with MOVED_TO before deciding moved, renamed, or relocated.
 */

#include "alfred_record_adapter.h"

#include <assert.h>
#include <string.h>

static alfred_raw_event_t make_raw(uint32_t mask, const char *path)
{
    alfred_raw_event_t raw;

    memset(&raw, 0, sizeof(raw));
    raw.ts_ns = 42u;
    raw.source = ALFRED_SRC_INOTIFY;
    raw.mask = mask;
    raw.cookie = 77u;
    raw.pid = 123;
    raw.path = path;

    return raw;
}

static void assert_common_raw_record(const alfred_record_t *record,
                                     const alfred_raw_event_t *raw)
{
    assert(record->schema_version == ALFRED_RECORD_SCHEMA_VERSION);
    assert(record->layer == ALFRED_RECORD_LAYER_NORMALIZED_RAW);
    assert(record->category == ALFRED_RECORD_CATEGORY_FILESYSTEM);
    assert(record->ts_ns == raw->ts_ns);
    assert(record->source == raw->source);
    assert(record->raw_mask == raw->mask);
    assert(record->cookie == raw->cookie);
    assert(record->pid == raw->pid);
    assert(record->path == raw->path);
}

static void test_create_dir_keeps_raw_type_and_dir_bit(void)
{
    alfred_raw_event_t raw;
    alfred_record_t record;

    raw = make_raw(ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR, "/tmp/root/dir");

    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert_common_raw_record(&record, &raw);
    assert(record.type == ALFRED_RECORD_TYPE_RAW_CREATE);
}

static void test_moved_from_does_not_become_semantic_move(void)
{
    alfred_raw_event_t raw;
    alfred_record_t record;

    raw = make_raw(ALFRED_RAW_MOVED_FROM, "/tmp/root/old");

    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert_common_raw_record(&record, &raw);
    assert(record.type == ALFRED_RECORD_TYPE_RAW_MOVED_FROM);
    assert(record.type != ALFRED_RECORD_TYPE_FILE_MOVED);
    assert(record.type != ALFRED_RECORD_TYPE_FILE_RENAMED);
    assert(record.type != ALFRED_RECORD_TYPE_FILE_RELOCATED);
}

static void test_other_raw_masks_map_to_raw_record_types(void)
{
    alfred_raw_event_t raw;
    alfred_record_t record;

    raw = make_raw(ALFRED_RAW_CLOSE_WRITE, "/tmp/root/file");
    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert(record.type == ALFRED_RECORD_TYPE_RAW_CLOSE_WRITE);

    raw = make_raw(ALFRED_RAW_ATTRIB, "/tmp/root/file");
    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert(record.type == ALFRED_RECORD_TYPE_RAW_ATTRIB);

    raw = make_raw(ALFRED_RAW_OVERFLOW, "");
    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert(record.type == ALFRED_RECORD_TYPE_RAW_OVERFLOW);
}

static void test_invalid_input_fails(void)
{
    alfred_raw_event_t raw;
    alfred_record_t record;

    raw = make_raw(0u, "/tmp/root/unknown");

    assert(alfred_record_from_raw(NULL, &record) != 0);
    assert(alfred_record_from_raw(&raw, NULL) != 0);
    assert(alfred_record_from_raw(&raw, &record) != 0);
}

static void test_ambiguous_raw_masks_fail(void)
{
    alfred_raw_event_t raw;
    alfred_record_t record;

    /*
     * The adapter is a contract boundary, not just an app helper. It must reject
     * masks that contain multiple primary actions instead of selecting one by
     * priority, otherwise future producers could emit records whose meaning
     * depends on formatter order.
     */
    raw = make_raw(ALFRED_RAW_MOVED_FROM | ALFRED_RAW_CLOSE_WRITE,
                   "/tmp/root/ambiguous");
    assert(alfred_record_from_raw(&raw, &record) != 0);

    raw = make_raw(ALFRED_RAW_CREATE | ALFRED_RAW_DELETE,
                   "/tmp/root/ambiguous");
    assert(alfred_record_from_raw(&raw, &record) != 0);

    raw = make_raw(ALFRED_RAW_OVERFLOW | ALFRED_RAW_ISDIR, "");
    assert(alfred_record_from_raw(&raw, &record) != 0);
}

int main(void)
{
    test_create_dir_keeps_raw_type_and_dir_bit();
    test_moved_from_does_not_become_semantic_move();
    test_other_raw_masks_map_to_raw_record_types();
    test_invalid_input_fails();
    test_ambiguous_raw_masks_fail();

    return 0;
}
