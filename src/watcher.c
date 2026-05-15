/* ============================================================================
 * watcher.c
 * Professional watcher table manager
 *
 * Responsibility:
 *   Maintain mapping:
 *
 *      watch descriptor (wd)  <-> absolute path
 *
 * Used by:
 *   app.c
 *   event_engine.c
 *
 * Design goals:
 *   - O(1) direct access by wd
 *   - dynamic resize
 *   - safe memory management
 *   - clean API
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
 * Ensure table can contain given wd index.
 *
 * Since inotify watch descriptors are integers that may grow,
 * we expand array when needed.
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

    /* zero initialize new area */
    memset(tmp + wt->capacity,
           0,
           (new_capacity - wt->capacity)
           * sizeof(watcher_entry_t));

    wt->items    = tmp;
    wt->capacity = new_capacity;

    return 0;
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

/*
 * Initialize watcher table.
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
 * Free all memory.
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
 * Store or replace mapping:
 *
 *   wd -> path
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

    /* count only if new active entry */
    if (!slot->active)
        wt->count++;

    slot->wd     = wd;
    slot->active = 1;

    snprintf(slot->path,
             sizeof(slot->path),
             "%s",
             path);

    return 0;
}

/*
 * Remove watcher entry.
 *
 * Does NOT call inotify_rm_watch().
 * Only internal table cleanup.
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
    slot->path[0] = '\0';

    if (wt->count > 0)
        wt->count--;
}

/*
 * Return path for wd.
 *
 * NULL if invalid.
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
 * Check if watcher exists.
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
 * Optional helper:
 * number of active watchers
 */
size_t watcher_count(const watcher_table_t *wt)
{
    if (wt == NULL)
        return 0;

    return wt->count;
}

/*
 * Optional helper:
 * print debugging table
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

        printf("wd=%d path=%s\n",
               e->wd,
               e->path);
    }

    printf("=======================\n");
}
