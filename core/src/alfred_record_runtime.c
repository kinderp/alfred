/* ============================================================================
 * alfred_record_runtime.c - single-threaded record runtime drain helper
 * ========================================================================== */

#include "alfred_record_runtime.h"

static void fill_result(alfred_record_runtime_drain_result_t *result,
                        size_t max_records,
                        size_t dispatched,
                        const alfred_record_queue_t *queue,
                        int status)
{
    if (result == NULL) {
        return;
    }

    result->max_records = max_records;
    result->dispatched = dispatched;
    result->remaining = alfred_record_queue_count(queue);
    result->status = status;
}

int alfred_record_runtime_drain_once(
    const alfred_record_dispatcher_t *dispatcher,
    alfred_record_queue_t *queue,
    size_t max_records,
    alfred_record_runtime_drain_result_t *result)
{
    size_t dispatched = 0u;
    int status;

    if (dispatcher == NULL || queue == NULL) {
        fill_result(result, max_records, 0u, queue, -1);
        return -1;
    }

    status = alfred_record_dispatcher_drain_queue(
        dispatcher,
        queue,
        max_records,
        &dispatched);

    fill_result(result, max_records, dispatched, queue, status);
    return status;
}
