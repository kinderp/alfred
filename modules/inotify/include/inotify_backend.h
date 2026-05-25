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
#include "config.h"
#include "logger.h"
#include "watcher.h"

/*
 * inotify_backend_t - runtime state owned by the inotify backend
 * @fd: nonblocking inotify descriptor, or -1 when closed
 * @watchers: mapping from inotify watch descriptors to watched paths
 *
 * The application owns the containing app_t object, but these fields are
 * managed only through the inotify backend API. Keeping them grouped makes the
 * remaining app.c dependency explicit and prepares a future split where the
 * backend can own an independent context.
 */
typedef struct inotify_backend {
    int fd;
    watcher_table_t watchers;
} inotify_backend_t;

/*
 * inotify_backend_context_t - borrowed dependencies for backend operations
 * @runtime: backend-owned runtime state to mutate
 * @config: application-owned configuration read by backend helpers
 * @logger: application-owned logger used for backend diagnostics
 *
 * The context does not own any pointed-to object. It exists to avoid passing
 * the whole app_t into backend helpers that only need fd/watchers, selected
 * configuration fields, and logging.
 */
typedef struct inotify_backend_context {
    inotify_backend_t *runtime;
    const config_t *config;
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
 * inotify_backend_init - initialize descriptor, watcher table, and shadow state
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
