/* ============================================================================
 * watcher.h - in-memory table for inotify watch descriptors
 *
 * inotify events carry a watch descriptor (wd), not the full watched path. This
 * table stores the mapping needed by the backend to rebuild complete paths and
 * the reliability state of that mapping. The table is backend state only; the
 * core never sees wd values.
 * ========================================================================== */

#ifndef WATCHER_H
#define WATCHER_H

#include <limits.h>
#include <stddef.h>

/*
 * watcher_state_t - reliability state of one watch-table slot
 * @WATCHER_STATE_REMOVED: slot does not currently contain an active watch
 * @WATCHER_STATE_VALID: wd -> path mapping is considered reliable
 * @WATCHER_STATE_STALE: mapping exists but must not be trusted blindly
 * @WATCHER_STATE_RESYNCING: recovery is currently inspecting this watch/subtree
 *
 * The state is intentionally stored next to the path mapping. Events from
 * inotify arrive with a wd, so the backend needs to know in one lookup whether
 * the path reconstructed from that wd is still trustworthy.
 */
typedef enum {
    WATCHER_STATE_REMOVED = 0,
    WATCHER_STATE_VALID,
    WATCHER_STATE_STALE,
    WATCHER_STATE_RESYNCING
} watcher_state_t;

/*
 * watcher_entry_t - one wd -> path slot
 * @wd: inotify watch descriptor used as table index
 * @active: nonzero when this slot contains a valid mapping
 * @state: reliability state for the wd -> path mapping
 * @path: watched path copied into fixed storage
 */
typedef struct {
    int wd;
    int active;
    watcher_state_t state;
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
int watcher_set_state(watcher_table_t *wt, int wd, watcher_state_t state);
watcher_state_t watcher_get_state(const watcher_table_t *wt, int wd);
int watcher_is_stale(const watcher_table_t *wt, int wd);
size_t watcher_count(const watcher_table_t *wt);
size_t watcher_count_state(const watcher_table_t *wt, watcher_state_t state);

#endif
