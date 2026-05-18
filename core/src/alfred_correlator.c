/*======================================================================
 *
 * FILE: src/alfred_correlator.c
 *
 * CORE CORRELATION ENGINE
 *
 * PURPOSE:
 *   Transform low-level filesystem events into high-level semantic
 *   events usable by applications (indexers, backup systems, etc).
 *
 * ARCHITECTURE:
 *
 *   RAW INPUT (inotify / fanotify adapters)
 *              ↓
 *   INTERNAL STATE (tables.c)
 *              ↓
 *   CORRELATION LOGIC (THIS FILE)
 *              ↓
 *   HIGH LEVEL EVENTS (callback)
 *
 * KEY RESPONSIBILITY:
 *   - event merging
 *   - rename/move detection
 *   - debounce logic
 *   - lifecycle tracking (create → ready → modify → delete)
 *
 *======================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alfred_correlator.h"
#include "alfred_utils.h"
#include "alfred_tables.h"

/*======================================================================
 * DEFAULT CONFIGURATION
 *======================================================================*/

void alfred_config_default(alfred_config_t *cfg)
{
    if (!cfg)
        return;

    cfg->move_timeout_ms = 250;
    cfg->modify_debounce_ms = 50;
    cfg->create_ready_ms = 250;
}

/*======================================================================
 * ENGINE ALLOCATION
 *======================================================================*/

alfred_engine_t *alfred_create(
    const alfred_config_t *cfg,
    alfred_emit_fn emit_cb,
    void *userdata)
{
    alfred_engine_t *e;

    if (!cfg || !emit_cb)
        return NULL;

    e = calloc(1, sizeof(*e));

    if (!e)
        return NULL;

    e->cfg = *cfg;
    e->emit = emit_cb;
    e->userdata = userdata;
    e->seq = 0;

    return e;
}

/*======================================================================
 * DESTROY ENGINE
 *======================================================================*/

void alfred_destroy(alfred_engine_t *e)
{
    if (!e)
        return;

    alfred_tables_destroy(e);
    free(e);
}

/*======================================================================
 * INTERNAL EVENT EMITTER
 *======================================================================*/

static void emit(
    alfred_engine_t *e,
    alfred_event_type_t type,
    const char *src,
    const char *dst,
    int pid)
{
    alfred_event_t ev;

    ev.seq = ++e->seq;
    ev.ts_ns = alfred_now_ns();
    ev.type = type;
    ev.pid = pid;

    ev.src_path = src;
    ev.dst_path = dst;

    /*
     * Callback is user-controlled.
     * MUST be fast and non-blocking.
     */
    e->emit(&ev, e->userdata);
}

/*======================================================================
 * CLASSIFY MOVE TYPE
 *======================================================================*/

/*
 * Decide if operation is:
 *   - rename (same directory)
 *   - move   (different directory)
 *   - relocate (mixed case)
 */
static alfred_event_type_t classify_move(
    const char *src,
    const char *dst,
    int is_dir)
{
    char ps[4096];
    char pd[4096];

    alfred_parent_dir(src, ps, sizeof(ps));
    alfred_parent_dir(dst, pd, sizeof(pd));

    int same_parent = strcmp(ps, pd) == 0;
    int same_name = strcmp(
        alfred_basename_ptr(src),
        alfred_basename_ptr(dst)
    ) == 0;

    if (is_dir) {

        if (same_parent && !same_name)
            return ALFRED_EV_DIR_RENAMED;

        if (!same_parent && same_name)
            return ALFRED_EV_DIR_MOVED;

        return ALFRED_EV_DIR_RELOCATED;
    }

    if (same_parent && !same_name)
        return ALFRED_EV_FILE_RENAMED;

    if (!same_parent && same_name)
        return ALFRED_EV_FILE_MOVED;

    return ALFRED_EV_FILE_RELOCATED;
}

/*======================================================================
 * RAW PROCESSING ENTRY POINT
 *======================================================================*/

int alfred_process(
    alfred_engine_t *e,
    const alfred_raw_event_t *r)
{
    alfred_debounce_entry_t *d;
    alfred_move_entry_t *m;
    uint64_t now;

    if (!e || !r)
        return -1;

    now = r->ts_ns;

    /*
     * STEP 1: cleanup stale move pairs
     */
    alfred_move_sweep(e, now);

    /*==============================================================
     * OVERFLOW HANDLING
     *==============================================================*/

    if (r->mask & ALFRED_RAW_OVERFLOW) {

        emit(e, ALFRED_EV_OVERFLOW, NULL, NULL, r->pid);
        return 0;
    }

    /*==============================================================
     * CREATE EVENT
     *==============================================================*/

    if (r->mask & ALFRED_RAW_CREATE) {

        emit(
            e,
            (r->mask & ALFRED_RAW_ISDIR)
                ? ALFRED_EV_DIR_CREATED
                : ALFRED_EV_FILE_CREATED,
            r->path,
            NULL,
            r->pid
        );

        return 0;
    }

    /*==============================================================
     * CLOSE_WRITE => FILE_READY / FILE_MODIFIED
     *==============================================================*/

    if (r->mask & ALFRED_RAW_CLOSE_WRITE) {

        emit(e, ALFRED_EV_FILE_READY, r->path, NULL, r->pid);
        return 0;
    }

    /*==============================================================
     * MODIFY (DEBOUNCED)
     *==============================================================*/

    if (r->mask & ALFRED_RAW_MODIFY) {

        d = alfred_debounce_get(e, r->path);

        if (!alfred_debounce_should_emit(e, d, now))
            return 0;

        emit(e, ALFRED_EV_FILE_MODIFIED, r->path, NULL, r->pid);
        return 0;
    }

    /*==============================================================
     * DELETE
     *==============================================================*/

    if (r->mask & ALFRED_RAW_DELETE) {

        emit(
            e,
            (r->mask & ALFRED_RAW_ISDIR)
                ? ALFRED_EV_DIR_DELETED
                : ALFRED_EV_FILE_DELETED,
            r->path,
            NULL,
            r->pid
        );

        return 0;
    }

    /*==============================================================
     * MOVE_FROM (store pending)
     *==============================================================*/

    if (r->mask & ALFRED_RAW_MOVED_FROM) {

        alfred_move_insert(e, r);
        return 0;
    }

    /*==============================================================
     * MOVE_TO (correlate)
     *==============================================================*/

    if (r->mask & ALFRED_RAW_MOVED_TO) {

        m = alfred_move_take(e, r->cookie);

        /*
         * No matching FROM:
         * treat as creation (fallback safety)
         */
        if (!m) {

            emit(
                e,
                (r->mask & ALFRED_RAW_ISDIR)
                    ? ALFRED_EV_DIR_CREATED
                    : ALFRED_EV_FILE_CREATED,
                r->path,
                NULL,
                r->pid
            );

            return 0;
        }

        /*
         * CLASSIFY MOVE TYPE
         */
        alfred_event_type_t type =
            classify_move(m->src_path, r->path, m->is_dir);

        emit(e, type, m->src_path, r->path, r->pid);

        free(m->src_path);
        free(m);

        return 0;
    }

    return 0;
}

/*======================================================================
 * PERIODIC TICK
 *======================================================================*/

int alfred_tick(alfred_engine_t *e)
{
    if (!e)
        return -1;

    alfred_move_sweep(e, alfred_now_ns());

    return 0;
}

/*======================================================================
 * FLUSH
 *======================================================================*/

int alfred_flush(alfred_engine_t *e)
{
    /*
     * In full production system:
     *   flush pending debounce + move state
     *
     * Here simplified:
     */
    return alfred_tick(e);
}

/*======================================================================
 * EVENT NAME
 *======================================================================*/

const char *alfred_event_name(uint32_t type)
{
    switch (type) {

        case ALFRED_EV_FILE_CREATED: return "FILE_CREATED";
        case ALFRED_EV_FILE_READY: return "FILE_READY";
        case ALFRED_EV_FILE_MODIFIED: return "FILE_MODIFIED";
        case ALFRED_EV_FILE_DELETED: return "FILE_DELETED";

        case ALFRED_EV_DIR_CREATED: return "DIR_CREATED";
        case ALFRED_EV_DIR_DELETED: return "DIR_DELETED";

        case ALFRED_EV_FILE_RENAMED: return "FILE_RENAMED";
        case ALFRED_EV_FILE_MOVED: return "FILE_MOVED";
        case ALFRED_EV_FILE_RELOCATED: return "FILE_RELOCATED";

        case ALFRED_EV_DIR_RENAMED: return "DIR_RENAMED";
        case ALFRED_EV_DIR_MOVED: return "DIR_MOVED";
        case ALFRED_EV_DIR_RELOCATED: return "DIR_RELOCATED";

        case ALFRED_EV_OVERFLOW: return "OVERFLOW";
    }

    return "UNKNOWN";
}

/*======================================================================
 * VERSION STRING
 *======================================================================*/

const char *alfred_version_string(void)
{
    return "1.0.0";
}

/*======================================================================
 * END OF FILE
 *======================================================================*/
