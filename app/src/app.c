/* ============================================================================
 * app.c - application lifecycle and inotify event loop
 *
 * This file owns process-level initialization, signal handling, backend
 * polling orchestration, and shutdown ordering. Backend-specific inotify reads
 * are delegated to modules/inotify.
 *
 * The current boundary is intentionally narrow: app.c decides startup and
 * shutdown order, then repeatedly calls inotify_backend_poll(). It does not
 * parse inotify records and does not classify filesystem semantics.
 * ============================================================================
 */

#include "app.h"
#include "core_logger.h"
#include "errors.h"
#include "inotify_backend.h"
#ifdef ALFRED_ENABLE_LEGACY_SHADOW
#include "events.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* ============================================================================
 * Local Declarations
 * ========================================================================== */

static void setup_signals(void);
static void handle_signal(int sig);
static void app_build_inotify_backend_context(
    app_t *app,
    inotify_backend_context_t *ctx
);
static int app_init_legacy_shadow(app_t *app);
static void app_shutdown_legacy_shadow(app_t *app);
#ifdef ALFRED_ENABLE_LEGACY_SHADOW
static void app_legacy_shadow_dispatch(void *userdata,
                                       const struct inotify_event *ev);
#endif
static int handle_backend_event(const alfred_raw_event_t *raw,
                                void *userdata);

/* ============================================================================
 * Signal State
 * ========================================================================== */

/*
 * Signal handlers cannot receive user data, so the application context is
 * exposed through a single file-local pointer. The handler only clears a flag;
 * all resource cleanup remains in normal process context.
 */
static app_t *g_app = NULL;

/*
 * app_build_inotify_backend_context - expose backend dependencies explicitly
 * @app: application context that owns runtime, config, and logger
 * @ctx: backend context to fill with borrowed pointers
 *
 * app_t remains the ownership root, but the clean backend lifecycle APIs no
 * longer receive the full application object. app.c builds the narrow context
 * at the orchestration boundary and passes only what the backend needs.
 */
static void app_build_inotify_backend_context(
    app_t *app,
    inotify_backend_context_t *ctx
)
{
    ctx->runtime = &app->inotify;
    ctx->config = &app->config;
    ctx->logger = &app->logger;
}

/*
 * app_init_legacy_shadow - initialize the optional legacy comparison path
 * @app: application context that owns configuration and logger
 *
 * The inotify backend is raw-oriented and must not initialize semantic legacy
 * state. Shadow mode is an application-level comparison feature, so app.c owns
 * the build/runtime checks and the lifetime of events.c.
 *
 * Return: ERR_OK when shadow is disabled or initialized, otherwise ERR_CONFIG
 * for an unavailable shadow build or ERR_ALLOC on legacy allocation failure.
 */
static int app_init_legacy_shadow(app_t *app)
{
    if (app->config.event_engine_mode != EVENT_ENGINE_SHADOW)
        return ERR_OK;

#ifdef ALFRED_ENABLE_LEGACY_SHADOW
    if (legacy_events_init(app->config.move_cache_size) != 0) {
        logger_error(&app->logger,
                     "legacy_events_init failed");
        return ERR_ALLOC;
    }

    logger_info(&app->logger,
                "legacy event dispatcher initialized");
    return ERR_OK;
#else
    logger_error(&app->logger,
                 "shadow event engine requires build with "
                 "ENABLE_LEGACY_SHADOW=1");
    return ERR_CONFIG;
#endif
}

/*
 * app_shutdown_legacy_shadow - release optional legacy comparison state
 * @app: application context used to inspect the selected event engine
 *
 * legacy_events_shutdown() is idempotent, but the runtime mode check keeps the
 * ownership rule clear for readers: app.c releases the shadow-only semantic
 * dispatcher only when shadow was requested.
 */
static void app_shutdown_legacy_shadow(app_t *app)
{
    if (app->config.event_engine_mode != EVENT_ENGINE_SHADOW)
        return;

#ifdef ALFRED_ENABLE_LEGACY_SHADOW
    legacy_events_shutdown();
#else
    (void)app;
#endif
}

#ifdef ALFRED_ENABLE_LEGACY_SHADOW
/*
 * app_legacy_shadow_dispatch - adapt the opaque bridge back to legacy app_t
 * @userdata: application context supplied by app_run()
 * @ev: kernel inotify event read by the backend
 *
 * The backend intentionally knows only callback + userdata. This adapter keeps
 * the historical app_t requirement local to app.c and events.c, which makes the
 * temporary shadow path visible without leaking app_t into the backend API.
 */
static void app_legacy_shadow_dispatch(void *userdata,
                                       const struct inotify_event *ev)
{
    legacy_events_dispatch((app_t *)userdata, ev);
}
#endif

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
 * @raw: optional raw event for the core
 * @userdata: application context supplied by app_run()
 *
 * The backend owns inotify parsing, raw conversion, and recursive watch repair.
 * Shadow lifecycle is owned by app.c; during polling the backend may invoke the
 * opaque shadow callback separately. The app callback remains deliberately
 * small: it bridges "backend raw fact" to "core semantic processing".
 */
static int handle_backend_event(const alfred_raw_event_t *raw,
                                void *userdata)
{
    app_t *app = (app_t *)userdata;

    if (app == NULL)
        return ERR_INVALID_ARG;

    if (raw != NULL && app->core != NULL) {
        if (alfred_process(app->core, raw) != 0) {
            logger_error(&app->logger,
                         "core failed to process raw event");
        }
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
 * logging, core state, inotify backend, signal handling, and startup watches.
 * A single failure path delegates cleanup to app_shutdown().
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
     * semantic events through logger_event(). In core mode this is the official
     * stream. In shadow mode app.c also initializes the legacy dispatcher so
     * the two streams can be compared.
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

    error = app_init_legacy_shadow(app);
    if (error != ERR_OK)
        goto fail;

    inotify_backend_context_t backend_ctx;
    app_build_inotify_backend_context(app, &backend_ctx);

    error = inotify_backend_init(&backend_ctx);
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
        error = inotify_backend_add_startup_watch(&backend_ctx, argv[i]);
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

    inotify_backend_context_t backend_ctx;
    app_build_inotify_backend_context(app, &backend_ctx);

    legacy_shadow_bridge_t legacy = {
#ifdef ALFRED_ENABLE_LEGACY_SHADOW
        .dispatch = app_legacy_shadow_dispatch,
        .userdata = app
#else
        .dispatch = NULL,
        .userdata = NULL
#endif
    };

    while (app->running) {

        if (inotify_backend_poll(&backend_ctx,
                                 &legacy,
                                 handle_backend_event,
                                 app) != ERR_OK) {
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

    inotify_backend_context_t backend_ctx;
    app_build_inotify_backend_context(app, &backend_ctx);
    inotify_backend_shutdown(&backend_ctx);

    app_shutdown_legacy_shadow(app);

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
