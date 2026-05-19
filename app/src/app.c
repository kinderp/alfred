/* ============================================================================
 * app.c - application lifecycle and inotify event loop
 *
 * This file owns process-level initialization, signal handling, the current
 * inotify read loop, and shutdown ordering. It wires subsystems together but
 * should not contain long-term filesystem event semantics.
 *
 * TODO(core-integration): move backend-specific state and raw event reading
 * into modules/inotify once the backend interface is introduced.
 * ============================================================================
 */

#include "app.h"
#include "core_logger.h"
#include "errors.h"
#include "inotify_adapter.h"
#include "utils.h"
#include "watch_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/inotify.h>

/* ============================================================================
 * Configuration Constants
 * ========================================================================== */

#ifndef EVENT_BUFFER_SIZE
#define EVENT_BUFFER_SIZE 65536
#endif

/* ============================================================================
 * Local Declarations
 * ========================================================================== */

static void setup_signals(void);
static void handle_signal(int sig);
static void dispatch_event_to_core(app_t *app,
                                   const struct inotify_event *ev);

/*
 * TODO(core-integration): this function is implemented by the current inotify
 * semantic dispatcher. It should disappear from app.c when the backend emits
 * alfred_raw_event_t records into the core.
 */
extern void app_dispatch_raw_event(app_t *app,
                                   const struct inotify_event *ev);

/* ============================================================================
 * Signal State
 * ========================================================================== */

/*
 * Signal handlers cannot receive user data, so the application context is
 * exposed through a single file-local pointer. The handler only clears a flag;
 * all resource cleanup remains in normal process context.
 */
static app_t *g_app = NULL;

/* ============================================================================
 * Signal Handling
 * ========================================================================== */

/*
 * handle_signal - request cooperative shutdown from a signal handler
 * @sig: received signal number
 *
 * The handler intentionally avoids logging, allocation, locking, or closing
 * descriptors. It only clears app->running so the event loop can terminate
 * safely in normal execution context.
 */
static void handle_signal(int sig)
{
    (void)sig;

    if (g_app != NULL) {
        g_app->running = 0;
    }
}

/*
 * setup_signals - install process termination handlers
 *
 * SIGINT and SIGTERM are converted into cooperative shutdown requests. The
 * main event loop observes app->running and exits cleanly.
 */
static void setup_signals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;

    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

/* ============================================================================
 * Core Shadow Dispatch
 * ========================================================================== */

/*
 * dispatch_event_to_core - feed one inotify event to the core in shadow mode
 * @app: initialized application context
 * @ev: inotify event read from the kernel
 *
 * Converts the backend-specific event into alfred_raw_event_t and sends it to
 * the core. The legacy dispatcher still handles the official runtime behavior;
 * this path exists so both outputs can be compared during integration.
 */
static void dispatch_event_to_core(app_t *app,
                                   const struct inotify_event *ev)
{
    if (app == NULL || app->core == NULL || ev == NULL)
        return;

    const char *parent =
        watcher_get_path(&app->watchers, ev->wd);

    /*
     * Some inotify records, such as queue overflow, may not map to a watched
     * path. The legacy dispatcher still handles those cases for now.
     */
    if (parent == NULL)
        return;

    char full_path[PATH_MAX];
    alfred_raw_event_t raw;

    if (inotify_adapter_build_raw(ev,
                                  parent,
                                  full_path,
                                  sizeof(full_path),
                                  &raw) != 0) {
        logger_error(&app->logger,
                     "failed to build core raw event wd=%d",
                     ev->wd);
        return;
    }

    if (alfred_process(app->core, &raw) != 0) {
        logger_error(&app->logger,
                     "core failed to process raw event wd=%d",
                     ev->wd);
    }
}

/* ============================================================================
 * Application Lifecycle
 * ========================================================================== */

/*
 * app_init - initialize the application runtime
 * @app: application context to initialize
 * @argc: command-line argument count
 * @argv: command-line argument vector
 *
 * Initializes all runtime subsystems in dependency order: configuration,
 * logging, watcher state, move cache, inotify, signal handling, and startup
 * watches. A single failure path delegates cleanup to app_shutdown().
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int app_init(app_t *app, int argc, char **argv)
{
    error_t error = ERR_UNKNOWN;

    if (app == NULL) {
        error = ERR_INVALID_ARG;
        goto fail;
    }

    memset(app, 0, sizeof(*app));

    app->running    = 1;
    app->inotify_fd = -1;

    /*
     * Defaults are loaded before any subsystem initialization so later steps
     * can rely on configured paths, capacities, and watch masks.
     */
    config_defaults(&app->config);

    /*
     * The logger is initialized early because every following subsystem uses
     * it for diagnostics. Failures before this point must use stderr.
     */
    if (logger_init(&app->logger,
                    app->config.raw_log,
                    app->config.event_log,
                    app->config.error_log) != 0) {

        fprintf(stderr, "logger_init failed\n");
        error = ERR_IO;
        goto fail;
    }

    logger_info(&app->logger, "logger initialized");

    /*
     * The core is initialized after the logger because its emit callback writes
     * semantic events through logger_event(). For now the core runs in shadow
     * mode beside the legacy dispatcher.
     */
    alfred_config_default(&app->core_config);
    app->core = alfred_create(&app->core_config,
                              core_logger_on_event,
                              &app->logger);

    if (app->core == NULL) {
        logger_error(&app->logger,
                     "alfred core initialization failed");

        error = ERR_ALLOC;
        goto fail;
    }

    logger_info(&app->logger,
                "alfred core initialized in shadow mode");

    /*
     * The watcher table maps inotify watch descriptors back to paths. Event
     * dispatch depends on this mapping to reconstruct full file names.
     */
    if (watcher_init(&app->watchers,
                     app->config.watcher_capacity) != 0) {

        logger_error(&app->logger,
                     "watcher_init failed");

        error = ERR_ALLOC;
        goto fail;
    }

    logger_info(&app->logger,
                "watcher table initialized");

    /*
     * The current inotify dispatcher correlates MOVED_FROM and MOVED_TO with
     * this cache. This is temporary until move correlation is owned by core.
     */
    if (move_cache_init(&app->moves,
                        app->config.move_cache_size) != 0) {

        logger_error(&app->logger,
                     "move_cache_init failed");

        error = ERR_ALLOC;
        goto fail;
    }

    logger_info(&app->logger,
                "move cache initialized");

    /*
     * The descriptor is nonblocking so the event loop can poll cooperatively
     * and react to signal-driven shutdown without blocking indefinitely.
     */
    app->inotify_fd =
        inotify_init1(IN_NONBLOCK | IN_CLOEXEC);

    if (app->inotify_fd < 0) {

        logger_error(&app->logger,
                     "inotify_init1 failed errno=%d (%s)",
                     errno,
                     strerror(errno));

        error = ERR_INOTIFY;
        goto fail;
    }

    logger_info(&app->logger,
                "inotify initialized fd=%d",
                app->inotify_fd);

    /*
     * Signal handlers use g_app only to request shutdown. Cleanup is left to
     * app_shutdown(), which runs after app_run() returns or init fails.
     */
    g_app = app;
    setup_signals();

    logger_info(&app->logger,
                "signal handlers installed");

    /*
     * At least one startup path is required. Paths are watched recursively or
     * non-recursively according to the active configuration.
     */
    if (argc < 2) {
        logger_error(&app->logger,
                     "no startup paths provided");
        error = ERR_INVALID_ARG;
        goto fail;
    }

    for (int i = 1; i < argc; i++) {
        if (app->config.recursive) {
            watch_manager_add_recursive(app,
                                        argv[i]);
        }
        else {
            watch_manager_add(app,
                              argv[i]);
        }
    }

    logger_info(&app->logger,
                "application startup complete");

    return ERR_OK;

fail:
    app_shutdown(app);
    return error;
}

/*
 * app_run - execute the inotify event loop
 * @app: initialized application context
 *
 * Reads packed struct inotify_event records from the nonblocking descriptor,
 * logs their raw form, and forwards each record to the current dispatcher.
 *
 * Return: ERR_OK on normal termination, a negative error_t value on invalid
 * input.
 */
int app_run(app_t *app)
{
    if (app == NULL)
        return ERR_INVALID_ARG;

    char buffer[EVENT_BUFFER_SIZE];

    logger_info(&app->logger,
                "event loop started");

    char mask_str[256];
    while (app->running) {

        ssize_t bytes =
            read(app->inotify_fd,
                 buffer,
                 sizeof(buffer));

        /*
         * EAGAIN is expected for a nonblocking descriptor when no events are
         * queued. Sleep briefly to avoid a busy loop, then poll again.
         */
        if (bytes < 0) {

            if (errno == EAGAIN ||
                errno == EWOULDBLOCK) {

                usleep(10000); /* 10ms */
                continue;
            }

            if (errno == EINTR)
                continue;

            logger_error(&app->logger,
                         "read failed errno=%d (%s)",
                         errno,
                         strerror(errno));

            break;
        }

        /*
         * read() returning zero should not happen for an inotify descriptor.
         * Treat it as an abnormal condition and leave the loop.
         */
        if (bytes == 0) {
            logger_error(&app->logger,
                         "unexpected EOF on inotify fd");
            break;
        }

        logger_raw(&app->logger,
                   "read %zd bytes from inotify",
                   bytes);

        /*
         * inotify returns a byte stream containing one or more variable-sized
         * records. Each record is followed by ev->len bytes containing name.
         */
        char *ptr = buffer;

        while (ptr < buffer + bytes) {

            struct inotify_event *ev =
                (struct inotify_event *)ptr;

            raw_event_name_from_mask(ev->mask, mask_str, sizeof(mask_str));
            logger_raw(&app->logger,
                       "%s wd=%d path=%s name=%s",
                       mask_str,
                       ev->wd,
                       watcher_get_path(&app->watchers, ev->wd),
                       ev->len ? ev->name : ""
            );

            app_dispatch_raw_event(app, ev);
            dispatch_event_to_core(app, ev);

            ptr += sizeof(struct inotify_event)
                   + ev->len;
        }
    }

    logger_info(&app->logger,
                "event loop terminated");

    return ERR_OK;
}

/*
 * app_shutdown - release resources owned by app_t
 * @app: application context to release
 *
 * Releases resources in an order that keeps logging available until the end.
 * The function accepts NULL for convenience in failure paths.
 */
void app_shutdown(app_t *app)
{
    if (app == NULL)
        return;

    logger_info(&app->logger,
                "shutdown started");

    /*
     * Closing the descriptor first stops further kernel event delivery before
     * the watcher table backing those descriptors is destroyed.
     */
    if (app->inotify_fd >= 0) {

        close(app->inotify_fd);
        app->inotify_fd = -1;
    }

    /* The watcher table owns copied path strings for active descriptors. */
    watcher_destroy(&app->watchers);

    /* The move cache owns pending rename/move source names. */
    move_cache_destroy(&app->moves);

    /*
     * Destroy the core before closing the logger. Future flush behavior may
     * emit final semantic events during shutdown.
     */
    if (app->core != NULL) {
        alfred_destroy(app->core);
        app->core = NULL;
    }

    /*
     * The logger is closed last so shutdown activity can still be recorded.
     */
    logger_info(&app->logger,
                "resources released");

    logger_close(&app->logger);

    g_app = NULL;
}
