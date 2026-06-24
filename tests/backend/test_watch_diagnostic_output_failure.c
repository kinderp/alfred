/*
 * test_watch_diagnostic_output_failure.c - fail-closed watch diagnostic test
 *
 * This test exercises the backend diagnostic output boundary directly:
 *
 * watch_manager_add()
 * -> WATCH_ADDED compatibility events.log record
 * -> ctx->emit_record(WATCH_ADDED)
 * -> simulated structured output failure
 * -> rollback of the just-installed watch
 *
 * Expected contract:
 * - watch_manager_add() returns -1 when routed WATCH_ADDED output fails
 * - the watch table does not retain the failed watch
 * - the compatibility log can still contain WATCH_ADDED because events.log was
 *   written before the structured output callback failed
 * - the helper still calls emit_record when a long diagnostic path forces the
 *   compatibility text sink through the legacy logger fallback
 * - errors.log records the structured diagnostic output failure
 *
 * Meaning:
 * The inotify backend does not decide process policy and does not know JSONL.
 * It only propagates callback failure. The application layer decides that
 * output_enabled=true is fail-closed. This prevents Alfred from silently
 * continuing with an incomplete structured ledger.
 */

#include "inotify_config.h"
#include "inotify_backend.h"
#include "logger.h"
#include "watch_manager.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Include the implementation only for this focused boundary test. The public
 * watch_manager_add() path cannot create a filesystem path longer than PATH_MAX,
 * while the regression needs a synthetic path that exceeds the helper's fixed
 * compatibility text buffer. Including the C file lets the test call the static
 * helper directly and lock down the fallback contract without changing the
 * production API.
 */
#include "../../modules/inotify/src/watch_manager.c"

typedef struct {
    unsigned calls;
    int fail;
    alfred_record_type_t expected_type;
    const char *expected_path;
} failing_emit_context_t;

static int collect_or_fail_emit_record(const alfred_record_t *record,
                                       void *userdata)
{
    failing_emit_context_t *ctx = userdata;

    assert(record != NULL);
    assert(ctx != NULL);
    assert(record->type == ctx->expected_type);

    if (ctx->expected_path != NULL) {
        assert(record->path != NULL);
        assert(strcmp(record->path, ctx->expected_path) == 0);
    }

    ctx->calls++;
    return ctx->fail ? -1 : 0;
}

static void fill_long_path(char *dst, size_t dst_size)
{
    assert(dst != NULL);
    assert(dst_size > 8u);

    strcpy(dst, "/tmp/");
    memset(dst + 5, 'w', dst_size - 7u);
    dst[dst_size - 2u] = 'z';
    dst[dst_size - 1u] = '\0';
}

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
    const char *dir_path = "/tmp/alfred_watch_output_failure_dir";
    const char *raw_log = "/tmp/alfred_watch_output_failure_raw.log";
    const char *event_log = "/tmp/alfred_watch_output_failure_events.log";
    const char *error_log = "/tmp/alfred_watch_output_failure_errors.log";

    rmdir(dir_path);
    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    assert(mkdir(dir_path, 0700) == 0);

    inotify_backend_t runtime;
    memset(&runtime, 0, sizeof(runtime));
    runtime.fd = inotify_init1(IN_NONBLOCK);
    assert(runtime.fd >= 0);

    inotify_config_t config;
    inotify_config_defaults(&config);

    logger_t logger;
    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(watcher_init(&runtime.watchers, config.watcher_capacity) == 0);

    failing_emit_context_t emit_ctx;
    memset(&emit_ctx, 0, sizeof(emit_ctx));
    emit_ctx.fail = 1;
    emit_ctx.expected_type = ALFRED_RECORD_TYPE_WATCH_ADDED;
    emit_ctx.expected_path = dir_path;

    inotify_backend_context_t ctx = {
        .runtime = &runtime,
        .config = &config,
        .logger = &logger,
        .emit_record = collect_or_fail_emit_record,
        .emit_record_userdata = &emit_ctx
    };

    assert(watch_manager_add(&ctx, dir_path) == -1);
    assert(emit_ctx.calls == 1u);
    assert(watcher_count(&runtime.watchers) == 0u);
    assert(!watcher_has_path(&runtime.watchers, dir_path));

    char long_path[PATH_MAX + 512u];

    fill_long_path(long_path, sizeof(long_path));
    emit_ctx.fail = 0;
    emit_ctx.expected_type = ALFRED_RECORD_TYPE_WATCH_ADDED;
    emit_ctx.expected_path = long_path;

    assert(watch_manager_log_simple_watch_diagnostic(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_ADDED,
               77,
               long_path) == 0);
    assert(emit_ctx.calls == 2u);

    emit_ctx.fail = 1;

    assert(watch_manager_log_simple_watch_diagnostic(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_ADDED,
               78,
               long_path) == -1);
    assert(emit_ctx.calls == 3u);

    logger_close(&logger);
    watcher_destroy(&runtime.watchers);
    close(runtime.fd);

    assert(file_contains(event_log, "WATCH_ADDED"));
    assert(file_contains(event_log, "WATCH_ADDED wd=77"));
    assert(file_contains(event_log, "WATCH_ADDED wd=78"));
    assert(file_contains(error_log,
                         "failed to emit watch diagnostic output record"));

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(dir_path);

    return 0;
}
