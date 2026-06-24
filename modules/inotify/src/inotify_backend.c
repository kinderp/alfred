/* ============================================================================
 * inotify_backend.c - inotify runtime backend
 *
 * This file owns the current inotify polling boundary. It operates on the
 * backend context provided by app.c while app.c remains responsible for
 * high-level orchestration.
 *
 * The backend must stop at raw facts. It may know how to read inotify records,
 * map watch descriptors to paths, keep recursive watches alive, and synthesize
 * raw directory-create facts discovered during a recursive scan. It must not
 * decide final FILE_* or DIR_* semantics; that belongs to the core.
 * ========================================================================== */

#include "inotify_backend.h"

#include "alfred_record_diagnostic.h"
#include "alfred_record_sink.h"
#include "alfred_record_text.h"
#include "alfred_record_text_sink.h"
#include "errors.h"
#include "fs_scanner.h"
#include "inotify_adapter.h"
#include "logger.h"
#include "utils.h"
#include "watch_manager.h"
#include "watcher.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* ============================================================================
 * Configuration Constants
 * ========================================================================== */

#ifndef EVENT_BUFFER_SIZE
#define EVENT_BUFFER_SIZE 65536
#endif

#if defined(__GNUC__)
#define BACKEND_MAYBE_UNUSED __attribute__((unused))
#else
#define BACKEND_MAYBE_UNUSED
#endif

/*
 * Diagnostic records may render more than one path-like field, for example
 * old_path/new_path in lost-scope recovery. Keep the compatibility text-sink
 * buffer large enough for two maximum-size paths plus stable field labels so
 * valid long paths do not silently bypass the record/sink bridge.
 */
#define BACKEND_RECORD_TEXT_BUFFER_SIZE ((PATH_MAX * 2u) + 512u)

/* ============================================================================
 * Local Types
 * ========================================================================== */

typedef struct backend_emit_context {
    inotify_backend_context_t *ctx;
    inotify_backend_event_fn on_event;
    void *userdata;
    int failed;
} backend_emit_context_t;

typedef struct backend_resync_scan_context {
    inotify_backend_context_t *ctx;
    size_t directories_seen;
    size_t directories_watched;
    size_t directories_missing_watch;

    /*
     * Missing paths are copied because fs_scan_entry_t paths are borrowed from
     * the scanner callback and expire when the callback returns. The resync
     * reinstall phase consumes the whole list after fs_scan_tree() completes,
     * so every path stored here must be owned by the backend context.
     */
    char **missing_paths;
    size_t missing_paths_count;
    size_t missing_paths_capacity;

    /*
     * fs_scan_tree() treats callback stop as a successful bounded scan. If path
     * collection fails, the callback records that fact here and stops early so
     * the caller can keep the resync conservative.
     */
    int missing_paths_failed;
} backend_resync_scan_context_t;

/*
 * backend_resync_watch_ops - watch operations used by resync reinstall policy
 * @add: install a watch for one missing directory path
 * @remove: remove one watch descriptor installed during this resync attempt
 *
 * The reinstall policy is easier to reason about if it is separated from the
 * concrete watch implementation. In production, these callbacks are thin
 * wrappers around watch_manager_add() and watch_manager_remove(). In focused
 * tests, they are fake callbacks that can deterministically say "the first add
 * succeeds, the second add fails".
 *
 * This is not a public plugin/backend API. It is a tiny internal seam used to
 * test a failure branch that would otherwise require a fragile race between
 * fs_scan_tree(), filesystem mutation, and inotify_add_watch().
 */
typedef struct backend_resync_watch_ops {
    int (*add)(inotify_backend_context_t *ctx, const char *path);
    int (*remove)(inotify_backend_context_t *ctx, int wd);
} backend_resync_watch_ops_t;

typedef enum backend_resync_scan_class {
    BACKEND_RESYNC_SCAN_EMPTY,
    BACKEND_RESYNC_SCAN_COVERED,
    BACKEND_RESYNC_SCAN_NEEDS_REINSTALL
} backend_resync_scan_class_t;

#define BACKEND_LOST_SCOPE_MAX_ATTEMPTS 8U
#define BACKEND_LOST_SCOPE_NS_PER_MS 1000000ULL
#define BACKEND_LOST_SCOPE_POLL_BATCH_SIZE 1U

/*
 * backend_lost_scope_recovery_result - outcome of one queued identity search
 *
 * These values describe backend recovery control flow for one queued
 * lost-scope entry. They are not Alfred raw events and they are not semantic
 * core events. They exist so the delayed recovery worker/test code can
 * distinguish "nothing to do", "recovered", "not found in this scan scope",
 * and "technical failure".
 *
 * The successful path is intentionally stricter than "identity was found".
 * BACKEND_LOST_SCOPE_RECOVERY_FOUND is returned only after the backend has
 * found the saved filesystem identity, rewritten stale watcher-table prefixes,
 * scanned recovered subtree coverage, reinstalled every missing watch, and
 * marked the recovered subtree VALID. Any failure after identity discovery
 * still returns BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED so callers keep the
 * subtree conservative.
 */
typedef enum backend_lost_scope_recovery_result {
    /*
     * The lost-scope queue had no entry to process. This is a normal idle
     * result for a future worker and should not produce diagnostics.
     */
    BACKEND_LOST_SCOPE_RECOVERY_EMPTY,

    /*
     * A queued scope was fully recovered. The matching identity was found under
     * the scanned root, watcher-table prefixes were rewritten, missing watches
     * were reinstalled, and the subtree was marked VALID.
     */
    BACKEND_LOST_SCOPE_RECOVERY_FOUND,

    /*
     * The scan completed but did not find the queued identity in the selected
     * root. The object may be gone or may live outside the scanned scope.
     */
    BACKEND_LOST_SCOPE_RECOVERY_NOT_FOUND,

    /*
     * The scanner or a later repair step could not complete reliably. Recovery
     * must stay conservative because partial search, prefix repair, coverage
     * scan, watch reinstall, or state repair cannot prove the subtree is safe.
     */
    BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED
} backend_lost_scope_recovery_result_t;

/*
 * backend_lost_scope_scan_context - state for one identity-search scan
 * @entry: queued lost-scope record whose saved identity is being searched
 * @found_path: caller-owned buffer that receives the matching path
 * @found_path_size: size of @found_path in bytes
 * @found: set to nonzero when the callback finds @entry identity
 *
 * fs_scan_tree() calls backend_lost_scope_scan_for_identity() with borrowed
 * paths and stat data. This context keeps the queued identity and the caller's
 * output buffer together so the callback can stop at the first matching
 * directory and copy its current path out of the scanner's temporary storage.
 */
typedef struct backend_lost_scope_scan_context {
    const inotify_lost_scope_entry_t *entry;
    char *found_path;
    size_t found_path_size;
    int found;
} backend_lost_scope_scan_context_t;

/*
 * backend_text_sink_context_t - route one formatted payload to the right log
 * @logger: destination logger owned by the application/backend context
 * @use_error_channel: nonzero to write through logger_error(), zero for events
 *
 * The record text sink only formats payloads. This tiny context gives the
 * backend bridge enough information to preserve the historical log channel for
 * diagnostics such as WATCH_RESYNC_SCAN_FAILED without teaching the core sink
 * anything about Alfred's logger_t.
 */
typedef struct backend_text_sink_context {
    logger_t *logger;
    int use_error_channel;
} backend_text_sink_context_t;

/*
 * backend_write_event_payload - bridge text-sink payloads to logger_event
 * @userdata: logger_t destination
 * @payload: formatted diagnostic payload
 *
 * The text sink is intentionally logger-agnostic. Until the inotify backend
 * context owns a first-class record sink, this adapter keeps logger ownership
 * at the backend boundary while allowing selected diagnostics to flow through
 * emit(record).
 *
 * Return: 0 on success, -1 on invalid input.
 */
static int backend_write_event_payload(void *userdata, const char *payload)
{
    logger_t *logger = userdata;

    if (logger == NULL || payload == NULL) {
        return -1;
    }

    logger_event(logger, "%s", payload);
    return 0;
}

/*
 * backend_write_routed_payload - bridge text-sink payloads to event/error logs
 * @userdata: backend_text_sink_context_t route description
 * @payload: formatted diagnostic payload
 *
 * Some backend diagnostics intentionally live in the error log, while most are
 * event-log diagnostics. The record text sink only knows how to produce a
 * payload. This bridge keeps the channel decision local to the backend helper
 * that knows the diagnostic type.
 *
 * Return: 0 on success, -1 on invalid input.
 */
static int backend_write_routed_payload(void *userdata, const char *payload)
{
    backend_text_sink_context_t *sink_context = userdata;

    if (sink_context == NULL ||
        sink_context->logger == NULL ||
        payload == NULL) {
        return -1;
    }

    if (sink_context->use_error_channel) {
        logger_error(sink_context->logger, "%s", payload);
    } else {
        logger_event(sink_context->logger, "%s", payload);
    }

    return 0;
}

/*
 * backend_log_watch_diagnostic_record - emit one WATCH_* diagnostic record
 * @ctx: narrowed backend context used by the poll path
 * @type: WATCH_* diagnostic type to build
 * @wd: inotify watch descriptor involved in the diagnostic
 * @path: borrowed primary diagnostic path, or NULL when unavailable
 * @state: optional watch/recovery state or result string
 * @reason: optional stable reason string, such as IN_MOVE_SELF
 * @error: optional normalized failure string
 * @os_error_code: optional errno-like OS error code, or 0 when absent
 * @os_error_name: optional symbolic OS error name, or NULL when unavailable
 * @os_error_message: optional human-readable OS error message, or NULL
 * @fallback_name: textual WATCH_* token used if record formatting fails
 *
 * Backend API v0 moves diagnostics toward structured records, but the current
 * runtime still writes text logs through logger_event(). This helper centralizes
 * the behavior-neutral bridge: build alfred_record_t, format the historical
 * text payload, and fall back to the same compact WATCH_* shape if the bridge
 * cannot be used. It intentionally covers only field shapes already supported
 * by alfred_record_format_text(): plain wd/path, reason, reason+error, and the
 * limited reason+result cases handled by the formatter. OS error fields remain
 * optional: callers pass them only for diagnostics that need to preserve
 * syscall evidence while keeping Alfred's stable @error token separate.
 *
 * The output pipeline bridge is intentionally narrower than this helper:
 * WATCH_STALE is routed to the optional structured output callback, while
 * WATCH_RESYNC_* and WATCH_LOST_* stay text-only until their richer recovery
 * payloads are migrated in separate reviewable steps.
 */
static int backend_log_watch_diagnostic_record(
    inotify_backend_context_t *ctx,
    alfred_record_type_t type,
    int wd,
    const char *path,
    const char *state,
    const char *reason,
    const char *error,
    int os_error_code,
    const char *os_error_name,
    const char *os_error_message,
    const char *fallback_name)
{
    alfred_record_t record;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    char payload[BACKEND_RECORD_TEXT_BUFFER_SIZE];

    if (ctx == NULL || ctx->logger == NULL)
        return -1;

    if (alfred_record_build_watch_diagnostic_with_os_error(
            type,
            "inotify",
            wd,
            path,
            state,
            reason,
            error,
            os_error_code,
            os_error_name,
            os_error_message,
            &record) == 0) {
        text_sink.write = backend_write_event_payload;
        text_sink.userdata = ctx->logger;
        text_sink.buffer = payload;
        text_sink.buffer_size = sizeof(payload);

        if (alfred_record_text_sink_init(&text_sink, &sink) == 0 &&
            alfred_record_sink_emit(&sink, &record) == 0) {
            /*
             * Keep events.log compatibility first, then expose WATCH_STALE to
             * optional structured output. Queue-based output must clone this
             * borrowed record before returning.
             */
            if (type == ALFRED_RECORD_TYPE_WATCH_STALE &&
                ctx->emit_record != NULL &&
                ctx->emit_record(&record,
                                 ctx->emit_record_userdata) != 0) {
                logger_error(ctx->logger,
                             "failed to emit watch stale output record");
            }

            return 0;
        }

        if (alfred_record_format_text(&record, payload, sizeof(payload)) > 0) {
            logger_event(ctx->logger, "%s", payload);
            return 0;
        }
    }

    if (fallback_name == NULL)
        return -1;

    if (reason != NULL && error != NULL) {
        if (os_error_code != 0) {
            if (os_error_message != NULL) {
                logger_event(ctx->logger,
                             "%s wd=%d path=%s reason=%s error=%s errno=%d (%s)",
                             fallback_name,
                             wd,
                             path != NULL ? path : "",
                             reason,
                             error,
                             os_error_code,
                             os_error_message);
                return 0;
            }

            logger_event(ctx->logger,
                         "%s wd=%d path=%s reason=%s error=%s errno=%d",
                         fallback_name,
                         wd,
                         path != NULL ? path : "",
                         reason,
                         error,
                         os_error_code);
            return 0;
        }

        logger_event(ctx->logger,
                     "%s wd=%d path=%s reason=%s error=%s",
                     fallback_name,
                     wd,
                     path != NULL ? path : "",
                     reason,
                     error);
        return 0;
    }

    if (reason != NULL) {
        logger_event(ctx->logger,
                     "%s wd=%d path=%s reason=%s",
                     fallback_name,
                     wd,
                     path != NULL ? path : "",
                     reason);
        return 0;
    }

    logger_event(ctx->logger,
                 "%s wd=%d path=%s",
                 fallback_name,
                 wd,
                 path != NULL ? path : "");

    return 0;
}

/*
 * backend_log_watch_stale - emit WATCH_STALE through Event Model records
 * @ctx: narrowed backend context used by the poll path
 * @wd: inotify watch descriptor whose path is no longer reliable
 * @path: borrowed path from the watcher table, or NULL when unavailable
 * @reason: stable kernel/backend reason string, such as IN_MOVE_SELF
 *
 * WATCH_STALE is backend watch-table reliability state, not a semantic
 * filesystem event. The helper keeps that decision local while delegating the
 * shared record/sink/formatter/fallback work to
 * backend_log_watch_diagnostic_record().
 */
static void backend_log_watch_stale(inotify_backend_context_t *ctx,
                                    int wd,
                                    const char *path,
                                    const char *reason)
{
    (void)backend_log_watch_diagnostic_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_STALE,
        wd,
        path,
        "stale",
        reason,
        NULL,
        0,
        NULL,
        NULL,
        "WATCH_STALE");
}

/*
 * backend_log_stale_event_dropped - emit WATCH_STALE_EVENT_DROPPED
 * @ctx: narrowed backend context used by the poll path
 * @wd: stale inotify watch descriptor that received the kernel event
 * @path: borrowed stale path associated with @wd
 * @event_mask: borrowed textual inotify mask for the dropped event
 * @event_name: borrowed child name from the dropped event, or NULL
 *
 * A stale watch can still receive kernel events because inotify follows the
 * filesystem object, not Alfred's saved textual path. Forwarding that event to
 * raw/core would attach a possibly false path, so Alfred logs a diagnostic and
 * deliberately drops the raw/core conversion. The record preserves mask/name as
 * structured watch payload, then follows the same compatibility-first policy as
 * the other watch diagnostics.
 */
static void backend_log_stale_event_dropped(inotify_backend_context_t *ctx,
                                            int wd,
                                            const char *path,
                                            const char *event_mask,
                                            const char *event_name)
{
    alfred_record_t record;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    char payload[BACKEND_RECORD_TEXT_BUFFER_SIZE];

    if (ctx == NULL || ctx->logger == NULL)
        return;

    if (alfred_record_build_stale_event_dropped("inotify",
                                                wd,
                                                path,
                                                event_mask,
                                                event_name,
                                                &record) == 0) {
        text_sink.write = backend_write_event_payload;
        text_sink.userdata = ctx->logger;
        text_sink.buffer = payload;
        text_sink.buffer_size = sizeof(payload);

        if (alfred_record_text_sink_init(&text_sink, &sink) == 0 &&
            alfred_record_sink_emit(&sink, &record) == 0) {
            if (ctx->emit_record != NULL &&
                ctx->emit_record(&record,
                                 ctx->emit_record_userdata) != 0) {
                logger_error(ctx->logger,
                             "failed to emit stale dropped output record");
            }

            return;
        }

        if (alfred_record_format_text(&record, payload, sizeof(payload)) > 0) {
            logger_event(ctx->logger, "%s", payload);
            return;
        }
    }

    logger_event(ctx->logger,
                 "WATCH_STALE_EVENT_DROPPED wd=%d path=%s mask=%s name=%s",
                 wd,
                 path != NULL ? path : "",
                 event_mask != NULL ? event_mask : "",
                 event_name != NULL ? event_name : "");
}

/*
 * backend_resync_probe_result - internal outcomes of stale-watch recovery
 *
 * These values are diagnostic states for the backend resync probe. They are
 * deliberately not Alfred semantic events: the core must not infer filesystem
 * meaning from them. The backend uses the values to keep WATCH_RESYNC_FAILED
 * messages normalized while the resync implementation grows in small steps.
 */
typedef enum backend_resync_probe_result {
    /*
     * The stale watch was repaired through the local old-path probe and can
     * return to VALID. This is the only non-failure result and is never logged
     * as WATCH_RESYNC_FAILED.
     */
    BACKEND_RESYNC_PROBE_VALID,

    /*
     * The watcher table no longer has an entry for the watch descriptor that
     * produced the self-event. There is no stable path or saved identity to
     * probe, so recovery must stop immediately.
     */
    BACKEND_RESYNC_PROBE_MISSING_WATCH,

    /*
     * The recovery path was called for a watch that is not currently STALE.
     * Resync is valid only after IN_MOVE_SELF/IN_DELETE_SELF style handling has
     * marked the watch as stale; otherwise probing would race normal operation.
     */
    BACKEND_RESYNC_PROBE_NOT_STALE,

    /*
     * The backend could not move the watch from STALE to RESYNCING. Without the
     * intermediate state, later logs would suggest a recovery attempt that the
     * watcher table did not actually record.
     */
    BACKEND_RESYNC_PROBE_SET_RESYNCING_FAILED,

    /*
     * stat(2) could not reach the old watched path. The original object may
     * still exist elsewhere, but the backend cannot repair this watch through
     * the saved path.
     */
    BACKEND_RESYNC_PROBE_PATH_UNREACHABLE,

    /*
     * The old path is reachable but is no longer a directory. Recursive watch
     * recovery is directory-scoped, so this path cannot be used as a safe resync
     * root.
     */
    BACKEND_RESYNC_PROBE_NOT_DIRECTORY,

    /*
     * The backend tried to leave a failed probe in STALE state but the watcher
     * table update failed. This is a bookkeeping failure after another recovery
     * check has already failed.
     */
    BACKEND_RESYNC_PROBE_SET_STALE_FAILED,

    /*
     * The probe succeeded, but the backend could not move the watch back to
     * VALID. Keeping this as a distinct outcome makes state-transition bugs
     * visible in diagnostic logs.
     */
    BACKEND_RESYNC_PROBE_SET_VALID_FAILED,

    /*
     * The watcher entry has no saved filesystem identity. The backend cannot
     * compare the current path with the originally watched object, so it refuses
     * to trust the path.
     */
    BACKEND_RESYNC_PROBE_MISSING_IDENTITY,

    /*
     * The old path exists, but its current (st_dev, st_ino) pair differs from
     * the identity captured when the watch was installed. This means the path
     * was reused by another object and must not be repaired as the old watch.
     */
    BACKEND_RESYNC_PROBE_IDENTITY_MISMATCH,

    /*
     * The identity check passed and the scanner found a missing child watch,
     * but the first limited watch reinstallation failed. The parent watch is
     * left STALE because the subtree coverage is known to be incomplete.
     */
    BACKEND_RESYNC_PROBE_REINSTALL_FAILED
} backend_resync_probe_result_t;

/* ============================================================================
 * Local Declarations
 * ========================================================================== */

static void backend_handle_dir_create(inotify_backend_context_t *ctx,
                                      const struct inotify_event *ev,
                                      inotify_backend_event_fn on_event,
                                      void *userdata);

static int backend_init(inotify_backend_context_t *ctx);

static int backend_add_startup_watch(inotify_backend_context_t *ctx,
                                     const char *path);

static int backend_configured_roots_add(inotify_backend_t *runtime,
                                        const char *path);

static void backend_configured_roots_destroy(inotify_backend_t *runtime);

static const char *backend_configured_root_for_path(
    const inotify_backend_t *runtime,
    const char *path
);

static int backend_path_matches_prefix(const char *path, const char *prefix);

static void backend_shutdown(inotify_backend_context_t *ctx);

static int backend_process_scanned_dir_create(const fs_scan_entry_t *entry,
                                              void *userdata);

static void backend_handle_ignored(inotify_backend_context_t *ctx,
                                   const struct inotify_event *ev);

static void backend_handle_move_self(inotify_backend_context_t *ctx,
                                     const struct inotify_event *ev);

static void backend_handle_delete_self(inotify_backend_context_t *ctx,
                                       const struct inotify_event *ev);

static void backend_handle_unmount(inotify_backend_context_t *ctx,
                                   const struct inotify_event *ev);

static void backend_resync_watch(inotify_backend_context_t *ctx,
                                 int wd,
                                 const char *reason);

static backend_resync_probe_result_t backend_probe_stale_watch_identity(
    inotify_backend_context_t *ctx,
    int wd,
    const char *reason
);

static int backend_resync_probe_result_needs_lost_scope(
    backend_resync_probe_result_t result
);

static void backend_enqueue_lost_scope(inotify_backend_context_t *ctx,
                                       int wd,
                                       const char *path,
                                       const char *reason,
                                       backend_resync_probe_result_t result);

static error_t backend_resync_watch_subtree_dirs(inotify_backend_context_t *ctx,
                                                int wd,
                                                const char *path,
                                                const char *reason);

static error_t backend_resync_scan_subtree_dirs(
    inotify_backend_context_t *ctx,
    const char *path,
    int emit_root,
    backend_resync_scan_context_t *scan_context
);

static error_t backend_resync_reinstall_missing_watches(
    inotify_backend_context_t *ctx,
    int wd,
    const char *path,
    const char *reason,
    const backend_resync_scan_context_t *scan_context,
    const backend_resync_watch_ops_t *ops
);

static error_t backend_lost_scope_reinstall_missing_watches(
    inotify_backend_context_t *ctx,
    int wd,
    const char *path,
    const char *reason,
    const backend_resync_scan_context_t *scan_context,
    const backend_resync_watch_ops_t *ops
);

static int backend_resync_watch_manager_add(inotify_backend_context_t *ctx,
                                            const char *path);

static int backend_resync_watch_manager_remove(inotify_backend_context_t *ctx,
                                               int wd);

static int backend_resync_scan_add_missing_path(
    backend_resync_scan_context_t *scan_context,
    const char *path
);

static void backend_resync_scan_context_destroy(
    backend_resync_scan_context_t *scan_context
);

static int backend_count_resync_scanned_dir(const fs_scan_entry_t *entry,
                                            void *userdata);

static backend_resync_scan_class_t backend_classify_resync_scan(
    const backend_resync_scan_context_t *scan_context
);

static const char *backend_resync_scan_class_name(
    backend_resync_scan_class_t scan_class
);

static const char *backend_resync_probe_result_name(
    backend_resync_probe_result_t result
);

static int backend_lost_scope_queue_init(inotify_lost_scope_queue_t *queue,
                                         size_t capacity);

static void backend_lost_scope_queue_destroy(
    inotify_lost_scope_queue_t *queue
);

static BACKEND_MAYBE_UNUSED size_t backend_lost_scope_queue_count(
    const inotify_lost_scope_queue_t *queue
);

static BACKEND_MAYBE_UNUSED int backend_lost_scope_queue_enqueue(
    inotify_lost_scope_queue_t *queue,
    int wd,
    const char *old_path,
    dev_t device_id,
    ino_t inode_id,
    const char *scan_root,
    const char *reason,
    uint64_t first_seen_ns,
    uint64_t retry_after_ns
);

static int backend_lost_scope_queue_enqueue_entry(
    inotify_lost_scope_queue_t *queue,
    const inotify_lost_scope_entry_t *source
);

static BACKEND_MAYBE_UNUSED int backend_lost_scope_queue_pop(
    inotify_lost_scope_queue_t *queue,
    inotify_lost_scope_entry_t *out
);

static const inotify_lost_scope_entry_t *backend_lost_scope_queue_peek(
    const inotify_lost_scope_queue_t *queue
);

static int backend_lost_scope_queue_expand(
    inotify_lost_scope_queue_t *queue
);

static BACKEND_MAYBE_UNUSED backend_lost_scope_recovery_result_t
backend_lost_scope_recover_next(inotify_backend_context_t *ctx,
                                const char *root,
                                char *found_path,
                                size_t found_path_size);

static backend_lost_scope_recovery_result_t
backend_lost_scope_recover_next_with_ops(
    inotify_backend_context_t *ctx,
    const char *root,
    char *found_path,
    size_t found_path_size,
    const backend_resync_watch_ops_t *watch_ops
);

static backend_lost_scope_recovery_result_t
backend_lost_scope_recover_entry_with_ops(
    inotify_backend_context_t *ctx,
    const inotify_lost_scope_entry_t *entry,
    const char *root,
    char *found_path,
    size_t found_path_size,
    const backend_resync_watch_ops_t *watch_ops
);

static BACKEND_MAYBE_UNUSED size_t backend_lost_scope_process_due_with_ops(
    inotify_backend_context_t *ctx,
    const char *root,
    uint64_t now_ns,
    size_t batch_size,
    const backend_resync_watch_ops_t *watch_ops
);

static size_t backend_lost_scope_process_due_runtime(
    inotify_backend_context_t *ctx
);

static BACKEND_MAYBE_UNUSED size_t
backend_lost_scope_process_due_runtime_with_ops(
    inotify_backend_context_t *ctx,
    const backend_resync_watch_ops_t *watch_ops
);

static uint64_t backend_lost_scope_retry_delay_ns(unsigned failed_attempts);

static int backend_lost_scope_schedule_retry(
    inotify_backend_context_t *ctx,
    const inotify_lost_scope_entry_t *attempted,
    const char *retry_path,
    backend_lost_scope_recovery_result_t result,
    uint64_t now_ns
);

static const char *backend_lost_scope_recovery_result_name(
    backend_lost_scope_recovery_result_t result
);

static int backend_lost_scope_scan_for_identity(const fs_scan_entry_t *entry,
                                                void *userdata);

static void backend_log_resync_failure(inotify_backend_context_t *ctx,
                                       int wd,
                                       const char *path,
                                       const char *reason,
                                       backend_resync_probe_result_t result,
                                       int saved_errno);

static int backend_log_resync_record(inotify_backend_context_t *ctx,
                                     alfred_record_type_t type,
                                     int wd,
                                     const char *path,
                                     const char *reason,
                                     const char *state,
                                     const char *detail_path,
                                     int related_watch_id,
                                     int result_code,
                                     size_t directories_seen,
                                     size_t directories_watched,
                                     size_t directories_missing);

static int backend_log_lost_scope_record(inotify_backend_context_t *ctx,
                                         alfred_record_type_t type,
                                         int wd,
                                         const char *path,
                                         const char *old_path,
                                         const char *new_path,
                                         const char *reason,
                                         const char *state,
                                         const char *error,
                                         const char *detail_path,
                                         int related_watch_id,
                                         size_t directories_seen,
                                         size_t directories_watched,
                                         size_t directories_missing,
                                         size_t pending_count,
                                         size_t children_count,
                                         size_t watches_count,
                                         unsigned retry_count,
                                         uint64_t delay_ms);

static int backend_build_overflow_raw(const struct inotify_event *ev,
                                      alfred_raw_event_t *out);

static int backend_raw_mask_has_dispatch_action(uint32_t mask);

static void backend_raw_event_name_from_mask(uint32_t mask,
                                             char *dest,
                                             size_t dest_size);

static int backend_poll(inotify_backend_context_t *ctx,
                        inotify_backend_event_fn on_event,
                        void *userdata);

static uint64_t backend_now_ns(void);

static int backend_emit_synthetic_dir_create(
    inotify_backend_context_t *ctx,
    const char *path,
    inotify_backend_event_fn on_event,
    void *userdata
);

static const backend_resync_watch_ops_t BACKEND_RESYNC_WATCH_MANAGER_OPS = {
    .add = backend_resync_watch_manager_add,
    .remove = backend_resync_watch_manager_remove
};

/* ============================================================================
 * Lost Scope Queue
 * ========================================================================== */

/*
 * backend_lost_scope_queue_init - initialize delayed recovery storage
 * @queue: queue object owned by inotify_backend_t
 * @capacity: initial slot count, or 0 for the default
 *
 * The lost-scope queue stores stale watched scopes that cannot be repaired by
 * the immediate old-path identity probe. This first implementation is only a
 * single-threaded FIFO data structure; later code can add debounce, backoff,
 * worker-thread ownership, and monitored-root scanning on top of the same
 * recorded evidence.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
static int backend_lost_scope_queue_init(inotify_lost_scope_queue_t *queue,
                                         size_t capacity)
{
    if (queue == NULL)
        return -1;

    memset(queue, 0, sizeof(*queue));

    if (capacity == 0)
        capacity = 8;

    queue->items =
        calloc(capacity, sizeof(inotify_lost_scope_entry_t));

    if (queue->items == NULL)
        return -1;

    queue->capacity = capacity;
    return 0;
}

/*
 * backend_lost_scope_queue_destroy - release delayed recovery storage
 * @queue: queue object owned by inotify_backend_t
 *
 * Entries store fixed-size path/reason copies, so cleanup only frees the
 * backing array and resets counters. Resetting makes repeated test setup and
 * partial backend initialization failure paths deterministic.
 */
static void backend_lost_scope_queue_destroy(inotify_lost_scope_queue_t *queue)
{
    if (queue == NULL)
        return;

    free(queue->items);

    queue->items = NULL;
    queue->count = 0;
    queue->capacity = 0;
    queue->head = 0;
}

/*
 * backend_lost_scope_queue_count - read the number of pending recoveries
 * @queue: queue to inspect
 *
 * Return: pending entry count, or 0 for a NULL queue.
 */
static size_t backend_lost_scope_queue_count(
    const inotify_lost_scope_queue_t *queue
)
{
    if (queue == NULL)
        return 0;

    return queue->count;
}

/*
 * backend_lost_scope_queue_enqueue - store one stale scope for later recovery
 * @queue: queue object owned by inotify_backend_t
 * @wd: inotify watch descriptor that became stale
 * @old_path: last path associated with @wd before trust was lost
 * @device_id: st_dev captured when the watch was installed
 * @inode_id: st_ino captured when the watch was installed
 * @scan_root: bounded root to search before wider fallback policies
 * @reason: backend/kernel reason for the lost scope
 * @first_seen_ns: monotonic time when the scope was queued
 * @retry_after_ns: earliest monotonic time for the next recovery attempt
 *
 * The function copies @old_path and @reason into the queue entry. Callers may
 * pass stack or watcher-table pointers without extending their lifetime. The
 * queue grows as needed because a burst of stale scopes should not be silently
 * dropped before policy has a chance to batch and process them.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
static int backend_lost_scope_queue_enqueue(
    inotify_lost_scope_queue_t *queue,
    int wd,
    const char *old_path,
    dev_t device_id,
    ino_t inode_id,
    const char *scan_root,
    const char *reason,
    uint64_t first_seen_ns,
    uint64_t retry_after_ns
)
{
    inotify_lost_scope_entry_t entry;

    if (queue == NULL || old_path == NULL || scan_root == NULL ||
        reason == NULL) {
        return -1;
    }

    if (wd < 0)
        return -1;

    memset(&entry, 0, sizeof(entry));

    entry.wd = wd;
    entry.device_id = device_id;
    entry.inode_id = inode_id;
    entry.first_seen_ns = first_seen_ns;
    entry.retry_after_ns = retry_after_ns;
    entry.retry_count = 0;

    snprintf(entry.old_path,
             sizeof(entry.old_path),
             "%s",
             old_path);
    snprintf(entry.scan_root,
             sizeof(entry.scan_root),
             "%s",
             scan_root);
    snprintf(entry.reason,
             sizeof(entry.reason),
             "%s",
             reason);

    return backend_lost_scope_queue_enqueue_entry(queue, &entry);
}

/*
 * backend_lost_scope_queue_enqueue_entry - append an existing recovery entry
 * @queue: FIFO queue that stores delayed recovery work
 * @source: already-built entry to copy into the queue tail
 *
 * The normal enqueue helper creates the first recovery record with
 * retry_count=0. Retry scheduling needs a lower-level primitive because it must
 * preserve first_seen_ns, saved identity, and incremented retry_count while
 * only changing retry_after_ns and, when useful, the best known stale path.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
static int backend_lost_scope_queue_enqueue_entry(
    inotify_lost_scope_queue_t *queue,
    const inotify_lost_scope_entry_t *source
)
{
    size_t tail;

    if (queue == NULL || source == NULL)
        return -1;

    if (source->wd < 0)
        return -1;

    if (source->old_path[0] == '\0' || source->scan_root[0] == '\0' ||
        source->reason[0] == '\0') {
        return -1;
    }

    if (queue->count == queue->capacity) {
        if (backend_lost_scope_queue_expand(queue) != 0)
            return -1;
    }

    tail = (queue->head + queue->count) % queue->capacity;
    queue->items[tail] = *source;
    queue->count++;

    return 0;
}

/*
 * backend_lost_scope_queue_pop - remove the oldest queued recovery request
 * @queue: queue object owned by inotify_backend_t
 * @out: destination for a copy of the popped entry
 *
 * The pop operation copies the entry out instead of returning an internal
 * pointer. That keeps future worker/scanner code from holding references into
 * a circular buffer that may later grow or be reused.
 *
 * Return: 0 on success, -1 for invalid input or an empty queue.
 */
static int backend_lost_scope_queue_pop(inotify_lost_scope_queue_t *queue,
                                        inotify_lost_scope_entry_t *out)
{
    if (queue == NULL || out == NULL)
        return -1;

    if (queue->count == 0)
        return -1;

    *out = queue->items[queue->head];
    memset(&queue->items[queue->head],
           0,
           sizeof(queue->items[queue->head]));

    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;

    if (queue->count == 0)
        queue->head = 0;

    return 0;
}

/*
 * backend_lost_scope_queue_peek - inspect the oldest queued recovery request
 * @queue: queue object owned by inotify_backend_t
 *
 * The due-entry processor must check retry_after_ns before deciding whether
 * the head entry can be consumed. Returning a const pointer keeps this helper
 * read-only: callers can inspect scheduling metadata but cannot mutate FIFO
 * state or hold ownership of the entry.
 *
 * Return: pointer to the oldest queued entry, or NULL when the queue is empty.
 */
static const inotify_lost_scope_entry_t *backend_lost_scope_queue_peek(
    const inotify_lost_scope_queue_t *queue
)
{
    if (queue == NULL || queue->count == 0)
        return NULL;

    return &queue->items[queue->head];
}

/*
 * backend_lost_scope_queue_expand - double FIFO storage without reordering
 * @queue: queue object owned by inotify_backend_t
 *
 * The queue uses a circular buffer so repeated pop/enqueue operations are
 * cheap. Growing a wrapped buffer must linearize entries in FIFO order;
 * otherwise a delayed resync worker would process scopes in a different order
 * from the one in which path trust was lost.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
static int backend_lost_scope_queue_expand(
    inotify_lost_scope_queue_t *queue
)
{
    if (queue == NULL || queue->capacity == 0)
        return -1;

    size_t new_capacity = queue->capacity * 2;

    if (new_capacity < queue->capacity)
        return -1;

    inotify_lost_scope_entry_t *tmp =
        calloc(new_capacity, sizeof(inotify_lost_scope_entry_t));

    if (tmp == NULL)
        return -1;

    for (size_t i = 0; i < queue->count; i++) {
        size_t old_index = (queue->head + i) % queue->capacity;
        tmp[i] = queue->items[old_index];
    }

    free(queue->items);

    queue->items = tmp;
    queue->capacity = new_capacity;
    queue->head = 0;

    return 0;
}

/*
 * backend_lost_scope_recover_next - synchronously search one queued identity
 * @ctx: backend context that owns the lost-scope queue and logger
 * @root: monitored root inside which the identity may be searched
 * @found_path: caller buffer that receives the discovered current path
 * @found_path_size: size of @found_path
 *
 * This helper is intentionally synchronous and limited. It consumes one queued
 * lost scope, scans one caller-provided root, and reports whether a directory
 * with the saved (st_dev, st_ino) identity was found. If the identity is found,
 * it repairs watcher-table paths for the recovered root and its already-known
 * children by replacing the stale old prefix with the discovered path. It then
 * measures coverage, reinstalls all missing watches with all-or-stale rollback,
 * and returns the recovered subtree to VALID only after every step succeeds.
 *
 * Return: recovery result describing empty queue, found, not found, or scan
 * failure.
 */
static backend_lost_scope_recovery_result_t
backend_lost_scope_recover_next(inotify_backend_context_t *ctx,
                                const char *root,
                                char *found_path,
                                size_t found_path_size)
{
    return backend_lost_scope_recover_next_with_ops(
        ctx,
        root,
        found_path,
        found_path_size,
        &BACKEND_RESYNC_WATCH_MANAGER_OPS
    );
}

/*
 * backend_lost_scope_recover_next_with_ops - recover one lost scope with ops
 * @ctx: backend context that owns the lost-scope queue and logger
 * @root: monitored root inside which the identity may be searched
 * @found_path: caller buffer that receives the discovered current path
 * @found_path_size: size of @found_path
 * @watch_ops: watch installation/removal operations for runtime or tests
 *
 * The public-in-this-file wrapper uses the real watch manager operations. Unit
 * tests can pass fake operations to exercise the all-or-stale branch without
 * depending on kernel inotify descriptors or filesystem races.
 *
 * Return: recovery result describing empty queue, found, not found, or scan
 * failure.
 */
static backend_lost_scope_recovery_result_t
backend_lost_scope_recover_next_with_ops(
    inotify_backend_context_t *ctx,
    const char *root,
    char *found_path,
    size_t found_path_size,
    const backend_resync_watch_ops_t *watch_ops
)
{
    inotify_lost_scope_entry_t entry;

    if (ctx == NULL || ctx->runtime == NULL || root == NULL ||
        found_path == NULL || found_path_size == 0 || watch_ops == NULL) {
        return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
    }

    if (backend_lost_scope_queue_pop(&ctx->runtime->lost_scopes,
                                     &entry) != 0) {
        return BACKEND_LOST_SCOPE_RECOVERY_EMPTY;
    }

    return backend_lost_scope_recover_entry_with_ops(ctx,
                                                     &entry,
                                                     root,
                                                     found_path,
                                                     found_path_size,
                                                     watch_ops);
}

/*
 * backend_lost_scope_recover_entry_with_ops - try one root for one lost entry
 * @ctx: backend context that owns watcher state and logger
 * @entry: queued recovery entry already removed from the FIFO
 * @root: monitored root inside which the identity may be searched
 * @found_path: caller buffer that receives the discovered current path
 * @found_path_size: size of @found_path
 * @watch_ops: watch installation/removal operations for runtime or tests
 *
 * This helper does not pop from the queue. That lets the due processor try the
 * entry's primary scan_root first and, when the result is a clean NOT_FOUND,
 * try other configured roots before deciding whether the entry should be
 * requeued. Technical failures stay conservative and stop the attempt.
 *
 * Return: recovery result for this root.
 */
static backend_lost_scope_recovery_result_t
backend_lost_scope_recover_entry_with_ops(
    inotify_backend_context_t *ctx,
    const inotify_lost_scope_entry_t *entry,
    const char *root,
    char *found_path,
    size_t found_path_size,
    const backend_resync_watch_ops_t *watch_ops
)
{
    if (ctx == NULL || ctx->runtime == NULL || entry == NULL ||
        root == NULL || found_path == NULL || found_path_size == 0 ||
        watch_ops == NULL) {
        return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
    }

    found_path[0] = '\0';

    (void)backend_log_lost_scope_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN,
        -1,
        root,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        0,
        0,
        backend_lost_scope_queue_count(&ctx->runtime->lost_scopes),
        0,
        0,
        0,
        0);

    backend_lost_scope_scan_context_t scan_context;

    memset(&scan_context, 0, sizeof(scan_context));
    scan_context.entry = entry;
    scan_context.found_path = found_path;
    scan_context.found_path_size = found_path_size;

    fs_scan_options_t opts;

    fs_scan_options_defaults(&opts);
    opts.include_dirs = 1;
    opts.include_files = 0;
    opts.include_symlinks = 0;
    opts.include_other = 0;
    opts.follow_symlinks = 0;
    opts.strict_child_errors = 1;
    opts.emit_root = 1;

    error_t rc =
        fs_scan_tree(root,
                     &opts,
                     backend_lost_scope_scan_for_identity,
                     &scan_context);

    if (rc != ERR_OK) {
        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
            entry->wd,
            entry->old_path,
            NULL,
            NULL,
            entry->reason,
            NULL,
            "scan-failed",
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);
        return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
    }

    if (scan_context.found) {
        size_t updated_count = 0;

        if (!watcher_exists(&ctx->runtime->watchers, entry->wd)) {
            (void)backend_log_lost_scope_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
                entry->wd,
                entry->old_path,
                NULL,
                NULL,
                entry->reason,
                NULL,
                "update-path-failed",
                NULL,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0);
            return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
        }

        /*
         * Rewrite the recovered root and any child watches in one validated
         * watcher-table operation. Calling watcher_update_path() first would
         * repair the root before knowing whether children can be repaired,
         * which could leave mixed old/new prefixes after an overflow failure.
         */
        if (watcher_update_path_prefix(&ctx->runtime->watchers,
                                       entry->old_path,
                                       found_path,
                                       &updated_count) != 0 ||
            updated_count == 0) {
            (void)backend_log_lost_scope_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
                entry->wd,
                entry->old_path,
                NULL,
                NULL,
                entry->reason,
                NULL,
                "update-prefix-failed",
                NULL,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0);
            return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
        }

        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_FOUND,
            entry->wd,
            NULL,
            entry->old_path,
            found_path,
            entry->reason,
            NULL,
            NULL,
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);
        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED,
            entry->wd,
            NULL,
            entry->old_path,
            found_path,
            NULL,
            NULL,
            NULL,
            NULL,
            0,
            0,
            0,
            0,
            0,
            updated_count - 1,
            0,
            0,
            0);

        backend_resync_scan_context_t coverage_context;

        error_t coverage_rc =
            backend_resync_scan_subtree_dirs(ctx,
                                             found_path,
                                             1,
                                             &coverage_context);

        if (coverage_rc != ERR_OK) {
            (void)backend_log_lost_scope_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
                entry->wd,
                found_path,
                NULL,
                NULL,
                entry->reason,
                NULL,
                "coverage-scan-failed",
                NULL,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0);
            backend_resync_scan_context_destroy(&coverage_context);
            return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
        }

        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE,
            entry->wd,
            found_path,
            NULL,
            NULL,
            entry->reason,
            NULL,
            NULL,
            NULL,
            0,
            coverage_context.directories_seen,
            coverage_context.directories_watched,
            coverage_context.directories_missing_watch,
            0,
            0,
            0,
            0,
            0);

        for (size_t i = 0; i < coverage_context.missing_paths_count; i++) {
            (void)backend_log_lost_scope_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING,
                entry->wd,
                found_path,
                NULL,
                NULL,
                entry->reason,
                NULL,
                NULL,
                coverage_context.missing_paths[i],
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0);
        }

        backend_resync_scan_class_t coverage_class =
            backend_classify_resync_scan(&coverage_context);

        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS,
            entry->wd,
            found_path,
            NULL,
            NULL,
            entry->reason,
            backend_resync_scan_class_name(coverage_class),
            NULL,
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);

        error_t reinstall_rc =
            backend_lost_scope_reinstall_missing_watches(ctx,
                                                         entry->wd,
                                                         found_path,
                                                         entry->reason,
                                                         &coverage_context,
                                                         watch_ops);

        if (reinstall_rc != ERR_OK) {
            (void)backend_log_lost_scope_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
                entry->wd,
                found_path,
                NULL,
                NULL,
                entry->reason,
                NULL,
                "reinstall-failed",
                NULL,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0);
            backend_resync_scan_context_destroy(&coverage_context);
            return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
        }

        size_t valid_count = 0;

        if (watcher_set_state_prefix(&ctx->runtime->watchers,
                                     found_path,
                                     WATCHER_STATE_VALID,
                                     &valid_count) != 0 ||
            valid_count == 0) {
            (void)backend_log_lost_scope_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
                entry->wd,
                found_path,
                NULL,
                NULL,
                entry->reason,
                NULL,
                "set-valid-failed",
                NULL,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0);
            backend_resync_scan_context_destroy(&coverage_context);
            return BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED;
        }

        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END,
            entry->wd,
            found_path,
            NULL,
            NULL,
            entry->reason,
            "valid",
            NULL,
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            valid_count,
            0,
            0);

        backend_resync_scan_context_destroy(&coverage_context);
        return BACKEND_LOST_SCOPE_RECOVERY_FOUND;
    }

    (void)backend_log_lost_scope_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND,
        entry->wd,
        entry->old_path,
        NULL,
        NULL,
        entry->reason,
        NULL,
        NULL,
        NULL,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        entry->retry_count,
        0);
    return BACKEND_LOST_SCOPE_RECOVERY_NOT_FOUND;
}

/*
 * backend_lost_scope_process_due_with_ops - process mature queued recoveries
 * @ctx: backend context that owns the lost-scope queue and logger
 * @root: legacy fallback root for entries that do not carry scan_root
 * @now_ns: current monotonic time used for retry_after_ns comparisons
 * @batch_size: maximum number of due entries to process in this call
 * @watch_ops: watch installation/removal operations for runtime or tests
 *
 * This is the single-threaded bridge toward the future worker. It does not
 * sleep, does not spawn a thread, and does not scan unbounded work. It only
 * consumes FIFO head entries whose retry_after_ns is mature at @now_ns and
 * stops when the queue is empty, @batch_size is reached, or the next head entry
 * is still delayed.
 *
 * The function intentionally stops at the first non-due head entry instead of
 * searching later slots. That preserves FIFO ordering and makes future backoff
 * behavior easier to reason about. A failed attempt is appended again with a
 * later retry_after_ns, so one delayed scope does not permanently block other
 * due entries that were already waiting behind it.
 *
 * Each lost-scope entry carries its own scan_root. That field is the bounded
 * root chosen when the backend queued the stale scope, so the processor uses it
 * instead of guessing from the caller. @root remains a defensive fallback for
 * older tests or transitional call paths that might build entries manually.
 * A clean NOT_FOUND on scan_root triggers a search across the other configured
 * roots before retry/backoff is charged. Technical failures do not fall through
 * to other roots because a failed scan is not reliable absence evidence.
 *
 * Return: number of entries attempted.
 */
static size_t backend_lost_scope_process_due_with_ops(
    inotify_backend_context_t *ctx,
    const char *root,
    uint64_t now_ns,
    size_t batch_size,
    const backend_resync_watch_ops_t *watch_ops
)
{
    size_t processed = 0;

    if (ctx == NULL || ctx->runtime == NULL || root == NULL ||
        watch_ops == NULL || batch_size == 0) {
        return 0;
    }

    while (processed < batch_size) {
        const inotify_lost_scope_entry_t *entry =
            backend_lost_scope_queue_peek(&ctx->runtime->lost_scopes);

        if (entry == NULL)
            break;

        if (entry->retry_after_ns > now_ns)
            break;

        inotify_lost_scope_entry_t attempted;

        if (backend_lost_scope_queue_pop(&ctx->runtime->lost_scopes,
                                         &attempted) != 0) {
            break;
        }

        const char *scan_root = attempted.scan_root;

        if (scan_root[0] == '\0')
            scan_root = root;

        char found_path[PATH_MAX];
        backend_lost_scope_recovery_result_t result =
            backend_lost_scope_recover_entry_with_ops(ctx,
                                                      &attempted,
                                                      scan_root,
                                                      found_path,
                                                      sizeof(found_path),
                                                      watch_ops);

        /*
         * A clean NOT_FOUND only says the identity was not inside the primary
         * scan_root. If the backend has other configured roots, try them before
         * spending retry budget. Technical failures stop immediately because a
         * failed scan is not reliable evidence that the identity is absent.
         */
        if (result == BACKEND_LOST_SCOPE_RECOVERY_NOT_FOUND) {
            for (size_t i = 0;
                 i < ctx->runtime->configured_roots_count &&
                 result == BACKEND_LOST_SCOPE_RECOVERY_NOT_FOUND;
                 i++) {

                const char *candidate = ctx->runtime->configured_roots[i];

                if (strcmp(candidate, scan_root) == 0)
                    continue;

                result =
                    backend_lost_scope_recover_entry_with_ops(ctx,
                                                              &attempted,
                                                              candidate,
                                                              found_path,
                                                              sizeof(found_path),
                                                              watch_ops);
            }
        }

        if (result == BACKEND_LOST_SCOPE_RECOVERY_NOT_FOUND ||
            result == BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED) {
            const char *retry_path = attempted.old_path;

            /*
             * Some technical failures happen after the identity was found and
             * watcher-table prefixes were already rewritten. In that case the
             * next retry must use the best current path, not the original stale
             * path, otherwise the retry would try to rewrite an old prefix that
             * no longer exists in the watcher table.
             */
            if (found_path[0] != '\0')
                retry_path = found_path;

            backend_lost_scope_schedule_retry(ctx,
                                              &attempted,
                                              retry_path,
                                              result,
                                              now_ns);
        }

        processed++;
    }

    return processed;
}

/*
 * backend_lost_scope_process_due_runtime - process a tiny runtime batch
 * @ctx: backend context used by the inotify poll path
 *
 * The poll loop calls this helper after an idle read and after a consumed
 * event buffer. Recovery is intentionally synchronous in this first runtime
 * integration step: it keeps locking, worker shutdown, and debounce policy out
 * of the design until tests show that a separate worker is needed.
 *
 * The batch size is deliberately one. A lost-scope scan may traverse part of a
 * monitored tree, so each poll iteration spends a bounded amount of recovery
 * work and then returns to fresh kernel events.
 *
 * Return: number of mature entries attempted.
 */
static size_t backend_lost_scope_process_due_runtime(
    inotify_backend_context_t *ctx
)
{
    return backend_lost_scope_process_due_runtime_with_ops(
        ctx,
        &BACKEND_RESYNC_WATCH_MANAGER_OPS);
}

/*
 * backend_lost_scope_process_due_runtime_with_ops - testable runtime wrapper
 * @ctx: backend context used by the inotify poll path
 * @watch_ops: watch operations used when recovery reinstalls missing watches
 *
 * Runtime entries normally carry scan_root, so a mature entry can always drive
 * its first bounded scan from its own data. The configured root list is still
 * useful as a defensive fallback for older manually-built entries and, more
 * importantly, for cross-root recovery after a clean NOT_FOUND on scan_root. If
 * no configured root exists and the head entry has no scan_root either, the
 * helper leaves the queue untouched because scanning an unbounded or empty path
 * would make the recovery contract harder to reason about.
 *
 * Return: number of mature entries attempted.
 */
static BACKEND_MAYBE_UNUSED size_t
backend_lost_scope_process_due_runtime_with_ops(
    inotify_backend_context_t *ctx,
    const backend_resync_watch_ops_t *watch_ops
)
{
    const inotify_lost_scope_entry_t *entry;
    const char *fallback_root;
    uint64_t now_ns;

    if (ctx == NULL || ctx->runtime == NULL || watch_ops == NULL)
        return 0;

    entry = backend_lost_scope_queue_peek(&ctx->runtime->lost_scopes);

    if (entry == NULL)
        return 0;

    if (ctx->runtime->configured_roots_count > 0)
        fallback_root = ctx->runtime->configured_roots[0];
    else if (entry->scan_root[0] != '\0')
        fallback_root = entry->scan_root;
    else
        return 0;

    now_ns = backend_now_ns();

    return backend_lost_scope_process_due_with_ops(
        ctx,
        fallback_root,
        now_ns,
        BACKEND_LOST_SCOPE_POLL_BATCH_SIZE,
        watch_ops);
}

/*
 * backend_lost_scope_retry_delay_ns - choose the next retry delay
 * @failed_attempts: number of failed attempts including the one just completed
 *
 * The first version uses a small fixed backoff table. It avoids a tight loop
 * when a scope is still outside the scanned root and keeps later retries slow
 * enough for the event loop to prioritize fresh kernel events. The values are
 * deliberately internal constants until the recovery contract is stable enough
 * to expose them through configuration.
 *
 * Return: delay in nanoseconds before the next attempt may run.
 */
static uint64_t backend_lost_scope_retry_delay_ns(unsigned failed_attempts)
{
    static const uint64_t delays_ms[] = {
        100,
        250,
        500,
        1000,
        2000
    };
    size_t index;

    if (failed_attempts == 0)
        failed_attempts = 1;

    index = failed_attempts - 1;

    if (index >= sizeof(delays_ms) / sizeof(delays_ms[0]))
        index = (sizeof(delays_ms) / sizeof(delays_ms[0])) - 1;

    return delays_ms[index] * BACKEND_LOST_SCOPE_NS_PER_MS;
}

/*
 * backend_lost_scope_schedule_retry - requeue or abandon a failed recovery
 * @ctx: backend context that owns the queue and logger
 * @attempted: entry consumed by the failed attempt
 * @retry_path: best path to preserve for the next attempt
 * @result: failure class returned by the recovery attempt
 * @now_ns: current monotonic time used as the new scheduling base
 *
 * NOT_FOUND and technical failures are not treated as immediate permanent
 * loss. The watched object may reappear under a scanned root, or a transient
 * scan/watch error may clear on a later pass. This helper increments the retry
 * counter, computes a conservative backoff, and appends the entry to the queue
 * tail. When the bounded attempt budget is exhausted, it emits a diagnostic and
 * leaves the watcher stale instead of looping forever.
 *
 * Return: 0 when the entry is requeued, -1 when it is abandoned or cannot be
 * requeued.
 */
static int backend_lost_scope_schedule_retry(
    inotify_backend_context_t *ctx,
    const inotify_lost_scope_entry_t *attempted,
    const char *retry_path,
    backend_lost_scope_recovery_result_t result,
    uint64_t now_ns
)
{
    inotify_lost_scope_entry_t retry_entry;
    unsigned failed_attempts;
    uint64_t delay_ns;

    if (ctx == NULL || ctx->runtime == NULL || attempted == NULL ||
        retry_path == NULL || retry_path[0] == '\0') {
        return -1;
    }

    failed_attempts = attempted->retry_count + 1;

    if (failed_attempts >= BACKEND_LOST_SCOPE_MAX_ATTEMPTS) {
        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP,
            attempted->wd,
            retry_path,
            NULL,
            NULL,
            attempted->reason,
            backend_lost_scope_recovery_result_name(result),
            NULL,
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            failed_attempts,
            0);
        return -1;
    }

    retry_entry = *attempted;
    retry_entry.retry_count = failed_attempts;
    delay_ns = backend_lost_scope_retry_delay_ns(failed_attempts);
    retry_entry.retry_after_ns = now_ns + delay_ns;

    snprintf(retry_entry.old_path,
             sizeof(retry_entry.old_path),
             "%s",
             retry_path);

    if (backend_lost_scope_queue_enqueue_entry(&ctx->runtime->lost_scopes,
                                               &retry_entry) != 0) {
        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
            attempted->wd,
            retry_path,
            NULL,
            NULL,
            attempted->reason,
            NULL,
            "requeue-failed",
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);
        return -1;
    }

    (void)backend_log_lost_scope_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED,
        retry_entry.wd,
        retry_entry.old_path,
        NULL,
        NULL,
        retry_entry.reason,
        backend_lost_scope_recovery_result_name(result),
        NULL,
        NULL,
        0,
        0,
        0,
        0,
        backend_lost_scope_queue_count(&ctx->runtime->lost_scopes),
        0,
        0,
        retry_entry.retry_count,
        delay_ns / BACKEND_LOST_SCOPE_NS_PER_MS);

    return 0;
}

/*
 * backend_lost_scope_recovery_result_name - stringify recovery result
 * @result: internal recovery outcome
 *
 * Retry diagnostics need a stable text value that explains why the entry is
 * being scheduled again or abandoned. These names are backend diagnostics, not
 * Alfred raw/core event names.
 *
 * Return: static string for @result.
 */
static const char *backend_lost_scope_recovery_result_name(
    backend_lost_scope_recovery_result_t result
)
{
    switch (result) {
    case BACKEND_LOST_SCOPE_RECOVERY_EMPTY:
        return "empty";
    case BACKEND_LOST_SCOPE_RECOVERY_FOUND:
        return "found";
    case BACKEND_LOST_SCOPE_RECOVERY_NOT_FOUND:
        return "not-found";
    case BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED:
        return "scan-failed";
    default:
        return "unknown";
    }
}

/*
 * backend_lost_scope_scan_for_identity - scanner callback for one lost scope
 * @entry: filesystem entry reported by fs_scan_tree()
 * @userdata: backend_lost_scope_scan_context_t
 *
 * The delayed recovery search compares filesystem identity, not names. A path
 * can be renamed or reused; the saved (st_dev, st_ino) pair is the evidence
 * that lets Alfred recognize the same directory at a new path. The callback
 * stops the scan on the first matching directory.
 *
 * Return: nonzero to stop after a match, zero to keep scanning.
 */
static int backend_lost_scope_scan_for_identity(const fs_scan_entry_t *entry,
                                                void *userdata)
{
    backend_lost_scope_scan_context_t *scan_context = userdata;

    if (entry == NULL || entry->st == NULL || scan_context == NULL ||
        scan_context->entry == NULL) {
        return 0;
    }

    if (entry->type != FS_SCAN_DIR)
        return 0;

    if (entry->st->st_dev != scan_context->entry->device_id ||
        entry->st->st_ino != scan_context->entry->inode_id) {
        return 0;
    }

    snprintf(scan_context->found_path,
             scan_context->found_path_size,
             "%s",
             entry->path);
    scan_context->found = 1;

    return 1;
}

/* ============================================================================
 * Lifecycle
 * ========================================================================== */

/*
 * inotify_backend_init - initialize the inotify backend runtime
 * @ctx: backend context containing config, logger, and backend storage
 *
 * Initializes the watcher table first, then opens a nonblocking inotify file
 * descriptor. The removed legacy shadow path deliberately does not appear
 * here: the backend owns only inotify runtime state and raw-event production.
 * It does not initialize semantic state; the core owns that state separately.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_init(inotify_backend_context_t *ctx)
{
    if (ctx == NULL)
        return ERR_INVALID_ARG;

    return backend_init(ctx);
}

/*
 * backend_init - initialize backend runtime through context
 * @ctx: narrowed backend context with runtime, config, and logger
 *
 * This helper is the context-shaped form of backend initialization. It keeps
 * the same backend cleanup order as the public function previously had:
 * watcher table first, inotify descriptor second.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
static int backend_init(inotify_backend_context_t *ctx)
{
    ctx->runtime->fd = -1;

    if (watcher_init(&ctx->runtime->watchers,
                     ctx->config->watcher_capacity) != 0) {

        logger_error(ctx->logger,
                     "watcher_init failed");

        return ERR_ALLOC;
    }

    logger_info(ctx->logger,
                "watcher table initialized");

    if (backend_lost_scope_queue_init(&ctx->runtime->lost_scopes, 0) != 0) {
        logger_error(ctx->logger,
                     "lost scope queue initialization failed");

        watcher_destroy(&ctx->runtime->watchers);
        return ERR_ALLOC;
    }

    ctx->runtime->fd =
        inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (ctx->runtime->fd < 0) {
        logger_error(ctx->logger,
                     "inotify_init1 failed errno=%d (%s)",
                     errno,
                     strerror(errno));

        backend_lost_scope_queue_destroy(&ctx->runtime->lost_scopes);
        watcher_destroy(&ctx->runtime->watchers);
        return ERR_INOTIFY;
    }

    logger_info(ctx->logger,
                "inotify backend initialized fd=%d",
                ctx->runtime->fd);

    return ERR_OK;
}

/*
 * inotify_backend_add_startup_watch - add one startup path to the backend
 * @ctx: initialized backend context
 * @path: path supplied on the command line
 *
 * Startup watch installation is backend state, not semantic event processing.
 * Recursive mode installs watches for the existing tree before the event loop
 * starts; later recursive updates are handled when directory-create raw facts
 * arrive from inotify.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_add_startup_watch(inotify_backend_context_t *ctx,
                                      const char *path)
{
    if (ctx == NULL || path == NULL)
        return ERR_INVALID_ARG;

    return backend_add_startup_watch(ctx, path);
}

/*
 * backend_add_startup_watch - install one startup watch through backend context
 * @ctx: narrowed backend context with runtime, config, and logger
 * @path: path supplied on the command line
 *
 * This helper keeps the startup-watch decision separate from public argument
 * validation. The backend decision only needs the context and the path.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
static int backend_add_startup_watch(inotify_backend_context_t *ctx,
                                     const char *path)
{
    if (ctx->config->recursive) {
        if (watch_manager_add_recursive(ctx, path) < 0)
            return ERR_INOTIFY;
    }
    else {
        if (watch_manager_add(ctx, path) < 0)
            return ERR_INOTIFY;
    }

    if (backend_configured_roots_add(ctx->runtime, path) != 0) {
        logger_error(ctx->logger,
                     "configured root registration failed path=%s",
                     path);
        return ERR_ALLOC;
    }

    return ERR_OK;
}

/*
 * backend_configured_roots_add - remember one successfully watched root
 * @runtime: backend runtime that owns the root list
 * @path: startup path that was accepted by watch installation
 *
 * Lost-scope recovery needs a bounded search scope that is independent from the
 * stale path. The application passes startup paths during initialization, but
 * app.c should not be queried later from backend recovery code. This helper
 * copies each successfully installed startup root into backend-owned storage so
 * delayed recovery can find the root that originally contained a stale watch.
 *
 * Duplicate paths are ignored. Paths are stored as supplied by startup; a later
 * normalization pass can tighten this if Alfred starts canonicalizing startup
 * paths before watch installation.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
static int backend_configured_roots_add(inotify_backend_t *runtime,
                                        const char *path)
{
    size_t path_len;

    if (runtime == NULL || path == NULL)
        return -1;

    path_len = strlen(path);

    if (path_len == 0 || path_len >= PATH_MAX)
        return -1;

    for (size_t i = 0; i < runtime->configured_roots_count; i++) {
        if (strcmp(runtime->configured_roots[i], path) == 0)
            return 0;
    }

    if (runtime->configured_roots_count == runtime->configured_roots_capacity) {
        size_t new_capacity = runtime->configured_roots_capacity;

        if (new_capacity == 0)
            new_capacity = 4;
        else
            new_capacity *= 2;

        char (*tmp)[PATH_MAX] =
            realloc(runtime->configured_roots, new_capacity * PATH_MAX);

        if (tmp == NULL)
            return -1;

        runtime->configured_roots = tmp;
        runtime->configured_roots_capacity = new_capacity;
    }

    snprintf(runtime->configured_roots[runtime->configured_roots_count],
             PATH_MAX,
             "%s",
             path);
    runtime->configured_roots_count++;

    return 0;
}

/*
 * backend_configured_roots_destroy - release backend-owned root storage
 * @runtime: backend runtime that owns the root list
 *
 * The roots are stored in one dynamic array of fixed PATH_MAX slots, so cleanup
 * is a single free plus counter reset. The function is safe for partially
 * initialized backends and repeated shutdown paths.
 */
static void backend_configured_roots_destroy(inotify_backend_t *runtime)
{
    if (runtime == NULL)
        return;

    free(runtime->configured_roots);
    runtime->configured_roots = NULL;
    runtime->configured_roots_count = 0;
    runtime->configured_roots_capacity = 0;
}

/*
 * backend_path_matches_prefix - check path prefix with directory boundaries
 * @path: path to classify
 * @prefix: candidate root/prefix
 *
 * A textual prefix is not enough: "/tmp/root-old" must not be treated as a
 * child of "/tmp/root". Matching accepts exact equality, children separated by
 * '/', and the filesystem root "/" as a prefix of every absolute path.
 *
 * Return: nonzero when @path belongs to @prefix.
 */
static int backend_path_matches_prefix(const char *path, const char *prefix)
{
    size_t prefix_len;

    if (path == NULL || prefix == NULL)
        return 0;

    prefix_len = strlen(prefix);

    if (prefix_len == 0)
        return 0;

    if (strcmp(prefix, "/") == 0)
        return path[0] == '/';

    if (strncmp(path, prefix, prefix_len) != 0)
        return 0;

    return path[prefix_len] == '\0' || path[prefix_len] == '/';
}

/*
 * backend_configured_root_for_path - find the most specific configured root
 * @runtime: backend runtime containing registered startup roots
 * @path: stale or current path to classify
 *
 * Multiple configured roots can be nested, for example "/home/user" and
 * "/home/user/project". Lost-scope recovery should start from the narrowest
 * matching root to reduce scan cost and avoid searching unrelated directories.
 *
 * Return: borrowed pointer to the best matching root, or NULL if none matches.
 */
static const char *backend_configured_root_for_path(
    const inotify_backend_t *runtime,
    const char *path
)
{
    const char *best = NULL;
    size_t best_len = 0;

    if (runtime == NULL || path == NULL)
        return NULL;

    for (size_t i = 0; i < runtime->configured_roots_count; i++) {
        const char *root = runtime->configured_roots[i];
        size_t root_len = strlen(root);

        if (!backend_path_matches_prefix(path, root))
            continue;

        if (root_len > best_len) {
            best = root;
            best_len = root_len;
        }
    }

    return best;
}

/*
 * inotify_backend_shutdown - release backend runtime resources
 * @ctx: backend context containing runtime state
 *
 * The function is safe for partial initialization paths.
 */
void inotify_backend_shutdown(inotify_backend_context_t *ctx)
{
    if (ctx == NULL)
        return;

    backend_shutdown(ctx);
}

/*
 * backend_shutdown - release backend runtime resources through context
 * @ctx: narrowed backend context with runtime state
 *
 * This is the context-shaped form of backend shutdown. The normal backend
 * cleanup only needs fd and the watcher table.
 */
static void backend_shutdown(inotify_backend_context_t *ctx)
{
    if (ctx->runtime->fd >= 0) {
        close(ctx->runtime->fd);
        ctx->runtime->fd = -1;
    }

    backend_lost_scope_queue_destroy(&ctx->runtime->lost_scopes);
    backend_configured_roots_destroy(ctx->runtime);
    watcher_destroy(&ctx->runtime->watchers);
}

/* ============================================================================
 * Polling
 * ========================================================================== */

/*
 * inotify_backend_poll - process one nonblocking inotify read
 * @ctx: backend context used by the raw/core path
 * @on_event: callback used to deliver raw events to the application/core layer
 * @userdata: opaque callback context passed through unchanged
 *
 * The processing order is deliberate. The backend first records what Linux
 * reported, then produces an Alfred raw fact, and only then repairs backend
 * watch state. Semantic interpretation stays downstream in the core.
 *
 * 1. log the kernel raw event for diagnostics
 * 2. convert the inotify record into alfred_raw_event_t when possible
 * 3. deliver the raw event to the core through @on_event
 * 4. update backend watch state
 * 5. update recursive watches and emit synthetic raw directory creates
 *
 * This keeps the core as the only semantic path. WATCH_ADDED, WATCH_REMOVED,
 * and recursive discovery are backend diagnostics/state repair, not a second
 * user-facing event stream.
 *
 * Return: ERR_OK on success or idle poll, a negative error_t value on failure.
 */
int inotify_backend_poll(inotify_backend_context_t *ctx,
                         inotify_backend_event_fn on_event,
                         void *userdata)
{
    if (ctx == NULL || on_event == NULL)
        return ERR_INVALID_ARG;

    return backend_poll(ctx, on_event, userdata);
}

/*
 * backend_poll - process one nonblocking inotify read through backend context
 * @ctx: narrowed backend context used by the raw/core path
 * @on_event: callback used to deliver raw events to the application/core layer
 * @userdata: opaque callback context passed through unchanged
 *
 * The main backend path uses @ctx for runtime, configuration, and diagnostics.
 * It deliberately accepts a raw callback instead of a semantic logger. That
 * callback boundary prevents this file from recreating the old semantic
 * dispatcher that used to live in the inotify module.
 *
 * Return: ERR_OK on success or idle poll, a negative error_t value on failure.
 */
static int backend_poll(inotify_backend_context_t *ctx,
                        inotify_backend_event_fn on_event,
                        void *userdata)
{
    char buffer[EVENT_BUFFER_SIZE];

    ssize_t bytes =
        read(ctx->runtime->fd,
             buffer,
             sizeof(buffer));

    if (bytes < 0) {

        if (errno == EAGAIN ||
            errno == EWOULDBLOCK) {

            backend_lost_scope_process_due_runtime(ctx);
            usleep(10000); /* 10ms */
            return ERR_OK;
        }

        if (errno == EINTR)
            return ERR_OK;

        logger_error(ctx->logger,
                     "read failed errno=%d (%s)",
                     errno,
                     strerror(errno));

        return ERR_IO;
    }

    if (bytes == 0) {
        logger_error(ctx->logger,
                     "unexpected EOF on inotify fd");
        return ERR_IO;
    }

    logger_raw(ctx->logger,
               "read %zd bytes from inotify",
               bytes);

    char *ptr = buffer;
    char mask_str[256];

    while (ptr < buffer + bytes) {

        struct inotify_event *ev =
            (struct inotify_event *)ptr;

        const char *parent =
            watcher_get_path(&ctx->runtime->watchers, ev->wd);
        int stale_watch =
            watcher_is_stale(&ctx->runtime->watchers, ev->wd);

        backend_raw_event_name_from_mask(ev->mask,
                                         mask_str,
                                         sizeof(mask_str));
        logger_raw(ctx->logger,
                   "%s wd=%d path=%s name=%s",
                   mask_str,
                   ev->wd,
                   parent ? parent : "",
                   ev->len ? ev->name : "");

        alfred_raw_event_t raw;
        char full_path[PATH_MAX];
        const alfred_raw_event_t *raw_ptr = NULL;

        if (backend_build_overflow_raw(ev, &raw) == 0) {
            raw_ptr = &raw;
        }
        else if (parent != NULL && !stale_watch) {
            if (inotify_adapter_build_raw(ev,
                                          parent,
                                          full_path,
                                          sizeof(full_path),
                                          &raw) == 0) {
                if (backend_raw_mask_has_dispatch_action(raw.mask))
                    raw_ptr = &raw;
            }
            else {
                logger_error(ctx->logger,
                             "failed to build core raw event wd=%d",
                             ev->wd);
            }
        }
        else if (parent != NULL && stale_watch) {
            /*
             * A stale wd may still receive kernel events because inotify watches
             * the filesystem object, not the textual path Alfred saved earlier.
             * Until resync proves a trustworthy path again, forwarding a raw
             * event would give the core a path that may now be false.
             */
            backend_log_stale_event_dropped(ctx,
                                            ev->wd,
                                            parent,
                                            mask_str,
                                            ev->len ? ev->name : "");
        }

        int callback_status =
            on_event(raw_ptr, userdata);

        if (callback_status != ERR_OK)
            return callback_status;

        backend_handle_move_self(ctx, ev);

        backend_handle_delete_self(ctx, ev);

        backend_handle_unmount(ctx, ev);

        backend_handle_ignored(ctx, ev);

        backend_handle_dir_create(ctx, ev, on_event, userdata);

        ptr += sizeof(struct inotify_event) + ev->len;
    }

    backend_lost_scope_process_due_runtime(ctx);

    return ERR_OK;
}

/*
 * backend_handle_ignored - remove a watch after an IN_IGNORED notification
 * @ctx: narrowed backend context used by the poll path
 * @ev: raw inotify event currently being processed
 *
 * IN_IGNORED means the kernel removed a watch, commonly because the watched
 * directory disappeared. This is backend state maintenance, not filesystem
 * semantics. The diagnostic WATCH_REMOVED log remains useful, but the core must
 * not turn it into a user-facing semantic event.
 *
 */
static void backend_handle_ignored(inotify_backend_context_t *ctx,
                                   const struct inotify_event *ev)
{
    if (ctx == NULL || ev == NULL)
        return;

    if ((ev->mask & IN_IGNORED) == 0)
        return;

    watch_manager_remove(ctx, ev->wd);
}

/*
 * backend_handle_move_self - mark a watched path as no longer reliable
 * @ctx: narrowed backend context used by the poll path
 * @ev: raw inotify event currently being processed
 *
 * IN_MOVE_SELF says that the watched object itself moved, but the event does
 * not contain the new destination path. The backend therefore must not invent
 * a rename, move, or relocation event for the core. Instead it marks the
 * existing wd -> path mapping as STALE so later resync policy can decide
 * whether the watch can be repaired, invalidated, or only reported as
 * diagnostic state.
 *
 * This handler intentionally runs before IN_IGNORED cleanup. Some kernels or
 * scenarios may deliver both facts close together; marking stale first records
 * the loss of path reliability while the wd -> path mapping is still available
 * for diagnostics.
 */
static void backend_handle_move_self(inotify_backend_context_t *ctx,
                                     const struct inotify_event *ev)
{
    if (ctx == NULL || ev == NULL)
        return;

    if ((ev->mask & IN_MOVE_SELF) == 0)
        return;

    const char *path =
        watcher_get_path(&ctx->runtime->watchers, ev->wd);

    if (watcher_set_state(&ctx->runtime->watchers,
                          ev->wd,
                          WATCHER_STATE_STALE) != 0) {

        logger_error(ctx->logger,
                     "failed to mark watch stale wd=%d reason=IN_MOVE_SELF",
                     ev->wd);

        return;
    }

    backend_log_watch_stale(ctx,
                            ev->wd,
                            path,
                            "IN_MOVE_SELF");

    backend_resync_watch(ctx, ev->wd, "IN_MOVE_SELF");
}

/*
 * backend_handle_delete_self - mark a watched path as deleted/stale
 * @ctx: narrowed backend context used by the poll path
 * @ev: raw inotify event currently being processed
 *
 * IN_DELETE_SELF says that the watched object itself was deleted. That makes
 * the wd -> path mapping unreliable immediately, but it still does not justify
 * inventing delete events for children or emitting a core delete for the
 * watched path before the project defines that semantic contract.
 *
 * The handler marks the watch STALE first and lets IN_IGNORED perform the
 * final table cleanup afterwards. This preserves a diagnostic transition:
 * VALID -> STALE because the watched path disappeared, then STALE -> REMOVED
 * when the kernel confirms that the watch is gone.
 */
static void backend_handle_delete_self(inotify_backend_context_t *ctx,
                                       const struct inotify_event *ev)
{
    if (ctx == NULL || ev == NULL)
        return;

    if ((ev->mask & IN_DELETE_SELF) == 0)
        return;

    const char *path =
        watcher_get_path(&ctx->runtime->watchers, ev->wd);

    if (watcher_set_state(&ctx->runtime->watchers,
                          ev->wd,
                          WATCHER_STATE_STALE) != 0) {

        logger_error(ctx->logger,
                     "failed to mark watch stale wd=%d reason=IN_DELETE_SELF",
                     ev->wd);

        return;
    }

    backend_log_watch_stale(ctx,
                            ev->wd,
                            path,
                            "IN_DELETE_SELF");
}

/*
 * backend_handle_unmount - mark a watched filesystem scope as unavailable
 * @ctx: narrowed backend context used by the poll path
 * @ev: raw inotify event currently being processed
 *
 * IN_UNMOUNT says that the filesystem containing the watched object was
 * unmounted. That is not a semantic delete: the object may become available
 * again if the filesystem is mounted again, and inotify does not describe
 * child paths that should be deleted. The only safe immediate contract is
 * backend diagnostics: mark the wd -> path mapping STALE, then let the
 * following IN_IGNORED cleanup remove the watch-table slot.
 *
 * Unlike IN_MOVE_SELF, this handler does not start lost-scope recovery. The
 * watched filesystem is not available through the current mount namespace, so
 * an identity scan would spend work on a scope that cannot be trusted.
 */
static void backend_handle_unmount(inotify_backend_context_t *ctx,
                                   const struct inotify_event *ev)
{
    if (ctx == NULL || ev == NULL)
        return;

    if ((ev->mask & IN_UNMOUNT) == 0)
        return;

    const char *path =
        watcher_get_path(&ctx->runtime->watchers, ev->wd);

    if (watcher_set_state(&ctx->runtime->watchers,
                          ev->wd,
                          WATCHER_STATE_STALE) != 0) {

        logger_error(ctx->logger,
                     "failed to mark watch stale wd=%d reason=IN_UNMOUNT",
                     ev->wd);

        return;
    }

    backend_log_watch_stale(ctx,
                            ev->wd,
                            path,
                            "IN_UNMOUNT");
}

/*
 * backend_resync_watch - orchestrate recovery for one stale watch
 * @ctx: narrowed backend context used by the poll path
 * @wd: inotify watch descriptor whose mapping may need recovery
 * @reason: kernel/backend reason that triggered recovery
 *
 * This wrapper is deliberately small. Today recovery has only one phase: a
 * stat(2)-based identity probe on the old wd -> path mapping. Future
 * scanner-based resync can be added here as a second phase without burying
 * subtree scan policy inside the minimal identity check.
 */
static void backend_resync_watch(inotify_backend_context_t *ctx,
                                 int wd,
                                 const char *reason)
{
    if (ctx == NULL || reason == NULL)
        return;

    backend_resync_probe_result_t result =
        backend_probe_stale_watch_identity(ctx, wd, reason);

    if (!backend_resync_probe_result_needs_lost_scope(result))
        return;

    const char *path =
        watcher_get_path(&ctx->runtime->watchers, wd);

    backend_enqueue_lost_scope(ctx, wd, path ? path : "", reason, result);
}

/*
 * backend_probe_stale_watch_identity - verify a stale watch's old path identity
 * @ctx: narrowed backend context used by the poll path
 * @wd: inotify watch descriptor whose mapping was marked stale
 * @reason: kernel/backend reason that triggered the probe
 *
 * This is intentionally not the full scanner-based recovery. It only checks
 * whether the old wd -> path mapping is still a reachable directory and then
 * compares its current stat(2) identity with the identity captured when the
 * watch was installed. For IN_MOVE_SELF, reachability alone is not enough: the
 * watched object may have moved while another directory was created at the old
 * location. Only a matching identity can move the watch back to VALID.
 * Otherwise Alfred keeps the watch STALE and logs an explicit failure instead
 * of inventing raw Alfred events or semantic core events.
 *
 * Return: BACKEND_RESYNC_PROBE_VALID on local recovery, otherwise the failure
 * classification already logged as WATCH_RESYNC_FAILED.
 */
static backend_resync_probe_result_t backend_probe_stale_watch_identity(
    inotify_backend_context_t *ctx,
    int wd,
    const char *reason
)
{
    if (ctx == NULL || reason == NULL)
        return BACKEND_RESYNC_PROBE_MISSING_WATCH;

    backend_resync_probe_result_t result;
    int saved_errno = 0;

    const char *path =
        watcher_get_path(&ctx->runtime->watchers, wd);

    if (path == NULL) {
        backend_log_resync_failure(ctx,
                                   wd,
                                   "",
                                   reason,
                                   BACKEND_RESYNC_PROBE_MISSING_WATCH,
                                   0);
        return BACKEND_RESYNC_PROBE_MISSING_WATCH;
    }

    if (watcher_get_state(&ctx->runtime->watchers, wd) !=
        WATCHER_STATE_STALE) {

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   BACKEND_RESYNC_PROBE_NOT_STALE,
                                   0);
        return BACKEND_RESYNC_PROBE_NOT_STALE;
    }

    (void)backend_log_resync_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_RESYNC_BEGIN,
        wd,
        path,
        reason,
        NULL,
        NULL,
        0,
        0,
        0,
        0,
        0);

    if (watcher_set_state(&ctx->runtime->watchers,
                          wd,
                          WATCHER_STATE_RESYNCING) != 0) {

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   BACKEND_RESYNC_PROBE_SET_RESYNCING_FAILED,
                                   0);
        return BACKEND_RESYNC_PROBE_SET_RESYNCING_FAILED;
    }

    struct stat st;

    if (stat(path, &st) != 0) {
        saved_errno = errno;
        result = BACKEND_RESYNC_PROBE_PATH_UNREACHABLE;

        if (watcher_set_state(&ctx->runtime->watchers,
                              wd,
                              WATCHER_STATE_STALE) != 0) {

            backend_log_resync_failure(ctx,
                                       wd,
                                       path,
                                       reason,
                                       BACKEND_RESYNC_PROBE_SET_STALE_FAILED,
                                       0);
            return BACKEND_RESYNC_PROBE_SET_STALE_FAILED;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   saved_errno);
        return result;
    }

    if (!S_ISDIR(st.st_mode)) {
        result = BACKEND_RESYNC_PROBE_NOT_DIRECTORY;

        if (watcher_set_state(&ctx->runtime->watchers,
                              wd,
                              WATCHER_STATE_STALE) != 0) {

            backend_log_resync_failure(ctx,
                                       wd,
                                       path,
                                       reason,
                                       BACKEND_RESYNC_PROBE_SET_STALE_FAILED,
                                       0);
            return BACKEND_RESYNC_PROBE_SET_STALE_FAILED;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   0);
        return result;
    }

    dev_t stored_device_id;
    ino_t stored_inode_id;

    if (watcher_get_identity(&ctx->runtime->watchers,
                             wd,
                             &stored_device_id,
                             &stored_inode_id) != 0) {
        result = BACKEND_RESYNC_PROBE_MISSING_IDENTITY;

        if (watcher_set_state(&ctx->runtime->watchers,
                              wd,
                              WATCHER_STATE_STALE) != 0) {

            backend_log_resync_failure(ctx,
                                       wd,
                                       path,
                                       reason,
                                       BACKEND_RESYNC_PROBE_SET_STALE_FAILED,
                                       0);
            return BACKEND_RESYNC_PROBE_SET_STALE_FAILED;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   0);
        return result;
    }

    if (stored_device_id != st.st_dev ||
        stored_inode_id != st.st_ino) {
        result = BACKEND_RESYNC_PROBE_IDENTITY_MISMATCH;

        /*
         * The old path is reachable, but it now names a different object.
         * Do not add a watch here: that would silently subscribe a reused path
         * while the stale wd still represents the moved object. A future
         * scanner-based resync must decide whether the configured/root scope is
         * trustworthy enough to watch the replacement directory.
         */
        if (watcher_set_state(&ctx->runtime->watchers,
                              wd,
                              WATCHER_STATE_STALE) != 0) {

            backend_log_resync_failure(ctx,
                                       wd,
                                       path,
                                       reason,
                                       BACKEND_RESYNC_PROBE_SET_STALE_FAILED,
                                       0);
            return BACKEND_RESYNC_PROBE_SET_STALE_FAILED;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   0);
        return result;
    }

    /*
     * The old path is now proven to be the original watched object. Run the
     * scanner phase before returning to VALID. The resync helper must restore
     * every missing child watch in the trusted scope; otherwise the parent
     * remains STALE.
     */
    if (backend_resync_watch_subtree_dirs(ctx, wd, path, reason) != ERR_OK) {
        if (watcher_set_state(&ctx->runtime->watchers,
                              wd,
                              WATCHER_STATE_STALE) != 0) {

            backend_log_resync_failure(ctx,
                                       wd,
                                       path,
                                       reason,
                                       BACKEND_RESYNC_PROBE_SET_STALE_FAILED,
                                       0);
            return BACKEND_RESYNC_PROBE_SET_STALE_FAILED;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   BACKEND_RESYNC_PROBE_REINSTALL_FAILED,
                                   0);
        return BACKEND_RESYNC_PROBE_REINSTALL_FAILED;
    }

    if (watcher_set_state(&ctx->runtime->watchers,
                          wd,
                          WATCHER_STATE_VALID) != 0) {

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   BACKEND_RESYNC_PROBE_SET_VALID_FAILED,
                                   0);
        return BACKEND_RESYNC_PROBE_SET_VALID_FAILED;
    }

    (void)backend_log_resync_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_RESYNC_END,
        wd,
        path,
        reason,
        "valid",
        NULL,
        0,
        0,
        0,
        0,
        0);

    return BACKEND_RESYNC_PROBE_VALID;
}

/*
 * backend_resync_probe_result_needs_lost_scope - classify wide recovery need
 * @result: outcome produced by backend_probe_stale_watch_identity()
 *
 * The lost-scope queue is for path-trust failures where saved filesystem
 * identity can still help a later scan find the moved object in monitored
 * roots. It is not used for programming/bookkeeping failures, missing identity,
 * or reinstall failures where the old path was already proven trustworthy.
 *
 * Return: nonzero when @result should enqueue delayed lost-scope recovery.
 */
static int backend_resync_probe_result_needs_lost_scope(
    backend_resync_probe_result_t result
)
{
    return result == BACKEND_RESYNC_PROBE_PATH_UNREACHABLE ||
           result == BACKEND_RESYNC_PROBE_NOT_DIRECTORY ||
           result == BACKEND_RESYNC_PROBE_IDENTITY_MISMATCH;
}

/*
 * backend_enqueue_lost_scope - record one failed local recovery for later scan
 * @ctx: narrowed backend context used by the resync path
 * @wd: stale watch descriptor that could not be locally repaired
 * @path: last known path for @wd, possibly no longer trustworthy
 * @reason: kernel/backend reason that triggered the recovery attempt
 * @result: local probe failure that made wide recovery necessary
 *
 * This helper is the bridge between the immediate self-event path and the
 * future delayed scanner. It copies the saved filesystem identity from the
 * watcher table and stores it with the stale path, but it does not scan yet and
 * does not emit raw/core events. WATCH_LOST_QUEUED is diagnostic evidence that
 * Alfred has recovery work queued, not proof that the directory was found.
 *
 * The backend now stores configured watch roots. The first search scope is the
 * most specific registered root that contains @path. If no root matches, the
 * helper keeps the old local-path fallback so diagnostic recovery state is
 * still self-contained instead of dropping the entry.
 */
static void backend_enqueue_lost_scope(inotify_backend_context_t *ctx,
                                       int wd,
                                       const char *path,
                                       const char *reason,
                                       backend_resync_probe_result_t result)
{
    if (ctx == NULL || path == NULL || reason == NULL)
        return;

    dev_t device_id;
    ino_t inode_id;

    if (watcher_get_identity(&ctx->runtime->watchers,
                             wd,
                             &device_id,
                             &inode_id) != 0) {
        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_SKIPPED,
            wd,
            path,
            NULL,
            NULL,
            reason,
            NULL,
            "missing-identity",
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);
        return;
    }

    uint64_t now_ns = backend_now_ns();
    const char *scan_root =
        backend_configured_root_for_path(ctx->runtime, path);

    if (scan_root == NULL)
        scan_root = path;

    if (backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                         wd,
                                         path,
                                         device_id,
                                         inode_id,
                                         scan_root,
                                         reason,
                                         now_ns,
                                         now_ns) != 0) {
        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED,
            wd,
            path,
            NULL,
            NULL,
            reason,
            NULL,
            backend_resync_probe_result_name(result),
            NULL,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);
        return;
    }

    (void)backend_log_lost_scope_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED,
        wd,
        path,
        NULL,
        NULL,
        reason,
        NULL,
        backend_resync_probe_result_name(result),
        NULL,
        0,
        0,
        0,
        0,
        backend_lost_scope_queue_count(&ctx->runtime->lost_scopes),
        0,
        0,
        0,
        0);
}

/*
 * backend_resync_scan_subtree_dirs - count directory watch coverage
 * @ctx: narrowed backend context used by recovery code
 * @path: trusted root path to scan
 * @emit_root: nonzero to include @path itself in scan counters
 * @scan_context: output scan state with counters and owned missing paths
 *
 * This helper performs the read-only half of subtree recovery. It scans only
 * directories, asks the watcher table whether each path is already covered,
 * and copies missing paths into @scan_context. It does not install watches and
 * does not log: callers decide whether the scan belongs to local resync
 * diagnostics or delayed lost-scope diagnostics.
 *
 * Return: ERR_OK on complete scan and collection, otherwise error_t.
 */
static error_t backend_resync_scan_subtree_dirs(
    inotify_backend_context_t *ctx,
    const char *path,
    int emit_root,
    backend_resync_scan_context_t *scan_context
)
{
    if (ctx == NULL || path == NULL || scan_context == NULL)
        return ERR_INVALID_ARG;

    memset(scan_context, 0, sizeof(*scan_context));
    scan_context->ctx = ctx;

    fs_scan_options_t opts;

    fs_scan_options_defaults(&opts);
    opts.include_dirs = 1;
    opts.include_files = 0;
    opts.include_symlinks = 0;
    opts.include_other = 0;
    opts.follow_symlinks = 0;
    opts.strict_child_errors = 1;
    opts.emit_root = emit_root ? 1 : 0;

    error_t rc =
        fs_scan_tree(path,
                     &opts,
                     backend_count_resync_scanned_dir,
                     scan_context);

    if (rc != ERR_OK)
        return rc;

    if (scan_context->missing_paths_failed)
        return ERR_ALLOC;

    return ERR_OK;
}

/*
 * backend_resync_watch_subtree_dirs - scan subtree and reinstall missing watches
 * @ctx: narrowed backend context used by the poll path
 * @wd: stale watch descriptor that would own the recovery scope
 * @path: trusted root path to scan
 * @reason: kernel/backend reason that triggered recovery
 *
 * This helper runs only after a prior phase has proven that the old path still
 * names the original watched object. It prepares phase two of recovery: a
 * directory-only scan below that trusted path. The scan uses emit_root=0
 * because the root is the watch being recovered; the future resync phase needs
 * child directories to decide which recursive watches are missing.
 *
 * The callback counts discovered directories and copies every directory
 * missing from the watcher table. This helper then delegates the actual
 * watch reinstallation policy to backend_resync_reinstall_missing_watches().
 *
 * Return: ERR_OK when scan and reinstallation succeed, otherwise error_t.
 */
static error_t backend_resync_watch_subtree_dirs(inotify_backend_context_t *ctx,
                                                int wd,
                                                const char *path,
                                                const char *reason)
{
    if (ctx == NULL || path == NULL || reason == NULL)
        return ERR_INVALID_ARG;

    backend_resync_scan_context_t scan_context;

    error_t rc =
        backend_resync_scan_subtree_dirs(ctx,
                                         path,
                                         0,
                                         &scan_context);

    if (rc != ERR_OK) {
        (void)backend_log_resync_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_FAILED,
            wd,
            path,
            reason,
            NULL,
            NULL,
            0,
            rc,
            0,
            0,
            0);
        backend_resync_scan_context_destroy(&scan_context);
        return rc;
    }

    (void)backend_log_resync_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_DONE,
        wd,
        path,
        reason,
        NULL,
        NULL,
        0,
        0,
        scan_context.directories_seen,
        scan_context.directories_watched,
        scan_context.directories_missing_watch);

    backend_resync_scan_class_t scan_class =
        backend_classify_resync_scan(&scan_context);

    (void)backend_log_resync_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_CLASS,
        wd,
        path,
        reason,
        backend_resync_scan_class_name(scan_class),
        NULL,
        0,
        0,
        0,
        0,
        0);

    /*
     * Runtime recovery must mutate the real watcher table and kernel watches.
     * The indirection exists only so the same policy can be tested with fake
     * operations; production always uses the watch manager wrappers below.
     */
    const backend_resync_watch_ops_t *watch_ops =
        &BACKEND_RESYNC_WATCH_MANAGER_OPS;

    rc = backend_resync_reinstall_missing_watches(ctx,
                                                  wd,
                                                  path,
                                                  reason,
                                                  &scan_context,
                                                  watch_ops);

    if (rc != ERR_OK) {
        backend_resync_scan_context_destroy(&scan_context);
        return rc;
    }

    backend_resync_scan_context_destroy(&scan_context);
    return ERR_OK;
}

/*
 * backend_resync_watch_manager_add - production add operation for resync
 * @ctx: backend context containing fd, watcher table, config, and logger
 * @path: missing directory path to watch
 *
 * This wrapper intentionally contains no policy. It adapts the real watch
 * manager function to backend_resync_watch_ops_t so the reinstall helper can be
 * tested without exporting itself or depending directly on the concrete watch
 * manager symbol.
 *
 * Return: new watch descriptor on success, -1 on failure.
 */
static int backend_resync_watch_manager_add(inotify_backend_context_t *ctx,
                                            const char *path)
{
    return watch_manager_add(ctx, path);
}

/*
 * backend_resync_watch_manager_remove - production rollback operation
 * @ctx: backend context containing fd, watcher table, config, and logger
 * @wd: watch descriptor installed earlier in the same resync attempt
 *
 * A failed all-or-stale reinstall attempt must undo only the watches it added
 * during that attempt. Delegating to watch_manager_remove() keeps normal
 * watcher table cleanup and WATCH_REMOVED diagnostics in one place.
 *
 * Return: 0 on success, -1 on failure.
 */
static int backend_resync_watch_manager_remove(inotify_backend_context_t *ctx,
                                               int wd)
{
    return watch_manager_remove(ctx, wd);
}

/*
 * backend_resync_reinstall_missing_watches - restore all missing child watches
 * @ctx: narrowed backend context used by the poll path
 * @wd: parent watch descriptor whose trusted scope was scanned
 * @path: trusted parent path used for diagnostic logs
 * @reason: kernel/backend reason that triggered recovery
 * @scan_context: completed scan context with owned missing path copies
 * @ops: watch installation/removal operations used by runtime or tests
 *
 * This helper owns the all-or-stale policy for a trusted resync scope. A
 * missing path is logged before each add attempt, then ops->add installs the
 * watch. If every missing path succeeds, the caller may safely continue toward
 * WATCHER_STATE_VALID. If any path fails, the helper removes every watch
 * installed during this attempt and returns ERR_INOTIFY so the caller keeps
 * the parent watch STALE.
 *
 * Runtime passes the real watch manager operations. Unit tests can pass fake
 * operations to force a deterministic failure after one or more successful
 * reinstalls, avoiding a fragile filesystem race.
 *
 * Return: ERR_OK on complete coverage, otherwise error_t.
 */
static error_t backend_resync_reinstall_missing_watches(
    inotify_backend_context_t *ctx,
    int wd,
    const char *path,
    const char *reason,
    const backend_resync_scan_context_t *scan_context,
    const backend_resync_watch_ops_t *ops
)
{
    /*
     * Treat missing callbacks as a programmer error. Without both operations
     * the helper could install watches without being able to roll them back,
     * which would break the all-or-stale guarantee.
     */
    if (ctx == NULL || path == NULL || reason == NULL ||
        scan_context == NULL || ops == NULL || ops->add == NULL ||
        ops->remove == NULL) {
        return ERR_INVALID_ARG;
    }

    /*
     * A covered subtree is already healthy. Return success without allocating
     * rollback storage or touching the watch manager.
     */
    if (scan_context->missing_paths_count == 0)
        return ERR_OK;

    /*
     * Store only descriptors installed by this resync attempt. On a later
     * failure, rollback must remove these new watches, not pre-existing watches
     * that were already part of the trusted scope.
     */
    int *installed_wds =
        calloc(scan_context->missing_paths_count, sizeof(int));

    if (installed_wds == NULL)
        return ERR_ALLOC;

    size_t installed_count = 0;

    for (size_t i = 0; i < scan_context->missing_paths_count; i++) {
        const char *missing_path = scan_context->missing_paths[i];

        /*
         * Log the candidate before mutation. If the following add fails, the
         * diagnostic still shows exactly which missing path broke the repair.
         */
        (void)backend_log_resync_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_MISSING,
            wd,
            path,
            reason,
            NULL,
            missing_path,
            0,
            0,
            0,
            0,
            0);

        int new_wd = ops->add(ctx, missing_path);

        if (new_wd < 0) {
            /*
             * The trusted scope is now known to be only partially covered.
             * Roll back every watch installed in this attempt and report a
             * backend error so the caller leaves the parent watch STALE.
             */
            (void)backend_log_resync_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALL_FAILED,
                wd,
                path,
                reason,
                NULL,
                missing_path,
                0,
                0,
                0,
                0,
                0);

            for (size_t j = 0; j < installed_count; j++) {
                (void)backend_log_resync_record(
                    ctx,
                    ALFRED_RECORD_TYPE_WATCH_RESYNC_ROLLBACK,
                    wd,
                    path,
                    reason,
                    NULL,
                    NULL,
                    installed_wds[j],
                    0,
                    0,
                    0,
                    0);

                (void)ops->remove(ctx, installed_wds[j]);
            }

            free(installed_wds);
            return ERR_INOTIFY;
        }

        installed_wds[installed_count] = new_wd;
        installed_count++;

        (void)backend_log_resync_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALLED,
            wd,
            path,
            reason,
            NULL,
            missing_path,
            0,
            0,
            0,
            0,
            0);
    }

    free(installed_wds);
    return ERR_OK;
}

/*
 * backend_lost_scope_reinstall_missing_watches - repair lost-scope coverage
 * @ctx: narrowed backend context used by recovery code
 * @wd: recovered root watch descriptor
 * @path: recovered root path used for diagnostic logs
 * @reason: kernel/backend reason that triggered recovery
 * @scan_context: completed coverage scan with owned missing path copies
 * @ops: watch installation/removal operations used by runtime or tests
 *
 * Lost-scope recovery reaches this helper only after three earlier proofs:
 *
 * - the queued (st_dev, st_ino) identity was found under a monitored root;
 * - the watcher table prefixes were rewritten from the stale path to @path;
 * - a strict directory-only coverage scan completed and collected every
 *   directory that exists under @path but is not currently watched.
 *
 * The remaining gap is kernel watch coverage. A recovered subtree cannot
 * become VALID while even one real directory lacks an inotify watch, because
 * events below that directory could be missed immediately after recovery.
 * Therefore this helper implements an all-or-stale policy:
 *
 * - every missing path must be installed successfully through ops->add();
 * - every successful install is logged as WATCH_LOST_REINSTALLED;
 * - if a later install fails, WATCH_LOST_REINSTALL_FAILED identifies the path;
 * - every watch installed earlier in the same attempt is removed through
 *   ops->remove() and logged as WATCH_LOST_ROLLBACK;
 * - the caller receives an error and must keep the subtree non-VALID.
 *
 * Runtime passes wrappers around watch_manager_add()/watch_manager_remove().
 * Tests pass fake operations so success and rollback branches can be exercised
 * without relying on kernel-assigned watch descriptors or filesystem races.
 *
 * Return: ERR_OK on complete coverage, otherwise error_t.
 */
static error_t backend_lost_scope_reinstall_missing_watches(
    inotify_backend_context_t *ctx,
    int wd,
    const char *path,
    const char *reason,
    const backend_resync_scan_context_t *scan_context,
    const backend_resync_watch_ops_t *ops
)
{
    if (ctx == NULL || path == NULL || reason == NULL ||
        scan_context == NULL || ops == NULL || ops->add == NULL ||
        ops->remove == NULL) {
        return ERR_INVALID_ARG;
    }

    /*
     * A coverage scan with no missing paths is already complete. The caller can
     * continue toward WATCH_LOST_RECOVERY_END without touching the kernel.
     */
    if (scan_context->missing_paths_count == 0)
        return ERR_OK;

    /*
     * Rollback must remove only watches installed by this recovery attempt. It
     * must not remove pre-existing watches that were already part of the
     * recovered subtree before this helper started.
     */
    int *installed_wds =
        calloc(scan_context->missing_paths_count, sizeof(int));

    if (installed_wds == NULL)
        return ERR_ALLOC;

    size_t installed_count = 0;

    for (size_t i = 0; i < scan_context->missing_paths_count; i++) {
        const char *missing_path = scan_context->missing_paths[i];
        int new_wd = ops->add(ctx, missing_path);

        if (new_wd < 0) {
            /*
             * One missing watch failed to install, so the subtree is known to
             * be only partially covered. Roll back all watches installed in
             * this attempt and let the caller keep the recovered subtree stale.
             */
            (void)backend_log_lost_scope_record(
                ctx,
                ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED,
                wd,
                path,
                NULL,
                NULL,
                reason,
                NULL,
                NULL,
                missing_path,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                0);

            for (size_t j = 0; j < installed_count; j++) {
                (void)backend_log_lost_scope_record(
                    ctx,
                    ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK,
                    wd,
                    path,
                    NULL,
                    NULL,
                    reason,
                    NULL,
                    NULL,
                    NULL,
                    installed_wds[j],
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0);

                (void)ops->remove(ctx, installed_wds[j]);
            }

            free(installed_wds);
            return ERR_INOTIFY;
        }

        installed_wds[installed_count] = new_wd;
        installed_count++;

        (void)backend_log_lost_scope_record(
            ctx,
            ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED,
            wd,
            path,
            NULL,
            NULL,
            reason,
            NULL,
            NULL,
            missing_path,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            0);
    }

    free(installed_wds);
    return ERR_OK;
}

/*
 * backend_resync_scan_add_missing_path - copy one missing watch candidate
 * @scan_context: resync scan state that owns the copied path list
 * @path: borrowed scanner path to persist after the callback returns
 *
 * The scanner callback receives borrowed paths. Multi-missing reinstallation
 * needs those paths after fs_scan_tree() returns, so the backend stores private
 * copies in a growable array. The reinstall phase later consumes the whole
 * list so the parent watch returns VALID only after every missing watch in the
 * trusted scope has been restored.
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
 */
static int backend_resync_scan_add_missing_path(
    backend_resync_scan_context_t *scan_context,
    const char *path
)
{
    if (scan_context == NULL || path == NULL)
        return -1;

    if (scan_context->missing_paths_count ==
        scan_context->missing_paths_capacity) {

        size_t new_capacity = scan_context->missing_paths_capacity;

        if (new_capacity == 0)
            new_capacity = 4;
        else
            new_capacity *= 2;

        char **tmp =
            realloc(scan_context->missing_paths,
                    new_capacity * sizeof(char *));

        if (tmp == NULL)
            return -1;

        scan_context->missing_paths = tmp;
        scan_context->missing_paths_capacity = new_capacity;
    }

    size_t path_len = strlen(path) + 1;
    char *copy = malloc(path_len);

    if (copy == NULL)
        return -1;

    memcpy(copy, path, path_len);

    scan_context->missing_paths[scan_context->missing_paths_count] = copy;
    scan_context->missing_paths_count++;

    return 0;
}

/*
 * backend_resync_scan_context_destroy - release paths owned by a scan context
 * @scan_context: context initialized on the stack by the resync helper
 *
 * Only missing_paths contains heap allocations. Counters and ctx are borrowed
 * or scalar state and do not need cleanup.
 */
static void backend_resync_scan_context_destroy(
    backend_resync_scan_context_t *scan_context
)
{
    if (scan_context == NULL)
        return;

    for (size_t i = 0; i < scan_context->missing_paths_count; i++)
        free(scan_context->missing_paths[i]);

    free(scan_context->missing_paths);

    scan_context->missing_paths = NULL;
    scan_context->missing_paths_count = 0;
    scan_context->missing_paths_capacity = 0;
}

/*
 * backend_count_resync_scanned_dir - count directory facts seen by resync scan
 * @entry: scanner entry produced by fs_scan_tree()
 * @userdata: backend_resync_scan_context_t used by the resync helper
 *
 * The callback deliberately ignores non-directory facts. Current resync only
 * cares about the recursive watch structure; file/symlink/other entries belong
 * to future indexing or semantic recovery discussions. The check is read-only:
 * it asks the watcher table whether a path is already covered but does not
 * install missing watches. It only copies missing candidates into the resync
 * context so the caller can decide how many watches to reinstall after the scan
 * has completed.
 *
 * Return: 0 to continue scanning, nonzero to stop after a collection failure.
 */
static int backend_count_resync_scanned_dir(const fs_scan_entry_t *entry,
                                            void *userdata)
{
    backend_resync_scan_context_t *context =
        (backend_resync_scan_context_t *)userdata;

    if (context == NULL || context->ctx == NULL ||
        entry == NULL || entry->type != FS_SCAN_DIR) {
        return 0;
    }

    context->directories_seen++;

    if (watcher_has_path(&context->ctx->runtime->watchers, entry->path)) {
        context->directories_watched++;
    } else {
        context->directories_missing_watch++;

        if (backend_resync_scan_add_missing_path(context, entry->path) != 0) {
            context->missing_paths_failed = 1;
            return 1;
        }
    }

    return 0;
}

/*
 * backend_classify_resync_scan - classify a dry-run resync scan result
 * @scan_context: counters collected by backend_count_resync_scanned_dir()
 *
 * This function is the policy boundary before watch reinstallation. It turns
 * read-only scan counters into a small decision label without mutating kernel
 * watches or watcher-table entries. The next resync phase can switch on this
 * label to decide whether it should call watch_manager_add().
 *
 * Return: scan classification used only for backend diagnostics for now.
 */
static backend_resync_scan_class_t backend_classify_resync_scan(
    const backend_resync_scan_context_t *scan_context
)
{
    if (scan_context == NULL || scan_context->directories_seen == 0)
        return BACKEND_RESYNC_SCAN_EMPTY;

    if (scan_context->directories_missing_watch > 0)
        return BACKEND_RESYNC_SCAN_NEEDS_REINSTALL;

    return BACKEND_RESYNC_SCAN_COVERED;
}

/*
 * backend_resync_scan_class_name - convert scan classification to log text
 * @scan_class: classification produced by backend_classify_resync_scan()
 *
 * Return: stable token used in WATCH_RESYNC_SCAN_CLASS diagnostics.
 */
static const char *backend_resync_scan_class_name(
    backend_resync_scan_class_t scan_class
)
{
    switch (scan_class) {
    case BACKEND_RESYNC_SCAN_EMPTY:
        return "scan-empty";
    case BACKEND_RESYNC_SCAN_COVERED:
        return "scan-covered";
    case BACKEND_RESYNC_SCAN_NEEDS_REINSTALL:
        return "needs-reinstall";
    default:
        return "unknown";
    }
}

/*
 * backend_resync_probe_result_name - convert a probe result to log text
 * @result: internal classification produced by backend_resync_watch()
 *
 * These names are backend diagnostics, not public API. Keeping them behind an
 * enum prevents the resync path from growing ad hoc string literals as new
 * cases are added for scanner-based recovery, unmount, or overflow.
 *
 * Return: stable diagnostic token for @result.
 */
static const char *backend_resync_probe_result_name(
    backend_resync_probe_result_t result
)
{
    switch (result) {
    case BACKEND_RESYNC_PROBE_VALID:
        return "valid";
    case BACKEND_RESYNC_PROBE_MISSING_WATCH:
        return "missing-watch";
    case BACKEND_RESYNC_PROBE_NOT_STALE:
        return "not-stale";
    case BACKEND_RESYNC_PROBE_SET_RESYNCING_FAILED:
        return "set-resyncing";
    case BACKEND_RESYNC_PROBE_PATH_UNREACHABLE:
        return "path-unreachable";
    case BACKEND_RESYNC_PROBE_NOT_DIRECTORY:
        return "not-directory";
    case BACKEND_RESYNC_PROBE_SET_STALE_FAILED:
        return "set-stale";
    case BACKEND_RESYNC_PROBE_SET_VALID_FAILED:
        return "set-valid";
    case BACKEND_RESYNC_PROBE_MISSING_IDENTITY:
        return "missing-identity";
    case BACKEND_RESYNC_PROBE_IDENTITY_MISMATCH:
        return "identity-mismatch";
    case BACKEND_RESYNC_PROBE_REINSTALL_FAILED:
        return "reinstall-failed";
    }

    return "unknown";
}

/*
 * backend_log_resync_record - emit one local WATCH_RESYNC_* diagnostic record
 * @ctx: narrowed backend context used by resync code
 * @type: concrete WATCH_RESYNC_* record type
 * @wd: stale/recovered parent watch descriptor
 * @path: trusted or last-known parent path for the recovery attempt
 * @reason: kernel/backend reason that triggered the resync
 * @state: optional result/classification token, such as "valid"
 * @detail_path: optional child path for missing/reinstalled watch records
 * @related_watch_id: optional related watch descriptor, such as rollback wd
 * @result_code: optional backend-local numeric result, such as scan rc
 * @directories_seen: number of directories found by a coverage scan
 * @directories_watched: number of scanned directories already watched
 * @directories_missing: number of scanned directories missing a watch
 *
 * Local resync diagnostics have richer payloads than plain WATCH_STALE but are
 * still backend diagnostics, not semantic filesystem events. This bridge builds
 * the Event Model v0 record, fills the recovery payload used by the text sink,
 * and keeps a compatibility fallback with the historical text shape.
 * WATCH_RESYNC_SCAN_FAILED keeps the historical error-log channel through a
 * routed sink callback; the other local resync diagnostics remain event-log
 * records.
 *
 * Return: 0 after writing a diagnostic, -1 on unsupported type or invalid ctx.
 */
static int backend_log_resync_record(inotify_backend_context_t *ctx,
                                     alfred_record_type_t type,
                                     int wd,
                                     const char *path,
                                     const char *reason,
                                     const char *state,
                                     const char *detail_path,
                                     int related_watch_id,
                                     int result_code,
                                     size_t directories_seen,
                                     size_t directories_watched,
                                     size_t directories_missing)
{
    alfred_record_t record;
    backend_text_sink_context_t sink_context;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    char payload[BACKEND_RECORD_TEXT_BUFFER_SIZE];
    const char *safe_path = path != NULL ? path : "";

    if (ctx == NULL || ctx->logger == NULL || reason == NULL)
        return -1;

    if (alfred_record_build_watch_diagnostic_with_os_error(type,
                                                           "inotify",
                                                           wd,
                                                           path,
                                                           state,
                                                           reason,
                                                           NULL,
                                                           0,
                                                           NULL,
                                                           NULL,
                                                           &record) == 0) {
        record.recovery.directories_seen = directories_seen;
        record.recovery.directories_watched = directories_watched;
        record.recovery.directories_missing = directories_missing;
        record.recovery.detail_path = detail_path;
        record.recovery.related_watch_id = related_watch_id;
        record.recovery.result_code = result_code;

        sink_context.logger = ctx->logger;
        sink_context.use_error_channel =
            type == ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_FAILED;
        text_sink.write = backend_write_routed_payload;
        text_sink.userdata = &sink_context;
        text_sink.buffer = payload;
        text_sink.buffer_size = sizeof(payload);

        if (alfred_record_text_sink_init(&text_sink, &sink) == 0 &&
            alfred_record_sink_emit(&sink, &record) == 0) {
            return 0;
        }
    }

    switch (type) {
    case ALFRED_RECORD_TYPE_WATCH_RESYNC_BEGIN:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_BEGIN wd=%d path=%s reason=%s",
                     wd,
                     safe_path,
                     reason);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_FAILED:
        logger_error(ctx->logger,
                     "WATCH_RESYNC_SCAN_FAILED wd=%d path=%s reason=%s rc=%d",
                     wd,
                     safe_path,
                     reason,
                     result_code);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_DONE:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_SCAN_DONE wd=%d path=%s reason=%s "
                     "dirs=%zu watched=%zu missing=%zu",
                     wd,
                     safe_path,
                     reason,
                     directories_seen,
                     directories_watched,
                     directories_missing);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_CLASS:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_SCAN_CLASS wd=%d path=%s reason=%s result=%s",
                     wd,
                     safe_path,
                     reason,
                     state != NULL ? state : "");
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_MISSING:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_SCAN_MISSING wd=%d path=%s reason=%s "
                     "missing_path=%s",
                     wd,
                     safe_path,
                     reason,
                     detail_path != NULL ? detail_path : "");
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALL_FAILED:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_REINSTALL_FAILED wd=%d path=%s reason=%s "
                     "missing_path=%s",
                     wd,
                     safe_path,
                     reason,
                     detail_path != NULL ? detail_path : "");
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_ROLLBACK:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_ROLLBACK wd=%d path=%s reason=%s "
                     "removed_wd=%d",
                     wd,
                     safe_path,
                     reason,
                     related_watch_id);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALLED:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_REINSTALLED wd=%d path=%s reason=%s "
                     "installed_path=%s",
                     wd,
                     safe_path,
                     reason,
                     detail_path != NULL ? detail_path : "");
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_RESYNC_END:
        logger_event(ctx->logger,
                     "WATCH_RESYNC_END wd=%d path=%s reason=%s result=%s",
                     wd,
                     safe_path,
                     reason,
                     state != NULL ? state : "");
        return 0;

    default:
        return -1;
    }
}

/*
 * backend_log_lost_scope_record - emit one WATCH_LOST_* diagnostic record
 * @ctx: narrowed backend context used by lost-scope recovery code
 * @type: concrete WATCH_LOST_* record type
 * @wd: stale/recovered watch descriptor, or -1 when the record is root-scoped
 * @path: primary path or scan root for the diagnostic
 * @old_path: optional old scope/prefix path
 * @new_path: optional recovered scope/prefix path
 * @reason: kernel/backend reason that triggered recovery
 * @state: optional result/classification token, such as "valid" or "not-found"
 * @error: optional stable Alfred error token
 * @detail_path: optional missing/reinstalled child path
 * @related_watch_id: optional related watch descriptor, such as rollback wd
 * @directories_seen: number of directories found by a coverage scan
 * @directories_watched: number of scanned directories already watched
 * @directories_missing: number of scanned directories missing a watch
 * @pending_count: lost-scope queue size to include in the diagnostic
 * @children_count: known child prefixes updated by path-prefix recovery
 * @watches_count: watches repaired or marked valid by recovery
 * @retry_count: current retry counter to render as retry/retries
 * @delay_ms: retry delay in milliseconds, or 0 when not applicable
 *
 * This is the lost-scope counterpart of backend_log_resync_record(). It builds
 * an Event Model v0 record, fills the recovery payload, emits the record
 * through the text sink, and falls back to the same text shape if the sink path
 * fails. WATCH_LOST_QUEUE_FAILED keeps the historical error-log channel through
 * the same routed sink callback used by local resync failures.
 *
 * Return: 0 after writing a diagnostic, -1 on unsupported type or invalid ctx.
 */
static int backend_log_lost_scope_record(inotify_backend_context_t *ctx,
                                         alfred_record_type_t type,
                                         int wd,
                                         const char *path,
                                         const char *old_path,
                                         const char *new_path,
                                         const char *reason,
                                         const char *state,
                                         const char *error,
                                         const char *detail_path,
                                         int related_watch_id,
                                         size_t directories_seen,
                                         size_t directories_watched,
                                         size_t directories_missing,
                                         size_t pending_count,
                                         size_t children_count,
                                         size_t watches_count,
                                         unsigned retry_count,
                                         uint64_t delay_ms)
{
    alfred_record_t record;
    backend_text_sink_context_t sink_context;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    char payload[BACKEND_RECORD_TEXT_BUFFER_SIZE];
    const char *safe_path = path != NULL ? path : "";
    const char *safe_old_path = old_path != NULL ? old_path : "";
    const char *safe_new_path = new_path != NULL ? new_path : "";
    const char *safe_reason = reason != NULL ? reason : "";
    const char *safe_state = state != NULL ? state : "";
    const char *safe_error = error != NULL ? error : "";
    const char *safe_detail_path = detail_path != NULL ? detail_path : "";

    if (ctx == NULL || ctx->logger == NULL)
        return -1;

    if (alfred_record_build_watch_diagnostic_with_os_error(type,
                                                           "inotify",
                                                           wd,
                                                           path,
                                                           state,
                                                           reason,
                                                           error,
                                                           0,
                                                           NULL,
                                                           NULL,
                                                           &record) == 0) {
        record.old_path = old_path;
        record.new_path = new_path;
        record.watch.retry_count = retry_count;
        record.recovery.directories_seen = directories_seen;
        record.recovery.directories_watched = directories_watched;
        record.recovery.directories_missing = directories_missing;
        record.recovery.detail_path = detail_path;
        record.recovery.related_watch_id = related_watch_id;
        record.recovery.pending_count = pending_count;
        record.recovery.children_count = children_count;
        record.recovery.watches_count = watches_count;
        record.recovery.delay_ms = delay_ms;

        sink_context.logger = ctx->logger;
        sink_context.use_error_channel =
            type == ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED;
        text_sink.write = backend_write_routed_payload;
        text_sink.userdata = &sink_context;
        text_sink.buffer = payload;
        text_sink.buffer_size = sizeof(payload);

        if (alfred_record_text_sink_init(&text_sink, &sink) == 0 &&
            alfred_record_sink_emit(&sink, &record) == 0) {
            return 0;
        }
    }

    switch (type) {
    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED:
        logger_event(ctx->logger,
                     "WATCH_LOST_QUEUED wd=%d path=%s reason=%s error=%s "
                     "pending=%zu",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_error,
                     pending_count);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_SKIPPED:
        logger_event(ctx->logger,
                     "WATCH_LOST_QUEUE_SKIPPED wd=%d path=%s reason=%s "
                     "error=%s",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_error);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED:
        logger_error(ctx->logger,
                     "WATCH_LOST_QUEUE_FAILED wd=%d path=%s reason=%s "
                     "error=%s",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_error);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN:
        logger_event(ctx->logger,
                     "WATCH_LOST_SCAN_BEGIN root=%s pending=%zu",
                     safe_path,
                     pending_count);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_FOUND:
        logger_event(ctx->logger,
                     "WATCH_LOST_FOUND wd=%d old_path=%s new_path=%s reason=%s",
                     wd,
                     safe_old_path,
                     safe_new_path,
                     safe_reason);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED:
        logger_event(ctx->logger,
                     "WATCH_LOST_PREFIX_UPDATED wd=%d old_prefix=%s "
                     "new_prefix=%s children=%zu",
                     wd,
                     safe_old_path,
                     safe_new_path,
                     children_count);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE:
        logger_event(ctx->logger,
                     "WATCH_LOST_COVERAGE_DONE wd=%d path=%s reason=%s "
                     "dirs=%zu watched=%zu missing=%zu",
                     wd,
                     safe_path,
                     safe_reason,
                     directories_seen,
                     directories_watched,
                     directories_missing);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING:
        logger_event(ctx->logger,
                     "WATCH_LOST_COVERAGE_MISSING wd=%d path=%s reason=%s "
                     "missing_path=%s",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_detail_path);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS:
        logger_event(ctx->logger,
                     "WATCH_LOST_COVERAGE_CLASS wd=%d path=%s reason=%s "
                     "result=%s",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_state);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED:
        logger_event(ctx->logger,
                     "WATCH_LOST_REINSTALLED wd=%d path=%s reason=%s "
                     "installed_path=%s",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_detail_path);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED:
        logger_event(ctx->logger,
                     "WATCH_LOST_REINSTALL_FAILED wd=%d path=%s reason=%s "
                     "missing_path=%s",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_detail_path);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK:
        logger_event(ctx->logger,
                     "WATCH_LOST_ROLLBACK wd=%d path=%s reason=%s removed_wd=%d",
                     wd,
                     safe_path,
                     safe_reason,
                     related_watch_id);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND:
        logger_event(ctx->logger,
                     "WATCH_LOST_NOT_FOUND wd=%d path=%s reason=%s retry=%u",
                     wd,
                     safe_path,
                     safe_reason,
                     retry_count);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED:
        logger_event(ctx->logger,
                     "WATCH_LOST_RETRY_SCHEDULED wd=%d path=%s reason=%s "
                     "result=%s retry=%u delay_ms=%llu pending=%zu",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_state,
                     retry_count,
                     (unsigned long long)delay_ms,
                     pending_count);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP:
        logger_event(ctx->logger,
                     "WATCH_LOST_RECOVERY_GAVE_UP wd=%d path=%s reason=%s "
                     "result=%s retries=%u",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_state,
                     retry_count);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED:
        logger_event(ctx->logger,
                     "WATCH_LOST_RECOVERY_FAILED wd=%d path=%s reason=%s "
                     "error=%s",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_error);
        return 0;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END:
        logger_event(ctx->logger,
                     "WATCH_LOST_RECOVERY_END wd=%d path=%s reason=%s "
                     "result=%s watches=%zu",
                     wd,
                     safe_path,
                     safe_reason,
                     safe_state,
                     watches_count);
        return 0;

    default:
        return -1;
    }
}

/*
 * backend_log_resync_failure - write a normalized resync failure diagnostic
 * @ctx: narrowed backend context used by the poll path
 * @wd: inotify watch descriptor involved in the probe
 * @path: watched path, or an empty string when the watch is already missing
 * @reason: kernel/backend reason that triggered the probe
 * @result: internal probe classification
 * @saved_errno: errno captured at the failing syscall, or 0 when not relevant
 *
 * A single formatter keeps WATCH_RESYNC_FAILED readable while the internal
 * decision tree grows. Syscall failures include errno because they usually
 * need operating-system context. The Event Model v0 record keeps that OS
 * evidence in record.os_error while preserving @result as Alfred's stable
 * diagnostic error token.
 */
static void backend_log_resync_failure(inotify_backend_context_t *ctx,
                                       int wd,
                                       const char *path,
                                       const char *reason,
                                       backend_resync_probe_result_t result,
                                       int saved_errno)
{
    (void)backend_log_watch_diagnostic_record(
        ctx,
        ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,
        wd,
        path,
        NULL,
        reason,
        backend_resync_probe_result_name(result),
        saved_errno,
        NULL,
        saved_errno != 0 ? strerror(saved_errno) : NULL,
        "WATCH_RESYNC_FAILED");
}

/* ============================================================================
 * Recursive Discovery
 * ========================================================================== */

/*
 * backend_handle_dir_create - maintain recursive watches after a new directory
 * @ctx: narrowed backend context used by the poll path
 * @ev: raw inotify event currently being processed
 * @on_event: callback used for synthetic raw events
 * @userdata: callback context passed through unchanged
 *
 * A fast `mkdir -p one/two/three` can create children before the backend has a
 * watch on each new parent. The recursive scan repairs backend state by adding
 * watches below the created directory. When it discovers directories whose
 * kernel create event could not have been observed, it emits synthetic raw
 * CREATE|ISDIR facts so the core can still produce DIR_CREATED.
 *
 * The backend is not deciding that a semantic DIR_CREATED happened. It is
 * saying: "this directory exists and the kernel event was probably missed
 * because the watch tree was not ready yet." The core remains responsible for
 * final event semantics. No generic create deduplication policy is implemented
 * yet; if a duplicate appears, that policy must be designed explicitly.
 */
static void backend_handle_dir_create(inotify_backend_context_t *ctx,
                                      const struct inotify_event *ev,
                                      inotify_backend_event_fn on_event,
                                      void *userdata)
{
    if (ctx == NULL || ev == NULL)
        return;

    if (!ctx->config->recursive)
        return;

    if ((ev->mask & IN_CREATE) == 0 ||
        (ev->mask & IN_ISDIR) == 0) {
        return;
    }

    const char *base =
        watcher_get_path(&ctx->runtime->watchers, ev->wd);

    if (base == NULL)
        return;

    if (watcher_is_stale(&ctx->runtime->watchers, ev->wd))
        return;

    char full[PATH_MAX];

    if (path_join(full, sizeof(full), base, ev->name) != 0)
        return;

    backend_emit_context_t context;

    context.ctx = ctx;
    context.on_event = on_event;
    context.userdata = userdata;
    context.failed = 0;

    if (watch_manager_add(ctx, full) < 0)
        return;

    fs_scan_options_t opts;

    fs_scan_options_defaults(&opts);
    opts.emit_root = 0;

    error_t rc =
        fs_scan_tree(full,
                     &opts,
                     backend_process_scanned_dir_create,
                     &context);

    if (rc != ERR_OK || context.failed) {
        logger_error(ctx->logger,
                     "recursive directory discovery failed path=%s",
                     full);
    }
}

/*
 * backend_process_scanned_dir_create - add watch and emit synthetic raw create
 * @entry: scanner entry discovered below a newly created directory
 * @userdata: backend_emit_context_t supplied by backend_handle_dir_create()
 *
 * Runtime recursive discovery uses emit_root=0 because the root directory has
 * already produced a real inotify CREATE|ISDIR raw event. Every directory
 * reported here is nested below that root and may have been created before a
 * watch existed on its parent, so the backend installs a watch and emits the
 * same synthetic raw create fact that the old recursive discovery callback
 * produced.
 *
 * Return: 0 to continue scanning, nonzero to stop after a watch/raw failure.
 */
static int backend_process_scanned_dir_create(const fs_scan_entry_t *entry,
                                              void *userdata)
{
    backend_emit_context_t *context =
        (backend_emit_context_t *)userdata;

    if (context == NULL || context->ctx == NULL || entry == NULL ||
        entry->type != FS_SCAN_DIR) {
        return 0;
    }

    if (watch_manager_add(context->ctx, entry->path) < 0) {
        context->failed = 1;
        return 1;
    }

    if (backend_emit_synthetic_dir_create(context->ctx,
                                          entry->path,
                                          context->on_event,
                                          context->userdata) != ERR_OK) {
        context->failed = 1;
        return 1;
    }

    return 0;
}

/*
 * backend_now_ns - return monotonic time for synthetic raw events
 *
 * Synthetic raw events need the same time base as normal core input so move
 * timeouts and debounce windows remain comparable.
 *
 * Return: monotonic timestamp in nanoseconds.
 */
static uint64_t backend_now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) +
           (uint64_t)ts.tv_nsec;
}

/*
 * backend_build_overflow_raw - build the global raw event for IN_Q_OVERFLOW
 * @ev: raw inotify event currently being processed
 * @out: destination raw event for the core
 *
 * Queue overflow is different from ordinary inotify records: the kernel emits
 * it with wd=-1 because it describes the whole inotify instance, not one
 * watched path. The normal adapter path needs a watcher-table parent path, so
 * overflow must be bridged explicitly at the backend boundary.
 *
 * The resulting raw event intentionally carries an empty path. The core treats
 * ALFRED_RAW_OVERFLOW as a stream-integrity diagnostic and emits OVERFLOW
 * without reading raw.path. Complete cache/watch recovery remains a separate
 * resync policy.
 *
 * Return: 0 when @ev is an overflow event and @out was filled, -1 otherwise.
 */
static int backend_build_overflow_raw(const struct inotify_event *ev,
                                      alfred_raw_event_t *out)
{
    if (ev == NULL || out == NULL)
        return -1;

    if ((ev->mask & IN_Q_OVERFLOW) == 0)
        return -1;

    memset(out, 0, sizeof(*out));

    out->ts_ns = backend_now_ns();
    out->source = ALFRED_SRC_INOTIFY;
    out->mask = ALFRED_RAW_OVERFLOW;
    out->cookie = ev->cookie;
    out->pid = 0;
    out->path = "";

    return 0;
}

/*
 * backend_raw_mask_has_dispatch_action - decide whether raw facts reach core
 * @mask: ALFRED_RAW_* bitmask returned by the inotify adapter
 *
 * ALFRED_RAW_ISDIR is a modifier, not an action. Audit-only directory events
 * such as IN_OPEN | IN_ISDIR may therefore map to a non-zero Alfred mask that
 * contains only ALFRED_RAW_ISDIR. Forwarding that modifier-only fact would break
 * the audit contract: open/access/close-nowrite are raw-log-only until Alfred
 * has explicit audit raw events.
 *
 * Return: non-zero when @mask contains at least one raw fact that the core is
 * allowed to inspect, zero when it only contains modifiers or no bits.
 */
static int backend_raw_mask_has_dispatch_action(uint32_t mask)
{
    const uint32_t action_mask =
        ALFRED_RAW_CREATE |
        ALFRED_RAW_DELETE |
        ALFRED_RAW_MODIFY |
        ALFRED_RAW_ATTRIB |
        ALFRED_RAW_CLOSE_WRITE |
        ALFRED_RAW_MOVED_FROM |
        ALFRED_RAW_MOVED_TO |
        ALFRED_RAW_OVERFLOW;

    return (mask & action_mask) != 0;
}

/*
 * backend_raw_event_name_from_mask - render an inotify mask for raw logging
 * @mask: Linux inotify event mask
 * @dest: destination buffer
 * @dest_size: destination buffer length
 *
 * This helper is intentionally local to the inotify backend because the names
 * are Linux inotify flags, not Alfred semantic events and not generic app
 * utilities. The resulting string belongs in raw logs only.
 */
static void backend_raw_event_name_from_mask(uint32_t mask,
                                             char *dest,
                                             size_t dest_size)
{
    if (dest == NULL || dest_size == 0)
        return;

    dest[0] = '\0';

    if (mask & IN_CREATE)
        strncat(dest, "IN_CREATE ", dest_size - strlen(dest) - 1);
    if (mask & IN_DELETE)
        strncat(dest, "IN_DELETE ", dest_size - strlen(dest) - 1);
    if (mask & IN_MODIFY)
        strncat(dest, "IN_MODIFY ", dest_size - strlen(dest) - 1);
    if (mask & IN_ATTRIB)
        strncat(dest, "IN_ATTRIB ", dest_size - strlen(dest) - 1);
    if (mask & IN_CLOSE_WRITE)
        strncat(dest, "IN_CLOSE_WRITE ", dest_size - strlen(dest) - 1);
    if (mask & IN_CLOSE_NOWRITE)
        strncat(dest, "IN_CLOSE_NOWRITE ", dest_size - strlen(dest) - 1);
    if (mask & IN_OPEN)
        strncat(dest, "IN_OPEN ", dest_size - strlen(dest) - 1);
    if (mask & IN_ACCESS)
        strncat(dest, "IN_ACCESS ", dest_size - strlen(dest) - 1);
    if (mask & IN_MOVED_FROM)
        strncat(dest, "IN_MOVED_FROM ", dest_size - strlen(dest) - 1);
    if (mask & IN_MOVED_TO)
        strncat(dest, "IN_MOVED_TO ", dest_size - strlen(dest) - 1);
    if (mask & IN_ISDIR)
        strncat(dest, "IN_ISDIR ", dest_size - strlen(dest) - 1);
    if (mask & IN_DELETE_SELF)
        strncat(dest, "IN_DELETE_SELF ", dest_size - strlen(dest) - 1);
    if (mask & IN_MOVE_SELF)
        strncat(dest, "IN_MOVE_SELF ", dest_size - strlen(dest) - 1);
    if (mask & IN_UNMOUNT)
        strncat(dest, "IN_UNMOUNT ", dest_size - strlen(dest) - 1);
    if (mask & IN_IGNORED)
        strncat(dest, "IN_IGNORED ", dest_size - strlen(dest) - 1);
    if (mask & IN_Q_OVERFLOW)
        strncat(dest, "IN_Q_OVERFLOW ", dest_size - strlen(dest) - 1);

    if (dest[0] == '\0') {
        strncpy(dest, "UNRECOGNIZED", dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
}

/*
 * backend_emit_synthetic_dir_create - emit a synthetic raw directory create
 * @ctx: backend context used by the discovery path
 * @path: directory discovered by recursive scanning
 * @on_event: callback used to deliver raw events to the core path
 * @userdata: callback context passed through unchanged
 *
 * The event is synthetic only in the backend sense: the directory really
 * exists, but the kernel create notification was missed because the needed
 * watch did not exist yet. The core receives a normal ALFRED_RAW_CREATE |
 * ALFRED_RAW_ISDIR fact and does not need to know whether it came from an
 * inotify record or recursive discovery.
 *
 * This design keeps recovery local to the backend while preserving a single
 * raw-event contract for the core. The current core emits every CREATE fact it
 * receives. If a future scenario produces both synthetic and real CREATE facts
 * for the same directory, an explicit deduplication policy must be added before
 * suppressing either semantic event.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
static int backend_emit_synthetic_dir_create(
    inotify_backend_context_t *ctx,
    const char *path,
    inotify_backend_event_fn on_event,
    void *userdata
)
{
    if (ctx == NULL || path == NULL || on_event == NULL)
        return ERR_INVALID_ARG;

    alfred_raw_event_t raw;

    memset(&raw, 0, sizeof(raw));

    raw.ts_ns = backend_now_ns();
    raw.source = ALFRED_SRC_INOTIFY;
    raw.mask = ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR;
    raw.cookie = 0;
    raw.pid = 0;
    raw.path = path;

    return on_event(&raw, userdata);
}
