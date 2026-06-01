/* ============================================================================
 * watch_manager.h - high-level inotify watch operations
 *
 * This module is backend state management, not event semantics. It adds and
 * removes kernel watches, stores the wd -> path mapping through watcher.c, and
 * can recursively install watches below an existing startup root.
 *
 * WATCH_ADDED and WATCH_REMOVED are diagnostics about the watch table, not core
 * events. Runtime scanner discovery lives in the inotify backend because that
 * is where synthetic raw-event recovery policy belongs. The watch manager
 * itself must not emit raw or semantic FILE_* or DIR_* events.
 * ========================================================================== */

#ifndef WATCH_MANAGER_H
#define WATCH_MANAGER_H

#include "inotify_backend.h"

#include <stdint.h>

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
 * watch_manager_default_mask - return the default inotify subscription mask
 *
 * Return: OR-ed IN_* flags used by config_defaults().
 */
uint32_t watch_manager_default_mask(void);

#endif
