/* ============================================================================
 * inotify_config.h - inotify backend configuration
 *
 * This header groups the user-tunable values that belong specifically to the
 * Linux inotify backend. The application still owns the top-level config_t, but
 * backend code should receive only this narrower configuration view.
 * ========================================================================== */

#ifndef INOTIFY_CONFIG_H
#define INOTIFY_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/*
 * inotify_config_t - static configuration for the inotify backend
 * @recursive: add watches recursively for directory startup paths and repairs
 * @watcher_capacity: initial capacity for the wd -> path watcher table
 * @watch_mask: IN_* subscription mask passed to inotify_add_watch()
 *
 * This struct contains policy/configuration, not runtime state. The runtime
 * descriptor and watcher table remain in inotify_backend_t.
 */
typedef struct inotify_config {
    int recursive;
    size_t watcher_capacity;
    uint32_t watch_mask;
} inotify_config_t;

/*
 * inotify_config_defaults - initialize inotify backend defaults
 * @cfg: inotify configuration object to initialize
 *
 * Return: nothing. NULL input is ignored.
 */
void inotify_config_defaults(inotify_config_t *cfg);

/*
 * inotify_config_set_watch_mask - parse and assign a textual IN_* mask
 * @cfg: inotify configuration object to update
 * @value: comma-separated mask expression
 *
 * Supported forms:
 *
 *   default
 *   default,-IN_ATTRIB
 *   default,+IN_Q_OVERFLOW
 *   IN_CREATE,IN_DELETE,IN_MODIFY
 *
 * Only flags that Alfred can currently render in raw logs and convert into
 * Alfred raw masks are accepted. Unknown or unsupported tokens are rejected so
 * typos and premature configuration do not silently change the observed
 * filesystem contract.
 *
 * Return: 0 on success, -1 on invalid input.
 */
int inotify_config_set_watch_mask(inotify_config_t *cfg, const char *value);

#endif /* INOTIFY_CONFIG_H */
