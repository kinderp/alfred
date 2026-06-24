/* ============================================================================
 * alfred_record_runtime.h - single-threaded record runtime drain helper
 *
 * This header introduces a small runtime-level boundary above the low-level
 * queue and dispatcher helpers. It does not start threads and it does not own
 * writer resources. It names the batch-drain operation that a future writer
 * worker will perform repeatedly.
 * ========================================================================== */

#ifndef ALFRED_RECORD_RUNTIME_H
#define ALFRED_RECORD_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record_dispatcher.h"
#include "alfred_record_queue.h"

#include <stddef.h>

/*
 * alfred_record_runtime_drain_result_t - summary of one drain attempt
 * @max_records: caller-provided batch limit for this drain attempt
 * @dispatched: records successfully delivered to every registered sink
 * @remaining: records still queued after the drain attempt returns
 * @status: 0 on success, -1 on invalid input or first sink/dispatch failure
 *
 * This structure is intentionally small. It is not a full metrics object and it
 * is not a backpressure policy. It gives tests and future worker code a stable
 * summary of one single-threaded drain step before Alfred introduces threads,
 * retry/requeue policy, per-sink queues, or dead-letter handling.
 */
typedef struct {
    size_t max_records;
    size_t dispatched;
    size_t remaining;
    int status;
} alfred_record_runtime_drain_result_t;

/*
 * alfred_record_runtime_drain_once - process one bounded queue batch
 * @dispatcher: initialized dispatcher that routes records to registered sinks
 * @queue: owned-record queue to consume
 * @max_records: maximum records to process in this call
 * @result: optional summary filled on both success and failure
 *
 * The helper is deliberately single-threaded. It delegates pop/dispatch/destroy
 * ownership mechanics to alfred_record_dispatcher_drain_queue(), then records a
 * runtime-level summary suitable for tests and future worker loops.
 *
 * Return: 0 on success, -1 on invalid input or first dispatch failure.
 */
int alfred_record_runtime_drain_once(
    const alfred_record_dispatcher_t *dispatcher,
    alfred_record_queue_t *queue,
    size_t max_records,
    alfred_record_runtime_drain_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_RUNTIME_H */
