/* ============================================================================
 * app.c - application lifecycle and inotify event loop
 *
 * This file owns process-level initialization, signal handling, backend
 * polling orchestration, and shutdown ordering. Backend-specific inotify reads
 * are delegated to modules/inotify.
 *
 * TODO(core-integration): move remaining inotify-specific state out of app_t
 * once the backend owns a dedicated runtime context.
 * ============================================================================
 */

#include "app.h"
#include "core_logger.h"
#include "errors.h"
#include "inotify_backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* ============================================================================
 * Local Declarations
 * ========================================================================== */

static void setup_signals(void);
static void handle_signal(int sig);
static int handle_backend_event(app_t *app,
                                const struct inotify_event *ev,
                                const alfred_raw_event_t *raw,
                                void *userdata);

/*
 * TODO(core-integration): this function is implemented by the current inotify
 * semantic dispatcher. It should disappear from app.c when the backend emits
 * alfred_raw_event_t records into the core.
 */
extern void app_dispatch_raw_event(app_t *app,
                                   const struct inotify_event *ev);

/* ============================================================================
 * Signal State
 * ========================================================================== */

/*
 * Signal handlers cannot receive user data, so the application context is
 * exposed through a single file-local pointer. The handler only clears a flag;
 * all resource cleanup remains in normal process context.
 */
static app_t *g_app = NULL;

/* ============================================================================
 * Signal Handling
 * ========================================================================== */

/*
 * handle_signal - request cooperative shutdown from a signal handler
 * @sig: received signal number
 *
 * The handler intentionally avoids logging, allocation, locking, or closing
 * descriptors. It only clears app->running so the event loop can terminate
 * safely in normal execution context.
 */
static void handle_signal(int sig)
{
    (void)sig;

    if (g_app != NULL) {
        g_app->running = 0;
    }
}

/*
 * setup_signals - install process termination handlers
 *
 * SIGINT and SIGTERM are converted into cooperative shutdown requests. The
 * main event loop observes app->running and exits cleanly.
 */
static void setup_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;

    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* ============================================================================
 * Core Dispatch
 * ========================================================================== */

/*
 * handle_backend_event - consume one backend event
 * @app: initialized application context
 * @ev: optional original inotify event for legacy shadow dispatch
 * @raw: optional raw event for the core
 * @userdata: unused callback context
 *
 * The backend owns inotify parsing and raw conversion. The app only forwards
 * raw events to the core and keeps the legacy dispatcher alive in shadow mode
 * while events.c is still available for comparison.
 */
static int handle_backend_event(app_t *app,
                                const struct inotify_event *ev,
                                const alfred_raw_event_t *raw,
                                void *userdata)
{
    (void)userdata;

    if (app == NULL)
        return ERR_INVALID_ARG;

    if (raw != NULL && app->core != NULL) {
        if (alfred_process(app->core, raw) != 0) {
            logger_error(&app->logger,
                         "core failed to process raw event");
        }
    }

    if (ev != NULL &&
        app->config.event_engine_mode == EVENT_ENGINE_SHADOW) {
        app_dispatch_raw_event(app, ev);
    }

    return ERR_OK;
}

/* ============================================================================
 * Application Lifecycle
 * ========================================================================== */

/*
 * app_init - initialize the application runtime
 * @app: application context to initialize
 * @argc: command-line argument count
 * @argv: command-line argument vector
 *
 * Initializes all runtime subsystems in dependency order: configuration,
 * logging, watcher state, move cache, inotify, signal handling, and startup
 * watches. A single failure path delegates cleanup to app_shutdown().
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int app_init(app_t *app, int argc, char **argv)
{
    error_t error = ERR_UNKNOWN;

    if (app == NULL) {
        error = ERR_INVALID_ARG;
        goto fail;
    }

    memset(app, 0, sizeof(*app));

    app->running = 1;
    app->inotify.fd = -1;

    /*
     * Defaults are loaded before any subsystem initialization so later steps
     * can rely on configured paths, capacities, and watch masks.
     */
    config_defaults(&app->config);

    /*
     * Temporary integration override. config_load() can parse event_engine, but
     * startup does not yet accept a config file path, so the environment keeps
     * core-mode testing possible without changing the CLI contract.
     */
    const char *event_engine_env = getenv("ALFRED_EVENT_ENGINE");
    if (event_engine_env != NULL &&
        config_set_event_engine(&app->config, event_engine_env) != ERR_OK) {

        fprintf(stderr,
                "invalid ALFRED_EVENT_ENGINE=%s (expected shadow or core)\n",
                event_engine_env);
        error = ERR_INVALID_ARG;
        goto fail;
    }

    /*
     * The logger is initialized early because every following subsystem uses
     * it for diagnostics. Failures before this point must use stderr.
     */
    if (logger_init(&app->logger,
                    app->config.raw_log,
                    app->config.event_log,
                    app->config.error_log) != 0) {

        fprintf(stderr, "logger_init failed\n");
        error = ERR_IO;
        goto fail;
    }

    logger_info(&app->logger, "logger initialized");

    /*
     * The core is initialized after the logger because its emit callback writes
     * semantic events through logger_event(). For now the core runs in shadow
     * mode beside the legacy dispatcher.
     */
    alfred_config_default(&app->core_config);
    app->core_logger_context.logger = &app->logger;
    app->core_logger_context.event_engine_mode =
        app->config.event_engine_mode;

    app->core = alfred_create(&app->core_config,
                              core_logger_on_event,
                              &app->core_logger_context);

    if (app->core == NULL) {
        logger_error(&app->logger,
                     "alfred core initialization failed");

        error = ERR_ALLOC;
        goto fail;
    }

    logger_info(&app->logger,
                "alfred core initialized event_engine=%s",
                config_event_engine_name(app->config.event_engine_mode));

    /*
     * The current inotify dispatcher correlates MOVED_FROM and MOVED_TO with
     * this cache. This is temporary until move correlation is owned by core.
     */
    if (move_cache_init(&app->moves,
                        app->config.move_cache_size) != 0) {

        logger_error(&app->logger,
                     "move_cache_init failed");

        error = ERR_ALLOC;
        goto fail;
    }

    logger_info(&app->logger,
                "move cache initialized");

    error = inotify_backend_init(app);
    if (error != ERR_OK)
        goto fail;

    /*
     * Signal handlers use g_app only to request shutdown. Cleanup is left to
     * app_shutdown(), which runs after app_run() returns or init fails.
     */
    g_app = app;
    setup_signals();

    logger_info(&app->logger,
                "signal handlers installed");

    /*
     * At least one startup path is required. Paths are watched recursively or
     * non-recursively according to the active configuration.
     */
    if (argc < 2) {
        logger_error(&app->logger,
                     "no startup paths provided");
        error = ERR_INVALID_ARG;
        goto fail;
    }

    for (int i = 1; i < argc; i++) {
        error = inotify_backend_add_startup_watch(app, argv[i]);
        if (error != ERR_OK)
            goto fail;
    }

    logger_info(&app->logger,
                "application startup complete");

    return ERR_OK;

fail:
    app_shutdown(app);
    return error;
}

/*
 * app_run - execute the inotify event loop
 * @app: initialized application context
 *
 * Polls the active inotify backend. The backend reads and converts raw records;
 * app_run() only drives the loop and stops on fatal backend errors.
 *
 * Return: ERR_OK on normal termination, a negative error_t value on invalid
 * input.
 */
int app_run(app_t *app)
{
    if (app == NULL)
        return ERR_INVALID_ARG;

    logger_info(&app->logger,
                "event loop started");

    while (app->running) {

        if (inotify_backend_poll(app,
                                 handle_backend_event,
                                 NULL) != ERR_OK) {
            break;
        }
    }

    logger_info(&app->logger,
                "event loop terminated");

    return ERR_OK;
}

/*
 * app_shutdown - release resources owned by app_t
 * @app: application context to release
 *
 * Releases resources in an order that keeps logging available until the end.
 * The function accepts NULL for convenience in failure paths.
 */
void app_shutdown(app_t *app)
{
    if (app == NULL)
        return;

    logger_info(&app->logger,
                "shutdown started");

    inotify_backend_shutdown(app);

    /* The move cache owns pending rename/move source names. */
    move_cache_destroy(&app->moves);

    /*
     * Destroy the core before closing the logger. Future flush behavior may
     * emit final semantic events during shutdown.
     */
    if (app->core != NULL) {
        alfred_destroy(app->core);
        app->core = NULL;
    }

    /*
     * The logger is closed last so shutdown activity can still be recorded.
     */
    logger_info(&app->logger,
                "resources released");

    logger_close(&app->logger);

    g_app = NULL;
}
