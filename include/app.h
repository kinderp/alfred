/**
 * FILENAME: app.c
 * 
 * Application Context.
 * 
 * All runtime state in a central struct.
 * 
 */

#ifndef APP_H
#define APP_H

#include "config.h"
#include "watcher.h"
#include "logger.h"
#include "move_cache.h"

typedef struct {

    int running;
    int inotify_fd;

    config_t config;
    watcher_table_t watchers;
    logger_t logger;
    move_cache_t moves;

} app_t;

int app_init(app_t *app, int argc, char **argv);
int app_run(app_t *app);
void app_shutdown(app_t *app);

#endif
