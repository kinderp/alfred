/* ============================================================================
 * src/watch_manager.c
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

int watch_manager_add(app_t *app,
                      const char *path)
{
    if (app == NULL || path == NULL)
        return -1;

    int wd =
        inotify_add_watch(app->inotify_fd,
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

    if (watcher_store(&app->watchers,
                      wd,
                      path) != 0) {

        logger_error(&app->logger,
                     "watcher_store failed wd=%d path=%s",
                     wd,
                     path);

        inotify_rm_watch(app->inotify_fd,
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

int watch_manager_remove(app_t *app,
                         int wd)
{
    if (app == NULL)
        return -1;

    const char *path =
        watcher_get_path(&app->watchers,
                         wd);

    if (path == NULL)
        return -1;

    inotify_rm_watch(app->inotify_fd,
                     wd);

    watcher_remove(&app->watchers,
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

int watch_manager_add_recursive(app_t *app,
                                const char *root)
{
    if (app == NULL || root == NULL)
        return -1;

    return recursive_walk(app, root, NULL, NULL, 0);
}

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
