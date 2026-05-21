/* ============================================================================
 * watch_manager.h - high-level inotify watch operations
 *
 * This module is backend state management, not event semantics. It adds and
 * removes kernel watches, stores the wd -> path mapping through watcher.c, and
 * can recursively discover directories that need watches.
 *
 * Recursive discovery may report already-existing child directories through a
 * callback. The callback consumer decides what to do with that fact; the watch
 * manager itself must not emit semantic FILE_* or DIR_* events.
 * ========================================================================== */

#ifndef WATCH_MANAGER_H
#define WATCH_MANAGER_H

#include <stdint.h>

struct app;

/*
 * watch_manager_discovered_dir_fn - report a directory found during recursion
 * @app: application context used during the current integration phase
 * @path: discovered directory path
 * @userdata: opaque pointer supplied by the caller
 *
 * The callback is used by the inotify backend to turn discovery facts into
 * synthetic raw directory-create events for the core.
 */
typedef void (*watch_manager_discovered_dir_fn)(
    struct app *app,
    const char *path,
    void *userdata
);

/*
 * watch_manager_add - add one inotify watch and store wd -> path
 * @app: application context containing backend state and configuration
 * @path: path to watch
 *
 * Return: inotify watch descriptor on success, -1 on failure.
 */
int watch_manager_add(struct app *app,
                      const char *path);

/*
 * watch_manager_remove - remove one watch and clear its table entry
 * @app: application context containing backend state
 * @wd: inotify watch descriptor to remove
 *
 * Return: 0 on success, -1 on invalid or unknown watch descriptor.
 */
int watch_manager_remove(struct app *app,
                         int wd);

/*
 * watch_manager_add_recursive - add watches for a directory tree
 * @app: application context containing backend state
 * @root: root directory to scan
 *
 * Adds a watch for @root and every nested directory currently visible.
 *
 * Return: 0 on success, -1 on failure.
 */
int watch_manager_add_recursive(struct app *app,
                                const char *root);

/*
 * watch_manager_add_recursive_with_discovery - recursive add with callbacks
 * @app: application context containing backend state
 * @root: root directory to scan
 * @on_discovered: optional callback for discovered child directories
 * @userdata: opaque pointer passed to @on_discovered
 *
 * The root is watched but not reported as discovered. Child directories found
 * during the recursive walk are reported after their watch is added. This lets
 * the backend repair missed create notifications from fast recursive mkdir
 * scenarios without moving semantic decisions into the watch manager.
 *
 * Return: 0 on success, -1 on failure.
 */
int watch_manager_add_recursive_with_discovery(
    struct app *app,
    const char *root,
    watch_manager_discovered_dir_fn on_discovered,
    void *userdata
);

/*
 * watch_manager_default_mask - return the default inotify subscription mask
 *
 * Return: OR-ed IN_* flags used by config_defaults().
 */
uint32_t watch_manager_default_mask(void);

#endif
