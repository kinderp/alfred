/* ============================================================================
 * alfred_record_counter_sink.c - no-op counter sink for records
 * ========================================================================== */

#include "alfred_record_counter_sink.h"

#include <string.h>

static void count_layer(alfred_record_counter_sink_t *counter,
                        alfred_record_layer_t layer)
{
    switch (layer) {
    case ALFRED_RECORD_LAYER_BACKEND_OBSERVED:
        counter->backend_observed_records++;
        break;
    case ALFRED_RECORD_LAYER_NORMALIZED_RAW:
        counter->normalized_raw_records++;
        break;
    case ALFRED_RECORD_LAYER_SEMANTIC:
        counter->semantic_records++;
        break;
    case ALFRED_RECORD_LAYER_DIAGNOSTIC:
        counter->diagnostic_records++;
        break;
    default:
        break;
    }
}

static void count_category(alfred_record_counter_sink_t *counter,
                           alfred_record_category_t category)
{
    switch (category) {
    case ALFRED_RECORD_CATEGORY_FILESYSTEM:
        counter->filesystem_records++;
        break;
    case ALFRED_RECORD_CATEGORY_WATCH:
        counter->watch_records++;
        break;
    case ALFRED_RECORD_CATEGORY_RECOVERY:
        counter->recovery_records++;
        break;
    case ALFRED_RECORD_CATEGORY_BACKEND:
        counter->backend_records++;
        break;
    case ALFRED_RECORD_CATEGORY_POLICY:
        counter->policy_records++;
        break;
    default:
        break;
    }
}

int alfred_record_counter_sink_init(alfred_record_counter_sink_t *counter,
                                    alfred_record_sink_t *sink)
{
    if (counter == NULL || sink == NULL) {
        return -1;
    }

    memset(counter, 0, sizeof(*counter));

    sink->emit = alfred_record_counter_sink_emit;
    sink->userdata = counter;
    return 0;
}

int alfred_record_counter_sink_emit(void *userdata,
                                    const alfred_record_t *record)
{
    alfred_record_counter_sink_t *counter = userdata;

    if (counter == NULL || record == NULL) {
        return -1;
    }

    counter->total_records++;
    count_layer(counter, record->layer);
    count_category(counter, record->category);
    return 0;
}
