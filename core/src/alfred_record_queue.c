/* ============================================================================
 * alfred_record_queue.c - bounded owned-record queue for Event Model v0
 * ========================================================================== */

#include "alfred_record_queue.h"

#include "alfred_record_owned.h"

#include <stdlib.h>
#include <string.h>

int alfred_record_queue_init(alfred_record_queue_t *queue, size_t capacity)
{
    if (queue == NULL || capacity == 0u || queue->items != NULL) {
        return -1;
    }

    memset(queue, 0, sizeof(*queue));

    queue->items = calloc(capacity, sizeof(*queue->items));
    if (queue->items == NULL) {
        return -1;
    }

    queue->capacity = capacity;
    return 0;
}

void alfred_record_queue_clear(alfred_record_queue_t *queue)
{
    size_t i;

    if (queue == NULL || queue->items == NULL) {
        return;
    }

    for (i = 0u; i < queue->count; ++i) {
        size_t index = (queue->head + i) % queue->capacity;

        alfred_record_destroy_owned(&queue->items[index]);
    }

    queue->head = 0u;
    queue->tail = 0u;
    queue->count = 0u;
}

void alfred_record_queue_destroy(alfred_record_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }

    alfred_record_queue_clear(queue);
    free(queue->items);
    memset(queue, 0, sizeof(*queue));
}

int alfred_record_queue_push(alfred_record_queue_t *queue,
                             const alfred_record_t *record)
{
    alfred_record_t owned;

    if (queue == NULL || queue->items == NULL || record == NULL ||
        queue->count == queue->capacity) {
        return -1;
    }

    memset(&owned, 0, sizeof(owned));
    if (alfred_record_clone_owned(record, &owned) != 0) {
        return -1;
    }

    queue->items[queue->tail] = owned;
    queue->tail = (queue->tail + 1u) % queue->capacity;
    queue->count++;

    return 0;
}

int alfred_record_queue_pop(alfred_record_queue_t *queue,
                            alfred_record_t *record)
{
    if (queue == NULL || queue->items == NULL || record == NULL ||
        queue->count == 0u) {
        return -1;
    }

    *record = queue->items[queue->head];
    memset(&queue->items[queue->head], 0, sizeof(queue->items[queue->head]));
    queue->head = (queue->head + 1u) % queue->capacity;
    queue->count--;

    return 0;
}

size_t alfred_record_queue_count(const alfred_record_queue_t *queue)
{
    if (queue == NULL) {
        return 0u;
    }

    return queue->count;
}

size_t alfred_record_queue_capacity(const alfred_record_queue_t *queue)
{
    if (queue == NULL) {
        return 0u;
    }

    return queue->capacity;
}

int alfred_record_queue_is_empty(const alfred_record_queue_t *queue)
{
    return queue == NULL || queue->count == 0u;
}

int alfred_record_queue_is_full(const alfred_record_queue_t *queue)
{
    return queue != NULL && queue->capacity != 0u &&
           queue->count == queue->capacity;
}
