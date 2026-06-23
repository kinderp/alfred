/* ============================================================================
 * alfred_record_dispatcher.c - bounded fan-out for Event Model v0 records
 * ========================================================================== */

#include "alfred_record_dispatcher.h"

#include <string.h>

int alfred_record_dispatcher_init(alfred_record_dispatcher_t *dispatcher,
                                  alfred_record_dispatcher_sink_t *sinks,
                                  size_t capacity)
{
    if (dispatcher == NULL || sinks == NULL || capacity == 0u) {
        return -1;
    }

    memset(dispatcher, 0, sizeof(*dispatcher));
    memset(sinks, 0, capacity * sizeof(*sinks));

    dispatcher->sinks = sinks;
    dispatcher->capacity = capacity;
    return 0;
}

void alfred_record_dispatcher_clear(alfred_record_dispatcher_t *dispatcher)
{
    if (dispatcher == NULL || dispatcher->sinks == NULL) {
        return;
    }

    memset(dispatcher->sinks, 0, dispatcher->capacity * sizeof(*dispatcher->sinks));
    dispatcher->count = 0u;
}

int alfred_record_dispatcher_add_sink(
    alfred_record_dispatcher_t *dispatcher,
    const char *name,
    alfred_record_dispatcher_sink_class_t sink_class,
    const alfred_record_sink_t *sink)
{
    alfred_record_dispatcher_sink_t *entry;

    if (dispatcher == NULL || dispatcher->sinks == NULL || sink == NULL ||
        sink->emit == NULL || dispatcher->count == dispatcher->capacity) {
        return -1;
    }

    entry = &dispatcher->sinks[dispatcher->count];
    entry->name = name;
    entry->sink_class = sink_class;
    entry->sink = *sink;
    dispatcher->count++;

    return 0;
}

int alfred_record_dispatcher_dispatch_one(
    const alfred_record_dispatcher_t *dispatcher,
    const alfred_record_t *record)
{
    size_t i;

    if (dispatcher == NULL || dispatcher->sinks == NULL || record == NULL) {
        return -1;
    }

    for (i = 0u; i < dispatcher->count; ++i) {
        if (alfred_record_sink_emit(&dispatcher->sinks[i].sink, record) != 0) {
            return -1;
        }
    }

    return 0;
}

size_t alfred_record_dispatcher_count(
    const alfred_record_dispatcher_t *dispatcher)
{
    if (dispatcher == NULL) {
        return 0u;
    }

    return dispatcher->count;
}

size_t alfred_record_dispatcher_capacity(
    const alfred_record_dispatcher_t *dispatcher)
{
    if (dispatcher == NULL) {
        return 0u;
    }

    return dispatcher->capacity;
}

int alfred_record_dispatcher_is_full(
    const alfred_record_dispatcher_t *dispatcher)
{
    return dispatcher != NULL && dispatcher->capacity != 0u &&
           dispatcher->count == dispatcher->capacity;
}
