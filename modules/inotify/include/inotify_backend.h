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

#include "alfred_correlator.h"
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
 * core event. The entry preserves enough evidence for a future delayed scan to
 * search monitored roots for the same filesystem object without trusting the
 * stale textual path. @scan_root records the best bounded search scope known at
 * enqueue time. The current runtime still uses the stale local path as this
 * value; a later step will replace it with the configured watch root and then
 * add the configured-roots fallback policy.
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
 *
 * The application owns the containing app_t object, but these fields are
 * managed only through the inotify backend API. Keeping them grouped makes the
 * remaining app.c dependency explicit and prepares a future split where the
 * backend can own an independent context.
 */
typedef struct inotify_backend {
    int fd;
    watcher_table_t watchers;
    inotify_lost_scope_queue_t lost_scopes;
} inotify_backend_t;

/*
 * inotify_backend_context_t - borrowed dependencies for backend operations
 * @runtime: backend-owned runtime state to mutate
 * @config: application-owned inotify configuration read by backend helpers
 * @logger: application-owned logger used for backend diagnostics
 *
 * The context does not own any pointed-to object. It exists to avoid passing
 * the whole app_t or top-level config_t into backend helpers that only need
 * fd/watchers, inotify-specific configuration fields, and logging.
 */
typedef struct inotify_backend_context {
    inotify_backend_t *runtime;
    const inotify_config_t *config;
    logger_t *logger;
} inotify_backend_context_t;

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
 * inotify_backend_poll - read and dispatch available inotify records
 * @ctx: backend context containing runtime, configuration, and logger
 * @on_event: callback that forwards raw events to the app/core boundary
 * @userdata: opaque callback context
 *
 * Reads from the nonblocking inotify descriptor, logs backend raw diagnostics,
 * converts records to alfred_raw_event_t, and emits synthetic raw directory
 * creates when recursive discovery finds directories created before their
 * parent watch was installed.
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
