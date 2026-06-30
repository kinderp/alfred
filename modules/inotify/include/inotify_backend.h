/* ============================================================================
 * inotify_backend.h - inotify backend boundary
 *
 * This interface is the first step toward moving backend-specific runtime
 * ownership out of app.c. Backend operations receive an explicit backend
 * context and produce raw Alfred events for the core.
 *
 * The backend contract is intentionally raw-oriented: callers receive
 * alfred_raw_event_t records and leave semantic classification to the core.
 * ========================================================================== */

#ifndef INOTIFY_BACKEND_H
#define INOTIFY_BACKEND_H

#include "alfred_backend_capabilities.h"
#include "alfred_backend_ops.h"
#include "alfred_correlator.h"
#include "alfred_record.h"
#include "inotify_config.h"
#include "logger.h"
#include "watcher.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define INOTIFY_LOST_SCOPE_REASON_SIZE 64

/*
 * inotify_lost_scope_entry_t - delayed recovery request for one stale scope
 * @wd: inotify watch descriptor that lost path reliability
 * @device_id: filesystem device id captured when the watch was installed
 * @inode_id: inode id captured when the watch was installed
 * @first_seen_ns: monotonic time when the scope was first queued
 * @retry_after_ns: monotonic time before which recovery should not run again
 * @retry_count: number of failed or delayed recovery attempts already made
 * @old_path: last path Alfred associated with @wd before it became stale
 * @scan_root: bounded root that should be searched before wider fallbacks
 * @reason: backend/kernel reason that moved the scope into recovery
 *
 * This is backend recovery state, not an Alfred raw event and not a semantic
 * core event. The entry preserves enough evidence for delayed scans to search
 * monitored roots for the same filesystem object without trusting the stale
 * textual path. @scan_root records the best bounded search scope known at
 * enqueue time. Runtime recovery tries that root first and then, on a clean
 * not-found result, falls back to the other configured backend roots.
 */
typedef struct inotify_lost_scope_entry {
    int wd;
    dev_t device_id;
    ino_t inode_id;
    uint64_t first_seen_ns;
    uint64_t retry_after_ns;
    unsigned retry_count;
    char old_path[PATH_MAX];
    char scan_root[PATH_MAX];
    char reason[INOTIFY_LOST_SCOPE_REASON_SIZE];
} inotify_lost_scope_entry_t;

/*
 * inotify_lost_scope_queue_t - FIFO queue of scopes awaiting wide recovery
 * @items: circular buffer storing queued recovery entries
 * @count: number of entries currently stored
 * @capacity: allocated number of slots in @items
 * @head: index of the next entry to pop
 *
 * The queue lives in the inotify backend because it describes backend trust and
 * recovery, not core semantics. The first implementation is deliberately
 * single-threaded and testable; a later worker thread can consume the same
 * queue after debounce/backoff policy is agreed.
 */
typedef struct inotify_lost_scope_queue {
    inotify_lost_scope_entry_t *items;
    size_t count;
    size_t capacity;
    size_t head;
} inotify_lost_scope_queue_t;

/*
 * inotify_backend_t - runtime state owned by the inotify backend
 * @fd: nonblocking inotify descriptor, or -1 when closed
 * @watchers: mapping from inotify watch descriptors to watched paths
 * @lost_scopes: stale scopes waiting for delayed identity-based recovery
 * @configured_roots: startup roots successfully registered with the backend
 * @configured_roots_count: number of stored startup roots
 * @configured_roots_capacity: allocated number of root slots
 *
 * The application owns the containing app_t object, but these fields are
 * managed only through the inotify backend API. Keeping them grouped makes the
 * remaining app.c dependency explicit and prepares a future split where the
 * backend can own an independent context. Configured roots are backend state
 * because lost-scope recovery must know the bounded search scopes without
 * reaching back into app.c or argv.
 */
typedef struct inotify_backend {
    int fd;
    watcher_table_t watchers;
    inotify_lost_scope_queue_t lost_scopes;
    char (*configured_roots)[PATH_MAX];
    size_t configured_roots_count;
    size_t configured_roots_capacity;
} inotify_backend_t;

/*
 * inotify_backend_record_emit_fn - offer one diagnostic record upstream
 * @record: borrowed Event Model v0 record built by the backend
 * @userdata: opaque pointer supplied in inotify_backend_context_t
 *
 * The inotify backend may build diagnostic records such as WATCH_ADDED or
 * WATCH_REMOVED, but it must not know app_t, output files, JSONL writers, or
 * future sink topology. This callback is the narrow ownership boundary: the
 * backend offers a borrowed record, and the application decides whether to
 * clone/enqueue it, ignore it, or report an output failure.
 *
 * Return: 0 on success or intentionally disabled output, nonzero on failure.
 */
typedef int (*inotify_backend_record_emit_fn)(
    const alfred_record_t *record,
    void *userdata
);

/*
 * inotify_backend_context_t - borrowed dependencies for backend operations
 * @runtime: backend-owned runtime state to mutate
 * @config: application-owned inotify configuration read by backend helpers
 * @logger: application-owned logger used for backend diagnostics
 * @emit_record: optional callback for structured backend diagnostic records
 * @emit_record_userdata: opaque context passed to @emit_record
 *
 * The context does not own any pointed-to object. It exists to avoid passing
 * the whole app_t or top-level config_t into backend helpers that only need
 * fd/watchers, inotify-specific configuration fields, logging, and an optional
 * structured output boundary. The backend does not know JSONL or concrete
 * writers, but helpers that call @emit_record must propagate callback failures:
 * when the application installs this boundary for a fail-closed ledger, losing
 * a structured diagnostic record is a runtime error, not best-effort logging.
 */
typedef struct inotify_backend_context {
    inotify_backend_t *runtime;
    const inotify_config_t *config;
    logger_t *logger;
    inotify_backend_record_emit_fn emit_record;
    void *emit_record_userdata;
} inotify_backend_context_t;

/*
 * inotify_backend_ops_config_t - concrete config for inotify Backend API v0 ops
 * @config: borrowed inotify backend configuration
 * @logger: borrowed logger used by the existing inotify initialization path
 *
 * Backend API v0 keeps alfred_backend_config_t opaque. The static inotify
 * adapter therefore receives this concrete module-local config object cast to
 * const alfred_backend_config_t*. The adapter does not own either pointer.
 */
typedef struct inotify_backend_ops_config {
    const inotify_config_t *config;
    logger_t *logger;
} inotify_backend_ops_config_t;

/*
 * inotify_backend_ops_runtime_t - concrete runtime for inotify Backend API v0
 * @runtime: existing inotify backend state
 * @context: existing narrowed context built from @runtime and borrowed config
 * @initialized: nonzero after ops init succeeds and before ops destroy releases
 *
 * Backend API v0 keeps alfred_backend_t opaque. Callers using the static
 * inotify ops adapter allocate this concrete runtime, zero it before first use,
 * and pass it cast to alfred_backend_t*. This type is not a dynamic plugin ABI;
 * it is the first static adapter bridge around the current inotify runtime.
 */
typedef struct inotify_backend_ops_runtime {
    inotify_backend_t runtime;
    inotify_backend_context_t context;
    int initialized;
} inotify_backend_ops_runtime_t;

/*
 * inotify_backend_event_fn - deliver one raw backend event to the application
 * @raw: raw Alfred event, or NULL when the inotify record cannot be converted
 * @userdata: opaque pointer supplied to inotify_backend_poll()
 *
 * The backend does not interpret @userdata. The application decides which
 * object to pass through; app.c currently passes app_t here.
 *
 * Return: ERR_OK to continue polling, or a negative error_t value to stop.
 */
typedef int (*inotify_backend_event_fn)(
    const alfred_raw_event_t *raw,
    void *userdata
);

/*
 * inotify_backend_capabilities - return static Backend API v0 capabilities
 *
 * The descriptor is backend metadata, not runtime configuration and not an
 * event-path operation. It says that the inotify backend can observe filesystem
 * mutation, recursive watches, metadata/self/overflow diagnostics, identity
 * tracking and lost-scope recovery. It deliberately does not claim API-level
 * audit events, process context, network context, permission events or
 * blocking/enforcement. The current inotify audit opt-in is raw-log-only.
 *
 * Return: borrowed pointer to static process-lifetime metadata.
 */
const alfred_backend_capabilities_t *inotify_backend_capabilities(void);

/*
 * inotify_backend_ops - return the static Backend API v0 ops skeleton
 *
 * This descriptor connects the inotify backend identity and capabilities to the
 * common Backend API v0 shape. app.c still calls the existing inotify-specific
 * functions directly, so the normal runtime behavior is unchanged. The staged
 * adapter path wires init/destroy/add_target/remove_target through the common
 * ops table for focused tests; polling, start and stop remain placeholders
 * until their own migration steps.
 *
 * Return: borrowed pointer to static process-lifetime metadata.
 */
const alfred_backend_ops_t *inotify_backend_ops(void);

/*
 * inotify_backend_init - initialize descriptor and watcher table
 * @ctx: backend context containing runtime, configuration, and logger
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_init(inotify_backend_context_t *ctx);

/*
 * inotify_backend_add_startup_watch - add one configured startup path
 * @ctx: initialized backend context
 * @path: filesystem path to watch
 *
 * The function respects ctx->config->recursive and delegates the actual watch
 * installation to watch_manager.c.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_add_startup_watch(inotify_backend_context_t *ctx,
                                      const char *path);

/*
 * inotify_backend_remove_startup_watch - remove one configured startup path
 * @ctx: initialized backend context
 * @path: filesystem path previously added as a startup target
 *
 * Removes the exact watched path. When recursive mode is active, also removes
 * watched child paths below @path so a target removal does not leave nested
 * watches active behind the Backend API boundary.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_remove_startup_watch(inotify_backend_context_t *ctx,
                                         const char *path);

/*
 * inotify_backend_poll - read and dispatch available inotify records
 * @ctx: backend context containing runtime, configuration, and logger
 * @on_event: callback that forwards raw events to the app/core boundary
 * @userdata: opaque callback context
 *
 * Reads from the nonblocking inotify descriptor, logs backend raw diagnostics,
 * converts records to alfred_raw_event_t, and emits synthetic raw directory
 * creates when recursive discovery finds directories created before their
 * parent watch was installed. The poll path also processes a tiny synchronous
 * batch of mature lost-scope recoveries after idle reads and after consumed
 * event buffers; heavier worker/debounce policy remains a later step.
 *
 * Return: ERR_OK on success or idle poll, a negative error_t value on failure.
 */
int inotify_backend_poll(inotify_backend_context_t *ctx,
                         inotify_backend_event_fn on_event,
                         void *userdata);

/*
 * inotify_backend_shutdown - release backend-owned runtime state
 * @ctx: backend context containing the runtime state
 *
 * Safe to call during partial initialization failure.
 */
void inotify_backend_shutdown(inotify_backend_context_t *ctx);

#endif /* INOTIFY_BACKEND_H */
