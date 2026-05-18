/*======================================================================
 *
 * FILE: src/alfred_tables.c
 *
 * INTERNAL STATE TABLES
 *
 * PURPOSE:
 *   This file implements all in-memory state required to:
 *
 *     - correlate MOVED_FROM / MOVED_TO (rename/move)
 *     - debounce MODIFY storms
 *     - track pending CREATE events
 *
 * DESIGN PHILOSOPHY:
 *
 *   We prefer:
 *     - simple hash tables
 *     - deterministic behavior
 *     - O(1) average operations
 *     - no external dependencies
 *
 *   We avoid:
 *     - complex balanced trees
 *     - malloc-heavy structures
 *     - recursive algorithms
 *
 *======================================================================*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "alfred_correlator.h"
#include "alfred_utils.h"

/*======================================================================
 * MOVE TABLE ENTRY
 *======================================================================*/

/*
 * PURPOSE:
 *   Stores first half of a rename/move operation.
 *
 * LINUX BEHAVIOR:
 *
 *   inotify emits:
 *
 *     IN_MOVED_FROM  (cookie = X)
 *     IN_MOVED_TO    (cookie = X)
 *
 *   These may arrive:
 *     - out of order
 *     - on different threads
 *     - separated in time
 *
 *   So we MUST store MOVED_FROM until MOVED_TO arrives.
 */
typedef struct alfred_move_entry {

    /*
     * Kernel-provided correlation ID.
     * Same for FROM and TO events.
     */
    uint32_t cookie;

    /*
     * Timestamp of MOVED_FROM event.
     * Used for timeout cleanup.
     */
    uint64_t ts_ns;

    /*
     * PID if available (fanotify or metadata source).
     */
    int pid;

    /*
     * TRUE if this entry refers to a directory.
     */
    int is_dir;

    /*
     * Source full path (old location).
     */
    char *src_path;

    struct alfred_move_entry *next;

} alfred_move_entry_t;

/*======================================================================
 * DEBOUNCE ENTRY
 *======================================================================*/

/*
 * PURPOSE:
 *   Coalesce multiple MODIFY events on same file.
 *
 * REAL WORLD:
 *
 *   Editors generate:
 *     MODIFY x 1000 times
 *
 *   We want:
 *     1 logical MODIFY event
 */
typedef struct alfred_debounce_entry {

    /*
     * File path key.
     */
    char *path;

    /*
     * Last time we emitted MODIFY for this file.
     */
    uint64_t last_emit_ns;

    /*
     * Creation timestamp (for CREATE->READY logic).
     */
    uint64_t create_ns;

    struct alfred_debounce_entry *next;

} alfred_debounce_entry_t;

/*======================================================================
 * ENGINE INTERNAL STATE
 *======================================================================*/

/*
 * NOTE:
 *   This struct is hidden from public API.
 */
struct alfred_engine {

    /*
     * Global configuration.
     */
    alfred_config_t cfg;

    /*
     * User callback.
     */
    alfred_emit_fn emit;
    void *userdata;

    /*
     * Monotonic sequence counter.
     */
    uint64_t seq;

    /*==============================================================
     * MOVE TABLE
     *
     * Hash table keyed by cookie.
     *==============================================================*/
    alfred_move_entry_t *moves[1024];

    /*==============================================================
     * DEBOUNCE TABLE
     *
     * Hash table keyed by file path.
     *==============================================================*/
    alfred_debounce_entry_t *debounce[1024];
};

/*======================================================================
 * MOVE HASH INDEX
 *======================================================================*/
static inline uint32_t move_index(uint32_t cookie)
{
    return alfred_hash_u32(cookie) & 1023;
}

/*======================================================================
 * PATH HASH INDEX
 *======================================================================*/
static inline uint32_t path_index(const char *path)
{
    return alfred_hash_str(path) & 1023;
}

/*======================================================================
 * MOVE TABLE INSERT
 *======================================================================*/

/*
 * Store MOVED_FROM event until MOVED_TO arrives.
 */
void alfred_move_insert(
    alfred_engine_t *e,
    const alfred_raw_event_t *r)
{
    uint32_t idx;
    alfred_move_entry_t *n;

    if (!e || !r)
        return;

    idx = move_index(r->cookie);

    n = calloc(1, sizeof(*n));

    n->cookie = r->cookie;
    n->ts_ns = r->ts_ns;
    n->pid = r->pid;
    n->is_dir = (r->mask & ALFRED_RAW_ISDIR);

    n->src_path = alfred_strdup(r->path);

    n->next = e->moves[idx];
    e->moves[idx] = n;
}

/*======================================================================
 * MOVE TABLE TAKE
 *======================================================================*/

/*
 * Retrieve and remove MOVE entry by cookie.
 *
 * RETURNS:
 *   pointer if found
 *   NULL otherwise
 */
alfred_move_entry_t *alfred_move_take(
    alfred_engine_t *e,
    uint32_t cookie)
{
    uint32_t idx;
    alfred_move_entry_t **pp;
    alfred_move_entry_t *cur;

    if (!e)
        return NULL;

    idx = move_index(cookie);
    pp = &e->moves[idx];

    while (*pp) {

        cur = *pp;

        if (cur->cookie == cookie) {

            /*
             * Remove from chain.
             */
            *pp = cur->next;

            return cur;
        }

        pp = &(*pp)->next;
    }

    return NULL;
}

/*======================================================================
 * MOVE TABLE SWEEP (TIMEOUT CLEANUP)
 *======================================================================*/

/*
 * If MOVED_TO never arrives, we must resolve pending entry.
 */
void alfred_move_sweep(
    alfred_engine_t *e,
    uint64_t now_ns)
{
    int i;

    for (i = 0; i < 1024; i++) {

        alfred_move_entry_t **pp = &e->moves[i];

        while (*pp) {

            alfred_move_entry_t *cur = *pp;

            /*
             * Timeout expired => orphan move_from
             */
            if (now_ns - cur->ts_ns >
                alfred_ms_to_ns(e->cfg.move_timeout_ms))
            {
                *pp = cur->next;

                /*
                 * NOTE:
                 * In real engine we emit "DELETE or MOVED_OUT"
                 * here. Kept minimal in tables layer.
                 */

                free(cur->src_path);
                free(cur);

                continue;
            }

            pp = &(*pp)->next;
        }
    }
}

/*======================================================================
 * DEBOUNCE GET OR CREATE
 *======================================================================*/

alfred_debounce_entry_t *alfred_debounce_get(
    alfred_engine_t *e,
    const char *path)
{
    uint32_t idx;
    alfred_debounce_entry_t *n;

    if (!e || !path)
        return NULL;

    idx = path_index(path);

    n = e->debounce[idx];

    while (n) {
        if (strcmp(n->path, path) == 0)
            return n;
        n = n->next;
    }

    /*
     * Create new entry if not found.
     */
    n = calloc(1, sizeof(*n));
    n->path = alfred_strdup(path);

    n->next = e->debounce[idx];
    e->debounce[idx] = n;

    return n;
}

/*======================================================================
 * DEBOUNCE SHOULD EMIT
 *======================================================================*/

/*
 * Decide whether MODIFY should be emitted or suppressed.
 */
int alfred_debounce_should_emit(
    alfred_engine_t *e,
    alfred_debounce_entry_t *d,
    uint64_t now_ns)
{
    if (!e || !d)
        return 1;

    if (now_ns - d->last_emit_ns <
        alfred_ms_to_ns(e->cfg.modify_debounce_ms))
    {
        return 0;
    }

    d->last_emit_ns = now_ns;
    return 1;
}

/*======================================================================
 * CLEANUP HELPERS (optional future use)
 *======================================================================*/

void alfred_tables_destroy(alfred_engine_t *e)
{
    int i;

    if (!e)
        return;

    for (i = 0; i < 1024; i++) {

        alfred_move_entry_t *m = e->moves[i];
        while (m) {
            alfred_move_entry_t *next = m->next;
            free(m->src_path);
            free(m);
            m = next;
        }

        alfred_debounce_entry_t *d = e->debounce[i];
        while (d) {
            alfred_debounce_entry_t *next = d->next;
            free(d->path);
            free(d);
            d = next;
        }
    }
}
