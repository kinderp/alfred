/* ============================================================================
 * inotify_backend.c - inotify runtime backend
 *
 * This file owns the current inotify polling boundary. It keeps the descriptor
 * and watcher table in app->inotify while app.c remains responsible for
 * high-level orchestration.
 *
 * The backend must stop at raw facts. It may know how to read inotify records,
 * map watch descriptors to paths, keep recursive watches alive, and synthesize
 * raw directory-create facts discovered during a recursive scan. It must not
 * decide final FILE_* or DIR_* semantics; that belongs to the core. When the
 * binary is built with ALFRED_ENABLE_LEGACY_SHADOW, it can also run the
 * temporary legacy dispatcher for shadow comparison.
 * ========================================================================== */

#include "inotify_backend.h"

#include "app.h"
#include "errors.h"
#ifdef ALFRED_ENABLE_LEGACY_SHADOW
#include "events.h"
#endif
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
    inotify_backend_context_t *ctx;
    inotify_backend_event_fn on_event;
    void *userdata;
} backend_emit_context_t;

/* ============================================================================
 * Local Declarations
 * ========================================================================== */

static void backend_handle_dir_create(inotify_backend_context_t *ctx,
                                      const struct inotify_event *ev,
                                      inotify_backend_event_fn on_event,
                                      void *userdata);

static void backend_context_from_app(app_t *app,
                                     inotify_backend_context_t *ctx);

static void backend_process_discovered_dir(inotify_backend_context_t *ctx,
                                           const char *path,
                                           void *userdata);

static uint64_t backend_now_ns(void);

static int backend_emit_synthetic_dir_create(
    inotify_backend_context_t *ctx,
    const char *path,
    inotify_backend_event_fn on_event,
    void *userdata
);

/*
 * backend_context_from_app - build the temporary narrowed backend context
 * @app: application context that still owns runtime/config/logger
 * @ctx: context object to fill
 *
 * This is the first migration step away from passing app_t into every backend
 * helper. The context borrows app-owned objects; it does not transfer
 * ownership.
 */
static void backend_context_from_app(app_t *app,
                                     inotify_backend_context_t *ctx)
{
    ctx->runtime = &app->inotify;
    ctx->config = &app->config;
    ctx->logger = &app->logger;
}

/* ============================================================================
 * Lifecycle
 * ========================================================================== */

/*
 * inotify_backend_init - initialize the inotify backend runtime
 * @app: application context containing config, logger, and backend storage
 *
 * Initializes the watcher table first, then opens a nonblocking inotify file
 * descriptor. If the binary was built with ALFRED_ENABLE_LEGACY_SHADOW,
 * shadow mode also initializes the legacy semantic dispatcher so the backend
 * can produce both the official core stream and the comparison legacy stream.
 * Without that build flag, requesting event_engine=shadow fails clearly instead
 * of silently falling back to core mode.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_init(app_t *app)
{
    if (app == NULL)
        return ERR_INVALID_ARG;

    inotify_backend_context_t ctx;

    backend_context_from_app(app, &ctx);

    ctx.runtime->fd = -1;

    if (watcher_init(&ctx.runtime->watchers,
                     ctx.config->watcher_capacity) != 0) {

        logger_error(ctx.logger,
                     "watcher_init failed");

        return ERR_ALLOC;
    }

    logger_info(ctx.logger,
                "watcher table initialized");

    ctx.runtime->fd =
        inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (ctx.runtime->fd < 0) {
        logger_error(ctx.logger,
                     "inotify_init1 failed errno=%d (%s)",
                     errno,
                     strerror(errno));

        watcher_destroy(&ctx.runtime->watchers);
        return ERR_INOTIFY;
    }

    logger_info(ctx.logger,
                "inotify backend initialized fd=%d",
                ctx.runtime->fd);

    if (ctx.config->event_engine_mode == EVENT_ENGINE_SHADOW) {
#ifdef ALFRED_ENABLE_LEGACY_SHADOW
        if (legacy_events_init(ctx.config->move_cache_size) != 0) {
            logger_error(ctx.logger,
                         "legacy_events_init failed");

            close(ctx.runtime->fd);
            ctx.runtime->fd = -1;
            watcher_destroy(&ctx.runtime->watchers);
            return ERR_ALLOC;
        }

        logger_info(ctx.logger,
                    "legacy event dispatcher initialized");
#else
        logger_error(ctx.logger,
                     "shadow event engine requires build with "
                     "ENABLE_LEGACY_SHADOW=1");

        close(ctx.runtime->fd);
        ctx.runtime->fd = -1;
        watcher_destroy(&ctx.runtime->watchers);
        return ERR_CONFIG;
#endif
    }

    return ERR_OK;
}

/*
 * inotify_backend_add_startup_watch - add one startup path to the backend
 * @app: initialized application context
 * @path: path supplied on the command line
 *
 * Startup watch installation is backend state, not semantic event processing.
 * Recursive mode installs watches for the existing tree before the event loop
 * starts; later recursive updates are handled when directory-create raw facts
 * arrive from inotify.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int inotify_backend_add_startup_watch(app_t *app,
                                      const char *path)
{
    if (app == NULL || path == NULL)
        return ERR_INVALID_ARG;

    inotify_backend_context_t ctx;

    backend_context_from_app(app, &ctx);

    if (ctx.config->recursive) {
        if (watch_manager_add_recursive(&ctx, path) < 0)
            return ERR_INOTIFY;
    }
    else {
        if (watch_manager_add(&ctx, path) < 0)
            return ERR_INOTIFY;
    }

    return ERR_OK;
}

/*
 * inotify_backend_shutdown - release backend runtime resources
 * @app: application context containing the backend state
 *
 * The function is safe for partial initialization paths. When legacy shadow is
 * compiled in, its shutdown stays here because the backend owns the temporary
 * bridge to events.c.
 */
void inotify_backend_shutdown(app_t *app)
{
    if (app == NULL)
        return;

    if (app->inotify.fd >= 0) {
        close(app->inotify.fd);
        app->inotify.fd = -1;
    }

#ifdef ALFRED_ENABLE_LEGACY_SHADOW
    legacy_events_shutdown();
#endif
    watcher_destroy(&app->inotify.watchers);
}

/* ============================================================================
 * Polling
 * ========================================================================== */

/*
 * inotify_backend_poll - process one nonblocking inotify read
 * @app: initialized application context
 * @on_event: callback used to deliver raw events to the application/core layer
 * @userdata: opaque callback context passed through unchanged
 *
 * The processing order is deliberate:
 *
 * 1. log the kernel raw event for diagnostics
 * 2. convert the inotify record into alfred_raw_event_t when possible
 * 3. deliver the raw event to the core through @on_event
 * 4. optionally run the compiled-in legacy dispatcher for shadow comparison
 * 5. update recursive watches and emit synthetic raw directory creates
 *
 * This keeps the core as the official semantic path while preserving the
 * legacy stream only as a comparison tool.
 *
 * Return: ERR_OK on success or idle poll, a negative error_t value on failure.
 */
int inotify_backend_poll(app_t *app,
                         inotify_backend_event_fn on_event,
                         void *userdata)
{
    if (app == NULL || on_event == NULL)
        return ERR_INVALID_ARG;

    inotify_backend_context_t ctx;
    backend_context_from_app(app, &ctx);

    char buffer[EVENT_BUFFER_SIZE];

    ssize_t bytes =
        read(ctx.runtime->fd,
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

        logger_error(ctx.logger,
                     "read failed errno=%d (%s)",
                     errno,
                     strerror(errno));

        return ERR_IO;
    }

    if (bytes == 0) {
        logger_error(ctx.logger,
                     "unexpected EOF on inotify fd");
        return ERR_IO;
    }

    logger_raw(ctx.logger,
               "read %zd bytes from inotify",
               bytes);

    char *ptr = buffer;
    char mask_str[256];

    while (ptr < buffer + bytes) {

        struct inotify_event *ev =
            (struct inotify_event *)ptr;

        const char *parent =
            watcher_get_path(&ctx.runtime->watchers, ev->wd);

        raw_event_name_from_mask(ev->mask, mask_str, sizeof(mask_str));
        logger_raw(ctx.logger,
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
                logger_error(ctx.logger,
                             "failed to build core raw event wd=%d",
                             ev->wd);
            }
        }

        int callback_status =
            on_event(raw_ptr, userdata);

        if (callback_status != ERR_OK)
            return callback_status;

        if (app->config.event_engine_mode == EVENT_ENGINE_SHADOW) {
#ifdef ALFRED_ENABLE_LEGACY_SHADOW
            legacy_events_dispatch(app, ev);
#else
            logger_error(ctx.logger,
                         "shadow event engine reached poll without "
                         "legacy shadow support");
            return ERR_CONFIG;
#endif
        }

        backend_handle_dir_create(&ctx, ev, on_event, userdata);

        ptr += sizeof(struct inotify_event) + ev->len;
    }

    return ERR_OK;
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

    char full[PATH_MAX];

    if (path_join(full, sizeof(full), base, ev->name) != 0)
        return;

    backend_emit_context_t context;

    context.ctx = ctx;
    context.on_event = on_event;
    context.userdata = userdata;

    watch_manager_add_recursive_with_discovery(
        ctx,
        full,
        backend_process_discovered_dir,
        &context);
}

/*
 * backend_process_discovered_dir - convert recursive discovery into raw input
 * @ctx: backend context used for discovery
 * @path: discovered directory path
 * @userdata: backend_emit_context_t supplied by backend_handle_dir_create()
 *
 * Discovery callbacks run while watch_manager.c walks the filesystem. This
 * helper adapts that backend-state notification into the same raw event
 * callback used for real inotify events.
 */
static void backend_process_discovered_dir(inotify_backend_context_t *ctx,
                                           const char *path,
                                           void *userdata)
{
    backend_emit_context_t *context =
        (backend_emit_context_t *)userdata;

    if (context == NULL)
        return;

    if (context->ctx == NULL)
        context->ctx = ctx;

    (void)backend_emit_synthetic_dir_create(context->ctx,
                                            path,
                                            context->on_event,
                                            context->userdata);
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
