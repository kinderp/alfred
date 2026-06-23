/* ============================================================================
 * alfred_record_queue.h - bounded owned-record queue for Event Model v0
 *
 * The queue is the first explicit boundary between synchronous producers and
 * future asynchronous dispatchers or writers. Producers pass borrowed records to
 * push(); the queue stores owned records so later consumers never depend on
 * backend stack frames, temporary inotify buffers, or formatter-local strings.
 * ========================================================================== */

#ifndef ALFRED_RECORD_QUEUE_H
#define ALFRED_RECORD_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"

#include <stddef.h>

/*
 * alfred_record_queue_t - fixed-capacity owned record queue
 * @items: circular buffer of owned records
 * @capacity: maximum number of records that can be stored
 * @head: index of the next record returned by pop()
 * @tail: index where the next pushed record will be stored
 * @count: number of currently queued records
 *
 * This v0 queue is deliberately single-threaded. It documents and tests the
 * lifetime contract before Alfred adds dispatcher threads, backpressure policy,
 * or per-sink queues. A future threaded queue can keep the same rule: borrowed
 * record in, owned queued record out.
 */
typedef struct {
    alfred_record_t *items;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} alfred_record_queue_t;

/*
 * alfred_record_queue_init - allocate a bounded owned-record queue
 * @queue: queue object to initialize
 * @capacity: maximum number of records accepted before push() reports full
 *
 * @queue must be zeroed or uninitialized. Reinitializing an active queue would
 * lose the owned record buffer; callers that need a different capacity must
 * first call alfred_record_queue_destroy(), then call init again.
 * Implementations reject active queues defensively when @queue->items is not
 * NULL, preserving the previous queue instead of silently leaking it.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
int alfred_record_queue_init(alfred_record_queue_t *queue, size_t capacity);

/*
 * alfred_record_queue_destroy - release all owned records and queue storage
 * @queue: queue initialized by alfred_record_queue_init()
 *
 * Safe to call with NULL. After this call, the queue is cleared to zero.
 *
 * Ownership effect:
 *   Every still-queued owned record is destroyed before the backing buffer is
 *   released. Records already popped from the queue are no longer owned by the
 *   queue and remain the caller's responsibility.
 */
void alfred_record_queue_destroy(alfred_record_queue_t *queue);

/*
 * alfred_record_queue_clear - release queued owned records but keep storage
 * @queue: initialized queue
 *
 * This is useful for future dispatcher shutdown and tests that want to reuse the
 * same bounded buffer without another allocation.
 *
 * Ownership effect:
 *   Every currently queued owned record is destroyed. The backing buffer remains
 *   allocated and can receive new pushed records with the same capacity.
 */
void alfred_record_queue_clear(alfred_record_queue_t *queue);

/*
 * alfred_record_queue_push - enqueue an owned clone of a borrowed record
 * @queue: initialized queue
 * @record: borrowed record produced by a backend, adapter, core, or diagnostic
 *
 * The queued entry is an owned clone. The caller may reuse or destroy its
 * borrowed buffers as soon as this function returns.
 *
 * Ownership effect:
 *   On success, the queue owns an independent copy of the record strings. On
 *   failure, the queue does not take ownership of anything from @record.
 *
 * Return: 0 on success, -1 if the queue is invalid, full, or cloning fails.
 */
int alfred_record_queue_push(alfred_record_queue_t *queue,
                             const alfred_record_t *record);

/*
 * alfred_record_queue_pop - remove the next owned record from the queue
 * @queue: initialized queue
 * @record: destination that receives ownership of the popped record
 *
 * On success, @record must later be passed to alfred_record_destroy_owned().
 * The queue slot is cleared after ownership is transferred.
 *
 * Ownership precondition:
 *   @record must be zeroed or must not currently own strings. pop() transfers
 *   one owned record into an empty destination; it is not a replace helper. If a
 *   caller reuses the same local alfred_record_t in a loop, it must call
 *   alfred_record_destroy_owned(record) before the next successful pop().
 *
 * Ownership effect:
 *   On success, ownership moves from the queue slot to @record. The queue slot
 *   is cleared so queue cleanup will not free the popped record again. On
 *   failure, no ownership is transferred.
 *
 * Return: 0 on success, -1 if the queue is invalid, empty, or @record is NULL.
 */
int alfred_record_queue_pop(alfred_record_queue_t *queue,
                            alfred_record_t *record);

size_t alfred_record_queue_count(const alfred_record_queue_t *queue);
size_t alfred_record_queue_capacity(const alfred_record_queue_t *queue);
int alfred_record_queue_is_empty(const alfred_record_queue_t *queue);
int alfred_record_queue_is_full(const alfred_record_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_QUEUE_H */
