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
 * - old_path=NULL
 * - new_path=NULL
 *
 * move-style semantic event:
 * - type=ALFRED_RECORD_TYPE_FILE_RENAMED
 * - path=NULL
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

typedef struct {
    uint32_t event_type;
    alfred_record_type_t record_type;
} semantic_type_mapping_t;

static void assert_common_fields(const alfred_event_t *event,
                                 const alfred_record_t *record,
                                 alfred_record_type_t expected_type)
{
    assert(record->schema_version == ALFRED_RECORD_SCHEMA_VERSION);
    assert(record->seq == event->seq);
    assert(record->ts_ns == event->ts_ns);
    assert(record->layer == ALFRED_RECORD_LAYER_SEMANTIC);
    assert(record->category == ALFRED_RECORD_CATEGORY_FILESYSTEM);
    assert(record->type == expected_type);
    assert(record->pid == event->pid);
}

static void test_single_path_events_use_only_path(void)
{
    static const semantic_type_mapping_t mappings[] = {
        {ALFRED_EV_FILE_CREATED, ALFRED_RECORD_TYPE_FILE_CREATED},
        {ALFRED_EV_FILE_READY, ALFRED_RECORD_TYPE_FILE_READY},
        {ALFRED_EV_FILE_MODIFIED, ALFRED_RECORD_TYPE_FILE_MODIFIED},
        {ALFRED_EV_FILE_DELETED, ALFRED_RECORD_TYPE_FILE_DELETED},
        {ALFRED_EV_DIR_CREATED, ALFRED_RECORD_TYPE_DIR_CREATED},
        {ALFRED_EV_DIR_DELETED, ALFRED_RECORD_TYPE_DIR_DELETED},
    };
    size_t i;

    for (i = 0u; i < sizeof(mappings) / sizeof(mappings[0]); i++) {
        alfred_event_t event;
        alfred_record_t record;

        event = make_event(mappings[i].event_type, "/tmp/root/item", NULL);

        assert(alfred_record_from_event(&event, &record) == 0);
        assert_common_fields(&event, &record, mappings[i].record_type);
        assert(record.path == event.src_path);
        assert(record.old_path == NULL);
        assert(record.new_path == NULL);
    }
}

static void test_move_style_events_use_only_path_pair(void)
{
    static const semantic_type_mapping_t mappings[] = {
        {ALFRED_EV_FILE_RENAMED, ALFRED_RECORD_TYPE_FILE_RENAMED},
        {ALFRED_EV_FILE_MOVED, ALFRED_RECORD_TYPE_FILE_MOVED},
        {ALFRED_EV_FILE_RELOCATED, ALFRED_RECORD_TYPE_FILE_RELOCATED},
        {ALFRED_EV_DIR_RENAMED, ALFRED_RECORD_TYPE_DIR_RENAMED},
        {ALFRED_EV_DIR_MOVED, ALFRED_RECORD_TYPE_DIR_MOVED},
        {ALFRED_EV_DIR_RELOCATED, ALFRED_RECORD_TYPE_DIR_RELOCATED},
    };
    size_t i;

    for (i = 0u; i < sizeof(mappings) / sizeof(mappings[0]); i++) {
        alfred_event_t event;
        alfred_record_t record;

        event = make_event(mappings[i].event_type,
                           "/tmp/root/old-item",
                           "/tmp/root/new-item");

        assert(alfred_record_from_event(&event, &record) == 0);
        assert_common_fields(&event, &record, mappings[i].record_type);
        assert(record.path == NULL);
        assert(record.old_path == event.src_path);
        assert(record.new_path == event.dst_path);
    }
}

static void test_overflow_has_no_paths(void)
{
    alfred_event_t event;
    alfred_record_t record;

    event = make_event(ALFRED_EV_OVERFLOW, NULL, NULL);

    assert(alfred_record_from_event(&event, &record) == 0);
    assert_common_fields(&event, &record, ALFRED_RECORD_TYPE_OVERFLOW);
    assert(record.path == NULL);
    assert(record.old_path == NULL);
    assert(record.new_path == NULL);
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
    test_single_path_events_use_only_path();
    test_move_style_events_use_only_path_pair();
    test_overflow_has_no_paths();
    test_invalid_input_fails();

    return 0;
}
