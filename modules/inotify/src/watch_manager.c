/* ============================================================================
 * watch_manager.c - inotify watch installation and recursive discovery
 *
 * This file owns the operations that mutate the backend watcher table:
 *
 *   inotify_add_watch() -> watcher_store()
 *   inotify_rm_watch()  -> watcher_remove()
 *
 * It deliberately does not classify filesystem events. WATCH_ADDED and
 * WATCH_REMOVED are backend diagnostics. Recursive discovery reports directory
 * facts to the backend through a callback; the core later decides whether those
 * facts become DIR_CREATED events.
 * ========================================================================== */

#include "watch_manager.h"
#include "app.h"
#include "watcher.h"
#include "logger.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <sys/inotify.h>

/* ============================================================================
 * DEFAULT WATCH MASK
 * ========================================================================== */

/*
 * watch_manager_default_mask - return the default inotify subscription mask
 *
 * The mask includes creation/deletion, modify/close-write, move pairs, watch
 * removal diagnostics, and queue overflow. It is stored in config_t so future
 * configuration can override it without changing watch_manager_add().
 *
 * Return: OR-ed IN_* mask flags.
 */
uint32_t watch_manager_default_mask(void)
{
    return
        IN_CREATE      |
        IN_DELETE      |
        IN_MODIFY      |
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
 * @app: application context containing inotify backend state
 * @path: filesystem path to watch
 *
 * On success the kernel returns a watch descriptor. The descriptor is then used
 * as an index into watcher_table_t, where watcher_store() saves the path needed
 * later to reconstruct full event paths from inotify records.
 *
 * Return: watch descriptor on success, -1 on failure.
 */
int watch_manager_add(app_t *app,
                      const char *path)
{
    if (app == NULL || path == NULL)
        return -1;

    int wd =
        inotify_add_watch(app->inotify.fd,
                          path,
                          app->config.watch_mask);

    if (wd < 0) {

        logger_error(&app->logger,
                     "inotify_add_watch failed path=%s errno=%d (%s)",
                     path,
                     errno,
                     strerror(errno));

        return -1;
    }

    if (watcher_store(&app->inotify.watchers,
                      wd,
                      path) != 0) {

        logger_error(&app->logger,
                     "watcher_store failed wd=%d path=%s",
                     wd,
                     path);

        inotify_rm_watch(app->inotify.fd,
                         wd);

        return -1;
    }

    logger_event(&app->logger,
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
 * @app: application context containing inotify backend state
 * @wd: watch descriptor to remove
 *
 * The path is looked up before table removal so the diagnostic log can still
 * identify which watched directory disappeared.
 *
 * Return: 0 on success, -1 when @wd is not known.
 */
int watch_manager_remove(app_t *app,
                         int wd)
{
    if (app == NULL)
        return -1;

    const char *path =
        watcher_get_path(&app->inotify.watchers,
                         wd);

    if (path == NULL)
        return -1;

    inotify_rm_watch(app->inotify.fd,
                     wd);

    watcher_remove(&app->inotify.watchers,
                   wd);

    logger_event(&app->logger,
                 "WATCH_REMOVED wd=%d path=%s",
                 wd,
                 path);

    return 0;
}

/* ============================================================================
 * INTERNAL RECURSIVE WALK
 * ========================================================================== */

/*
 * recursive_walk - add watches for a directory tree
 * @app: application context containing backend state
 * @root: directory currently being scanned
 * @on_discovered: optional callback for discovered directories
 * @userdata: opaque callback data
 * @notify_root: nonzero to report @root through @on_discovered
 *
 * The root passed by the public entry point is not reported because its real
 * inotify create event is already being processed by the backend. Nested
 * directories are reported because they may have been created before a watch
 * existed on their parent, which means the kernel could not deliver their
 * create event to Alfred.
 *
 * Return: 0 on success, -1 on opendir failure.
 */
static int recursive_walk(app_t *app,
                          const char *root,
                          watch_manager_discovered_dir_fn on_discovered,
                          void *userdata,
                          int notify_root)
{
    DIR *dir = opendir(root);

    if (dir == NULL) {

        logger_error(&app->logger,
                     "opendir failed path=%s",
                     root);

        return -1;
    }

    /*
     * Add the watch before reporting discovery. This ordering means a synthetic
     * raw create emitted by the backend describes a directory that is already
     * watched for subsequent changes.
     */
    watch_manager_add(app, root);

    if (notify_root && on_discovered != NULL) {
        on_discovered(app, root, userdata);
    }

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {

        if (strcmp(ent->d_name, ".") == 0)
            continue;

        if (strcmp(ent->d_name, "..") == 0)
            continue;

        if (ent->d_type != DT_DIR)
            continue;

        char child[PATH_MAX];

        if (path_join(child,
                      sizeof(child),
                      root,
                      ent->d_name) != 0) {

            continue;
        }

        recursive_walk(app,
                       child,
                       on_discovered,
                       userdata,
                       1);
    }

    closedir(dir);

    return 0;
}

/* ============================================================================
 * ADD RECURSIVE
 * ========================================================================== */

/*
 * watch_manager_add_recursive - add watches below a root directory
 * @app: application context containing backend state
 * @root: root directory to scan
 *
 * Used for startup recursive watching, where discovered children already exist
 * before Alfred starts and should not be emitted as create events.
 *
 * Return: 0 on success, -1 on failure.
 */
int watch_manager_add_recursive(app_t *app,
                                const char *root)
{
    if (app == NULL || root == NULL)
        return -1;

    return recursive_walk(app, root, NULL, NULL, 0);
}

/*
 * watch_manager_add_recursive_with_discovery - add watches and report children
 * @app: application context containing backend state
 * @root: newly created directory to scan
 * @on_discovered: callback for nested directories found during scanning
 * @userdata: opaque callback data
 *
 * Used after an inotify IN_CREATE|IN_ISDIR event while recursive mode is active.
 * The backend already has the real create event for @root. The discovery
 * callback reports nested directories that may have been missed in fast
 * recursive creation scenarios such as `mkdir -p one/two/three`.
 *
 * Return: 0 on success, -1 on failure.
 */
int watch_manager_add_recursive_with_discovery(
    app_t *app,
    const char *root,
    watch_manager_discovered_dir_fn on_discovered,
    void *userdata)
{
    if (app == NULL || root == NULL)
        return -1;

    return recursive_walk(app, root, on_discovered, userdata, 0);
}
