/* ============================================================================
 * alfred_record_counter_sink.h - no-op counter sink for Event Model v0 records
 *
 * This sink is a benchmark and contract helper. It consumes alfred_record_t
 * values through the generic sink boundary, increments counters, and performs
 * no formatting, I/O, allocation, string copy, or buffering.
 *
 * The goal is to measure and test record -> sink delivery without confusing the
 * result with text formatting, JSON escaping, file writes, flush policy, socket
 * sends, or future writer backpressure.
 * ========================================================================== */

#ifndef ALFRED_RECORD_COUNTER_SINK_H
#define ALFRED_RECORD_COUNTER_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record_sink.h"

#include <stddef.h>

/*
 * alfred_record_counter_sink_t - no-op sink state and counters
 * @total_records: all records received by the sink
 * @backend_observed_records: records from ALFRED_RECORD_LAYER_BACKEND_OBSERVED
 * @normalized_raw_records: records from ALFRED_RECORD_LAYER_NORMALIZED_RAW
 * @semantic_records: records from ALFRED_RECORD_LAYER_SEMANTIC
 * @diagnostic_records: records from ALFRED_RECORD_LAYER_DIAGNOSTIC
 * @filesystem_records: records in ALFRED_RECORD_CATEGORY_FILESYSTEM
 * @watch_records: records in ALFRED_RECORD_CATEGORY_WATCH
 * @recovery_records: records in ALFRED_RECORD_CATEGORY_RECOVERY
 * @backend_records: records in ALFRED_RECORD_CATEGORY_BACKEND
 * @policy_records: records in ALFRED_RECORD_CATEGORY_POLICY
 *
 * The sink intentionally stores only numeric counters. It never keeps borrowed
 * pointers from the record, so it cannot extend string lifetime or create
 * ownership obligations for the caller.
 */
typedef struct {
    size_t total_records;

    size_t backend_observed_records;
    size_t normalized_raw_records;
    size_t semantic_records;
    size_t diagnostic_records;

    size_t filesystem_records;
    size_t watch_records;
    size_t recovery_records;
    size_t backend_records;
    size_t policy_records;
} alfred_record_counter_sink_t;

/*
 * alfred_record_counter_sink_init - expose a counter sink as a generic sink
 * @counter: caller-owned counter sink state
 * @sink: destination generic sink handle
 *
 * The function resets all counters in @counter and stores @counter as the
 * generic sink userdata. The caller must keep @counter alive as long as @sink
 * can be used.
 *
 * Return: 0 on success, -1 on invalid input.
 */
int alfred_record_counter_sink_init(alfred_record_counter_sink_t *counter,
                                    alfred_record_sink_t *sink);

/*
 * alfred_record_counter_sink_emit - count one record without retaining it
 * @userdata: alfred_record_counter_sink_t passed through alfred_record_sink_t
 * @record: borrowed Event Model v0 record
 *
 * Return: 0 on success, -1 on invalid input.
 */
int alfred_record_counter_sink_emit(void *userdata,
                                    const alfred_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_COUNTER_SINK_H */
