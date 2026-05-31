/* ============================================================================
 * watcher.h - in-memory table for inotify watch descriptors
 *
 * inotify events carry a watch descriptor (wd), not the full watched path. This
 * table stores the mapping needed by the backend to rebuild complete paths.
 * The table is backend state only; the core never sees wd values.
 * ========================================================================== */

#ifndef WATCHER_H
#define WATCHER_H

#include <limits.h>
#include <stddef.h>

/*
 * watcher_entry_t - one wd -> path slot
 * @wd: inotify watch descriptor used as table index
 * @active: nonzero when this slot contains a valid mapping
 * @path: watched path copied into fixed storage
 */
typedef struct {
    int wd;
    int active;
    char path[PATH_MAX];
} watcher_entry_t;

/*
 * watcher_table_t - dynamic array of watcher entries
 * @items: array indexed by watch descriptor
 * @count: number of active entries
 * @capacity: allocated number of slots in @items
 *
 * The direct-index layout makes lookup cheap: an inotify event with wd=N can
 * read items[N] after bounds and active checks. Sparse wd values are accepted,
 * so capacity may be larger than count.
 */
typedef struct {
    watcher_entry_t *items;
    size_t count;
    size_t capacity;
} watcher_table_t;

int watcher_init(watcher_table_t *wt, size_t capacity);
void watcher_destroy(watcher_table_t *wt);

int watcher_store(watcher_table_t *wt, int wd, const char *path);
void watcher_remove(watcher_table_t *wt, int wd);

const char* watcher_get_path(const watcher_table_t *wt, int wd);
int watcher_exists(const watcher_table_t *wt, int wd);

#endif
