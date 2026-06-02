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

/* ============================================================================
 * Local Types
 * ========================================================================== */

typedef struct backend_emit_context {
    inotify_backend_context_t *ctx;
    inotify_backend_event_fn on_event;
    void *userdata;
    int failed;
} backend_emit_context_t;

typedef enum backend_resync_probe_result {
    BACKEND_RESYNC_PROBE_MISSING_WATCH,
    BACKEND_RESYNC_PROBE_NOT_STALE,
    BACKEND_RESYNC_PROBE_SET_RESYNCING_FAILED,
    BACKEND_RESYNC_PROBE_PATH_UNREACHABLE,
    BACKEND_RESYNC_PROBE_NOT_DIRECTORY,
    BACKEND_RESYNC_PROBE_SET_STALE_FAILED,
    BACKEND_RESYNC_PROBE_SET_VALID_FAILED,
    BACKEND_RESYNC_PROBE_MISSING_IDENTITY,
    BACKEND_RESYNC_PROBE_IDENTITY_MISMATCH
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

static void backend_shutdown(inotify_backend_context_t *ctx);

static int backend_process_scanned_dir_create(const fs_scan_entry_t *entry,
                                              void *userdata);

static void backend_handle_ignored(inotify_backend_context_t *ctx,
                                   const struct inotify_event *ev);

static void backend_handle_move_self(inotify_backend_context_t *ctx,
                                     const struct inotify_event *ev);

static void backend_handle_delete_self(inotify_backend_context_t *ctx,
                                       const struct inotify_event *ev);

static void backend_resync_watch(inotify_backend_context_t *ctx,
                                 int wd,
                                 const char *reason);

static const char *backend_resync_probe_result_name(
    backend_resync_probe_result_t result
);

static void backend_log_resync_failure(inotify_backend_context_t *ctx,
                                       int wd,
                                       const char *path,
                                       const char *reason,
                                       backend_resync_probe_result_t result,
                                       int saved_errno);

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

    ctx->runtime->fd =
        inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (ctx->runtime->fd < 0) {
        logger_error(ctx->logger,
                     "inotify_init1 failed errno=%d (%s)",
                     errno,
                     strerror(errno));

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

    return ERR_OK;
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

        if (parent != NULL) {
            if (inotify_adapter_build_raw(ev,
                                          parent,
                                          full_path,
                                          sizeof(full_path),
                                          &raw) == 0) {
                raw_ptr = &raw;
            }
            else {
                logger_error(ctx->logger,
                             "failed to build core raw event wd=%d",
                             ev->wd);
            }
        }

        int callback_status =
            on_event(raw_ptr, userdata);

        if (callback_status != ERR_OK)
            return callback_status;

        backend_handle_move_self(ctx, ev);

        backend_handle_delete_self(ctx, ev);

        backend_handle_ignored(ctx, ev);

        backend_handle_dir_create(ctx, ev, on_event, userdata);

        ptr += sizeof(struct inotify_event) + ev->len;
    }

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

    logger_event(ctx->logger,
                 "WATCH_STALE wd=%d path=%s reason=IN_MOVE_SELF",
                 ev->wd,
                 path ? path : "");

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

    logger_event(ctx->logger,
                 "WATCH_STALE wd=%d path=%s reason=IN_DELETE_SELF",
                 ev->wd,
                 path ? path : "");
}

/*
 * backend_resync_watch - run the first conservative resync probe for one watch
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
 */
static void backend_resync_watch(inotify_backend_context_t *ctx,
                                 int wd,
                                 const char *reason)
{
    if (ctx == NULL || reason == NULL)
        return;

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
        return;
    }

    if (watcher_get_state(&ctx->runtime->watchers, wd) !=
        WATCHER_STATE_STALE) {

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   BACKEND_RESYNC_PROBE_NOT_STALE,
                                   0);
        return;
    }

    logger_event(ctx->logger,
                 "WATCH_RESYNC_BEGIN wd=%d path=%s reason=%s",
                 wd,
                 path,
                 reason);

    if (watcher_set_state(&ctx->runtime->watchers,
                          wd,
                          WATCHER_STATE_RESYNCING) != 0) {

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   BACKEND_RESYNC_PROBE_SET_RESYNCING_FAILED,
                                   0);
        return;
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
            return;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   saved_errno);
        return;
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
            return;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   0);
        return;
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
            return;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   0);
        return;
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
            return;
        }

        backend_log_resync_failure(ctx,
                                   wd,
                                   path,
                                   reason,
                                   result,
                                   0);
        return;
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
        return;
    }

    logger_event(ctx->logger,
                 "WATCH_RESYNC_END wd=%d path=%s reason=%s result=valid",
                 wd,
                 path,
                 reason);
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
    }

    return "unknown";
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
 * need operating-system context; logical failures use only an error token.
 */
static void backend_log_resync_failure(inotify_backend_context_t *ctx,
                                       int wd,
                                       const char *path,
                                       const char *reason,
                                       backend_resync_probe_result_t result,
                                       int saved_errno)
{
    if (saved_errno != 0) {
        logger_event(ctx->logger,
                     "WATCH_RESYNC_FAILED wd=%d path=%s reason=%s error=%s errno=%d (%s)",
                     wd,
                     path,
                     reason,
                     backend_resync_probe_result_name(result),
                     saved_errno,
                     strerror(saved_errno));
        return;
    }

    logger_event(ctx->logger,
                 "WATCH_RESYNC_FAILED wd=%d path=%s reason=%s error=%s",
                 wd,
                 path,
                 reason,
                 backend_resync_probe_result_name(result));
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
