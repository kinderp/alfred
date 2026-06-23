/*
 * test_record_queue.c - owned queue tests for Event Model v0 records
 *
 * This test fixes the first queue contract before Alfred wires any queue into
 * the runtime path. Backends, adapters, and diagnostics still emit records
 * synchronously today. A future dispatcher needs a bounded buffer whose records
 * remain valid after producer-owned or stack-owned strings disappear.
 *
 * Expected queue contract:
 *
 * push:
 * - accepts a borrowed alfred_record_t
 * - stores an owned clone inside the queue
 * - rejects pushes when the bounded queue is full
 *
 * pop:
 * - returns records in FIFO order
 * - transfers ownership to the caller
 * - clears the internal slot so queue cleanup does not double-free it
 * - requires the destination record to be zeroed or already destroyed
 *
 * clear/destroy:
 * - release any owned records still queued
 * - keep queue state auditable after cleanup
 *
 * init:
 * - rejects reinitialization of an already active queue
 * - preserves the original queue when that invalid reinit is rejected
 *
 * Meaning:
 * This is not the final high-performance queue. It is a deliberately small v0
 * contract that lets us test lifetime and backpressure behavior before adding
 * threads, mutexes, ring-buffer optimizations, per-sink queues, or benchmarks.
 */

#include "alfred_record_owned.h"
#include "alfred_record_queue.h"

#include <assert.h>
#include <string.h>

static alfred_record_t make_raw_record(const char *path,
                                       alfred_record_type_t type)
{
    alfred_record_t record;

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_NORMALIZED_RAW;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = type;
    record.source = 1u;
    record.raw_mask = 256u;
    record.path = path;

    return record;
}

static void test_queue_push_pop_owns_borrowed_path(void)
{
    alfred_record_queue_t queue;
    alfred_record_t src;
    alfred_record_t popped;
    char path[] = "/tmp/root/created";

    memset(&queue, 0, sizeof(queue));
    memset(&popped, 0, sizeof(popped));

    assert(alfred_record_queue_init(&queue, 2u) == 0);

    src = make_raw_record(path, ALFRED_RECORD_TYPE_RAW_CREATE);
    assert(alfred_record_queue_push(&queue, &src) == 0);
    assert(alfred_record_queue_count(&queue) == 1u);
    assert(!alfred_record_queue_is_empty(&queue));

    /*
     * Mutate the producer-owned buffer after push(). If the queue kept a borrowed
     * pointer, the popped path would change too.
     */
    strcpy(path, "/tmp/root/changed");

    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/created") == 0);
    assert(popped.path != src.path);
    assert(alfred_record_queue_count(&queue) == 0u);
    assert(alfred_record_queue_is_empty(&queue));

    alfred_record_destroy_owned(&popped);
    alfred_record_queue_destroy(&queue);
}

static void test_queue_preserves_fifo_order_across_wraparound(void)
{
    alfred_record_queue_t queue;
    alfred_record_t first;
    alfred_record_t second;
    alfred_record_t third;
    alfred_record_t popped;

    memset(&queue, 0, sizeof(queue));
    memset(&popped, 0, sizeof(popped));

    assert(alfred_record_queue_init(&queue, 2u) == 0);

    first = make_raw_record("/tmp/root/one", ALFRED_RECORD_TYPE_RAW_CREATE);
    second = make_raw_record("/tmp/root/two", ALFRED_RECORD_TYPE_RAW_DELETE);
    third = make_raw_record("/tmp/root/three", ALFRED_RECORD_TYPE_RAW_MODIFY);

    assert(alfred_record_queue_push(&queue, &first) == 0);
    assert(alfred_record_queue_push(&queue, &second) == 0);
    assert(alfred_record_queue_is_full(&queue));

    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/one") == 0);
    assert(popped.type == ALFRED_RECORD_TYPE_RAW_CREATE);
    alfred_record_destroy_owned(&popped);

    assert(alfred_record_queue_push(&queue, &third) == 0);

    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/two") == 0);
    assert(popped.type == ALFRED_RECORD_TYPE_RAW_DELETE);
    alfred_record_destroy_owned(&popped);

    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/three") == 0);
    assert(popped.type == ALFRED_RECORD_TYPE_RAW_MODIFY);
    alfred_record_destroy_owned(&popped);

    assert(alfred_record_queue_is_empty(&queue));
    alfred_record_queue_destroy(&queue);
}

static void test_queue_rejects_overflow_and_invalid_input(void)
{
    alfred_record_queue_t queue;
    alfred_record_t src;
    alfred_record_t popped;

    memset(&queue, 0, sizeof(queue));
    memset(&popped, 0, sizeof(popped));

    src = make_raw_record("/tmp/root/one", ALFRED_RECORD_TYPE_RAW_CREATE);

    assert(alfred_record_queue_init(NULL, 1u) != 0);
    assert(alfred_record_queue_init(&queue, 0u) != 0);
    assert(alfred_record_queue_init(&queue, 1u) == 0);

    assert(alfred_record_queue_push(NULL, &src) != 0);
    assert(alfred_record_queue_push(&queue, NULL) != 0);
    assert(alfred_record_queue_push(&queue, &src) == 0);
    assert(alfred_record_queue_push(&queue, &src) != 0);
    assert(alfred_record_queue_is_full(&queue));

    assert(alfred_record_queue_pop(NULL, &popped) != 0);
    assert(alfred_record_queue_pop(&queue, NULL) != 0);
    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    alfred_record_destroy_owned(&popped);
    assert(alfred_record_queue_pop(&queue, &popped) != 0);

    alfred_record_queue_destroy(&queue);
    assert(alfred_record_queue_capacity(&queue) == 0u);
}

static void test_queue_clear_releases_records_and_reuses_storage(void)
{
    alfred_record_queue_t queue;
    alfred_record_t first;
    alfred_record_t second;
    alfred_record_t popped;

    memset(&queue, 0, sizeof(queue));
    memset(&popped, 0, sizeof(popped));

    first = make_raw_record("/tmp/root/first", ALFRED_RECORD_TYPE_RAW_CREATE);
    second = make_raw_record("/tmp/root/second", ALFRED_RECORD_TYPE_RAW_DELETE);

    assert(alfred_record_queue_init(&queue, 2u) == 0);
    assert(alfred_record_queue_push(&queue, &first) == 0);
    alfred_record_queue_clear(&queue);
    assert(alfred_record_queue_count(&queue) == 0u);
    assert(alfred_record_queue_capacity(&queue) == 2u);

    assert(alfred_record_queue_push(&queue, &second) == 0);
    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/second") == 0);

    alfred_record_destroy_owned(&popped);
    alfred_record_queue_destroy(&queue);
}

static void test_queue_pop_destination_can_be_reused_after_destroy(void)
{
    alfred_record_queue_t queue;
    alfred_record_t first;
    alfred_record_t second;
    alfred_record_t popped;

    memset(&queue, 0, sizeof(queue));
    memset(&popped, 0, sizeof(popped));

    first = make_raw_record("/tmp/root/first", ALFRED_RECORD_TYPE_RAW_CREATE);
    second = make_raw_record("/tmp/root/second", ALFRED_RECORD_TYPE_RAW_DELETE);

    assert(alfred_record_queue_init(&queue, 2u) == 0);
    assert(alfred_record_queue_push(&queue, &first) == 0);
    assert(alfred_record_queue_push(&queue, &second) == 0);

    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/first") == 0);

    /*
     * pop() transfers ownership into an empty/non-owned destination. Reusing
     * the same local destination is correct only after destroying the previous
     * owned record received from the queue.
     */
    alfred_record_destroy_owned(&popped);
    assert(popped.path == NULL);

    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/second") == 0);
    assert(popped.path != second.path);

    alfred_record_destroy_owned(&popped);
    alfred_record_queue_destroy(&queue);
}

static void test_queue_rejects_reinit_without_destroy(void)
{
    alfred_record_queue_t queue;
    alfred_record_t src;
    alfred_record_t popped;

    memset(&queue, 0, sizeof(queue));
    memset(&popped, 0, sizeof(popped));

    src = make_raw_record("/tmp/root/kept", ALFRED_RECORD_TYPE_RAW_CREATE);

    assert(alfred_record_queue_init(&queue, 2u) == 0);
    assert(alfred_record_queue_push(&queue, &src) == 0);

    /*
     * A second init without destroy must fail. Otherwise init would overwrite
     * queue.items and leak both the old queue buffer and any owned records
     * still stored inside it.
     */
    assert(alfred_record_queue_init(&queue, 4u) != 0);

    assert(alfred_record_queue_capacity(&queue) == 2u);
    assert(alfred_record_queue_count(&queue) == 1u);
    assert(alfred_record_queue_pop(&queue, &popped) == 0);
    assert(strcmp(popped.path, "/tmp/root/kept") == 0);

    alfred_record_destroy_owned(&popped);
    alfred_record_queue_destroy(&queue);
}

int main(void)
{
    test_queue_push_pop_owns_borrowed_path();
    test_queue_preserves_fifo_order_across_wraparound();
    test_queue_rejects_overflow_and_invalid_input();
    test_queue_clear_releases_records_and_reuses_storage();
    test_queue_pop_destination_can_be_reused_after_destroy();
    test_queue_rejects_reinit_without_destroy();

    return 0;
}
