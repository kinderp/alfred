/*
 * test_lost_scope_queue.c - unit test for delayed lost-scope recovery storage
 *
 * A stale watch caused by IN_MOVE_SELF can still identify the same filesystem
 * object by (st_dev, st_ino), but Alfred may no longer know a trustworthy
 * textual path. The lost-scope queue records that recovery debt so a later
 * scanner pass can search monitored roots without emitting false raw/core
 * events while the path is unknown.
 *
 * This test includes inotify_backend.c directly so it can exercise the static
 * queue helpers without exporting them through the public backend API. The
 * queue is backend implementation detail: production code owns it through
 * inotify_backend_t, while the core should never see these records.
 *
 * Expected log contract:
 *
 * raw.log:
 * - none; this is a focused C test and does not read the kernel inotify queue
 *
 * events log stream:
 * - none; the queue is internal recovery state and does not log by itself
 *
 * Forbidden events:
 * - FILE_*, DIR_*, WATCH_RESYNC_* or WATCH_LOST_* diagnostics
 *
 * Meaning:
 * The test stores multiple stale scopes, verifies FIFO order across circular
 * buffer growth, verifies that path/reason strings are copied into queue
 * storage, and verifies invalid enqueue/pop inputs fail without changing the
 * queue. Runtime enqueue and WATCH_LOST_* diagnostics are intentionally left
 * for a later micro-step.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "../../modules/inotify/src/inotify_backend.c"

/*
 * assert_scope_entry - verify one popped lost-scope record
 * @entry: popped queue entry to inspect
 * @wd: expected watch descriptor
 * @old_path: expected copied stale path
 * @device_id: expected saved st_dev value
 * @inode_id: expected saved st_ino value
 * @reason: expected copied backend reason
 * @first_seen_ns: expected first queued timestamp
 * @retry_after_ns: expected retry timestamp
 *
 * Keeping this check in one helper makes the individual scenarios read as
 * state transitions rather than a long list of field comparisons.
 */
static void assert_scope_entry(const inotify_lost_scope_entry_t *entry,
                               int wd,
                               const char *old_path,
                               dev_t device_id,
                               ino_t inode_id,
                               const char *reason,
                               uint64_t first_seen_ns,
                               uint64_t retry_after_ns)
{
    assert(entry != NULL);
    assert(old_path != NULL);
    assert(reason != NULL);

    assert(entry->wd == wd);
    assert(entry->device_id == device_id);
    assert(entry->inode_id == inode_id);
    assert(entry->first_seen_ns == first_seen_ns);
    assert(entry->retry_after_ns == retry_after_ns);
    assert(entry->retry_count == 0);
    assert(strcmp(entry->old_path, old_path) == 0);
    assert(strcmp(entry->reason, reason) == 0);
}

/*
 * test_init_destroy_count - queue lifecycle starts empty and resets storage
 *
 * The backend initializes this queue together with inotify_backend_t. A clean
 * zero count is important because recovery code will later ask whether there
 * is work to process after a polling batch.
 */
static void test_init_destroy_count(void)
{
    inotify_lost_scope_queue_t queue;

    assert(backend_lost_scope_queue_init(&queue, 2) == 0);
    assert(backend_lost_scope_queue_count(&queue) == 0);
    assert(queue.capacity == 2);
    assert(queue.head == 0);
    assert(queue.items != NULL);

    backend_lost_scope_queue_destroy(&queue);
    assert(backend_lost_scope_queue_count(&queue) == 0);
    assert(queue.items == NULL);
    assert(queue.capacity == 0);
    assert(queue.head == 0);
}

/*
 * test_fifo_order_survives_wrap_and_growth - delayed scopes keep event order
 *
 * The queue is circular so popping one entry and adding more entries can wrap
 * the tail before growth. Recovery must still consume scopes in the order in
 * which Alfred lost path trust, otherwise diagnostics and later batching would
 * be harder to reason about.
 */
static void test_fifo_order_survives_wrap_and_growth(void)
{
    inotify_lost_scope_queue_t queue;
    inotify_lost_scope_entry_t entry;

    assert(backend_lost_scope_queue_init(&queue, 2) == 0);

    assert(backend_lost_scope_queue_enqueue(&queue,
                                            10,
                                            "/tmp/root/a",
                                            100,
                                            200,
                                            "IN_MOVE_SELF",
                                            1000,
                                            1500) == 0);
    assert(backend_lost_scope_queue_enqueue(&queue,
                                            11,
                                            "/tmp/root/b",
                                            101,
                                            201,
                                            "IN_MOVE_SELF",
                                            2000,
                                            2500) == 0);

    assert(backend_lost_scope_queue_pop(&queue, &entry) == 0);
    assert_scope_entry(&entry,
                       10,
                       "/tmp/root/a",
                       100,
                       200,
                       "IN_MOVE_SELF",
                       1000,
                       1500);

    assert(backend_lost_scope_queue_enqueue(&queue,
                                            12,
                                            "/tmp/root/c",
                                            102,
                                            202,
                                            "IN_MOVE_SELF",
                                            3000,
                                            3500) == 0);
    assert(backend_lost_scope_queue_enqueue(&queue,
                                            13,
                                            "/tmp/root/d",
                                            103,
                                            203,
                                            "IN_DELETE_SELF",
                                            4000,
                                            4500) == 0);

    assert(queue.capacity >= 4);
    assert(backend_lost_scope_queue_count(&queue) == 3);

    assert(backend_lost_scope_queue_pop(&queue, &entry) == 0);
    assert_scope_entry(&entry,
                       11,
                       "/tmp/root/b",
                       101,
                       201,
                       "IN_MOVE_SELF",
                       2000,
                       2500);

    assert(backend_lost_scope_queue_pop(&queue, &entry) == 0);
    assert_scope_entry(&entry,
                       12,
                       "/tmp/root/c",
                       102,
                       202,
                       "IN_MOVE_SELF",
                       3000,
                       3500);

    assert(backend_lost_scope_queue_pop(&queue, &entry) == 0);
    assert_scope_entry(&entry,
                       13,
                       "/tmp/root/d",
                       103,
                       203,
                       "IN_DELETE_SELF",
                       4000,
                       4500);

    assert(backend_lost_scope_queue_count(&queue) == 0);
    assert(backend_lost_scope_queue_pop(&queue, &entry) == -1);

    backend_lost_scope_queue_destroy(&queue);
}

/*
 * test_enqueue_copies_borrowed_strings - watcher-table pointers may expire
 *
 * Runtime enqueue will usually receive path data from the watcher table or a
 * stack-local reason string. The queue must keep its own copies so later
 * delayed recovery does not depend on those borrowed inputs still containing
 * the original text.
 */
static void test_enqueue_copies_borrowed_strings(void)
{
    inotify_lost_scope_queue_t queue;
    inotify_lost_scope_entry_t entry;
    char path[] = "/tmp/root/original";
    char reason[] = "IN_MOVE_SELF";

    assert(backend_lost_scope_queue_init(&queue, 1) == 0);
    assert(backend_lost_scope_queue_enqueue(&queue,
                                            21,
                                            path,
                                            300,
                                            400,
                                            reason,
                                            5000,
                                            6000) == 0);

    strcpy(path, "/tmp/root/changed");
    strcpy(reason, "CHANGED");

    assert(backend_lost_scope_queue_pop(&queue, &entry) == 0);
    assert_scope_entry(&entry,
                       21,
                       "/tmp/root/original",
                       300,
                       400,
                       "IN_MOVE_SELF",
                       5000,
                       6000);

    backend_lost_scope_queue_destroy(&queue);
}

/*
 * test_invalid_inputs_are_rejected - malformed recovery records are ignored
 *
 * A lost-scope record without a queue, path, reason, or valid wd would not be
 * actionable. Rejecting invalid input before mutation keeps the recovery queue
 * free of entries that a later scanner cannot interpret.
 */
static void test_invalid_inputs_are_rejected(void)
{
    inotify_lost_scope_queue_t queue;
    inotify_lost_scope_entry_t entry;

    assert(backend_lost_scope_queue_init(&queue, 1) == 0);

    assert(backend_lost_scope_queue_enqueue(NULL,
                                            1,
                                            "/tmp/root",
                                            1,
                                            1,
                                            "IN_MOVE_SELF",
                                            0,
                                            0) == -1);
    assert(backend_lost_scope_queue_enqueue(&queue,
                                            -1,
                                            "/tmp/root",
                                            1,
                                            1,
                                            "IN_MOVE_SELF",
                                            0,
                                            0) == -1);
    assert(backend_lost_scope_queue_enqueue(&queue,
                                            1,
                                            NULL,
                                            1,
                                            1,
                                            "IN_MOVE_SELF",
                                            0,
                                            0) == -1);
    assert(backend_lost_scope_queue_enqueue(&queue,
                                            1,
                                            "/tmp/root",
                                            1,
                                            1,
                                            NULL,
                                            0,
                                            0) == -1);
    assert(backend_lost_scope_queue_count(&queue) == 0);

    assert(backend_lost_scope_queue_pop(NULL, &entry) == -1);
    assert(backend_lost_scope_queue_pop(&queue, NULL) == -1);
    assert(backend_lost_scope_queue_pop(&queue, &entry) == -1);

    backend_lost_scope_queue_destroy(&queue);
}

int main(void)
{
    test_init_destroy_count();
    test_fifo_order_survives_wrap_and_growth();
    test_enqueue_copies_borrowed_strings();
    test_invalid_inputs_are_rejected();

    return 0;
}
