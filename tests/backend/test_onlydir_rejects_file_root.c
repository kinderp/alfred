/*
 * test_onlydir_rejects_file_root.c - focused test for IN_ONLYDIR hardening
 *
 * Alfred's inotify backend is directory-scoped: it watches configured roots
 * and recursive subdirectories, while file events are observed through the
 * parent directory watch. Installing one watch per file would consume kernel
 * watch descriptors and memory without improving the current event model.
 *
 * This test exercises watch_manager_add() directly with a regular file. The
 * production watch manager adds IN_ONLYDIR as an inotify_add_watch()
 * installation flag, so the kernel should reject the file with ENOTDIR and the
 * watcher table must remain empty.
 *
 * Expected log contract:
 *
 * events log:
 * - no WATCH_ADDED for the regular file
 *
 * errors log:
 * - inotify_add_watch failed ... errno=20 (Not a directory)
 *
 * Meaning:
 * IN_ONLYDIR is backend hardening, not an Alfred raw event and not a core
 * semantic event. It protects the directory-scoped watch model.
 */

#include "inotify_config.h"
#include "inotify_backend.h"
#include "logger.h"
#include "watch_manager.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

static int file_contains(const char *path, const char *needle)
{
    FILE *fp = fopen(path, "r");

    assert(fp != NULL);

    char line[512];

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, needle) != NULL) {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);
    return 0;
}

int main(void)
{
    const char *file_path = "/tmp/alfred_onlydir_regular_file.txt";
    const char *raw_log = "/tmp/alfred_onlydir_raw.log";
    const char *event_log = "/tmp/alfred_onlydir_events.log";
    const char *error_log = "/tmp/alfred_onlydir_errors.log";

    unlink(file_path);
    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    int file_fd = open(file_path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
    assert(file_fd >= 0);
    close(file_fd);

    inotify_backend_t runtime;
    memset(&runtime, 0, sizeof(runtime));
    runtime.fd = inotify_init1(IN_NONBLOCK);
    assert(runtime.fd >= 0);

    inotify_config_t config;
    inotify_config_defaults(&config);

    logger_t logger;
    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(watcher_init(&runtime.watchers, config.watcher_capacity) == 0);

    inotify_backend_context_t ctx = {
        .runtime = &runtime,
        .config = &config,
        .logger = &logger
    };

    assert(watch_manager_add(&ctx, file_path) == -1);
    assert(watcher_count(&runtime.watchers) == 0);
    assert(!watcher_has_path(&runtime.watchers, file_path));

    logger_close(&logger);
    watcher_destroy(&runtime.watchers);
    close(runtime.fd);

    assert(file_contains(error_log, "inotify_add_watch failed"));
    assert(file_contains(error_log, "errno=20"));
    assert(!file_contains(event_log, "WATCH_ADDED"));

    unlink(file_path);
    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    return 0;
}
