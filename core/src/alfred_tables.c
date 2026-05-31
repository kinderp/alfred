/* ============================================================================
 * alfred_tables.c - internal hash tables for core correlation state
 *
 * The core needs memory between raw events. MOVED_FROM must wait for a matching
 * MOVED_TO, and MODIFY events must remember the last emitted timestamp for each
 * path. This file owns those two internal tables.
 *
 * The tables are deliberately simple fixed-bucket hash tables with linked-list
 * buckets. They keep the code easy to inspect for students and avoid external
 * dependencies while still providing O(1) average lookup for the project scale.
 * This is not meant to be a generic container library: each table exists only
 * because one semantic rule needs memory across raw events.
 * ========================================================================== */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "alfred_correlator.h"
#include "alfred_utils.h"

#include "alfred_tables.h"

/* ============================================================================
 * MOVE HASH INDEX
 * ========================================================================== */

/*
 * move_index - map an inotify move cookie to a bucket index
 * @cookie: raw move cookie
 *
 * Return: bucket index in the fixed moves table.
 */
static inline uint32_t move_index(uint32_t cookie)
{
    return alfred_hash_u32(cookie) & 1023;
}

/* ============================================================================
 * PATH HASH INDEX
 * ========================================================================== */

/*
 * path_index - map a filesystem path to a debounce bucket index
 * @path: path key
 *
 * Return: bucket index in the fixed debounce table.
 */
static inline uint32_t path_index(const char *path)
{
    return alfred_hash_str(path) & 1023;
}

/* ============================================================================
 * MOVE TABLE INSERT
 * ========================================================================== */

/*
 * alfred_move_insert - store MOVED_FROM until MOVED_TO arrives
 * @e: core engine
 * @r: raw MOVED_FROM event
 *
 * The entry copies r->path because the raw event path buffer is usually owned
 * by the backend stack frame and becomes invalid after alfred_process()
 * returns.
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
    if (!n)
        return;

    n->cookie = r->cookie;
    n->ts_ns = r->ts_ns;
    n->pid = r->pid;
    n->is_dir = (r->mask & ALFRED_RAW_ISDIR);

    n->src_path = alfred_strdup(r->path);
    if (!n->src_path) {
        free(n);
        return;
    }

    n->next = e->moves[idx];
    e->moves[idx] = n;
}

/* ============================================================================
 * MOVE TABLE TAKE
 * ========================================================================== */

/*
 * alfred_move_take - retrieve and remove a pending move by cookie
 * @e: core engine
 * @cookie: raw move cookie
 *
 * Ownership of the returned entry moves to the caller, which must free
 * src_path and the entry after semantic classification.
 *
 * Return: removed entry on success, NULL otherwise.
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

/* ============================================================================
 * MOVE TABLE SWEEP (TIMEOUT CLEANUP)
 * ========================================================================== */

/*
 * alfred_move_sweep - remove stale pending move entries
 * @e: core engine
 * @now_ns: current monotonic timestamp
 *
 * If MOVED_TO never arrives, the current implementation drops the stale
 * MOVED_FROM entry. A future policy may emit a moved-out/delete-style event,
 * but that decision belongs in the semantic layer, not in table mechanics.
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

                free(cur->src_path);
                free(cur);

                continue;
            }

            pp = &(*pp)->next;
        }
    }
}

/* ============================================================================
 * DEBOUNCE GET OR CREATE
 * ========================================================================== */

/*
 * alfred_debounce_get - find or create debounce state for a path
 * @e: core engine
 * @path: path used as debounce key
 *
 * Return: debounce entry on success, NULL on invalid input.
 */
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

    /* Create a new entry if the path has not been seen before. */
    n = calloc(1, sizeof(*n));
    if (!n)
        return NULL;

    n->path = alfred_strdup(path);
    if (!n->path) {
        free(n);
        return NULL;
    }

    n->next = e->debounce[idx];
    e->debounce[idx] = n;

    return n;
}

/* ============================================================================
 * DEBOUNCE SHOULD EMIT
 * ========================================================================== */

/*
 * alfred_debounce_should_emit - apply the MODIFY debounce window
 * @e: core engine
 * @d: debounce state for the path
 * @now_ns: current monotonic timestamp
 *
 * Return: 1 when FILE_MODIFIED should be emitted, 0 when suppressed.
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

/* ============================================================================
 * CLEANUP HELPERS (optional future use)
 * ========================================================================== */

/*
 * alfred_tables_destroy - release all move and debounce table entries
 * @e: core engine to clean up
 */
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
