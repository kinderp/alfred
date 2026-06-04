/*
 * test_lost_scope_recovery.c - unit test for synchronous lost-scope search
 *
 * Expected log contract:
 *
 * raw.log:
 * - none; this test does not read the kernel inotify queue
 *
 * events log stream:
 * - WATCH_LOST_SCAN_BEGIN ... pending=0
 * - WATCH_LOST_FOUND ... old_path=<root>/original new_path=<root>/renamed
 * - WATCH_LOST_NOT_FOUND ... path=<root>/gone retry=0
 *
 * Forbidden events:
 * - FILE_*
 * - DIR_*
 * - WATCH_LOST_RECOVERY_END
 *
 * Meaning:
 * The test verifies the first delayed recovery building block: consume one
 * queued stale scope and search one monitored root for the saved filesystem
 * identity. A renamed directory is found by st_dev/st_ino even though its old
 * textual path is stale. A deleted directory is not found. The helper must not
 * update watcher paths, reinstall watches, or emit semantic filesystem events.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

/*
 * make_backend_context - build minimal backend state for lost-scope helpers
 * @runtime: backend runtime whose lost_scopes queue has been initialized
 * @logger: logger with a temporary event stream
 *
 * The recovery helper needs only the queue and logger in this micro-step. It
 * does not open inotify, mutate watcher_table_t, or install watches.
 *
 * Return: initialized backend context borrowing @runtime and @logger.
 */
static inotify_backend_context_t make_backend_context(
    inotify_backend_t *runtime,
    logger_t *logger
)
{
    inotify_backend_context_t ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = runtime;
    ctx.logger = logger;

    return ctx;
}

/*
 * join_path - build a child path inside the test root
 * @dest: destination buffer
 * @dest_size: destination buffer size
 * @root: test root path
 * @name: child name to append
 */
static void join_path(char *dest,
                      size_t dest_size,
                      const char *root,
                      const char *name)
{
    int written =
        snprintf(dest, dest_size, "%s/%s", root, name);

    assert(written > 0);
    assert((size_t)written < dest_size);
}

/*
 * assert_event_log_contains - verify one backend diagnostic fragment
 * @logger: logger whose event stream is a temporary file
 * @needle: fragment expected in the event stream
 */
static void assert_event_log_contains(logger_t *logger, const char *needle)
{
    char buffer[8192];
    size_t bytes_read;

    assert(logger != NULL);
    assert(logger->event != NULL);
    assert(needle != NULL);

    fflush(logger->event);
    rewind(logger->event);

    bytes_read = fread(buffer, 1, sizeof(buffer) - 1, logger->event);
    assert(bytes_read < sizeof(buffer));

    buffer[bytes_read] = '\0';

    assert(strstr(buffer, needle) != NULL);
}

/*
 * test_renamed_directory_is_found_by_identity - positive delayed search
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root used as bounded scan scope
 *
 * The stale entry records the old path, but the scan must find the directory at
 * its new path by comparing the saved st_dev/st_ino pair.
 */
static void test_renamed_directory_is_found_by_identity(
    inotify_backend_context_t *ctx,
    const char *root
)
{
    char original[PATH_MAX];
    char renamed[PATH_MAX];
    char found[PATH_MAX];
    struct stat st;

    join_path(original, sizeof(original), root, "original");
    join_path(renamed, sizeof(renamed), root, "renamed");

    assert(mkdir(original, 0700) == 0);
    assert(stat(original, &st) == 0);
    assert(rename(original, renamed) == 0);

    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            7,
                                            original,
                                            st.st_dev,
                                            st.st_ino,
                                            "IN_MOVE_SELF",
                                            1000,
                                            1000) == 0);

    assert(backend_lost_scope_recover_next(ctx,
                                           root,
                                           found,
                                           sizeof(found)) ==
           BACKEND_LOST_SCOPE_RECOVERY_FOUND);
    assert(strcmp(found, renamed) == 0);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);

    assert_event_log_contains(ctx->logger, "WATCH_LOST_SCAN_BEGIN");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_FOUND");
    assert_event_log_contains(ctx->logger, "old_path=");
    assert_event_log_contains(ctx->logger, "new_path=");
}

/*
 * test_deleted_directory_is_not_found - negative delayed search
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root used as bounded scan scope
 *
 * If the directory identity is no longer anywhere under the scanned root, the
 * helper reports NOT_FOUND. It does not invent a delete event and does not
 * remove or rewrite any watcher-table entry.
 */
static void test_deleted_directory_is_not_found(inotify_backend_context_t *ctx,
                                                const char *root)
{
    char gone[PATH_MAX];
    char found[PATH_MAX];
    struct stat st;

    join_path(gone, sizeof(gone), root, "gone");

    assert(mkdir(gone, 0700) == 0);
    assert(stat(gone, &st) == 0);
    assert(rmdir(gone) == 0);

    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            8,
                                            gone,
                                            st.st_dev,
                                            st.st_ino,
                                            "IN_MOVE_SELF",
                                            2000,
                                            2000) == 0);

    assert(backend_lost_scope_recover_next(ctx,
                                           root,
                                           found,
                                           sizeof(found)) ==
           BACKEND_LOST_SCOPE_RECOVERY_NOT_FOUND);
    assert(found[0] == '\0');
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);

    assert_event_log_contains(ctx->logger, "WATCH_LOST_NOT_FOUND");
}

/*
 * test_empty_queue_is_noop - no recovery work is available
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root path
 */
static void test_empty_queue_is_noop(inotify_backend_context_t *ctx,
                                     const char *root)
{
    char found[PATH_MAX];

    assert(backend_lost_scope_recover_next(ctx,
                                           root,
                                           found,
                                           sizeof(found)) ==
           BACKEND_LOST_SCOPE_RECOVERY_EMPTY);
}

int main(int argc, char **argv)
{
    assert(argc == 2);

    const char *root = argv[1];
    inotify_backend_t runtime;
    logger_t logger;

    memset(&runtime, 0, sizeof(runtime));
    memset(&logger, 0, sizeof(logger));

    assert(backend_lost_scope_queue_init(&runtime.lost_scopes, 1) == 0);

    logger.event = tmpfile();
    assert(logger.event != NULL);

    inotify_backend_context_t ctx =
        make_backend_context(&runtime, &logger);

    test_renamed_directory_is_found_by_identity(&ctx, root);
    test_deleted_directory_is_not_found(&ctx, root);
    test_empty_queue_is_noop(&ctx, root);

    fclose(logger.event);
    backend_lost_scope_queue_destroy(&runtime.lost_scopes);

    return 0;
}
