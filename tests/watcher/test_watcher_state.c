/*
 * test_watcher_state.c - direct tests for watcher reliability states
 *
 * These tests stay below the inotify backend. They do not need a real kernel
 * watch because the goal is to fix the in-memory contract: when the backend
 * later receives IN_MOVE_SELF or starts a resync, it must be able to trust the
 * state transitions stored in watcher_table_t.
 */

#include "watcher.h"

#include <assert.h>
#include <string.h>

typedef struct iter_capture {
    int visited;
    int stop_after;
    int wds[8];
    const char *paths[8];
} iter_capture_t;

static int capture_entry(const watcher_entry_t *entry, void *userdata)
{
    iter_capture_t *capture = (iter_capture_t *)userdata;

    assert(entry != NULL);
    assert(capture != NULL);
    assert(capture->visited < 8);

    capture->wds[capture->visited] = entry->wd;
    capture->paths[capture->visited] = entry->path;
    capture->visited++;

    if (capture->stop_after > 0 &&
        capture->visited >= capture->stop_after) {
        return 77;
    }

    return 0;
}

static void test_store_starts_valid(void)
{
    watcher_table_t table;
    dev_t device_id = 0;
    ino_t inode_id = 0;

    /*
     * A newly initialized table contains only removed/unused slots. The test
     * starts with capacity 1 on purpose: storing wd=4 also exercises table
     * expansion before the state assertions.
     */
    assert(watcher_init(&table, 1) == 0);

    /*
     * Storing a watch means "Alfred currently knows this wd -> path mapping and
     * considers it reliable". That is the normal state used by the backend to
     * reconstruct child event paths.
     */
    assert(watcher_store(&table, 4, "/tmp/root") == 0);
    assert(watcher_exists(&table, 4) == 1);
    assert(strcmp(watcher_get_path(&table, 4), "/tmp/root") == 0);
    assert(watcher_get_state(&table, 4) == WATCHER_STATE_VALID);
    assert(watcher_get_identity(&table, 4, &device_id, &inode_id) == -1);

    /*
     * VALID is not STALE. This matters because future code can cheaply ask
     * whether an event path needs recovery policy before being trusted.
     */
    assert(watcher_is_stale(&table, 4) == 0);

    watcher_destroy(&table);
}

/*
 * test_store_identity_records_device_and_inode - keep identity next to path
 *
 * The backend captures st_dev/st_ino when a real inotify watch is installed.
 * Those two values let future resync code distinguish "the old path still
 * exists" from "the old path is still the same filesystem object".
 */
static void test_store_identity_records_device_and_inode(void)
{
    watcher_table_t table;
    dev_t device_id = 0;
    ino_t inode_id = 0;

    assert(watcher_init(&table, 1) == 0);

    assert(watcher_store_identity(&table,
                                  6,
                                  "/tmp/with-id",
                                  (dev_t)1234,
                                  (ino_t)5678) == 0);

    assert(watcher_exists(&table, 6) == 1);
    assert(watcher_get_state(&table, 6) == WATCHER_STATE_VALID);
    assert(watcher_get_identity(&table, 6, &device_id, &inode_id) == 0);
    assert(device_id == (dev_t)1234);
    assert(inode_id == (ino_t)5678);

    /*
     * Replacing the slot through watcher_store() deliberately clears identity.
     * This keeps the contract explicit: identity exists only when the caller
     * captured it and chose watcher_store_identity().
     */
    assert(watcher_store(&table, 6, "/tmp/no-id") == 0);
    assert(watcher_get_identity(&table, 6, &device_id, &inode_id) == -1);

    assert(watcher_store_identity(&table,
                                  6,
                                  "/tmp/with-id-again",
                                  (dev_t)4321,
                                  (ino_t)8765) == 0);
    assert(watcher_get_identity(&table, 6, &device_id, &inode_id) == 0);
    assert(device_id == (dev_t)4321);
    assert(inode_id == (ino_t)8765);

    watcher_remove(&table, 6);
    assert(watcher_get_identity(&table, 6, &device_id, &inode_id) == -1);

    watcher_destroy(&table);
}

/*
 * test_state_can_be_marked_stale_and_restored - exercise active-state changes
 *
 * The important distinction is that STALE and RESYNCING are still active
 * watches. Their path remains stored, but the backend must interpret the path
 * with different confidence.
 */
static void test_state_can_be_marked_stale_and_restored(void)
{
    watcher_table_t table;

    assert(watcher_init(&table, 1) == 0);
    assert(watcher_store(&table, 2, "/tmp/project") == 0);

    /*
     * VALID -> STALE is the transition expected after an event such as
     * IN_MOVE_SELF: the wd may still exist, but the old path might no longer be
     * the true filesystem location.
     */
    assert(watcher_set_state(&table, 2, WATCHER_STATE_STALE) == 0);
    assert(watcher_get_state(&table, 2) == WATCHER_STATE_STALE);
    assert(watcher_is_stale(&table, 2) == 1);

    /*
     * STALE does not delete the mapping. Keeping the path is useful for logs,
     * diagnostics, and possible resync decisions, even though callers must not
     * blindly expose it as a correct semantic path.
     */
    assert(strcmp(watcher_get_path(&table, 2), "/tmp/project") == 0);

    /*
     * STALE -> RESYNCING means recovery has started. It is no longer just a
     * passive warning flag; the backend is actively comparing or rebuilding
     * state. For now this is only a data-model contract.
     */
    assert(watcher_set_state(&table, 2, WATCHER_STATE_RESYNCING) == 0);
    assert(watcher_get_state(&table, 2) == WATCHER_STATE_RESYNCING);
    assert(watcher_is_stale(&table, 2) == 0);

    /*
     * RESYNCING -> VALID represents a successful future recovery. The current
     * runtime does not perform that recovery yet, but the table already exposes
     * the transition needed by that later policy.
     */
    assert(watcher_set_state(&table, 2, WATCHER_STATE_VALID) == 0);
    assert(watcher_get_state(&table, 2) == WATCHER_STATE_VALID);

    watcher_destroy(&table);
}

/*
 * test_remove_clears_state - removed is table cleanup, not stale recovery
 *
 * Removing a watch is stronger than marking it stale. After removal there is no
 * active wd -> path mapping left, so later lookups must behave as if the watch
 * is gone, regardless of the previous reliability state.
 */
static void test_remove_clears_state(void)
{
    watcher_table_t table;

    assert(watcher_init(&table, 1) == 0);
    assert(watcher_store(&table, 9, "/tmp/gone") == 0);
    assert(watcher_set_state(&table, 9, WATCHER_STATE_STALE) == 0);

    /*
     * STALE -> REMOVED happens through watcher_remove(), not through
     * watcher_set_state(). This keeps the lifecycle clear: set_state changes
     * reliability of active watches, remove destroys the active mapping.
     */
    watcher_remove(&table, 9);

    assert(watcher_exists(&table, 9) == 0);
    assert(watcher_get_path(&table, 9) == NULL);
    assert(watcher_get_state(&table, 9) == WATCHER_STATE_REMOVED);
    assert(watcher_is_stale(&table, 9) == 0);

    watcher_destroy(&table);
}

/*
 * test_invalid_state_changes_fail - protect lifecycle invariants
 *
 * Callers must not be able to create inconsistent states such as "active but
 * removed" or "stale wd that was never stored".
 */
static void test_invalid_state_changes_fail(void)
{
    watcher_table_t table;

    assert(watcher_init(&table, 1) == 0);
    assert(watcher_store(&table, 1, "/tmp/root") == 0);

    /*
     * REMOVED is reserved for empty slots and watcher_remove(). Rejecting it in
     * watcher_set_state() avoids an active slot with contradictory state.
     */
    assert(watcher_set_state(&table, 1, WATCHER_STATE_REMOVED) == -1);
    assert(watcher_get_state(&table, 1) == WATCHER_STATE_VALID);

    /*
     * A non-existing wd cannot become STALE. The backend must first store a real
     * watch descriptor before assigning reliability state to it.
     */
    assert(watcher_set_state(&table, 99, WATCHER_STATE_STALE) == -1);
    assert(watcher_get_state(&table, 99) == WATCHER_STATE_REMOVED);

    watcher_destroy(&table);
}

/*
 * test_count_state_counts_only_active_entries - count watches by reliability
 *
 * Resync diagnostics will need answers such as "how many watches are stale?".
 * The count must ignore sparse, removed, or never-used slots even though those
 * slots also have the zero-valued REMOVED state in memory.
 */
static void test_count_state_counts_only_active_entries(void)
{
    watcher_table_t table;

    assert(watcher_init(&table, 1) == 0);

    assert(watcher_store(&table, 1, "/tmp/a") == 0);
    assert(watcher_store(&table, 3, "/tmp/b") == 0);
    assert(watcher_store(&table, 7, "/tmp/c") == 0);
    assert(watcher_count(&table) == 3);
    assert(watcher_count_state(&table, WATCHER_STATE_VALID) == 3);
    assert(watcher_count_state(&table, WATCHER_STATE_STALE) == 0);
    assert(watcher_count_state(&table, WATCHER_STATE_RESYNCING) == 0);
    assert(watcher_count_state(&table, WATCHER_STATE_REMOVED) == 0);

    assert(watcher_set_state(&table, 3, WATCHER_STATE_STALE) == 0);
    assert(watcher_set_state(&table, 7, WATCHER_STATE_RESYNCING) == 0);

    assert(watcher_count(&table) == 3);
    assert(watcher_count_state(&table, WATCHER_STATE_VALID) == 1);
    assert(watcher_count_state(&table, WATCHER_STATE_STALE) == 1);
    assert(watcher_count_state(&table, WATCHER_STATE_RESYNCING) == 1);

    watcher_remove(&table, 3);

    assert(watcher_count(&table) == 2);
    assert(watcher_count_state(&table, WATCHER_STATE_VALID) == 1);
    assert(watcher_count_state(&table, WATCHER_STATE_STALE) == 0);
    assert(watcher_count_state(&table, WATCHER_STATE_RESYNCING) == 1);
    assert(watcher_count_state(NULL, WATCHER_STATE_STALE) == 0);

    watcher_destroy(&table);
}

/*
 * test_foreach_state_visits_matching_active_entries - iterate by state
 *
 * Future resync code will need to visit stale watches one by one. The iterator
 * must filter by state, skip removed slots, preserve sparse wd indexing order,
 * and pass read-only entries to the callback.
 */
static void test_foreach_state_visits_matching_active_entries(void)
{
    watcher_table_t table;
    iter_capture_t capture;

    memset(&capture, 0, sizeof(capture));

    assert(watcher_init(&table, 1) == 0);
    assert(watcher_store(&table, 2, "/tmp/a") == 0);
    assert(watcher_store(&table, 5, "/tmp/b") == 0);
    assert(watcher_store(&table, 9, "/tmp/c") == 0);
    assert(watcher_set_state(&table, 2, WATCHER_STATE_STALE) == 0);
    assert(watcher_set_state(&table, 9, WATCHER_STATE_STALE) == 0);

    assert(watcher_foreach_state(&table,
                                 WATCHER_STATE_STALE,
                                 capture_entry,
                                 &capture) == 2);

    assert(capture.visited == 2);
    assert(capture.wds[0] == 2);
    assert(capture.wds[1] == 9);
    assert(strcmp(capture.paths[0], "/tmp/a") == 0);
    assert(strcmp(capture.paths[1], "/tmp/c") == 0);

    watcher_remove(&table, 2);
    memset(&capture, 0, sizeof(capture));

    assert(watcher_foreach_state(&table,
                                 WATCHER_STATE_STALE,
                                 capture_entry,
                                 &capture) == 1);
    assert(capture.visited == 1);
    assert(capture.wds[0] == 9);

    watcher_destroy(&table);
}

/*
 * test_foreach_state_can_stop_early - propagate callback stop condition
 *
 * Returning a nonzero value from the callback is how future resync code can
 * stop after the first failure without scanning the rest of the watcher table.
 */
static void test_foreach_state_can_stop_early(void)
{
    watcher_table_t table;
    iter_capture_t capture;

    memset(&capture, 0, sizeof(capture));
    capture.stop_after = 1;

    assert(watcher_init(&table, 1) == 0);
    assert(watcher_store(&table, 1, "/tmp/a") == 0);
    assert(watcher_store(&table, 2, "/tmp/b") == 0);
    assert(watcher_set_state(&table, 1, WATCHER_STATE_STALE) == 0);
    assert(watcher_set_state(&table, 2, WATCHER_STATE_STALE) == 0);

    assert(watcher_foreach_state(&table,
                                 WATCHER_STATE_STALE,
                                 capture_entry,
                                 &capture) == 77);
    assert(capture.visited == 1);
    assert(capture.wds[0] == 1);

    watcher_destroy(&table);
}

/*
 * test_foreach_state_rejects_invalid_inputs - protect iterator contract
 *
 * REMOVED is not iterable because removed slots are not active watches. NULL
 * table or callback arguments are also programmer errors.
 */
static void test_foreach_state_rejects_invalid_inputs(void)
{
    watcher_table_t table;
    iter_capture_t capture;

    memset(&capture, 0, sizeof(capture));

    assert(watcher_init(&table, 1) == 0);
    assert(watcher_store(&table, 1, "/tmp/a") == 0);

    assert(watcher_foreach_state(NULL,
                                 WATCHER_STATE_VALID,
                                 capture_entry,
                                 &capture) == -1);
    assert(watcher_foreach_state(&table,
                                 WATCHER_STATE_VALID,
                                 NULL,
                                 &capture) == -1);
    assert(watcher_foreach_state(&table,
                                 WATCHER_STATE_REMOVED,
                                 capture_entry,
                                 &capture) == -1);

    watcher_destroy(&table);
}

int main(void)
{
    test_store_starts_valid();
    test_store_identity_records_device_and_inode();
    test_state_can_be_marked_stale_and_restored();
    test_remove_clears_state();
    test_invalid_state_changes_fail();
    test_count_state_counts_only_active_entries();
    test_foreach_state_visits_matching_active_entries();
    test_foreach_state_can_stop_early();
    test_foreach_state_rejects_invalid_inputs();

    return 0;
}
