/* ============================================================================
 * app.h - application runtime state and lifecycle API
 *
 * This header defines the process-wide application context used by alfred.
 * The application layer currently owns configuration, logging, core state, and
 * a transitional inotify backend context.
 *
 * app_t is still a practical orchestration object: it wires configuration,
 * logging, the inotify backend, and the core together. The semantic state is
 * already owned by the core engine; what remains transitional is that the
 * inotify backend context is embedded here and backend functions still receive
 * app_t so they can reach configuration, logging, and the core callback.
 * ============================================================================
 */

#ifndef APP_H
#define APP_H

#include "config.h"
#include "alfred_correlator.h"
#include "core_logger.h"
#include "inotify_backend.h"
#include "logger.h"

/*
 * app_t - process-wide runtime context
 *
 * app_t is the ownership root for the running process. Initialization builds
 * each subsystem into this object, app_run() uses it while polling inotify,
 * and app_shutdown() releases resources in the reverse direction.
 *
 * The current design still embeds inotify_backend_t here. That is a temporary
 * integration boundary while the backend still needs app-level configuration
 * and logging.
 */
typedef struct app {

    /*
     * Cooperative shutdown flag. Signal handlers clear this flag instead of
     * doing work directly in signal context.
     */
    int running;

    /* Application configuration loaded before subsystem initialization. */
    config_t config;

    /*
     * inotify backend state. This now owns the nonblocking descriptor and the
     * watch descriptor table, though the backend still receives app_t during
     * the transition so it can use config and logger.
     */
    inotify_backend_t inotify;

    /* Raw, semantic, and error log sink. */
    logger_t logger;

    /*
     * Core correlator configuration, callback context, and engine.
     *
     * These fields are the semantic side of the runtime. The backend never
     * writes them directly: it delivers alfred_raw_event_t records through the
     * app callback, then alfred_process() updates the private engine state and
     * emits semantic events through core_logger_context.
     */
    alfred_config_t core_config;
    core_logger_context_t core_logger_context;
    alfred_engine_t *core;

} app_t;

/*
 * app_init - initialize the application runtime
 * @app: application context to initialize
 * @argc: command-line argument count
 * @argv: command-line argument vector
 *
 * Initializes configuration, logging, core state, the inotify backend, and
 * signal handling. Startup watch paths are read from @argv.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int app_init(app_t *app, int argc, char **argv);

/*
 * app_run - execute the main event loop
 * @app: initialized application context
 *
 * Polls the active inotify backend and dispatches resulting raw events to the
 * core.
 *
 * Return: ERR_OK on normal termination, a negative error_t value on invalid
 * input.
 */
int app_run(app_t *app);

/*
 * app_shutdown - release application resources
 * @app: application context to release
 *
 * Releases resources owned by app_t. The function is safe to call with NULL.
 */
void app_shutdown(app_t *app);

#endif
