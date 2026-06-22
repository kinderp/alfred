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
#include "alfred_record_adapter.h"
#include "alfred_record_sink.h"
#include "alfred_record_text_sink.h"
#include "core_logger.h"
#include "errors.h"
#include "inotify_backend.h"

#include <limits.h>
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
static int log_raw_record(logger_t *logger, const alfred_raw_event_t *raw);
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
    ctx->config = &app->config.inotify;
    ctx->logger = &app->logger;
}

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
 * write_raw_payload - bridge a text-sink payload to the raw application log
 * @userdata: logger_t destination
 * @payload: formatted Event Model v0 raw payload
 *
 * The text sink owns only record formatting. The application keeps ownership
 * of raw.log routing, timestamps, levels, and file handles through logger_raw().
 * This mirrors core_logger.c and is the first small runtime bridge for
 * normalized raw records.
 *
 * Return: 0 on success, -1 on invalid input.
 */
static int write_raw_payload(void *userdata, const char *payload)
{
    logger_t *logger = userdata;

    if (logger == NULL || payload == NULL) {
        return -1;
    }

    logger_raw(logger, "%s", payload);
    return 0;
}

/*
 * is_raw_record_sink_candidate - choose raw events migrated to the sink
 * @raw: borrowed raw event from the backend callback
 *
 * This helper is intentionally limited to raw facts whose compatibility text is
 * already handled by the Event Model v0 formatter. ALFRED_RAW_ISDIR is accepted
 * as a modifier because file and directory facts use the same action bit plus
 * optional directory type information. MOVED_FROM and MOVED_TO also carry the
 * backend move cookie, but they remain normalized raw facts: the core still
 * performs semantic move/rename correlation later.
 *
 * Return: 1 when @raw should be logged through the record sink, 0 otherwise.
 */
static int is_raw_record_sink_candidate(const alfred_raw_event_t *raw)
{
    const uint32_t create_mask = ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR;
    const uint32_t delete_mask = ALFRED_RAW_DELETE | ALFRED_RAW_ISDIR;
    const uint32_t attrib_mask = ALFRED_RAW_ATTRIB | ALFRED_RAW_ISDIR;
    const uint32_t modify_mask = ALFRED_RAW_MODIFY | ALFRED_RAW_ISDIR;
    const uint32_t close_write_mask =
        ALFRED_RAW_CLOSE_WRITE | ALFRED_RAW_ISDIR;
    const uint32_t moved_from_mask =
        ALFRED_RAW_MOVED_FROM | ALFRED_RAW_ISDIR;
    const uint32_t moved_to_mask =
        ALFRED_RAW_MOVED_TO | ALFRED_RAW_ISDIR;

    if (raw == NULL) {
        return 0;
    }

    if ((raw->mask & ALFRED_RAW_CREATE) != 0u) {
        return (raw->mask & ~create_mask) == 0u;
    }

    if ((raw->mask & ALFRED_RAW_DELETE) != 0u) {
        return (raw->mask & ~delete_mask) == 0u;
    }

    if ((raw->mask & ALFRED_RAW_ATTRIB) != 0u) {
        return (raw->mask & ~attrib_mask) == 0u;
    }

    if ((raw->mask & ALFRED_RAW_MODIFY) != 0u) {
        return (raw->mask & ~modify_mask) == 0u;
    }

    if ((raw->mask & ALFRED_RAW_CLOSE_WRITE) != 0u) {
        return (raw->mask & ~close_write_mask) == 0u;
    }

    if ((raw->mask & ALFRED_RAW_MOVED_FROM) != 0u) {
        return (raw->mask & ~moved_from_mask) == 0u;
    }

    if ((raw->mask & ALFRED_RAW_MOVED_TO) != 0u) {
        return (raw->mask & ~moved_to_mask) == 0u;
    }

    return 0;
}

/*
 * log_raw_record - emit selected raw facts through Event Model v0 sinks
 * @logger: application logger that owns raw.log
 * @raw: backend raw event to adapt
 *
 * The backend still delivers alfred_raw_event_t to app.c and the core still
 * consumes that value through alfred_process(). This helper migrates the
 * compatibility raw log for selected normalized raw facts to the same record ->
 * sink -> text sink shape used by semantic and watch diagnostics. It stays
 * deliberately narrow so overflow can be validated in a separate micro-step.
 *
 * Return: 0 on success or when @raw is not part of this micro-step, -1 when
 * conversion, sink setup, formatting, or logger bridging fails.
 */
static int log_raw_record(logger_t *logger, const alfred_raw_event_t *raw)
{
    alfred_record_t record;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    char payload[PATH_MAX + 64u];

    if (logger == NULL) {
        return -1;
    }

    if (!is_raw_record_sink_candidate(raw)) {
        return 0;
    }

    if (alfred_record_from_raw(raw, &record) != 0) {
        return -1;
    }

    text_sink.write = write_raw_payload;
    text_sink.userdata = logger;
    text_sink.buffer = payload;
    text_sink.buffer_size = sizeof(payload);

    if (alfred_record_text_sink_init(&text_sink, &sink) != 0) {
        return -1;
    }

    if (alfred_record_sink_emit(&sink, &record) != 0) {
        return -1;
    }

    return 0;
}

/*
 * handle_backend_event - consume one backend event
 * @raw: optional raw event for the core
 * @userdata: application context supplied by app_run()
 *
 * The backend owns inotify parsing, raw conversion, and recursive watch repair.
 * The app callback remains deliberately small: it bridges "backend raw fact"
 * to "core semantic processing". During Backend API v0 migration it also
 * routes selected normalized raw diagnostics through record sinks before
 * passing the original raw event to the core.
 */
static int handle_backend_event(const alfred_raw_event_t *raw,
                                void *userdata)
{
    app_t *app = (app_t *)userdata;

    if (app == NULL)
        return ERR_INVALID_ARG;

    if (raw != NULL) {
        if (log_raw_record(&app->logger, raw) != 0) {
            logger_error(&app->logger,
                         "failed to log raw record");
        }
    }

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

    const char *config_path = getenv("ALFRED_CONFIG");
    if (config_path != NULL &&
        config_load(&app->config, config_path) != ERR_OK) {

        fprintf(stderr,
                "invalid ALFRED_CONFIG=%s\n",
                config_path);
        error = ERR_CONFIG;
        goto fail;
    }

    /*
     * The environment override is applied after the optional config file so it
     * can reject stale or accidental event engine values with highest priority.
     */
    const char *event_engine_env = getenv("ALFRED_EVENT_ENGINE");
    if (event_engine_env != NULL &&
        config_set_event_engine(&app->config, event_engine_env) != ERR_OK) {

        fprintf(stderr,
                "invalid ALFRED_EVENT_ENGINE=%s (expected core)\n",
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
     * semantic events through logger_event(). Core mode is the official stream.
     */
    alfred_config_default(&app->core_config);
    app->core_logger_context.logger = &app->logger;

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
                "alfred core initialized event_engine=core");

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

    while (app->running) {

        if (inotify_backend_poll(&backend_ctx,
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
