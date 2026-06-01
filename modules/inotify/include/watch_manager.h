/* ============================================================================
 * watch_manager.h - high-level inotify watch operations
 *
 * This module is backend state management, not event semantics. It adds and
 * removes kernel watches, stores the wd -> path mapping through watcher.c, and
 * can recursively discover directories that need watches.
 *
 * WATCH_ADDED and WATCH_REMOVED are diagnostics about the watch table, not core
 * events. Runtime scanner discovery now lives in the inotify backend. The
 * callback-based recursive discovery API remains temporarily available while
 * the migration is cleaned up, but the watch manager itself must not emit raw
 * or semantic FILE_* or DIR_* events.
 * ========================================================================== */

#ifndef WATCH_MANAGER_H
#define WATCH_MANAGER_H

#include "inotify_backend.h"

#include <stdint.h>

/*
 * watch_manager_discovered_dir_fn - report a directory found during recursion
 * @ctx: backend context used while recursive discovery is running
 * @path: discovered directory path
 * @userdata: opaque pointer supplied by the caller
 *
 * The callback reports observation facts, not semantic decisions. This
 * transitional callback API is kept until the old recursive walk is removed.
 */
typedef void (*watch_manager_discovered_dir_fn)(
    inotify_backend_context_t *ctx,
    const char *path,
    void *userdata
);

/*
 * watch_manager_add - add one inotify watch and store wd -> path
 * @ctx: borrowed backend context containing runtime, config, and logger
 * @path: path to watch
 *
 * Return: inotify watch descriptor on success, -1 on failure.
 */
int watch_manager_add(inotify_backend_context_t *ctx,
                      const char *path);

/*
 * watch_manager_remove - remove one watch and clear its table entry
 * @ctx: borrowed backend context containing runtime and logger
 * @wd: inotify watch descriptor to remove
 *
 * Return: 0 on success, -1 on invalid or unknown watch descriptor.
 */
int watch_manager_remove(inotify_backend_context_t *ctx,
                         int wd);

/*
 * watch_manager_add_recursive - add watches for a directory tree
 * @ctx: borrowed backend context containing runtime, config, and logger
 * @root: root directory to scan
 *
 * Adds a watch for @root and every nested directory currently visible.
 *
 * Return: 0 on success, -1 on failure.
 */
int watch_manager_add_recursive(inotify_backend_context_t *ctx,
                                const char *root);

/*
 * watch_manager_add_recursive_with_discovery - recursive add with callbacks
 * @ctx: borrowed backend context containing runtime, config, and logger
 * @root: root directory to scan
 * @on_discovered: optional callback for discovered child directories
 * @userdata: opaque pointer passed to @on_discovered
 *
 * Transitional API kept while the scanner migration is completed. The root is
 * watched but not reported as discovered. Child directories found during the
 * recursive walk are reported after their watch is added. Callers decide what
 * to do with those facts.
 *
 * Return: 0 on success, -1 on failure.
 */
int watch_manager_add_recursive_with_discovery(
    inotify_backend_context_t *ctx,
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
