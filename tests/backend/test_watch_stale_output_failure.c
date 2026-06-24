/*
 * test_watch_stale_output_failure.c - fail-closed WATCH_STALE test
 *
 * Expected log contract:
 *
 * events.log:
 * - WATCH_STALE wd=42 path=/tmp/alfred-watch-stale-output-failure
 *   reason=IN_MOVE_SELF
 *
 * errors.log:
 * - failed to emit watch stale output record
 *
 * Meaning:
 * WATCH_STALE is backend diagnostic state: it says a watch descriptor still
 * exists, but Alfred no longer trusts the saved textual path. The backend
 * writes the compatibility events.log line first, then offers the same borrowed
 * alfred_record_t to the optional structured output callback. If that callback
 * fails, the helper must return an error so the poll path can fail closed when
 * output_enabled=true. The backend still does not know JSONL, files, queues, or
 * application policy; it only propagates the callback failure across the
 * backend/app boundary.
 */

#include "inotify_backend.h"
#include "logger.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

typedef struct failing_watch_stale_emit_context {
    unsigned calls;
    int fail;
    const char *expected_path;
} failing_watch_stale_emit_context_t;

/*
 * fail_watch_stale_emit_record - reject only WATCH_STALE records
 * @record: borrowed diagnostic record produced by the backend helper
 * @userdata: failing_watch_stale_emit_context_t used to count calls
 *
 * The test callback models an application output pipeline failure at the exact
 * boundary under review. It asserts that no unrelated diagnostic type reaches
 * this path, increments the call counter, and optionally returns failure.
 *
 * Return: -1 when @userdata requests failure, 0 otherwise.
 */
static int collect_or_fail_watch_stale_emit_record(const alfred_record_t *record,
                                                   void *userdata)
{
    failing_watch_stale_emit_context_t *ctx = userdata;

    assert(record != NULL);
    assert(ctx != NULL);
    assert(record->type == ALFRED_RECORD_TYPE_WATCH_STALE);
    assert(record->category == ALFRED_RECORD_CATEGORY_WATCH);
    assert(record->path != NULL);
    assert(ctx->expected_path != NULL);
    assert(strcmp(record->path, ctx->expected_path) == 0);
    assert(record->watch.reason != NULL);
    assert(strcmp(record->watch.reason, "IN_MOVE_SELF") == 0);

    ctx->calls++;
    return ctx->fail ? -1 : 0;
}

/*
 * fill_long_path - build a path that cannot fit the backend text sink buffer
 * @dst: destination buffer
 * @dst_size: size of @dst in bytes
 *
 * WATCH_STALE uses backend_log_watch_diagnostic_record(). The long path forces
 * that helper through its legacy logger fallback while the already-built
 * structured record must still cross emit_record.
 */
static void fill_long_path(char *dst, size_t dst_size)
{
    assert(dst != NULL);
    assert(dst_size > 8u);

    strcpy(dst, "/tmp/");
    memset(dst + 5, 's', dst_size - 7u);
    dst[dst_size - 2u] = 'z';
    dst[dst_size - 1u] = '\0';
}

/*
 * file_contains - check whether a log file contains one expected fragment
 * @path: filesystem path to the log file
 * @needle: substring that must appear in at least one line
 *
 * Return: nonzero when @needle is found.
 */
static int file_contains(const char *path, const char *needle)
{
    FILE *fp = fopen(path, "r");
    char line[512];

    assert(fp != NULL);

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
    const char *raw_log = "/tmp/alfred_watch_stale_failure_raw.log";
    const char *event_log = "/tmp/alfred_watch_stale_failure_events.log";
    const char *error_log = "/tmp/alfred_watch_stale_failure_errors.log";
    const char *stale_path = "/tmp/alfred-watch-stale-output-failure";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    logger_t logger;
    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    failing_watch_stale_emit_context_t emit_ctx;
    memset(&emit_ctx, 0, sizeof(emit_ctx));
    emit_ctx.fail = 1;
    emit_ctx.expected_path = stale_path;

    inotify_backend_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.logger = &logger;
    ctx.emit_record = collect_or_fail_watch_stale_emit_record;
    ctx.emit_record_userdata = &emit_ctx;

    assert(backend_log_watch_stale(&ctx,
                                   42,
                                   stale_path,
                                   "IN_MOVE_SELF") == -1);
    assert(emit_ctx.calls == 1u);

    char long_path[BACKEND_RECORD_TEXT_BUFFER_SIZE + 128u];

    fill_long_path(long_path, sizeof(long_path));
    emit_ctx.fail = 0;
    emit_ctx.expected_path = long_path;

    assert(backend_log_watch_stale(&ctx,
                                   43,
                                   long_path,
                                   "IN_MOVE_SELF") == 0);
    assert(emit_ctx.calls == 2u);

    emit_ctx.fail = 1;

    assert(backend_log_watch_stale(&ctx,
                                   44,
                                   long_path,
                                   "IN_MOVE_SELF") == -1);
    assert(emit_ctx.calls == 3u);

    logger_close(&logger);

    assert(file_contains(event_log, "WATCH_STALE wd=42"));
    assert(file_contains(event_log, "WATCH_STALE wd=43"));
    assert(file_contains(event_log, "WATCH_STALE wd=44"));
    assert(file_contains(event_log, stale_path));
    assert(file_contains(event_log, "reason=IN_MOVE_SELF"));
    assert(file_contains(error_log,
                         "failed to emit watch stale output record"));

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    return 0;
}
