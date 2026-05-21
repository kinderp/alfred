/* ============================================================================
 * inotify_backend.c - inotify runtime backend
 *
 * This file owns the current inotify polling boundary. During the transition it
 * still stores descriptor and watcher state in app_t, but app.c no longer reads
 * struct inotify_event buffers directly or performs recursive discovery.
 * ========================================================================== */

#include "inotify_backend.h"

#include "app.h"
#include "errors.h"
#include "inotify_adapter.h"
#include "logger.h"
#include "utils.h"
#include "watch_manager.h"
#include "watcher.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ============================================================================
 * Configuration Constants
 * ========================================================================== */

#ifndef EVENT_BUFFER_SIZE
#define EVENT_BUFFER_SIZE 65536
#endif

/* ============================================================================
 * Local Types
 * ========================================================================== */

typedef struct backend_emit_context {
    inotify_backend_event_fn on_event;
    void *userdata;
} backend_emit_context_t;

/* ============================================================================
 * Local Declarations
 * ========================================================================== */

static void backend_handle_dir_create(app_t *app,
                                      const struct inotify_event *ev,
                                      inotify_backend_event_fn on_event,
                                      void *userdata);

static void backend_process_discovered_dir(app_t *app,
                                           const char *path,
                                           void *userdata);

static uint64_t backend_now_ns(void);

static int backend_emit_synthetic_dir_create(
    app_t *app,
    const char *path,
    inotify_backend_event_fn on_event,
    void *userdata
);

/* ============================================================================
 * Lifecycle
 * ========================================================================== */

int inotify_backend_init(app_t *app)
{
    if (app == NULL)
        return ERR_INVALID_ARG;

    app->inotify_fd =
        inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (app->inotify_fd < 0) {
        logger_error(&app->logger,
                     "inotify_init1 failed errno=%d (%s)",
                     errno,
                     strerror(errno));

        return ERR_INOTIFY;
    }

    logger_info(&app->logger,
                "inotify backend initialized fd=%d",
                app->inotify_fd);

    return ERR_OK;
}

int inotify_backend_add_startup_watch(app_t *app,
                                      const char *path)
{
    if (app == NULL || path == NULL)
        return ERR_INVALID_ARG;

    if (app->config.recursive) {
        if (watch_manager_add_recursive(app, path) < 0)
            return ERR_INOTIFY;
    }
    else {
        if (watch_manager_add(app, path) < 0)
            return ERR_INOTIFY;
    }

    return ERR_OK;
}

void inotify_backend_shutdown(app_t *app)
{
    if (app == NULL)
        return;

    if (app->inotify_fd >= 0) {
        close(app->inotify_fd);
        app->inotify_fd = -1;
    }
}

/* ============================================================================
 * Polling
 * ========================================================================== */

int inotify_backend_poll(app_t *app,
                         inotify_backend_event_fn on_event,
                         void *userdata)
{
    if (app == NULL || on_event == NULL)
        return ERR_INVALID_ARG;

    char buffer[EVENT_BUFFER_SIZE];

    ssize_t bytes =
        read(app->inotify_fd,
             buffer,
             sizeof(buffer));

    if (bytes < 0) {

        if (errno == EAGAIN ||
            errno == EWOULDBLOCK) {

            usleep(10000); /* 10ms */
            return ERR_OK;
        }

        if (errno == EINTR)
            return ERR_OK;

        logger_error(&app->logger,
                     "read failed errno=%d (%s)",
                     errno,
                     strerror(errno));

        return ERR_IO;
    }

    if (bytes == 0) {
        logger_error(&app->logger,
                     "unexpected EOF on inotify fd");
        return ERR_IO;
    }

    logger_raw(&app->logger,
               "read %zd bytes from inotify",
               bytes);

    char *ptr = buffer;
    char mask_str[256];

    while (ptr < buffer + bytes) {

        struct inotify_event *ev =
            (struct inotify_event *)ptr;

        const char *parent =
            watcher_get_path(&app->watchers, ev->wd);

        raw_event_name_from_mask(ev->mask, mask_str, sizeof(mask_str));
        logger_raw(&app->logger,
                   "%s wd=%d path=%s name=%s",
                   mask_str,
                   ev->wd,
                   parent ? parent : "",
                   ev->len ? ev->name : "");

        alfred_raw_event_t raw;
        char full_path[PATH_MAX];
        const alfred_raw_event_t *raw_ptr = NULL;

        if (parent != NULL) {
            if (inotify_adapter_build_raw(ev,
                                          parent,
                                          full_path,
                                          sizeof(full_path),
                                          &raw) == 0) {
                raw_ptr = &raw;
            }
            else {
                logger_error(&app->logger,
                             "failed to build core raw event wd=%d",
                             ev->wd);
            }
        }

        int callback_status =
            on_event(app, ev, raw_ptr, userdata);

        if (callback_status != ERR_OK)
            return callback_status;

        backend_handle_dir_create(app, ev, on_event, userdata);

        ptr += sizeof(struct inotify_event) + ev->len;
    }

    return ERR_OK;
}

/* ============================================================================
 * Recursive Discovery
 * ========================================================================== */

static void backend_handle_dir_create(app_t *app,
                                      const struct inotify_event *ev,
                                      inotify_backend_event_fn on_event,
                                      void *userdata)
{
    if (app == NULL || ev == NULL)
        return;

    if (!app->config.recursive)
        return;

    if ((ev->mask & IN_CREATE) == 0 ||
        (ev->mask & IN_ISDIR) == 0) {
        return;
    }

    const char *base =
        watcher_get_path(&app->watchers, ev->wd);

    if (base == NULL)
        return;

    char full[PATH_MAX];

    if (path_join(full, sizeof(full), base, ev->name) != 0)
        return;

    backend_emit_context_t context;

    context.on_event = on_event;
    context.userdata = userdata;

    watch_manager_add_recursive_with_discovery(
        app,
        full,
        backend_process_discovered_dir,
        &context);
}

static void backend_process_discovered_dir(app_t *app,
                                           const char *path,
                                           void *userdata)
{
    backend_emit_context_t *context =
        (backend_emit_context_t *)userdata;

    if (context == NULL)
        return;

    (void)backend_emit_synthetic_dir_create(app,
                                            path,
                                            context->on_event,
                                            context->userdata);
}

static uint64_t backend_now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) +
           (uint64_t)ts.tv_nsec;
}

static int backend_emit_synthetic_dir_create(
    app_t *app,
    const char *path,
    inotify_backend_event_fn on_event,
    void *userdata
)
{
    if (app == NULL || path == NULL || on_event == NULL)
        return ERR_INVALID_ARG;

    alfred_raw_event_t raw;

    memset(&raw, 0, sizeof(raw));

    raw.ts_ns = backend_now_ns();
    raw.source = ALFRED_SRC_INOTIFY;
    raw.mask = ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR;
    raw.cookie = 0;
    raw.pid = 0;
    raw.path = path;

    return on_event(app, NULL, &raw, userdata);
}
