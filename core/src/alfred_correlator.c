/* ============================================================================
 * alfred_correlator.c - raw-to-semantic event correlation engine
 *
 * This file is the semantic center of Alfred. Backends feed ordered
 * alfred_raw_event_t records into alfred_process(); this engine emits stable
 * alfred_event_t records through the user callback.
 *
 * Responsibilities owned here:
 *
 *   - convert raw create/delete/modify/close-write facts into semantic events
 *   - debounce repeated modify events
 *   - correlate MOVED_FROM and MOVED_TO with the same cookie
 *   - classify rename, move, and relocated as distinct semantic outcomes
 *   - sweep stale pending move state on process/tick
 *
 * Responsibilities deliberately not owned here:
 *
 *   - reading inotify/fanotify descriptors
 *   - adding or removing watches
 *   - logging backend diagnostic events such as WATCH_ADDED
 *   - recovering the filesystem after queue overflow
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alfred_correlator.h"
#include "alfred_utils.h"
#include "alfred_tables.h"

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ========================================================================== */

/*
 * alfred_config_default - initialize core configuration defaults
 * @cfg: configuration object to initialize
 */
void alfred_config_default(alfred_config_t *cfg)
{
    if (!cfg)
        return;

    cfg->move_timeout_ms = 250;
    cfg->modify_debounce_ms = 50;
    cfg->create_ready_ms = 250;
}

/* ============================================================================
 * ENGINE ALLOCATION
 * ========================================================================== */

/*
 * alfred_create - allocate a semantic event engine
 * @cfg: configuration copied into the engine
 * @emit_cb: callback used for emitted semantic events
 * @userdata: opaque pointer passed to @emit_cb
 *
 * Return: allocated engine on success, NULL on invalid input or allocation
 * failure.
 */
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

/* ============================================================================
 * DESTROY ENGINE
 * ========================================================================== */

/*
 * alfred_destroy - release an engine and all pending correlation state
 * @e: engine to destroy, or NULL
 */
void alfred_destroy(alfred_engine_t *e)
{
    if (!e)
        return;

    alfred_tables_destroy(e);
    free(e);
}

/* ============================================================================
 * INTERNAL EVENT EMITTER
 * ========================================================================== */

/*
 * emit - build and publish one semantic event
 * @e: initialized engine
 * @type: ALFRED_EV_* semantic type
 * @src: source path, or NULL when not applicable
 * @dst: destination path for move-style events, or NULL
 * @pid: process id if known, otherwise 0
 *
 * The engine owns sequence numbering. Path pointers are valid only for the
 * duration of the callback, so users must copy them if they need persistence.
 */
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
     * The callback is user-controlled. It should be fast and non-blocking
     * because the engine calls it synchronously while processing raw input.
     */
    e->emit(&ev, e->userdata);
}

/* ============================================================================
 * CLASSIFY MOVE TYPE
 * ========================================================================== */

/*
 * classify_move - classify a correlated move pair
 * @src: original path from MOVED_FROM
 * @dst: destination path from MOVED_TO
 * @is_dir: nonzero when the moved object is a directory
 *
 * The core treats rename, move, and relocated as mutually exclusive semantic
 * outcomes. Legacy events.c used to emit MOVED plus RENAMED for the mixed case;
 * the core intentionally emits one RELOCATED event instead.
 *
 * Return: semantic ALFRED_EV_* move type.
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

/* ============================================================================
 * RAW PROCESSING ENTRY POINT
 * ========================================================================== */

/*
 * alfred_process - consume one raw event and emit zero or one semantic events
 * @e: initialized engine
 * @r: raw event produced by a backend or test harness
 *
 * Most raw events map immediately to one semantic event. MOVED_FROM is stored
 * until a matching MOVED_TO arrives or until a later sweep expires it. MODIFY
 * can emit no event when debounce suppresses a repeated modification.
 *
 * Return: 0 on success, -1 on invalid arguments.
 */
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
     * Sweep stale move pairs before processing the next raw event. This keeps
     * timeout behavior deterministic even if the caller does not tick while
     * events are flowing.
     */
    alfred_move_sweep(e, now);

    /* ========================================================================
     * OVERFLOW HANDLING
     * ====================================================================== */

    if (r->mask & ALFRED_RAW_OVERFLOW) {

        /*
         * Overflow is currently surfaced as a semantic diagnostic. A complete
         * resync/recovery policy is intentionally deferred until after the core
         * switch is complete.
         */
        emit(e, ALFRED_EV_OVERFLOW, NULL, NULL, r->pid);
        return 0;
    }

    /* ========================================================================
     * CREATE EVENT
     * ====================================================================== */

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

    /* ========================================================================
     * CLOSE_WRITE => FILE_READY
     * ====================================================================== */

    if (r->mask & ALFRED_RAW_CLOSE_WRITE) {

        emit(e, ALFRED_EV_FILE_READY, r->path, NULL, r->pid);
        return 0;
    }

    /* ========================================================================
     * MODIFY (DEBOUNCED)
     * ====================================================================== */

    if (r->mask & ALFRED_RAW_MODIFY) {

        d = alfred_debounce_get(e, r->path);

        if (!alfred_debounce_should_emit(e, d, now))
            return 0;

        emit(e, ALFRED_EV_FILE_MODIFIED, r->path, NULL, r->pid);
        return 0;
    }

    /* ========================================================================
     * DELETE
     * ====================================================================== */

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

    /* ========================================================================
     * MOVE_FROM (store pending)
     * ====================================================================== */

    if (r->mask & ALFRED_RAW_MOVED_FROM) {

        /*
         * MOVED_FROM alone is not yet semantic enough: it might become a
         * rename, a move, a relocated event, or eventually a timeout case.
         */
        alfred_move_insert(e, r);
        return 0;
    }

    /* ========================================================================
     * MOVE_TO (correlate)
     * ====================================================================== */

    if (r->mask & ALFRED_RAW_MOVED_TO) {

        m = alfred_move_take(e, r->cookie);

        /*
         * A MOVED_TO without its matching MOVED_FROM means the source was
         * outside the observed tree or the first half was lost. Treat it as a
         * create fallback so downstream users still learn that the object now
         * exists inside the watched area.
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

        alfred_event_type_t type =
            classify_move(m->src_path, r->path, m->is_dir);

        emit(e, type, m->src_path, r->path, r->pid);

        free(m->src_path);
        free(m);

        return 0;
    }

    return 0;
}

/* ============================================================================
 * PERIODIC TICK
 * ========================================================================== */

/*
 * alfred_tick - run time-based maintenance
 * @e: initialized engine
 *
 * Return: 0 on success, -1 on invalid arguments.
 */
int alfred_tick(alfred_engine_t *e)
{
    if (!e)
        return -1;

    alfred_move_sweep(e, alfred_now_ns());

    return 0;
}

/* ============================================================================
 * FLUSH
 * ========================================================================== */

/*
 * alfred_flush - flush pending state
 * @e: initialized engine
 *
 * The current implementation delegates to alfred_tick(). Future versions may
 * also flush debounce state or emit explicit timeout diagnostics.
 *
 * Return: 0 on success, -1 on invalid arguments.
 */
int alfred_flush(alfred_engine_t *e)
{
    return alfred_tick(e);
}

/* ============================================================================
 * EVENT NAME
 * ========================================================================== */

/*
 * alfred_event_name - convert an event type to its log/display name
 * @type: ALFRED_EV_* value
 *
 * Return: static string for @type, or "UNKNOWN".
 */
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

/* ============================================================================
 * VERSION STRING
 * ========================================================================== */

/*
 * alfred_version_string - return the core API version string
 *
 * Return: static semantic version string.
 */
const char *alfred_version_string(void)
{
    return "1.0.0";
}

/* ============================================================================
 * END OF FILE
 * ========================================================================== */
