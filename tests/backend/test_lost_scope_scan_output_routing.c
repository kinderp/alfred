/*
 * test_lost_scope_scan_output_routing.c - WATCH_LOST scan output boundary
 *
 * Expected compatibility log contract:
 *
 * events.log:
 * - WATCH_LOST_SCAN_BEGIN root=/tmp/alfred-lost-scan-output-root pending=2
 * - WATCH_LOST_FOUND wd=77 old_path=/tmp/alfred-lost-old
 *   new_path=/tmp/alfred-lost-new reason=IN_MOVE_SELF
 * - WATCH_LOST_PREFIX_UPDATED wd=77 old_prefix=/tmp/alfred-lost-old
 *   new_prefix=/tmp/alfred-lost-new children=4
 * - WATCH_LOST_COVERAGE_DONE wd=77 path=/tmp/alfred-lost-new
 *   reason=IN_MOVE_SELF dirs=5 watched=3 missing=2
 * - WATCH_LOST_COVERAGE_MISSING wd=77 path=/tmp/alfred-lost-new
 *   reason=IN_MOVE_SELF missing_path=/tmp/alfred-lost-new/missing
 * - WATCH_LOST_COVERAGE_CLASS wd=77 path=/tmp/alfred-lost-new
 *   reason=IN_MOVE_SELF result=needs-reinstall
 * - WATCH_LOST_NOT_FOUND wd=77 path=/tmp/alfred-lost-old
 *   reason=IN_MOVE_SELF retry=1
 * - WATCH_LOST_RECOVERY_FAILED wd=77 path=/tmp/alfred-lost-old
 *   reason=IN_MOVE_SELF error=scan-failed
 *
 * errors.log:
 * - failed to emit watch lost queue output record
 *
 * Meaning:
 * These WATCH_LOST_* records describe the search/classification phase of
 * delayed lost-scope recovery. They are recovery diagnostics, not semantic
 * filesystem events. The backend writes the compatibility text line first,
 * then offers the same borrowed alfred_record_t to emit_record. Reinstall,
 * rollback, retry, gave-up, and recovery-end records are intentionally not
 * covered by this micro-step.
 */

#include "inotify_backend.h"
#include "logger.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

typedef struct lost_scan_emit_context {
    unsigned calls;
    alfred_record_type_t fail_type;
    const char *expected_coverage_path;
    int expected_watch_id;
} lost_scan_emit_context_t;

/*
 * collect_or_fail_lost_scan_record - inspect routed WATCH_LOST scan records
 * @record: borrowed recovery record produced by the backend helper
 * @userdata: lost_scan_emit_context_t controlling optional failure
 *
 * The callback models the application output boundary for the scan phase.
 * It checks the fields that JSONL consumers need to reconstruct search,
 * prefix-repair, coverage, not-found, and failure diagnostics.
 *
 * Return: 0 on accepted record, -1 on the configured failure type.
 */
static int collect_or_fail_lost_scan_record(const alfred_record_t *record,
                                            void *userdata)
{
    lost_scan_emit_context_t *ctx = userdata;

    assert(record != NULL);
    assert(ctx != NULL);
    assert(record->category == ALFRED_RECORD_CATEGORY_RECOVERY);
    assert(record->backend != NULL);
    assert(strcmp(record->backend, "inotify") == 0);

    switch (record->type) {
    case ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN:
        assert(record->path != NULL);
        assert(strcmp(record->path,
                      "/tmp/alfred-lost-scan-output-root") == 0);
        assert(record->recovery.pending_count == 2u);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_FOUND:
        assert(record->watch.watch_id == ctx->expected_watch_id);
        assert(record->old_path != NULL);
        assert(record->new_path != NULL);
        assert(strcmp(record->old_path, "/tmp/alfred-lost-old") == 0);
        assert(strcmp(record->new_path, "/tmp/alfred-lost-new") == 0);
        assert(record->watch.reason != NULL);
        assert(strcmp(record->watch.reason, "IN_MOVE_SELF") == 0);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED:
        assert(record->watch.watch_id == ctx->expected_watch_id);
        assert(record->old_path != NULL);
        assert(record->new_path != NULL);
        assert(strcmp(record->old_path, "/tmp/alfred-lost-old") == 0);
        assert(strcmp(record->new_path, "/tmp/alfred-lost-new") == 0);
        assert(record->recovery.children_count == 4u);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE:
        assert(record->path != NULL);
        assert(ctx->expected_coverage_path != NULL);
        assert(strcmp(record->path, ctx->expected_coverage_path) == 0);
        assert(record->recovery.directories_seen == 5u);
        assert(record->recovery.directories_watched == 3u);
        assert(record->recovery.directories_missing == 2u);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING:
        assert(record->recovery.detail_path != NULL);
        assert(strcmp(record->recovery.detail_path,
                      "/tmp/alfred-lost-new/missing") == 0);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS:
        assert(record->watch.state != NULL);
        assert(strcmp(record->watch.state, "needs-reinstall") == 0);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND:
        assert(record->watch.retry_count == 1u);
        break;

    case ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED:
        assert(record->watch.error != NULL);
        assert(strcmp(record->watch.error, "scan-failed") == 0);
        break;

    default:
        assert(!"unexpected lost-scope scan record type");
    }

    ctx->calls++;

    if (record->type == ctx->fail_type)
        return -1;

    return 0;
}

/*
 * fill_long_path - build a path that cannot fit the backend text sink buffer
 * @dst: destination buffer
 * @dst_size: size of @dst in bytes
 *
 * The record still points to a valid borrowed path, but text formatting must
 * fail with truncation. The test then proves the helper writes the legacy
 * compatibility fallback and still offers the structured WATCH_LOST_* record to
 * emit_record.
 */
static void fill_long_path(char *dst, size_t dst_size)
{
    assert(dst != NULL);
    assert(dst_size > 8u);

    strcpy(dst, "/tmp/");
    memset(dst + 5, 'y', dst_size - 7u);
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

static void emit_expected_scan_records(inotify_backend_context_t *ctx)
{
    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN,
               -1,
               "/tmp/alfred-lost-scan-output-root",
               NULL,
               NULL,
               NULL,
               NULL,
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               2,
               0,
               0,
               0,
               0) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_FOUND,
               77,
               NULL,
               "/tmp/alfred-lost-old",
               "/tmp/alfred-lost-new",
               "IN_MOVE_SELF",
               NULL,
               NULL,
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

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED,
               77,
               NULL,
               "/tmp/alfred-lost-old",
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               0,
               4,
               0,
               0,
               0) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               NULL,
               NULL,
               0,
               5,
               3,
               2,
               0,
               0,
               0,
               0,
               0) == 0);

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING,
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
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS,
               77,
               "/tmp/alfred-lost-new",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               "needs-reinstall",
               NULL,
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

    assert(backend_log_lost_scope_record(
               ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND,
               77,
               "/tmp/alfred-lost-old",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               1,
               0) == 0);
}

int main(void)
{
    const char *raw_log = "/tmp/alfred_lost_scan_output_raw.log";
    const char *event_log = "/tmp/alfred_lost_scan_output_events.log";
    const char *error_log = "/tmp/alfred_lost_scan_output_errors.log";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    logger_t logger;
    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    lost_scan_emit_context_t emit_ctx;
    memset(&emit_ctx, 0, sizeof(emit_ctx));
    emit_ctx.fail_type = ALFRED_RECORD_TYPE_UNKNOWN;
    emit_ctx.expected_coverage_path = "/tmp/alfred-lost-new";
    emit_ctx.expected_watch_id = 77;

    inotify_backend_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.logger = &logger;
    ctx.emit_record = collect_or_fail_lost_scan_record;
    ctx.emit_record_userdata = &emit_ctx;

    emit_expected_scan_records(&ctx);
    assert(emit_ctx.calls == 7u);

    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
               77,
               "/tmp/alfred-lost-old",
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               "scan-failed",
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
    assert(emit_ctx.calls == 8u);

    emit_ctx.fail_type = ALFRED_RECORD_TYPE_WATCH_LOST_FOUND;
    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_FOUND,
               77,
               NULL,
               "/tmp/alfred-lost-old",
               "/tmp/alfred-lost-new",
               "IN_MOVE_SELF",
               NULL,
               NULL,
               NULL,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0) == -1);
    assert(emit_ctx.calls == 9u);

    char long_path[BACKEND_RECORD_TEXT_BUFFER_SIZE + 128u];

    fill_long_path(long_path, sizeof(long_path));
    emit_ctx.fail_type = ALFRED_RECORD_TYPE_UNKNOWN;
    emit_ctx.expected_coverage_path = long_path;
    emit_ctx.expected_watch_id = 88;

    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE,
               88,
               long_path,
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               NULL,
               NULL,
               0,
               5,
               3,
               2,
               0,
               0,
               0,
               0,
               0) == 0);
    assert(emit_ctx.calls == 10u);

    emit_ctx.fail_type = ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE;
    emit_ctx.expected_watch_id = 89;

    assert(backend_log_lost_scope_record(
               &ctx,
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE,
               89,
               long_path,
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               NULL,
               NULL,
               0,
               5,
               3,
               2,
               0,
               0,
               0,
               0,
               0) == -1);
    assert(emit_ctx.calls == 11u);

    logger_close(&logger);

    assert(file_contains(event_log, "WATCH_LOST_SCAN_BEGIN"));
    assert(file_contains(event_log, "WATCH_LOST_FOUND wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_PREFIX_UPDATED wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_COVERAGE_DONE wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_COVERAGE_DONE wd=88"));
    assert(file_contains(event_log, "WATCH_LOST_COVERAGE_DONE wd=89"));
    assert(file_contains(event_log, "WATCH_LOST_COVERAGE_MISSING wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_COVERAGE_CLASS wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_NOT_FOUND wd=77"));
    assert(file_contains(event_log, "WATCH_LOST_RECOVERY_FAILED wd=77"));
    assert(file_contains(error_log,
                         "failed to emit watch lost queue output record"));

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    return 0;
}
