/* ============================================================================
 * inotify_backend.h - inotify backend boundary
 *
 * This interface is the first step toward moving backend-specific runtime
 * ownership out of app.c. It still uses app_t as temporary state storage, but
 * centralizes inotify reading, raw conversion, and recursive watch discovery.
 *
 * The backend contract is intentionally raw-oriented: callers receive
 * alfred_raw_event_t records and leave semantic classification to the core.
 * Legacy semantic dispatch is an implementation detail used only by shadow
 * mode while the migration is still reversible.
 * ========================================================================== */

#ifndef INOTIFY_BACKEND_H
#define INOTIFY_BACKEND_H

#include "alfred_correlator.h"
#include "watcher.h"

#include <sys/inotify.h>

struct app;

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
 * inotify_backend_event_fn - deliver one raw backend event to the application
 * @app: application context used during the current integration phase
 * @raw: raw Alfred event, or NULL when the inotify record cannot be converted
 * @userdata: opaque pointer supplied to inotify_backend_poll()
 *
 * Return: ERR_OK to continue polling, or a negative error_t value to stop.
 */
typedef int (*inotify_backend_event_fn)(
    struct app *app,
    const alfred_raw_event_t *raw,
    void *userdata
);

/*
 * inotify_backend_init - initialize descriptor, watcher table, and shadow state
 * @app: application context containing configuration and backend storage
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_init(struct app *app);

/*
 * inotify_backend_add_startup_watch - add one configured startup path
 * @app: initialized application context
 * @path: filesystem path to watch
 *
 * The function respects app->config.recursive and delegates the actual watch
 * installation to watch_manager.c.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_add_startup_watch(struct app *app,
                                      const char *path);

/*
 * inotify_backend_poll - read and dispatch available inotify records
 * @app: initialized application context
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
int inotify_backend_poll(struct app *app,
                         inotify_backend_event_fn on_event,
                         void *userdata);

/*
 * inotify_backend_shutdown - release backend-owned runtime state
 * @app: application context containing the backend state
 *
 * Safe to call during partial initialization failure.
 */
void inotify_backend_shutdown(struct app *app);

#endif /* INOTIFY_BACKEND_H */
