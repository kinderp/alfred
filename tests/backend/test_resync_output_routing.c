/*
 * test_resync_output_routing.c - WATCH_RESYNC output pipeline boundary
 *
 * Expected compatibility log contract:
 *
 * events.log:
 * - WATCH_RESYNC_BEGIN wd=77 path=/tmp/alfred-resync-output-routing
 *   reason=IN_MOVE_SELF
 * - WATCH_RESYNC_FAILED wd=77 path=/tmp/alfred-resync-output-routing
 *   reason=IN_MOVE_SELF error=identity-mismatch
 *
 * errors.log:
 * - failed to emit watch resync output record
 *
 * Meaning:
 * WATCH_RESYNC_* diagnostics are backend recovery state, not semantic
 * filesystem events. The backend writes the compatibility events.log/errors.log
 * line first, then offers the same borrowed alfred_record_t to the optional
 * structured output callback. If that callback fails, the helper returns an
 * error so the caller can fail closed when output_enabled=true. These records
 * keep WATCH_* names for log compatibility, but their structured category is
 * RECOVERY because they describe a recovery attempt rather than watch lifecycle
 * state such as WATCH_ADDED or WATCH_STALE.
 */

#include "inotify_backend.h"
#include "logger.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

typedef struct resync_emit_context {
    unsigned calls;
    alfred_record_type_t fail_type;
} resync_emit_context_t;

/*
 * collect_or_fail_resync_record - inspect routed WATCH_RESYNC records
 * @record: borrowed diagnostic record produced by the backend helper
 * @userdata: resync_emit_context_t controlling optional failure
 *
 * The callback models the application output boundary. It verifies that only
 * WATCH_RESYNC_BEGIN and WATCH_RESYNC_FAILED reach this focused test, checks
 * the key recovery/watch fields, counts the call, and optionally rejects one
 * configured type.
 *
 * Return: 0 on accepted record, -1 on the configured failure type.
 */
static int collect_or_fail_resync_record(const alfred_record_t *record,
                                         void *userdata)
{
    resync_emit_context_t *ctx = userdata;

    assert(record != NULL);
    assert(ctx != NULL);
    assert(record->category == ALFRED_RECORD_CATEGORY_RECOVERY);
    assert(record->backend != NULL);
    assert(strcmp(record->backend, "inotify") == 0);
    assert(record->path != NULL);
    assert(strcmp(record->path,
                  "/tmp/alfred-resync-output-routing") == 0);
    assert(record->watch.watch_id == 77);
    assert(record->watch.reason != NULL);
    assert(strcmp(record->watch.reason, "IN_MOVE_SELF") == 0);

    assert(record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_BEGIN ||
           record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED);

    if (record->type == ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED) {
        assert(record->watch.error != NULL);
        assert(strcmp(record->watch.error, "identity-mismatch") == 0);
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

int main(void)
{
    const char *raw_log = "/tmp/alfred_resync_output_routing_raw.log";
    const char *event_log = "/tmp/alfred_resync_output_routing_events.log";
    const char *error_log = "/tmp/alfred_resync_output_routing_errors.log";
    const char *path = "/tmp/alfred-resync-output-routing";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    logger_t logger;
    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    resync_emit_context_t emit_ctx;
    memset(&emit_ctx, 0, sizeof(emit_ctx));
    emit_ctx.fail_type = ALFRED_RECORD_TYPE_UNKNOWN;

    inotify_backend_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.logger = &logger;
    ctx.emit_record = collect_or_fail_resync_record;
    ctx.emit_record_userdata = &emit_ctx;

    assert(backend_log_resync_record(&ctx,
                                     ALFRED_RECORD_TYPE_WATCH_RESYNC_BEGIN,
                                     77,
                                     path,
                                     "IN_MOVE_SELF",
                                     NULL,
                                     NULL,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0) == 0);
    assert(emit_ctx.calls == 1u);

    assert(backend_log_resync_failure(
               &ctx,
               77,
               path,
               "IN_MOVE_SELF",
               BACKEND_RESYNC_PROBE_IDENTITY_MISMATCH,
               0) == 0);
    assert(emit_ctx.calls == 2u);

    emit_ctx.fail_type = ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED;
    assert(backend_log_resync_failure(
               &ctx,
               77,
               path,
               "IN_MOVE_SELF",
               BACKEND_RESYNC_PROBE_IDENTITY_MISMATCH,
               0) == -1);
    assert(emit_ctx.calls == 3u);

    logger_close(&logger);

    assert(file_contains(event_log, "WATCH_RESYNC_BEGIN wd=77"));
    assert(file_contains(event_log,
                         "WATCH_RESYNC_FAILED wd=77"));
    assert(file_contains(event_log, "error=identity-mismatch"));
    assert(file_contains(error_log,
                         "failed to emit watch resync output record"));

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    return 0;
}
