/*
 * test_record_counter_sink.c - no-op counter sink test
 *
 * This test covers the lightweight benchmark sink used to measure the generic
 * record -> sink boundary without writer cost.
 *
 * Expected counter contract:
 *
 * emitted records:
 * - semantic filesystem FILE_CREATED
 * - normalized raw filesystem RAW_CREATE
 * - diagnostic recovery WATCH_RESYNC_FAILED
 *
 * expected counters:
 * - total_records = 3
 * - semantic_records = 1
 * - normalized_raw_records = 1
 * - diagnostic_records = 1
 * - filesystem_records = 2
 * - recovery_records = 1
 *
 * Meaning:
 * The counter sink must not format text, generate JSONL, write files, allocate
 * memory, or retain borrowed strings from the record. It increments numeric
 * counters only, giving future benchmarks a clean no-I/O baseline.
 */

#include "alfred_record_counter_sink.h"
#include "alfred_record_sink.h"

#include <assert.h>
#include <string.h>

static alfred_record_t make_record(alfred_record_layer_t layer,
                                   alfred_record_category_t category,
                                   alfred_record_type_t type,
                                   const char *path)
{
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = layer;
    record.category = category;
    record.type = type;
    record.path = path;
    return record;
}

static void test_counter_sink_counts_records(void)
{
    alfred_record_counter_sink_t counter;
    alfred_record_sink_t sink;
    alfred_record_t record;

    memset(&counter, 0xff, sizeof(counter));
    assert(alfred_record_counter_sink_init(&counter, &sink) == 0);
    assert(counter.total_records == 0u);

    record = make_record(ALFRED_RECORD_LAYER_SEMANTIC,
                         ALFRED_RECORD_CATEGORY_FILESYSTEM,
                         ALFRED_RECORD_TYPE_FILE_CREATED,
                         "/tmp/root/a.txt");
    assert(alfred_record_sink_emit(&sink, &record) == 0);

    record = make_record(ALFRED_RECORD_LAYER_NORMALIZED_RAW,
                         ALFRED_RECORD_CATEGORY_FILESYSTEM,
                         ALFRED_RECORD_TYPE_RAW_CREATE,
                         "/tmp/root/b.txt");
    assert(alfred_record_sink_emit(&sink, &record) == 0);

    record = make_record(ALFRED_RECORD_LAYER_DIAGNOSTIC,
                         ALFRED_RECORD_CATEGORY_RECOVERY,
                         ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,
                         "/tmp/root");
    assert(alfred_record_sink_emit(&sink, &record) == 0);

    assert(counter.total_records == 3u);
    assert(counter.semantic_records == 1u);
    assert(counter.normalized_raw_records == 1u);
    assert(counter.diagnostic_records == 1u);
    assert(counter.backend_observed_records == 0u);
    assert(counter.filesystem_records == 2u);
    assert(counter.recovery_records == 1u);
    assert(counter.watch_records == 0u);
    assert(counter.backend_records == 0u);
    assert(counter.policy_records == 0u);
}

static void test_counter_sink_counts_unknown_only_in_total(void)
{
    alfred_record_counter_sink_t counter;
    alfred_record_sink_t sink;
    alfred_record_t record;

    assert(alfred_record_counter_sink_init(&counter, &sink) == 0);

    record = make_record(ALFRED_RECORD_LAYER_UNKNOWN,
                         ALFRED_RECORD_CATEGORY_UNKNOWN,
                         ALFRED_RECORD_TYPE_UNKNOWN,
                         NULL);
    assert(alfred_record_sink_emit(&sink, &record) == 0);

    assert(counter.total_records == 1u);
    assert(counter.backend_observed_records == 0u);
    assert(counter.normalized_raw_records == 0u);
    assert(counter.semantic_records == 0u);
    assert(counter.diagnostic_records == 0u);
    assert(counter.filesystem_records == 0u);
    assert(counter.watch_records == 0u);
    assert(counter.recovery_records == 0u);
    assert(counter.backend_records == 0u);
    assert(counter.policy_records == 0u);
}

static void test_counter_sink_rejects_invalid_input(void)
{
    alfred_record_counter_sink_t counter;
    alfred_record_sink_t sink;
    alfred_record_t record;

    record = make_record(ALFRED_RECORD_LAYER_SEMANTIC,
                         ALFRED_RECORD_CATEGORY_FILESYSTEM,
                         ALFRED_RECORD_TYPE_FILE_CREATED,
                         "/tmp/root/a.txt");

    assert(alfred_record_counter_sink_init(NULL, &sink) == -1);
    assert(alfred_record_counter_sink_init(&counter, NULL) == -1);

    assert(alfred_record_counter_sink_init(&counter, &sink) == 0);
    assert(alfred_record_counter_sink_emit(NULL, &record) == -1);
    assert(alfred_record_counter_sink_emit(&counter, NULL) == -1);
    assert(alfred_record_sink_emit(NULL, &record) == -1);
}

int main(void)
{
    test_counter_sink_counts_records();
    test_counter_sink_counts_unknown_only_in_total();
    test_counter_sink_rejects_invalid_input();

    return 0;
}
