/* ============================================================================
 * inotify_backend.h - inotify backend boundary
 *
 * This interface is the first step toward moving backend-specific runtime
 * ownership out of app.c. It still uses app_t as temporary state storage, but
 * centralizes inotify reading, raw conversion, and recursive watch discovery.
 * ========================================================================== */

#ifndef INOTIFY_BACKEND_H
#define INOTIFY_BACKEND_H

#include "alfred_correlator.h"

#include <sys/inotify.h>

struct app;

typedef int (*inotify_backend_event_fn)(
    struct app *app,
    const struct inotify_event *ev,
    const alfred_raw_event_t *raw,
    void *userdata
);

int inotify_backend_init(struct app *app);

int inotify_backend_add_startup_watch(struct app *app,
                                      const char *path);

int inotify_backend_poll(struct app *app,
                         inotify_backend_event_fn on_event,
                         void *userdata);

void inotify_backend_shutdown(struct app *app);

#endif /* INOTIFY_BACKEND_H */
