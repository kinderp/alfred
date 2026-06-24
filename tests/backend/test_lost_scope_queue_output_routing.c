/*
 * test_lost_scope_queue_output_routing.c - WATCH_LOST queue output boundary
 *
 * Expected compatibility log contract:
 *
 * events.log:
 * - WATCH_LOST_QUEUED wd=77 path=/tmp/alfred-lost-queue-output-routing
 *   reason=IN_MOVE_SELF error=path-unreachable pending=3
 *
 * errors.log:
 * - WATCH_LOST_QUEUE_FAILED wd=77
 *   path=/tmp/alfred-lost-queue-output-routing reason=IN_MOVE_SELF
 *   error=path-unreachable
 * - failed to emit watch lost queue output record
 *
 * Meaning:
 * Queue-level WATCH_LOST_* diagnostics describe the handoff from immediate
 * local resync to delayed lost-scope recovery. They are recovery diagnostics,
 * not semantic filesystem events. The backend writes the compatibility text
 * line first, then offers the same borrowed alfred_record_t to emit_record.
 * If the output callback fails, the helper returns an error so the caller can
 * fail closed when structured output is enabled.
 */

#include "inotify_backend.h"
#include "logger.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

typedef struct lost_queue_emit_context {
    unsigned calls;
    alfred_record_type_t fail_type;
} lost_queue_emit_context_t;

/*
 * collect_or_fail_lost_queue_record - inspect routed WATCH_LOST queue records
 * @record: borrowed recovery record produced by the backend helper
 * @userdata: lost_queue_emit_context_t controlling optional failure
 *
 * The callback models the application output boundary. It verifies that only
 * queue-level WATCH_LOST_* records reach this focused test, checks the key
 * fields used by JSONL consumers, counts the call, and optionally rejects one
 * configured type.
 *
 * Return: 0 on accepted record, -1 on the configured failure type.
 */
static int collect_or_fail_lost_queue_record(const alfred_record_t *record,
                                             void *userdata)
{
    lost_queue_emit_context_t *ctx = userdata;

    assert(record != NULL);
    assert(ctx != NULL);
    assert(record->category == ALFRED_RECORD_CATEGORY_RECOVERY);
    assert(record->backend != NULL);
    assert(strcmp(record->backend, "inotify") == 0);
    assert(record->path != NULL);
    assert(strcmp(record->path,
                  "/tmp/alfred-lost-queue-output-routing") == 0);
    assert(record->watch.watch_id == 77);
    assert(record->watch.reason != NULL);
    assert(strcmp(record->watch.reason, "IN_MOVE_SELF") == 0);
    assert(record->watch.error != NULL);
    assert(strcmp(record->watch.error, "path-unreachable") == 0);

    assert(record->type == ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED ||
           record->type == ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED);

    if (record->type == ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED)
        assert(record->recovery.pending_count == 3u);

    ctx->calls++;

    if (record->type == ctx->fail_type)
        return -1;

    return 0;
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
    const char *raw_log = "/tmp/alfred_lost_queue_output_raw.log";
    const char *event_log = "/tmp/alfred_lost_queue_output_events.log";
    const char *error_log = "/tmp/alfred_lost_queue_output_errors.log";
    const char *path = "/tmp/alfred-lost-queue-output-routing";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    logger_t logger;
    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    lost_queue_emit_context_t emit_ctx;
    memset(&emit_ctx, 0, sizeof(emit_ctx));
    emit_ctx.fail_type = ALFRED_RECORD_TYPE_UNKNOWN;

    inotify_backend_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.logger = &logger;
    ctx.emit_record = collect_or_fail_lost_queue_record;
    ctx.emit_record_userdata = &emit_ctx;

    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED,
               77,
               path,
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               "path-unreachable",
               NULL,
               0,
               0,
               0,
               0,
               3,
               0,
               0,
               0,
               0) == 0);
    assert(emit_ctx.calls == 1u);

    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED,
               77,
               path,
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               "path-unreachable",
               NULL,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0) == 0);
    assert(emit_ctx.calls == 2u);

    emit_ctx.fail_type = ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED;
    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED,
               77,
               path,
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               "path-unreachable",
               NULL,
               0,
               0,
               0,
               0,
               3,
               0,
               0,
               0,
               0) == -1);
    assert(emit_ctx.calls == 3u);

    logger_close(&logger);

    assert(file_contains(event_log, "WATCH_LOST_QUEUED wd=77"));
    assert(file_contains(event_log, "pending=3"));
    assert(file_contains(error_log, "WATCH_LOST_QUEUE_FAILED wd=77"));
    assert(file_contains(error_log,
                         "failed to emit watch lost queue output record"));

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    return 0;
}
