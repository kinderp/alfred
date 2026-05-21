/* ============================================================================
 * app.h - application runtime state and lifecycle API
 *
 * This header defines the process-wide application context used by alfred.
 * The application layer currently owns configuration, logging, inotify state,
 * watcher tables, and the move cache used by the inotify event dispatcher.
 *
 * TODO(core-integration): move semantic event state out of app_t once the
 * inotify module emits raw events directly into the core correlator.
 * ============================================================================
 */

#ifndef APP_H
#define APP_H

#include "config.h"
#include "alfred_correlator.h"
#include "core_logger.h"
#include "watcher.h"
#include "logger.h"
#include "move_cache.h"

/*
 * app_t - process-wide runtime context
 *
 * app_t is the ownership root for the running process. Initialization builds
 * each subsystem into this object, app_run() uses it while polling inotify,
 * and app_shutdown() releases resources in the reverse direction.
 *
 * The current design still stores inotify-specific state here. That is a
 * temporary integration boundary: a future backend interface should hide
 * backend state behind a module-owned context.
 */
typedef struct app {

    /*
     * Cooperative shutdown flag. Signal handlers clear this flag instead of
     * doing work directly in signal context.
     */
    int running;

    /*
     * Nonblocking inotify descriptor owned by app_t. It is initialized in
     * app_init(), consumed by app_run(), and closed by app_shutdown().
     */
    int inotify_fd;

    /* Application configuration loaded before subsystem initialization. */
    config_t config;

    /* inotify watch descriptor to path mapping. */
    watcher_table_t watchers;

    /* Raw, semantic, and error log sink. */
    logger_t logger;

    /*
     * Core correlator configuration and engine.
     *
     * During integration the engine can run in shadow mode or official core
     * mode. In both cases the core receives raw events; the selected event
     * engine mode decides how core_logger_context formats them.
     */
    alfred_config_t core_config;
    core_logger_context_t core_logger_context;
    alfred_engine_t *core;

    /*
     * Temporary move correlation cache used by the current inotify dispatcher.
     * TODO(core-integration): this belongs in the core correlator.
     */
    move_cache_t moves;

} app_t;

/*
 * app_init - initialize the application runtime
 * @app: application context to initialize
 * @argc: command-line argument count
 * @argv: command-line argument vector
 *
 * Initializes configuration, logging, watcher state, move cache, inotify, and
 * signal handling. Startup watch paths are read from @argv.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int app_init(app_t *app, int argc, char **argv);

/*
 * app_run - execute the main event loop
 * @app: initialized application context
 *
 * Reads packed inotify events from the nonblocking descriptor and dispatches
 * them to the current inotify event handler.
 *
 * Return: ERR_OK on normal termination, a negative error_t value on invalid
 * input.
 */
int app_run(app_t *app);

/*
 * app_process_synthetic_dir_create - feed a discovered directory to the core
 * @app: application context owning the core instance
 * @path: full path of the directory discovered by backend scanning
 *
 * Used by recursive watch discovery to recover DIR_CREATED events that the
 * kernel could not deliver because a child directory was created before its
 * parent watch was installed.
 *
 * Return: 0 on success, -1 on invalid input or core processing failure.
 */
int app_process_synthetic_dir_create(app_t *app, const char *path);

/*
 * app_shutdown - release application resources
 * @app: application context to release
 *
 * Releases resources owned by app_t. The function is safe to call with NULL.
 */
void app_shutdown(app_t *app);

#endif
