/* ============================================================================
 * alfred_record_dispatcher.h - bounded fan-out for Event Model v0 records
 *
 * The dispatcher is the small routing layer between a record queue and one or
 * more sinks. It does not serialize records, interpret their meaning, or own
 * writer resources. It only calls registered sinks in a predictable order.
 * ========================================================================== */

#ifndef ALFRED_RECORD_DISPATCHER_H
#define ALFRED_RECORD_DISPATCHER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record_sink.h"

#include <stddef.h>

/*
 * alfred_record_dispatcher_sink_class_t - future delivery policy class
 *
 * The v0 dispatcher does not yet implement different backpressure policies, but
 * each registered sink carries the class that future dispatcher code will use to
 * decide whether a slow or failing sink is critical, best-effort, or debug-only.
 */
typedef enum {
    ALFRED_RECORD_DISPATCHER_SINK_CRITICAL = 0,
    ALFRED_RECORD_DISPATCHER_SINK_BEST_EFFORT = 1,
    ALFRED_RECORD_DISPATCHER_SINK_DEBUG = 2
} alfred_record_dispatcher_sink_class_t;

/*
 * alfred_record_dispatcher_sink_t - one registered dispatcher target
 * @name: borrowed human-readable sink name, useful for future diagnostics
 * @sink_class: delivery/backpressure class reserved for later policy
 * @sink: sink callback and userdata
 *
 * The dispatcher does not own @name or @sink.userdata. Those lifetimes belong to
 * the application, registry, or future writer/plugin layer.
 */
typedef struct {
    const char *name;
    alfred_record_dispatcher_sink_class_t sink_class;
    alfred_record_sink_t sink;
} alfred_record_dispatcher_sink_t;

/*
 * alfred_record_dispatcher_t - bounded sink registry and fan-out helper
 * @sinks: caller-provided array of registered sinks
 * @capacity: maximum number of sinks that can be registered
 * @count: number of active sink entries
 *
 * "Bounded" here means that the dispatcher has an explicit maximum fan-out
 * width. It does not allocate or grow internally when more sinks are added.
 * This keeps the v0 behavior predictable and makes overflow/backpressure policy
 * visible instead of hidden behind unbounded memory growth.
 */
typedef struct {
    alfred_record_dispatcher_sink_t *sinks;
    size_t capacity;
    size_t count;
} alfred_record_dispatcher_t;

/*
 * alfred_record_dispatcher_init - initialize a bounded dispatcher
 * @dispatcher: dispatcher object to initialize
 * @sinks: caller-owned array used as dispatcher storage
 * @capacity: number of entries available in @sinks
 *
 * Return: 0 on success, -1 on invalid input.
 */
int alfred_record_dispatcher_init(alfred_record_dispatcher_t *dispatcher,
                                  alfred_record_dispatcher_sink_t *sinks,
                                  size_t capacity);

/*
 * alfred_record_dispatcher_clear - remove all registered sinks
 * @dispatcher: dispatcher initialized by alfred_record_dispatcher_init()
 *
 * The function clears only registration entries. It does not destroy writer
 * userdata because the dispatcher does not own writer contexts.
 */
void alfred_record_dispatcher_clear(alfred_record_dispatcher_t *dispatcher);

/*
 * alfred_record_dispatcher_add_sink - register one sink
 * @dispatcher: initialized dispatcher
 * @name: borrowed sink name, or NULL when no diagnostic name is available
 * @sink_class: future delivery/backpressure class
 * @sink: sink callback and userdata to store
 *
 * Return: 0 on success, -1 if the dispatcher is invalid, full, or the sink is
 * missing its emit callback.
 */
int alfred_record_dispatcher_add_sink(
    alfred_record_dispatcher_t *dispatcher,
    const char *name,
    alfred_record_dispatcher_sink_class_t sink_class,
    const alfred_record_sink_t *sink);

/*
 * alfred_record_dispatcher_dispatch_one - send one record to every sink
 * @dispatcher: initialized dispatcher
 * @record: borrowed record valid for the duration of this call
 *
 * The v0 dispatcher calls sinks synchronously and in registration order. If one
 * sink fails, dispatch stops immediately and the error is propagated. Future
 * versions can add per-class retry/drop policy after tests and benchmarks define
 * the expected behavior.
 *
 * Return: 0 if every sink succeeds, -1 on invalid input or first sink failure.
 */
int alfred_record_dispatcher_dispatch_one(
    const alfred_record_dispatcher_t *dispatcher,
    const alfred_record_t *record);

size_t alfred_record_dispatcher_count(
    const alfred_record_dispatcher_t *dispatcher);
size_t alfred_record_dispatcher_capacity(
    const alfred_record_dispatcher_t *dispatcher);
int alfred_record_dispatcher_is_full(
    const alfred_record_dispatcher_t *dispatcher);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_DISPATCHER_H */
