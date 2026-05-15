/* ============================================================================
 * app.c
 * Production-grade lifecycle manager for filesystem monitor
 * ============================================================================
 */

#include "app.h"
#include "errors.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/inotify.h>

/* ============================================================================
 * CONFIG
 * ========================================================================== */

#ifndef EVENT_BUFFER_SIZE
#define EVENT_BUFFER_SIZE 65536
#endif

/* ============================================================================
 * FORWARD DECLARATIONS
 * ========================================================================== */

static void setup_signals(void);
static void handle_signal(int sig);
static int add_initial_paths(app_t *app, int argc, char **argv);

/* Questo verrà implementato nel modulo event_engine.c */
extern void app_dispatch_raw_event(app_t *app,
                                   const struct inotify_event *ev);

/* ============================================================================
 * GLOBAL CONTROLLED POINTER (used only by signal handler)
 * ========================================================================== */

static app_t *g_app = NULL;

/* ============================================================================
 * SIGNAL HANDLING
 * ========================================================================== */

static void handle_signal(int sig)
{
    (void)sig;

    if (g_app != NULL) {
        g_app->running = 0;
    }
}

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
 * INTERNAL: ADD STARTUP WATCH PATHS
 * ========================================================================== */

static int add_initial_paths(app_t *app, int argc, char **argv)
{
    uint32_t mask =
        IN_CREATE      |
        IN_DELETE      |
        IN_MOVED_FROM  |
        IN_MOVED_TO    |
        IN_DELETE_SELF |
        IN_IGNORED     |
        IN_Q_OVERFLOW;

    for (int i = 1; i < argc; i++) {

        int wd = inotify_add_watch(app->inotify_fd,
                                   argv[i],
                                   mask);

        if (wd < 0) {
            logger_error(&app->logger,
                         "cannot watch path=%s errno=%d (%s)",
                         argv[i],
                         errno,
                         strerror(errno));
            continue;
        }

        if (watcher_store(&app->watchers,
                          wd,
                          argv[i]) != 0) {

            logger_error(&app->logger,
                         "watcher_store failed path=%s wd=%d",
                         argv[i],
                         wd);

            inotify_rm_watch(app->inotify_fd, wd);
            continue;
        }

        logger_info(&app->logger,
                    "watch added wd=%d path=%s",
                    wd,
                    argv[i]);
    }

    return 0;
}

/* ============================================================================
 * PUBLIC: APP INIT
 * ========================================================================== */

int app_init(app_t *app, int argc, char **argv)
{
    if (app == NULL)
        return ERR_INVALID_ARG;

    memset(app, 0, sizeof(*app));

    app->running    = 1;
    app->inotify_fd = -1;

    /* ------------------------------------------------------------
     * Load default config
     * ---------------------------------------------------------- */
    config_defaults(&app->config);

    /* ------------------------------------------------------------
     * Logger init
     * ---------------------------------------------------------- */
    if (logger_init(&app->logger,
                    app->config.raw_log,
                    app->config.event_log,
                    app->config.error_log) != 0) {

        fprintf(stderr, "logger_init failed\n");
        return ERR_IO;
    }

    logger_info(&app->logger, "logger initialized");

    /* ------------------------------------------------------------
     * Watcher table init
     * ---------------------------------------------------------- */
    if (watcher_init(&app->watchers,
                     app->config.watcher_capacity) != 0) {

        logger_error(&app->logger,
                     "watcher_init failed");

        return ERR_ALLOC;
    }

    logger_info(&app->logger,
                "watcher table initialized");

    /* ------------------------------------------------------------
     * Move cache init
     * ---------------------------------------------------------- */
    if (move_cache_init(&app->moves,
                        app->config.move_cache_size) != 0) {

        logger_error(&app->logger,
                     "move_cache_init failed");

        return ERR_ALLOC;
    }

    logger_info(&app->logger,
                "move cache initialized");

    /* ------------------------------------------------------------
     * Inotify init
     * ---------------------------------------------------------- */
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
                "inotify initialized fd=%d",
                app->inotify_fd);

    /* ------------------------------------------------------------
     * Signal handling
     * ---------------------------------------------------------- */
    g_app = app;
    setup_signals();

    logger_info(&app->logger,
                "signal handlers installed");

    /* ------------------------------------------------------------
     * CLI paths
     * ---------------------------------------------------------- */
    if (argc < 2) {
        logger_error(&app->logger,
                     "no startup paths provided");
        return ERR_INVALID_ARG;
    }

    add_initial_paths(app, argc, argv);

    logger_info(&app->logger,
                "application startup complete");

    return ERR_OK;
}

/* ============================================================================
 * PUBLIC: MAIN LOOP
 * ========================================================================== */

int app_run(app_t *app)
{
    if (app == NULL)
        return ERR_INVALID_ARG;

    char buffer[EVENT_BUFFER_SIZE];

    logger_info(&app->logger,
                "event loop started");

    while (app->running) {

        ssize_t bytes =
            read(app->inotify_fd,
                 buffer,
                 sizeof(buffer));

        /* --------------------------------------------------------
         * Non blocking: no data available
         * ------------------------------------------------------ */
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

        /* --------------------------------------------------------
         * EOF impossible here but checked
         * ------------------------------------------------------ */
        if (bytes == 0) {
            logger_error(&app->logger,
                         "unexpected EOF on inotify fd");
            break;
        }

        logger_raw(&app->logger,
                   "read %zd bytes from inotify",
                   bytes);

        /* --------------------------------------------------------
         * Iterate packed events
         * ------------------------------------------------------ */
        char *ptr = buffer;

        while (ptr < buffer + bytes) {

            struct inotify_event *ev =
                (struct inotify_event *)ptr;

            app_dispatch_raw_event(app, ev);

            ptr += sizeof(struct inotify_event)
                   + ev->len;
        }
    }

    logger_info(&app->logger,
                "event loop terminated");

    return ERR_OK;
}

/* ============================================================================
 * PUBLIC: SHUTDOWN
 * ========================================================================== */

void app_shutdown(app_t *app)
{
    if (app == NULL)
        return;

    logger_info(&app->logger,
                "shutdown started");

    /* ------------------------------------------------------------
     * Close inotify fd
     * ---------------------------------------------------------- */
    if (app->inotify_fd >= 0) {

        close(app->inotify_fd);
        app->inotify_fd = -1;
    }

    /* ------------------------------------------------------------
     * Free watcher table
     * ---------------------------------------------------------- */
    watcher_destroy(&app->watchers);

    /* ------------------------------------------------------------
     * Free move cache
     * ---------------------------------------------------------- */
    move_cache_destroy(&app->moves);

    /* ------------------------------------------------------------
     * Close logger last
     * ---------------------------------------------------------- */
    logger_info(&app->logger,
                "resources released");

    logger_close(&app->logger);

    g_app = NULL;
}
