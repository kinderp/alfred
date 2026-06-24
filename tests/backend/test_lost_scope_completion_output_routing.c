/*
 * test_lost_scope_completion_output_routing.c - WATCH_LOST completion output
 *
 * Expected compatibility log contract:
 *
 * events.log:
 * - WATCH_LOST_REINSTALLED ... installed_path=/tmp/alfred-lost-new/missing
 * - WATCH_LOST_REINSTALL_FAILED ... missing_path=/tmp/alfred-lost-new/missing
 * - WATCH_LOST_ROLLBACK ... removed_wd=101
 * - WATCH_LOST_RETRY_SCHEDULED ... result=not-found retry=2 delay_ms=500
 * - WATCH_LOST_RECOVERY_GAVE_UP ... result=not-found retries=8
 * - WATCH_LOST_RECOVERY_END ... result=valid watches=4
 *
 * errors.log:
 * - failed to emit watch lost queue output record
 *
 * Meaning:
 * These WATCH_LOST_* records describe stateful completion decisions in delayed
 * lost-scope recovery: installing missing watches, rolling back partial repair,
 * scheduling retry, giving up, or declaring the recovered subtree valid. They
 * are still backend recovery diagnostics, not semantic filesystem events. The
 * backend writes the compatibility text line first, then offers the same
 * borrowed alfred_record_t to emit_record.
 */

#include "inotify_backend.h"
#include "logger.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

typedef struct lost_completion_emit_context {
    unsigned calls;
    alfred_record_type_t fail_type;
} lost_completion_emit_context_t;

/*
 * collect_or_fail_lost_completion_record - inspect routed completion records
 * @record: borrowed recovery record produced by the backend helper
 * @userdata: lost_completion_emit_context_t controlling optional failure
 *
 * The callback verifies the fields that distinguish reinstall, rollback,
 * retry, gave-up, and recovery-end diagnostics in JSONL/future sinks.
 *
 * Return: 0 on accepted record, -1 on the configured failure type.
 */
static int collect_or_fail_lost_completion_record(
    const alfred_record_t *record,
    void *userdata
)
{
    lost_completion_emit_context_t *ctx = userdata;

    assert(record != NULL);
    assert(ctx != NULL);
    assert(record->category == ALFRED_RECORD_CATEGORY_RECOVERY);
    assert(record->backend != NULL);
    assert(strcmp(record->backend, "inotify") == 0);
    assert(record->path != NULL);
    assert(strcmp(record->path, "/tmp/alfred-lost-new") == 0);

    switch (record->type) {
    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED:
        assert(record->recovery.detail_path != NULL);
        assert(strcmp(record->recovery.detail_path,
                      "/tmp/alfred-lost-new/missing") == 0);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED:
        assert(record->recovery.detail_path != NULL);
        assert(strcmp(record->recovery.detail_path,
                      "/tmp/alfred-lost-new/missing") == 0);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK:
        assert(record->recovery.related_watch_id == 101);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED:
        assert(record->watch.state != NULL);
        assert(strcmp(record->watch.state, "not-found") == 0);
        assert(record->watch.retry_count == 2u);
        assert(record->recovery.delay_ms == 500u);
        assert(record->recovery.pending_count == 3u);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP:
        assert(record->watch.state != NULL);
        assert(strcmp(record->watch.state, "not-found") == 0);
        assert(record->watch.retry_count == 8u);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END:
        assert(record->watch.state != NULL);
        assert(strcmp(record->watch.state, "valid") == 0);
        assert(record->recovery.watches_count == 4u);
        break;

    default:
        assert(!"unexpected lost-scope completion record type");
    }

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

static void emit_expected_completion_records(inotify_backend_context_t *ctx)
{
    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               NULL,
               "/tmp/alfred-lost-new/missing",
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               NULL,
               "/tmp/alfred-lost-new/missing",
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               NULL,
               NULL,
               101,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               "not-found",
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               3,
               0,
               0,
               2,
               500) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               "not-found",
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               8,
               0) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               "valid",
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               0,
               0,
               4,
               0,
               0) == 0);
}

int main(void)
{
    const char *raw_log = "/tmp/alfred_lost_completion_output_raw.log";
    const char *event_log = "/tmp/alfred_lost_completion_output_events.log";
    const char *error_log = "/tmp/alfred_lost_completion_output_errors.log";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    logger_t logger;
    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    lost_completion_emit_context_t emit_ctx;
    memset(&emit_ctx, 0, sizeof(emit_ctx));
    emit_ctx.fail_type = ALFRED_RECORD_TYPE_UNKNOWN;

    inotify_backend_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.logger = &logger;
    ctx.emit_record = collect_or_fail_lost_completion_record;
    ctx.emit_record_userdata = &emit_ctx;

    emit_expected_completion_records(&ctx);
    assert(emit_ctx.calls == 6u);

    emit_ctx.fail_type = ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END;
    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               "valid",
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               0,
               0,
               4,
               0,
               0) == -1);
    assert(emit_ctx.calls == 7u);

    logger_close(&logger);

    assert(file_contains(event_log, "WATCH_LOST_REINSTALLED wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_REINSTALL_FAILED wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_ROLLBACK wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_RETRY_SCHEDULED wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_RECOVERY_GAVE_UP wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_RECOVERY_END wd=77"));
    assert(file_contains(error_log,
                         "failed to emit watch lost queue output record"));

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    return 0;
}
