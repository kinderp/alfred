/*
 * test_record_semantic_adapter.c - focused test for semantic event adaptation
 *
 * This test verifies the first runtime-facing bridge from alfred_event_t to
 * Event Model v0. Semantic core events may already be FILE_*, DIR_*, moved,
 * renamed, relocated, or OVERFLOW outcomes, so this adapter is allowed to
 * produce semantic record types instead of RAW_* record types.
 *
 * Expected record contract:
 *
 * single-path semantic event:
 * - layer=ALFRED_RECORD_LAYER_SEMANTIC
 * - category=ALFRED_RECORD_CATEGORY_FILESYSTEM
 * - type=ALFRED_RECORD_TYPE_FILE_CREATED
 * - path=<borrowed src_path pointer>
 *
 * move-style semantic event:
 * - type=ALFRED_RECORD_TYPE_FILE_RENAMED
 * - old_path=<borrowed src_path pointer>
 * - new_path=<borrowed dst_path pointer>
 *
 * Meaning:
 * This adapter is different from alfred_record_from_raw(). Raw facts stay
 * RAW_*, while semantic core events are already stable FILE_* or DIR_* output.
 */

#include "alfred_record_adapter.h"

#include <assert.h>
#include <string.h>

static alfred_event_t make_event(uint32_t type,
                                 const char *src_path,
                                 const char *dst_path)
{
    alfred_event_t event;

    memset(&event, 0, sizeof(event));
    event.seq = 9u;
    event.ts_ns = 42u;
    event.type = type;
    event.pid = 123;
    event.src_path = src_path;
    event.dst_path = dst_path;

    return event;
}

static void test_file_created_maps_to_semantic_record(void)
{
    alfred_event_t event;
    alfred_record_t record;

    event = make_event(ALFRED_EV_FILE_CREATED, "/tmp/root/a.txt", NULL);

    assert(alfred_record_from_event(&event, &record) == 0);
    assert(record.schema_version == ALFRED_RECORD_SCHEMA_VERSION);
    assert(record.seq == event.seq);
    assert(record.ts_ns == event.ts_ns);
    assert(record.layer == ALFRED_RECORD_LAYER_SEMANTIC);
    assert(record.category == ALFRED_RECORD_CATEGORY_FILESYSTEM);
    assert(record.type == ALFRED_RECORD_TYPE_FILE_CREATED);
    assert(record.pid == event.pid);
    assert(record.path == event.src_path);
    assert(record.old_path == event.src_path);
    assert(record.new_path == NULL);
}

static void test_file_renamed_preserves_from_to_paths(void)
{
    alfred_event_t event;
    alfred_record_t record;

    event = make_event(ALFRED_EV_FILE_RENAMED,
                       "/tmp/root/old.txt",
                       "/tmp/root/new.txt");

    assert(alfred_record_from_event(&event, &record) == 0);
    assert(record.type == ALFRED_RECORD_TYPE_FILE_RENAMED);
    assert(record.path == event.src_path);
    assert(record.old_path == event.src_path);
    assert(record.new_path == event.dst_path);
}

static void test_invalid_input_fails(void)
{
    alfred_event_t event;
    alfred_record_t record;

    event = make_event(0u, "/tmp/root/a.txt", NULL);

    assert(alfred_record_from_event(NULL, &record) != 0);
    assert(alfred_record_from_event(&event, NULL) != 0);
    assert(alfred_record_from_event(&event, &record) != 0);
}

int main(void)
{
    test_file_created_maps_to_semantic_record();
    test_file_renamed_preserves_from_to_paths();
    test_invalid_input_fails();

    return 0;
}
