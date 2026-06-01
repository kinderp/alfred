/* ============================================================================
 * watch_manager.c - inotify watch installation
 *
 * This file owns the operations that mutate the backend watcher table:
 *
 *   inotify_add_watch() -> watcher_store()
 *   inotify_rm_watch()  -> watcher_remove()
 *
 * It deliberately does not classify filesystem events. WATCH_ADDED and
 * WATCH_REMOVED are backend diagnostics about Alfred's observation state, not
 * semantic filesystem events. Startup recursive watching consumes fs_scanner
 * directory facts to install watches. Runtime recursive discovery also uses
 * fs_scanner, but its synthetic raw-event policy stays in the backend.
 * ========================================================================== */

#include "watch_manager.h"
#include "fs_scanner.h"
#include "watcher.h"
#include "logger.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <limits.h>
#include <sys/inotify.h>

/* ============================================================================
 * INTERNAL HELPERS
 * ========================================================================== */

typedef struct watch_manager_scan_context {
    inotify_backend_context_t *ctx;
    int failed;
} watch_manager_scan_context_t;

/*
 * watch_manager_context_is_valid - validate borrowed backend dependencies
 * @ctx: context to inspect
 *
 * Return: nonzero when all watch-manager dependencies are available.
 */
static int watch_manager_context_is_valid(
    const inotify_backend_context_t *ctx)
{
    return ctx != NULL &&
           ctx->runtime != NULL &&
           ctx->config != NULL &&
           ctx->logger != NULL;
}

/*
 * watch_manager_add_scanned_dir - add a watch for one scanner directory fact
 * @entry: filesystem entry reported by fs_scan_tree()
 * @userdata: watch_manager_scan_context_t used for watch installation
 *
 * Startup recursive watching needs no raw synthetic events: all directories
 * already exist before the event loop starts. The scanner only reports facts;
 * this adapter turns directory facts into inotify watches and records whether
 * watch installation failed.
 *
 * Return: 0 to keep scanning, nonzero to stop after a watch failure.
 */
static int watch_manager_add_scanned_dir(const fs_scan_entry_t *entry,
                                         void *userdata)
{
    watch_manager_scan_context_t *scan_ctx =
        (watch_manager_scan_context_t *)userdata;

    if (scan_ctx == NULL || scan_ctx->ctx == NULL ||
        entry == NULL || entry->type != FS_SCAN_DIR) {
        return 0;
    }

    if (watch_manager_add(scan_ctx->ctx, entry->path) < 0) {
        scan_ctx->failed = 1;
        return 1;
    }

    return 0;
}

/* ============================================================================
 * DEFAULT WATCH MASK
 * ========================================================================== */

/*
 * watch_manager_default_mask - return the default inotify subscription mask
 *
 * The mask includes creation/deletion, modify/attrib/close-write, move pairs,
 * watch removal diagnostics, and queue overflow. It is stored in config_t so
 * future configuration can override it without changing watch_manager_add().
 *
 * Return: OR-ed IN_* mask flags.
 */
uint32_t watch_manager_default_mask(void)
{
    return
        IN_CREATE      |
        IN_DELETE      |
        IN_MODIFY      |
        IN_ATTRIB      |
        IN_CLOSE_WRITE |
        IN_MOVED_FROM  |
        IN_MOVED_TO    |
        IN_DELETE_SELF |
        IN_IGNORED     |
        IN_Q_OVERFLOW;
}

/* ============================================================================
 * ADD SINGLE WATCH
 * ========================================================================== */

/*
 * watch_manager_add - add one kernel watch and store its path mapping
 * @ctx: borrowed backend context containing runtime, config, and logger
 * @path: filesystem path to watch
 *
 * On success the kernel returns a watch descriptor. The descriptor is then used
 * as an index into watcher_table_t, where watcher_store() saves the path needed
 * later to reconstruct full event paths from inotify records.
 *
 * The WATCH_ADDED log documents that Alfred can now observe @path. It is not a
 * substitute for DIR_CREATED and must not be consumed as a semantic core event.
 *
 * Return: watch descriptor on success, -1 on failure.
 */
int watch_manager_add(inotify_backend_context_t *ctx,
                      const char *path)
{
    if (!watch_manager_context_is_valid(ctx) || path == NULL)
        return -1;

    int wd =
        inotify_add_watch(ctx->runtime->fd,
                          path,
                          ctx->config->watch_mask);

    if (wd < 0) {

        logger_error(ctx->logger,
                     "inotify_add_watch failed path=%s errno=%d (%s)",
                     path,
                     errno,
                     strerror(errno));

        return -1;
    }

    if (watcher_store(&ctx->runtime->watchers,
                      wd,
                      path) != 0) {

        logger_error(ctx->logger,
                     "watcher_store failed wd=%d path=%s",
                     wd,
                     path);

        inotify_rm_watch(ctx->runtime->fd,
                         wd);

        return -1;
    }

    logger_event(ctx->logger,
                 "WATCH_ADDED wd=%d path=%s",
                 wd,
                 path);

    return wd;
}

/* ============================================================================
 * REMOVE WATCH
 * ========================================================================== */

/*
 * watch_manager_remove - remove one kernel watch and clear its table slot
 * @ctx: borrowed backend context containing runtime and logger
 * @wd: watch descriptor to remove
 *
 * The path is looked up before table removal so the diagnostic log can still
 * identify which watched directory disappeared.
 *
 * The WATCH_REMOVED log documents that Alfred stopped observing a path. The
 * delete or move semantics, if any, come from the inotify event stream and the
 * core, not from this watch-table cleanup.
 *
 * Return: 0 on success, -1 when @wd is not known.
 */
int watch_manager_remove(inotify_backend_context_t *ctx,
                         int wd)
{
    if (!watch_manager_context_is_valid(ctx))
        return -1;

    const char *path =
        watcher_get_path(&ctx->runtime->watchers,
                         wd);

    if (path == NULL)
        return -1;

    char removed_path[PATH_MAX];

    snprintf(removed_path,
             sizeof(removed_path),
             "%s",
             path);

    inotify_rm_watch(ctx->runtime->fd,
                     wd);

    watcher_remove(&ctx->runtime->watchers,
                   wd);

    logger_event(ctx->logger,
                 "WATCH_REMOVED wd=%d path=%s",
                 wd,
                 removed_path);

    return 0;
}

/* ============================================================================
 * ADD RECURSIVE
 * ========================================================================== */

/*
 * watch_manager_add_recursive - add watches below a root directory
 * @ctx: borrowed backend context containing runtime, config, and logger
 * @root: root directory to scan
 *
 * Used for startup recursive watching, where directories already exist before
 * Alfred starts and must not be emitted as create events. The filesystem walk
 * is delegated to fs_scan_tree(); this function only adapts scanner directory
 * facts into watch_manager_add() calls.
 *
 * Return: 0 on success, -1 on failure.
 */
int watch_manager_add_recursive(inotify_backend_context_t *ctx,
                                const char *root)
{
    if (!watch_manager_context_is_valid(ctx) || root == NULL)
        return -1;

    fs_scan_options_t opts;

    fs_scan_options_defaults(&opts);

    watch_manager_scan_context_t scan_ctx;

    scan_ctx.ctx = ctx;
    scan_ctx.failed = 0;

    error_t rc =
        fs_scan_tree(root,
                     &opts,
                     watch_manager_add_scanned_dir,
                     &scan_ctx);

    if (rc != ERR_OK || scan_ctx.failed)
        return -1;

    return 0;
}
