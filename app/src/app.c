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
#include "alfred_record_output_pipeline.h"
#include "alfred_record_runtime.h"
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
static int log_raw_record(logger_t *logger, const alfred_record_t *record);
static int app_init_output_pipeline(app_t *app);
static int app_shutdown_output_pipeline(app_t *app);
static int app_enqueue_output_record(app_t *app,
                                     const alfred_record_t *record);
static int app_drain_output_pipeline(app_t *app);
static int app_emit_output_record(app_t *app, const alfred_record_t *record);
static int app_emit_output_record_callback(const alfred_record_t *record,
                                           void *userdata);
static int handle_backend_event(const alfred_raw_event_t *raw,
                                void *userdata);

enum {
    APP_OUTPUT_QUEUE_CAPACITY = 1024u,
    APP_OUTPUT_FORMAT_BUFFER_SIZE = 8192u
};

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
    ctx->emit_record = app_emit_output_record_callback;
    ctx->emit_record_userdata = app;
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
 * write_output_bytes - write buffered JSONL bytes to the configured output file
 * @userdata: FILE stream opened by app_init_output_pipeline()
 * @data: borrowed byte buffer from the JSONL writer
 * @size: number of bytes to write
 *
 * This callback is intentionally byte-oriented rather than printf-oriented.
 * The JSONL writer has already formatted complete lines into its own buffer;
 * the application layer only moves those bytes to the configured destination.
 *
 * Return: 0 on success, -1 on invalid input or short write.
 */
static int write_output_bytes(void *userdata, const char *data, size_t size)
{
    FILE *fp = userdata;

    if (fp == NULL || (data == NULL && size > 0u)) {
        return -1;
    }

    if (size == 0u) {
        return 0;
    }

    return fwrite(data, 1u, size, fp) == size ? 0 : -1;
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
 * performs semantic move/rename correlation later. OVERFLOW is accepted as a
 * global raw fact with no path ownership and no directory modifier.
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

    if ((raw->mask & ALFRED_RAW_OVERFLOW) != 0u) {
        return raw->mask == ALFRED_RAW_OVERFLOW;
    }

    return 0;
}

/*
 * log_raw_record - emit one raw record through the compatibility text sink
 * @logger: application logger that owns raw.log
 * @record: already adapted Event Model v0 raw record
 *
 * The backend still delivers alfred_raw_event_t to app.c and the core consumes
 * that value through alfred_process(). The application adapts selected raw
 * facts once, then this helper keeps raw.log compatibility through the same
 * record -> sink -> text sink shape used by semantic and watch diagnostics.
 *
 * Return: 0 on success, -1 on invalid input, sink setup, formatting, or logger
 * bridging failure.
 */
static int log_raw_record(logger_t *logger, const alfred_record_t *record)
{
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    char payload[PATH_MAX + 64u];

    if (logger == NULL || record == NULL) {
        return -1;
    }

    text_sink.write = write_raw_payload;
    text_sink.userdata = logger;
    text_sink.buffer = payload;
    text_sink.buffer_size = sizeof(payload);

    if (alfred_record_text_sink_init(&text_sink, &sink) != 0) {
        return -1;
    }

    if (alfred_record_sink_emit(&sink, record) != 0) {
        return -1;
    }

    return 0;
}

/*
 * app_init_output_pipeline - initialize optional structured output runtime
 * @app: application context with parsed configuration
 *
 * The pipeline is disabled by default. When enabled in this first runtime
 * wiring step, it is additive to raw.log/events.log/errors.log and supports
 * only JSONL because that is the only buffered writer with a v0 contract.
 *
 * Return: ERR_OK on success, ERR_CONFIG/ERR_ALLOC/ERR_IO on failure.
 */
static int app_init_output_pipeline(app_t *app)
{
    alfred_record_output_pipeline_config_t pipeline_config;

    if (app == NULL) {
        return ERR_INVALID_ARG;
    }

    if (!app->config.output.enabled) {
        return ERR_OK;
    }

    if (app->config.output.format != OUTPUT_FORMAT_JSONL) {
        logger_error(&app->logger,
                     "output_enabled requires output_format=jsonl");
        return ERR_CONFIG;
    }

    if (app->config.output.buffer_size < APP_OUTPUT_FORMAT_BUFFER_SIZE) {
        logger_error(&app->logger,
                     "output_buffer_size must be at least %u bytes",
                     APP_OUTPUT_FORMAT_BUFFER_SIZE);
        return ERR_CONFIG;
    }

    app->output_stream = fopen(app->config.output_log, "a");
    if (app->output_stream == NULL) {
        logger_error(&app->logger,
                     "failed to open output log %s",
                     app->config.output_log);
        return ERR_IO;
    }

    /*
     * Alfred owns batching in alfred_record_jsonl_writer_t. Disable stdio's
     * extra buffering so write_output_bytes() observes device errors at the
     * writer flush boundary instead of deferring them until shutdown. This keeps
     * output failure policy testable with devices such as /dev/full while still
     * avoiding one write per event: JSONL writer flushes only when its own buffer
     * is full or during shutdown.
     */
    if (setvbuf(app->output_stream, NULL, _IONBF, 0) != 0) {
        logger_error(&app->logger,
                     "failed to configure output log buffering");
        return ERR_IO;
    }

    app->output_format_buffer = malloc(APP_OUTPUT_FORMAT_BUFFER_SIZE);
    app->output_buffer = malloc(app->config.output.buffer_size);

    if (app->output_format_buffer == NULL || app->output_buffer == NULL) {
        logger_error(&app->logger,
                     "failed to allocate output pipeline buffers");
        return ERR_ALLOC;
    }

    memset(&pipeline_config, 0, sizeof(pipeline_config));
    pipeline_config.enabled = 1;
    pipeline_config.format = ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_JSONL;
    pipeline_config.queue_capacity = APP_OUTPUT_QUEUE_CAPACITY;
    pipeline_config.drain_batch_size = APP_OUTPUT_QUEUE_CAPACITY;
    pipeline_config.write = write_output_bytes;
    pipeline_config.userdata = app->output_stream;
    pipeline_config.format_buffer = app->output_format_buffer;
    pipeline_config.format_buffer_size = APP_OUTPUT_FORMAT_BUFFER_SIZE;
    pipeline_config.output_buffer = app->output_buffer;
    pipeline_config.output_buffer_size = app->config.output.buffer_size;

    if (alfred_record_output_pipeline_init(&app->output_pipeline,
                                           &pipeline_config) != 0) {
        logger_error(&app->logger,
                     "failed to initialize output pipeline");
        return ERR_ALLOC;
    }

    logger_info(&app->logger,
                "output pipeline initialized format=jsonl path=%s",
                app->config.output_log);

    return ERR_OK;
}

/*
 * app_shutdown_output_pipeline - flush and release optional output runtime
 * @app: application context that may own an output pipeline
 *
 * Shutdown keeps the policy explicit: first flush buffered JSONL records, then
 * destroy the owned queue state, free borrowed writer buffers, and finally close
 * the destination stream. Flush failures are still logged, but they are also
 * returned to the caller so main() can avoid reporting a successful run when the
 * configured JSONL ledger is incomplete.
 *
 * Return: 0 when no output failure was observed, -1 when a final flush failed.
 */
static int app_shutdown_output_pipeline(app_t *app)
{
    int status = 0;

    if (app == NULL) {
        return 0;
    }

    if (app->output_pipeline.enabled) {
        if (alfred_record_output_pipeline_flush(&app->output_pipeline) != 0) {
            logger_error(&app->logger,
                         "failed to flush output pipeline");
            app->output_failed = 1;
            status = -1;
        }
    }

    if (app->output_stream != NULL && fflush(app->output_stream) != 0) {
        logger_error(&app->logger,
                     "failed to flush output log stream");
        app->output_failed = 1;
        status = -1;
    }

    alfred_record_output_pipeline_destroy(&app->output_pipeline);

    free(app->output_format_buffer);
    app->output_format_buffer = NULL;

    free(app->output_buffer);
    app->output_buffer = NULL;

    if (app->output_stream != NULL) {
        fclose(app->output_stream);
        app->output_stream = NULL;
    }

    return status;
}

/*
 * app_enqueue_output_record - enqueue one optional runtime output record
 * @app: application context
 * @record: borrowed Event Model v0 record
 *
 * This helper names the producer side of the structured output path. It turns a
 * borrowed runtime record into an owned queued record through
 * alfred_record_output_pipeline_enqueue(), which delegates to the bounded queue
 * and alfred_record_clone_owned().
 *
 * Failure policy:
 *   output_enabled=true makes the structured output path part of the runtime
 *   contract. If enqueue fails, the application marks output_failed so the
 *   event loop can stop instead of producing a silent partial JSONL ledger.
 *   Disabled output remains a successful no-op.
 *
 * Return: 0 on success or when disabled, -1 on enqueue failure.
 */
static int app_enqueue_output_record(app_t *app,
                                     const alfred_record_t *record)
{
    if (app == NULL || record == NULL) {
        return -1;
    }

    if (!app->output_pipeline.enabled) {
        return 0;
    }

    if (app->output_failed) {
        return -1;
    }

    if (alfred_record_output_pipeline_enqueue(&app->output_pipeline,
                                              record) != 0) {
        app->output_failed = 1;
        return -1;
    }

    return 0;
}

/*
 * app_drain_output_pipeline - drain queued records through dispatcher/writer
 * @app: application context
 *
 * This helper names the consumer side of the structured output path. In the v0
 * wiring it is still called synchronously after enqueue, but the separation
 * makes the future worker boundary explicit: drain consumes owned records from
 * the queue, dispatches them to sinks, and lets writers perform formatting/I/O.
 *
 * Return: 0 on success or when disabled, -1 on drain/write failure.
 */
static int app_drain_output_pipeline(app_t *app)
{
    alfred_record_runtime_drain_result_t drain_result;

    if (app == NULL) {
        return -1;
    }

    if (!app->output_pipeline.enabled) {
        return 0;
    }

    if (app->output_failed) {
        return -1;
    }

    if (alfred_record_output_pipeline_drain_once(&app->output_pipeline,
                                                 &drain_result) != 0) {
        app->output_failed = 1;
        return -1;
    }

    if (drain_result.status != 0) {
        app->output_failed = 1;
        return -1;
    }

    return 0;
}

/*
 * app_emit_output_record - enqueue and drain one optional runtime output record
 * @app: application context
 * @record: borrowed Event Model v0 record
 *
 * This wrapper preserves the current synchronous runtime behavior while making
 * the two architectural phases visible in code:
 *
 *   app_enqueue_output_record()
 *   app_drain_output_pipeline()
 *
 * A later worker step should keep producers on the enqueue side and move drain
 * to a dedicated runtime loop.
 *
 * Return: 0 on success or when disabled, -1 on enqueue/drain/write failure.
 */
static int app_emit_output_record(app_t *app, const alfred_record_t *record)
{
    if (app_enqueue_output_record(app, record) != 0) {
        return -1;
    }

    return app_drain_output_pipeline(app);
}

/*
 * app_emit_output_record_callback - core_logger compatible output callback
 * @record: borrowed Event Model v0 record emitted by the core logger
 * @userdata: app_t pointer supplied during app_init()
 *
 * core_logger does not know app_t. This adapter keeps the public callback shape
 * generic while reusing the application-owned output pipeline helper.
 *
 * Return: 0 on success or disabled output, -1 on invalid input or emit failure.
 */
static int app_emit_output_record_callback(const alfred_record_t *record,
                                           void *userdata)
{
    return app_emit_output_record((app_t *)userdata, record);
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
        if (is_raw_record_sink_candidate(raw)) {
            alfred_record_t record;

            if (alfred_record_from_raw(raw, &record) == 0) {
                if (log_raw_record(&app->logger, &record) != 0) {
                    logger_error(&app->logger,
                                 "failed to log raw record");
                }

                if (app_emit_output_record(app, &record) != 0) {
                    logger_error(&app->logger,
                                 "failed to emit output record");
                    return ERR_IO;
                }
            }
            else {
                logger_error(&app->logger,
                             "failed to adapt output record");
            }
        }
    }

    if (raw != NULL && app->core != NULL) {
        if (alfred_process(app->core, raw) != 0) {
            logger_error(&app->logger,
                         "core failed to process raw event");
        }
    }

    if (app->output_failed) {
        logger_error(&app->logger,
                     "structured output failed; stopping event loop");
        return ERR_IO;
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
    app->core_logger_context.emit_record = app_emit_output_record_callback;
    app->core_logger_context.emit_record_userdata = app;

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

    error = app_init_output_pipeline(app);
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
    error_t error = ERR_OK;

    if (app == NULL)
        return ERR_INVALID_ARG;

    logger_info(&app->logger,
                "event loop started");

    inotify_backend_context_t backend_ctx;
    app_build_inotify_backend_context(app, &backend_ctx);

    while (app->running) {

        error = inotify_backend_poll(&backend_ctx,
                                     handle_backend_event,
                                     app);
        if (error != ERR_OK) {
            break;
        }
    }

    logger_info(&app->logger,
                "event loop terminated");

    return error;
}

/*
 * app_shutdown - release resources owned by app_t
 * @app: application context to release
 *
 * Releases resources in an order that keeps logging available until the end.
 * The function accepts NULL for convenience in failure paths.
 *
 * Return: ERR_OK when cleanup did not observe an output failure, ERR_IO when
 * the final structured output flush failed.
 */
int app_shutdown(app_t *app)
{
    int shutdown_status = ERR_OK;

    if (app == NULL)
        return ERR_OK;

    logger_info(&app->logger,
                "shutdown started");

    inotify_backend_context_t backend_ctx;
    app_build_inotify_backend_context(app, &backend_ctx);
    inotify_backend_shutdown(&backend_ctx);

    if (app_shutdown_output_pipeline(app) != 0) {
        shutdown_status = ERR_IO;
    }

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

    return shutdown_status;
}
