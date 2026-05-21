/* ============================================================================
 * events.c - legacy inotify semantic dispatcher
 *
 * This file is retained only for shadow-mode comparison while Alfred switches
 * to the core event engine. It converts struct inotify_event records directly
 * into the historical event log format:
 *
 *   IN_CREATE      -> FILE_CREATED / DIR_CREATED
 *   IN_DELETE      -> FILE_DELETED / DIR_DELETED
 *   IN_MOVED_*     -> FILE_MOVED / FILE_RENAMED / ...
 *   IN_IGNORED     -> WATCH_REMOVED
 *   IN_Q_OVERFLOW  -> QUEUE_OVERFLOW
 *
 * This is no longer the target architecture. The official default path is:
 *
 *   inotify_backend.c -> alfred_raw_event_t -> core/src/alfred_correlator.c
 *
 * Important semantic difference: when both parent directory and basename
 * change, this legacy dispatcher emits MOVED and then RENAMED. The core emits
 * one RELOCATED event.
 * ========================================================================== */

#include "app.h"
#include "events.h"
#include "move_cache.h"
#include "utils.h"
#include "watch_manager.h"

#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>

/* ============================================================================
 * Legacy State
 * ========================================================================== */

/*
 * The legacy dispatcher keeps move correlation state as file-local globals
 * because it is no longer part of app_t. This limits the lifetime and ownership
 * of the old path while shadow mode remains available.
 */
static move_cache_t g_moves;
static int g_moves_initialized = 0;

/* ============================================================================
 * FORWARD DECLARATIONS
 * ========================================================================== */

static void handle_create(app_t *app,
                          const struct inotify_event *ev);

static void handle_delete(app_t *app,
                          const struct inotify_event *ev);

static void handle_move_from(app_t *app,
                             const struct inotify_event *ev);

static void handle_move_to(app_t *app,
                           const struct inotify_event *ev);

static void handle_ignored(app_t *app,
                           const struct inotify_event *ev);

static void handle_overflow(app_t *app);

static void emit_event(app_t *app,
                       event_type_t type,
                       const char *src,
                       const char *dst);

/* ============================================================================
 * EVENT TYPE STRING
 * ========================================================================== */

/*
 * event_type_str - render a legacy event type for the historical event log
 * @type: legacy EVT_* value
 *
 * Return: static event name string.
 */
const char* event_type_str(event_type_t type)
{
    switch (type) {

        case EVT_FILE_CREATED:   return "FILE_CREATED";
        case EVT_FILE_DELETED:   return "FILE_DELETED";
        case EVT_FILE_MOVED:     return "FILE_MOVED";
        case EVT_FILE_RENAMED:   return "FILE_RENAMED";

        case EVT_DIR_CREATED:    return "DIR_CREATED";
        case EVT_DIR_DELETED:    return "DIR_DELETED";
        case EVT_DIR_MOVED:      return "DIR_MOVED";
        case EVT_DIR_RENAMED:    return "DIR_RENAMED";

        case EVT_WATCH_REMOVED:  return "WATCH_REMOVED";
        case EVT_QUEUE_OVERFLOW: return "QUEUE_OVERFLOW";

        default:                 return "UNKNOWN";
    }
}

/* ============================================================================
 * LEGACY LIFECYCLE
 * ========================================================================== */

/*
 * legacy_events_init - initialize legacy move correlation state
 * @move_cache_size: number of legacy move slots to allocate
 *
 * Return: 0 on success, -1 on allocation failure.
 */
int legacy_events_init(size_t move_cache_size)
{
    if (move_cache_init(&g_moves, move_cache_size) != 0)
        return -1;

    g_moves_initialized = 1;
    return 0;
}

/*
 * legacy_events_shutdown - release legacy move correlation state
 */
void legacy_events_shutdown(void)
{
    if (!g_moves_initialized)
        return;

    move_cache_destroy(&g_moves);
    g_moves_initialized = 0;
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ========================================================================== */

/*
 * legacy_events_dispatch - classify one inotify event using legacy semantics
 * @app: application context used for watcher lookup and logging
 * @ev: inotify event read by the backend
 *
 * Shadow mode calls this after the same kernel event has already been offered
 * to the core path as alfred_raw_event_t. This function is kept to compare old
 * and new behavior, not to define the future event model.
 */
void legacy_events_dispatch(app_t *app,
                            const struct inotify_event *ev)
{
    if (app == NULL || ev == NULL)
        return;

    if (ev->mask & IN_Q_OVERFLOW) {
        handle_overflow(app);
        return;
    }

    if (ev->mask & IN_IGNORED) {
        handle_ignored(app, ev);
        return;
    }

    if (ev->mask & IN_MOVED_FROM) {
        handle_move_from(app, ev);
        return;
    }

    if (ev->mask & IN_MOVED_TO) {
        handle_move_to(app, ev);
        return;
    }

    if (ev->mask & IN_CREATE) {
        handle_create(app, ev);
        return;
    }

    if (ev->mask & IN_DELETE) {
        handle_delete(app, ev);
        return;
    }
}

/* ============================================================================
 * CREATE
 * ========================================================================== */

/*
 * handle_create - emit a legacy create event
 * @app: application context used for watcher lookup and logging
 * @ev: inotify create event
 */
static void handle_create(app_t *app,
                          const struct inotify_event *ev)
{
    const char *base =
        watcher_get_path(&app->inotify.watchers, ev->wd);

    if (base == NULL)
        return;

    char full[PATH_MAX];

    path_join(full,
              sizeof(full),
              base,
              ev->name);

    int is_dir =
        (ev->mask & IN_ISDIR) ? 1 : 0;

    emit_event(app,
        is_dir ? EVT_DIR_CREATED
               : EVT_FILE_CREATED,
        full,
        NULL);

}

/* ============================================================================
 * DELETE
 * ========================================================================== */

/*
 * handle_delete - emit a legacy delete event
 * @app: application context used for watcher lookup and logging
 * @ev: inotify delete event
 */
static void handle_delete(app_t *app,
                          const struct inotify_event *ev)
{
    const char *base =
        watcher_get_path(&app->inotify.watchers, ev->wd);

    if (base == NULL)
        return;

    char full[PATH_MAX];

    path_join(full,
              sizeof(full),
              base,
              ev->name);

    int is_dir =
        (ev->mask & IN_ISDIR) ? 1 : 0;

    emit_event(app,
        is_dir ? EVT_DIR_DELETED
               : EVT_FILE_DELETED,
        full,
        NULL);
}

/* ============================================================================
 * MOVE_FROM
 * Save first half
 * ========================================================================== */

/*
 * handle_move_from - store the first half of a legacy move pair
 * @app: unused application context
 * @ev: inotify MOVED_FROM event containing cookie and source name
 *
 * The source watch descriptor and basename are stored in move_cache. The core
 * has its own correlation state and does not use this cache.
 */
static void handle_move_from(app_t *app,
                             const struct inotify_event *ev)
{
    (void)app;

    if (!g_moves_initialized)
        return;

    move_cache_store(&g_moves,
                     ev->cookie,
                     ev->wd,
                     ev->name);
}

/* ============================================================================
 * MOVE_TO
 * Correlate with previous cookie
 * ========================================================================== */

/*
 * handle_move_to - correlate MOVED_TO with a cached MOVED_FROM
 * @app: application context used for watcher lookup and logging
 * @ev: inotify MOVED_TO event containing cookie and destination name
 *
 * This legacy implementation distinguishes rename and move by watch descriptor
 * and basename. If both directory and name changed, it emits two events. The
 * core intentionally maps that same raw pair to one RELOCATED event.
 */
static void handle_move_to(app_t *app,
                           const struct inotify_event *ev)
{
    move_slot_t *slot = NULL;

    if (g_moves_initialized)
        slot = move_cache_find(&g_moves, ev->cookie);

    const char *dst_base =
        watcher_get_path(&app->inotify.watchers,
                         ev->wd);

    if (dst_base == NULL)
        return;

    char dst[PATH_MAX];

    path_join(dst,
              sizeof(dst),
              dst_base,
              ev->name);

    /* ---------------------------------------------------------
     * No matching source => moved from outside watched area
     * treat as create
     * ------------------------------------------------------- */
    if (slot == NULL) {

        emit_event(app,
            (ev->mask & IN_ISDIR)
                ? EVT_DIR_CREATED
                : EVT_FILE_CREATED,
            dst,
            NULL);

        return;
    }

    const char *src_base =
        watcher_get_path(&app->inotify.watchers,
                         slot->src_wd);

    if (src_base == NULL)
        src_base = "";

    char src[PATH_MAX];

    path_join(src,
              sizeof(src),
              src_base,
              slot->src_name);

    int same_dir =
        (slot->src_wd == ev->wd);

    int same_name =
        (strcmp(slot->src_name,
                ev->name) == 0);

    int is_dir =
        (ev->mask & IN_ISDIR) ? 1 : 0;

    /* ---------------------------------------------------------
     * Rename inside same directory
     * ------------------------------------------------------- */
    if (same_dir && !same_name) {

        emit_event(app,
            is_dir ? EVT_DIR_RENAMED
                   : EVT_FILE_RENAMED,
            src,
            dst);
    }

    /* ---------------------------------------------------------
     * Move to another directory
     * ------------------------------------------------------- */
    else if (!same_dir) {

        emit_event(app,
            is_dir ? EVT_DIR_MOVED
                   : EVT_FILE_MOVED,
            src,
            dst);

        /* moved + renamed */
        if (!same_name) {

            emit_event(app,
                is_dir ? EVT_DIR_RENAMED
                       : EVT_FILE_RENAMED,
                src,
                dst);
        }
    }

    if (g_moves_initialized) {
        move_cache_clear(&g_moves,
                         ev->cookie);
    }
}

/* ============================================================================
 * WATCH REMOVED
 * ========================================================================== */

/*
 * handle_ignored - emit legacy WATCH_REMOVED and clear the watcher table
 * @app: application context containing backend watcher state
 * @ev: IN_IGNORED event
 *
 * WATCH_REMOVED is backend diagnostic output. It is not a core semantic event.
 */
static void handle_ignored(app_t *app,
                           const struct inotify_event *ev)
{
    const char *path =
        watcher_get_path(&app->inotify.watchers,
                         ev->wd);

    emit_event(app,
               EVT_WATCH_REMOVED,
               path ? path : "",
               NULL);

    watcher_remove(&app->inotify.watchers,
                   ev->wd);
}

/* ============================================================================
 * QUEUE OVERFLOW
 * ========================================================================== */

/*
 * handle_overflow - emit legacy queue overflow diagnostic
 * @app: application context used for logging
 */
static void handle_overflow(app_t *app)
{
    emit_event(app,
               EVT_QUEUE_OVERFLOW,
               NULL,
               NULL);
}

/* ============================================================================
 * CENTRALIZED EMITTER
 * ========================================================================== */

/*
 * emit_event - write one legacy event log record
 * @app: application context used for logging
 * @type: legacy event type
 * @src: source path, or NULL
 * @dst: destination path for move-style events, or NULL
 */
static void emit_event(app_t *app,
                       event_type_t type,
                       const char *src,
                       const char *dst)
{
    if (dst == NULL) {

        logger_event(&app->logger,
                     "%s path=%s",
                     event_type_str(type),
                     src ? src : "");
        return;
    }

    logger_event(&app->logger,
                 "%s from=%s to=%s",
                 event_type_str(type),
                 src ? src : "",
                 dst);
}
