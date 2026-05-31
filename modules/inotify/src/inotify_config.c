/* ============================================================================
 * inotify_config.c - inotify backend configuration defaults
 * ========================================================================== */

#include "inotify_config.h"
#include "watch_manager.h"

#include <string.h>

/*
 * inotify_config_defaults - initialize inotify backend defaults
 * @cfg: inotify configuration object to initialize
 *
 * Defaults favor development visibility: recursive watching is enabled, the
 * watcher table starts small but can grow, and the watch mask comes from the
 * watch manager's backend-defined default.
 */
void inotify_config_defaults(inotify_config_t *cfg)
{
    if (cfg == NULL)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->recursive = 1;
    cfg->watcher_capacity = 128;
    cfg->watch_mask = watch_manager_default_mask();
}
