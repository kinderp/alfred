/*
 * test_overflow_core_jsonl.c - synthetic JSONL golden for core OVERFLOW
 *
 * Expected pipeline:
 * - alfred_raw_event_t(mask=ALFRED_RAW_OVERFLOW)
 * - alfred_process()
 * - callback receives ALFRED_EV_OVERFLOW
 * - alfred_record_from_event()
 * - alfred_record_format_jsonl()
 *
 * Expected JSONL contract:
 * - layer=semantic
 * - category=filesystem
 * - type=OVERFLOW
 * - path is absent because stream overflow is not tied to one filesystem path
 *
 * Meaning:
 * A real IN_Q_OVERFLOW is timing-sensitive and depends on kernel queue
 * pressure, so CI should not try to trigger it. The raw-overflow JSONL test
 * locks down the backend/raw bridge. This test locks down the next layer: once
 * the core sees ALFRED_RAW_OVERFLOW, it emits the semantic stream-integrity
 * event OVERFLOW and the semantic adapter can serialize it as JSONL.
 */

#include "alfred_correlator.h"
#include "alfred_record_adapter.h"
#include "alfred_record_jsonl.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned int seen;
} overflow_test_context_t;

static void capture_overflow_jsonl(const alfred_event_t *event, void *userdata)
{
    overflow_test_context_t *context = userdata;
    alfred_record_t record;
    char jsonl[512];

    assert(context != NULL);
    assert(event != NULL);
    assert(event->type == ALFRED_EV_OVERFLOW);
    assert(event->src_path == NULL);
    assert(event->dst_path == NULL);

    assert(alfred_record_from_event(event, &record) == 0);
    assert(record.layer == ALFRED_RECORD_LAYER_SEMANTIC);
    assert(record.category == ALFRED_RECORD_CATEGORY_FILESYSTEM);
    assert(record.type == ALFRED_RECORD_TYPE_OVERFLOW);
    assert(record.path == NULL);
    assert(record.old_path == NULL);
    assert(record.new_path == NULL);

    assert(alfred_record_format_jsonl(&record, jsonl, sizeof(jsonl)) > 0);
    puts(jsonl);

    context->seen++;
}

int main(void)
{
    overflow_test_context_t context;
    alfred_config_t config;
    alfred_engine_t *engine;
    alfred_raw_event_t raw;

    memset(&context, 0, sizeof(context));
    alfred_config_default(&config);

    engine = alfred_create(&config, capture_overflow_jsonl, &context);
    assert(engine != NULL);

    memset(&raw, 0, sizeof(raw));
    raw.source = ALFRED_SRC_INOTIFY;
    raw.ts_ns = 42u;
    raw.pid = 123;
    raw.mask = ALFRED_RAW_OVERFLOW;
    raw.path = "";

    assert(alfred_process(engine, &raw) == 0);
    assert(context.seen == 1u);

    alfred_destroy(engine);

    return 0;
}
