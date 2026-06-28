/* ============================================================================
 * app.h - application runtime state and lifecycle API
 *
 * This header defines the process-wide application context used by alfred.
 * The application layer currently owns configuration, logging, core state, and
 * a transitional inotify backend context.
 *
 * app_t is still a practical orchestration object: it wires configuration,
 * logging, the inotify backend, and the core together. The semantic state is
 * already owned by the core engine. The inotify backend runtime is embedded
 * here, but backend functions receive an explicit inotify_backend_context_t
 * built by app.c instead of receiving the whole app_t object.
 * ============================================================================
 */

#ifndef APP_H
#define APP_H

#include "config.h"
#include "alfred_correlator.h"
#include "alfred_record_output_pipeline.h"
#include "core_logger.h"
#include "inotify_backend.h"
#include "logger.h"

#include <signal.h>
#include <stddef.h>
#include <stdio.h>

/*
 * app_output_runtime_stats_t - local Writer Runtime v0 counters
 *
 * These counters describe the current application-level output wiring. They
 * are deliberately not an Event Model v0 record and not a stable public API:
 * they help tests, benchmarks, and PR reviews understand the bounded queue
 * behavior before Alfred introduces worker threads or per-sink queues.
 */
typedef struct {
    /* Records offered to the enabled structured output path. */
    size_t enqueue_attempts;

    /* Records accepted into the bounded queue after any pressure-relief drain. */
    size_t enqueue_success;

    /* Records not accepted because enqueue or pressure drain failed. */
    size_t enqueue_failures;

    /* Times enqueue found the bounded queue full before pushing a record. */
    size_t pressure_drains;

    /* Pressure drains that failed and therefore made output fail closed. */
    size_t pressure_drain_failures;

    /* Explicit drain calls performed by app_run() or pressure relief. */
    size_t drain_calls;

    /* Drain calls that failed because dispatcher or writer failed. */
    size_t drain_failures;

    /* Records successfully dispatched by all drain calls. */
    size_t drained_records;

    /* Largest queue occupancy observed by the application runtime. */
    size_t max_pending;
} app_output_runtime_stats_t;

/*
 * app_t - process-wide runtime context
 *
 * app_t is the ownership root for the running process. Initialization builds
 * each subsystem into this object, app_run() uses it while polling inotify,
 * and app_shutdown() releases resources in the reverse direction.
 *
 * The current design still embeds inotify_backend_t here. app.c exposes it to
 * backend functions through inotify_backend_context_t together with borrowed
 * configuration and logger pointers. The backend never owns app_t; app_t is
 * only passed back as opaque callback userdata at the app/core boundary.
 */
typedef struct app {

    /*
     * Cooperative shutdown flag. Signal handlers clear this flag instead of
     * doing work directly in signal context. sig_atomic_t is the portable C type
     * for values that are written by a signal handler and read by normal code.
     */
    volatile sig_atomic_t running;

    /* Application configuration loaded before subsystem initialization. */
    config_t config;

    /*
     * inotify backend state. This owns the nonblocking descriptor and watch
     * descriptor table. Backend functions access it through
     * inotify_backend_context_t, not through app_t.
     */
    inotify_backend_t inotify;

    /* Raw, semantic, and error log sink. */
    logger_t logger;

    /*
     * Optional structured output pipeline. It is disabled by default and, when
     * enabled, is additive to the compatibility raw/events/errors logs. The
     * buffers are application-owned because the pipeline borrows writer storage.
     */
    alfred_record_output_pipeline_t output_pipeline;
    FILE *output_stream;
    char *output_format_buffer;
    char *output_buffer;
    /*
     * Set after the optional structured output path fails. When output is
     * explicitly enabled, Alfred treats a writer/pipeline failure as a fatal
     * runtime condition so output.jsonl cannot silently become an incomplete
     * ledger while the event loop keeps processing filesystem activity.
     */
    int output_failed;

    /*
     * Application-local observability for Writer Runtime v0. The counters are
     * logged at shutdown when structured output is enabled so benchmarks can
     * see queue pressure without parsing internal state.
     */
    app_output_runtime_stats_t output_stats;

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
 * If structured output is enabled, the final JSONL flush is part of the
 * runtime contract and a flush failure is reported to the caller.
 *
 * Return: ERR_OK when shutdown completed without output failure, ERR_IO when
 * the final structured output flush failed.
 */
int app_shutdown(app_t *app);

#endif
