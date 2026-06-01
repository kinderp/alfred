/* ============================================================================
 * watcher.c - dynamic wd -> path table
 *
 * The inotify backend needs to reconstruct full paths from kernel events.
 * Kernel records contain a watch descriptor and an optional name; they do not
 * repeat the watched directory path. This table stores that missing directory
 * path, tracks whether the mapping is still reliable, and lets the backend
 * perform direct lookup by wd.
 *
 * The table may be sparse because inotify watch descriptors are kernel-assigned
 * integers. watcher_expand() grows the array until the descriptor can be used
 * as an index.
 * ========================================================================== */

#include "watcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ============================================================================
 * INTERNAL HELPERS
 * ========================================================================== */

/*
 * watcher_expand - ensure the table can address a watch descriptor
 * @wt: table to grow
 * @required_wd: watch descriptor that must fit as an index
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
static int watcher_expand(watcher_table_t *wt, int required_wd)
{
    if (wt == NULL)
        return -1;

    if (required_wd < 0)
        return -1;

    if ((size_t)(required_wd) < wt->capacity)
        return 0;

    size_t new_capacity = wt->capacity;

    if (new_capacity == 0)
        new_capacity = 16;

    while (new_capacity <= (size_t)required_wd)
        new_capacity *= 2;

    watcher_entry_t *tmp =
        realloc(wt->items,
                new_capacity * sizeof(watcher_entry_t));

    if (tmp == NULL)
        return -1;

    /*
     * New slots must start inactive so later lookup can distinguish allocated
     * but unused wd values from real watcher entries.
     */
    memset(tmp + wt->capacity,
           0,
           (new_capacity - wt->capacity)
           * sizeof(watcher_entry_t));

    wt->items    = tmp;
    wt->capacity = new_capacity;

    return 0;
}

/*
 * watcher_state_is_valid - validate a public watcher state value
 * @state: state value supplied by a caller
 *
 * Return: nonzero when @state is part of watcher_state_t.
 */
static int watcher_state_is_valid(watcher_state_t state)
{
    return state == WATCHER_STATE_REMOVED ||
           state == WATCHER_STATE_VALID ||
           state == WATCHER_STATE_STALE ||
           state == WATCHER_STATE_RESYNCING;
}

/*
 * watcher_state_name - convert a watcher state to a debug string
 * @state: state value to render
 *
 * Return: static string used by watcher_dump().
 */
static const char *watcher_state_name(watcher_state_t state)
{
    switch (state) {
    case WATCHER_STATE_REMOVED:
        return "removed";
    case WATCHER_STATE_VALID:
        return "valid";
    case WATCHER_STATE_STALE:
        return "stale";
    case WATCHER_STATE_RESYNCING:
        return "resyncing";
    default:
        return "unknown";
    }
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

/*
 * watcher_init - initialize an empty watcher table
 * @wt: table to initialize
 * @capacity: initial slot count, or 0 for the default
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
int watcher_init(watcher_table_t *wt, size_t capacity)
{
    if (wt == NULL)
        return -1;

    memset(wt, 0, sizeof(*wt));

    if (capacity == 0)
        capacity = 16;

    wt->items =
        calloc(capacity, sizeof(watcher_entry_t));

    if (wt->items == NULL)
        return -1;

    wt->capacity = capacity;
    wt->count    = 0;

    return 0;
}

/*
 * watcher_destroy - free memory owned by a watcher table
 * @wt: table to destroy
 */
void watcher_destroy(watcher_table_t *wt)
{
    if (wt == NULL)
        return;

    free(wt->items);

    wt->items    = NULL;
    wt->capacity = 0;
    wt->count    = 0;
}

/*
 * watcher_store - store or replace one wd -> path mapping
 * @wt: table to update
 * @wd: inotify watch descriptor
 * @path: watched path to copy into the table
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
int watcher_store(watcher_table_t *wt,
                  int wd,
                  const char *path)
{
    if (wt == NULL || path == NULL)
        return -1;

    if (wd < 0)
        return -1;

    if (watcher_expand(wt, wd) != 0)
        return -1;

    watcher_entry_t *slot = &wt->items[wd];

    /*
     * Replacing an active slot updates its path but does not create another
     * active watcher.
     */
    if (!slot->active)
        wt->count++;

    slot->wd     = wd;
    slot->active = 1;
    slot->state  = WATCHER_STATE_VALID;

    snprintf(slot->path,
             sizeof(slot->path),
             "%s",
             path);

    return 0;
}

/*
 * watcher_remove - clear one wd -> path mapping
 * @wt: table to update
 * @wd: inotify watch descriptor to clear
 *
 * This function only mutates the in-memory table. Kernel watch removal is owned
 * by watch_manager_remove().
 */
void watcher_remove(watcher_table_t *wt, int wd)
{
    if (wt == NULL)
        return;

    if (wd < 0)
        return;

    if ((size_t)wd >= wt->capacity)
        return;

    watcher_entry_t *slot = &wt->items[wd];

    if (!slot->active)
        return;

    slot->active = 0;
    slot->wd     = -1;
    slot->state  = WATCHER_STATE_REMOVED;
    slot->path[0] = '\0';

    if (wt->count > 0)
        wt->count--;
}

/*
 * watcher_get_path - look up the path associated with a watch descriptor
 * @wt: table to read
 * @wd: inotify watch descriptor
 *
 * Return: stored path pointer, or NULL when @wd is invalid or inactive.
 */
const char* watcher_get_path(const watcher_table_t *wt,
                             int wd)
{
    if (wt == NULL)
        return NULL;

    if (wd < 0)
        return NULL;

    if ((size_t)wd >= wt->capacity)
        return NULL;

    const watcher_entry_t *slot =
        &wt->items[wd];

    if (!slot->active)
        return NULL;

    return slot->path;
}

/*
 * watcher_exists - test whether a watch descriptor has an active mapping
 * @wt: table to inspect
 * @wd: inotify watch descriptor
 *
 * Return: 1 when active, 0 otherwise.
 */
int watcher_exists(const watcher_table_t *wt,
                   int wd)
{
    if (wt == NULL)
        return 0;

    if (wd < 0)
        return 0;

    if ((size_t)wd >= wt->capacity)
        return 0;

    return wt->items[wd].active ? 1 : 0;
}

/*
 * watcher_set_state - update the reliability state of an active watch
 * @wt: table to update
 * @wd: inotify watch descriptor to update
 * @state: new reliability state
 *
 * Removed is represented by watcher_remove(), not by changing the state of an
 * active slot. This keeps "slot is active" and "slot reliability" separate and
 * prevents callers from leaving an active wd that says it is removed.
 *
 * Return: 0 on success, -1 on invalid input or inactive watch descriptor.
 */
int watcher_set_state(watcher_table_t *wt,
                      int wd,
                      watcher_state_t state)
{
    if (wt == NULL)
        return -1;

    if (!watcher_state_is_valid(state))
        return -1;

    if (state == WATCHER_STATE_REMOVED)
        return -1;

    if (!watcher_exists(wt, wd))
        return -1;

    wt->items[wd].state = state;

    return 0;
}

/*
 * watcher_get_state - read the reliability state of a watch descriptor
 * @wt: table to inspect
 * @wd: inotify watch descriptor
 *
 * Return: stored state for active slots, WATCHER_STATE_REMOVED otherwise.
 */
watcher_state_t watcher_get_state(const watcher_table_t *wt,
                                  int wd)
{
    if (!watcher_exists(wt, wd))
        return WATCHER_STATE_REMOVED;

    return wt->items[wd].state;
}

/*
 * watcher_is_stale - test whether a watch mapping is no longer reliable
 * @wt: table to inspect
 * @wd: inotify watch descriptor
 *
 * Return: 1 for active stale watches, 0 otherwise.
 */
int watcher_is_stale(const watcher_table_t *wt,
                     int wd)
{
    return watcher_get_state(wt, wd) == WATCHER_STATE_STALE ? 1 : 0;
}

/*
 * watcher_count - return the number of active watcher mappings
 * @wt: table to inspect
 *
 * Return: active entry count, or 0 for NULL.
 */
size_t watcher_count(const watcher_table_t *wt)
{
    if (wt == NULL)
        return 0;

    return wt->count;
}

/*
 * watcher_count_state - count active watcher mappings in one reliability state
 * @wt: table to inspect
 * @state: reliability state to count
 *
 * This is a small diagnostic helper for future resync policy. Counting stale
 * or resyncing watches lets the backend report how much state is no longer
 * trusted without exposing the table internals. REMOVED is treated specially:
 * removed slots are inactive, so the count is always zero even if unused array
 * slots contain the zero-valued REMOVED state.
 *
 * Return: number of active entries with @state, or 0 for NULL/invalid state.
 */
size_t watcher_count_state(const watcher_table_t *wt,
                           watcher_state_t state)
{
    if (wt == NULL)
        return 0;

    if (!watcher_state_is_valid(state))
        return 0;

    if (state == WATCHER_STATE_REMOVED)
        return 0;

    size_t count = 0;

    for (size_t i = 0; i < wt->capacity; i++) {
        const watcher_entry_t *slot =
            &wt->items[i];

        if (!slot->active)
            continue;

        if (slot->state == state)
            count++;
    }

    return count;
}

/*
 * watcher_foreach_state - visit active watcher entries in one state
 * @wt: table to inspect
 * @state: reliability state to visit
 * @callback: function invoked for each matching entry
 * @userdata: opaque pointer passed to @callback
 *
 * Future resync code needs to inspect stale or resyncing watches without
 * reaching into watcher_table_t internals. The callback receives a const entry
 * so it can read wd, path, and state, but cannot mutate the table while it is
 * being iterated through this API.
 *
 * REMOVED is not iterable because removed slots are inactive and do not
 * represent live watches. If @callback returns nonzero, iteration stops early
 * and that nonzero value is returned to the caller.
 *
 * Return: number of visited entries on success, -1 on invalid input, or the
 * callback's nonzero return value when iteration is stopped early.
 */
int watcher_foreach_state(const watcher_table_t *wt,
                          watcher_state_t state,
                          watcher_iter_fn callback,
                          void *userdata)
{
    if (wt == NULL || callback == NULL)
        return -1;

    if (!watcher_state_is_valid(state))
        return -1;

    if (state == WATCHER_STATE_REMOVED)
        return -1;

    int visited = 0;

    for (size_t i = 0; i < wt->capacity; i++) {
        const watcher_entry_t *slot =
            &wt->items[i];

        if (!slot->active)
            continue;

        if (slot->state != state)
            continue;

        int rc = callback(slot, userdata);

        if (rc != 0)
            return rc;

        visited++;
    }

    return visited;
}

/*
 * watcher_dump - print the table to stdout for manual debugging
 * @wt: table to inspect
 */
void watcher_dump(const watcher_table_t *wt)
{
    if (wt == NULL)
        return;

    printf("==== WATCHER TABLE ====\n");
    printf("capacity=%zu active=%zu\n",
           wt->capacity,
           wt->count);

    for (size_t i = 0; i < wt->capacity; i++) {

        const watcher_entry_t *e =
            &wt->items[i];

        if (!e->active)
            continue;

        printf("wd=%d state=%s path=%s\n",
               e->wd,
               watcher_state_name(e->state),
               e->path);
    }

    printf("=======================\n");
}
