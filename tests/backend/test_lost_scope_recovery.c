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
 * - WATCH_LOST_PREFIX_UPDATED ... old_prefix=<root>/original
 *   new_prefix=<root>/renamed children=1
 * - WATCH_LOST_COVERAGE_DONE ... dirs=3 watched=2 missing=1
 * - WATCH_LOST_COVERAGE_MISSING ... missing_path=<root>/renamed/unwatched
 * - WATCH_LOST_COVERAGE_CLASS ... result=needs-reinstall
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
 * textual path is stale. The helper updates watcher-table paths for the found
 * root and already-known children after a match, then scans the recovered
 * subtree to measure missing watch coverage. It must not reinstall watches,
 * mark the subtree VALID, or emit semantic filesystem events. A deleted
 * directory is not found.
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
 * The recovery helper needs only the queue, watcher table, and logger in this
 * micro-step. It does not open inotify or install watches.
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
 * its new path by comparing the saved st_dev/st_ino pair. Once found, the
 * helper updates the watched root and already-known child paths, then performs
 * a strict coverage scan. The deliberately unwatched child proves the scan can
 * detect missing watch coverage without installing a replacement yet.
 */
static void test_renamed_directory_is_found_by_identity(
    inotify_backend_context_t *ctx,
    const char *root
)
{
    char original[PATH_MAX];
    char original_child[PATH_MAX];
    char original_unwatched[PATH_MAX];
    char renamed[PATH_MAX];
    char renamed_child[PATH_MAX];
    char found[PATH_MAX];
    struct stat st;
    struct stat child_st;

    join_path(original, sizeof(original), root, "original");
    join_path(original_child, sizeof(original_child), original, "child");
    join_path(original_unwatched,
              sizeof(original_unwatched),
              original,
              "unwatched");
    join_path(renamed, sizeof(renamed), root, "renamed");
    join_path(renamed_child, sizeof(renamed_child), renamed, "child");

    assert(mkdir(original, 0700) == 0);
    assert(mkdir(original_child, 0700) == 0);
    assert(mkdir(original_unwatched, 0700) == 0);
    assert(stat(original, &st) == 0);
    assert(stat(original_child, &child_st) == 0);
    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  7,
                                  original,
                                  st.st_dev,
                                  st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             7,
                             WATCHER_STATE_STALE) == 0);
    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  9,
                                  original_child,
                                  child_st.st_dev,
                                  child_st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             9,
                             WATCHER_STATE_STALE) == 0);
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
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 7),
                  renamed) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 9),
                  renamed_child) == 0);
    assert(watcher_get_state(&ctx->runtime->watchers, 7) ==
           WATCHER_STATE_STALE);
    assert(watcher_get_state(&ctx->runtime->watchers, 9) ==
           WATCHER_STATE_STALE);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);

    assert_event_log_contains(ctx->logger, "WATCH_LOST_SCAN_BEGIN");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_FOUND");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_PREFIX_UPDATED");
    assert_event_log_contains(ctx->logger, "children=1");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_COVERAGE_DONE");
    assert_event_log_contains(ctx->logger, "dirs=3 watched=2 missing=1");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_COVERAGE_MISSING");
    assert_event_log_contains(ctx->logger, "missing_path=");
    assert_event_log_contains(ctx->logger, "/renamed/unwatched");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_COVERAGE_CLASS");
    assert_event_log_contains(ctx->logger, "result=needs-reinstall");
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
 * rewrite the watcher-table path.
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
    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  8,
                                  gone,
                                  st.st_dev,
                                  st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             8,
                             WATCHER_STATE_STALE) == 0);
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
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 8),
                  gone) == 0);
    assert(watcher_get_state(&ctx->runtime->watchers, 8) ==
           WATCHER_STATE_STALE);
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
    assert(watcher_init(&runtime.watchers, 4) == 0);

    logger.event = tmpfile();
    assert(logger.event != NULL);

    inotify_backend_context_t ctx =
        make_backend_context(&runtime, &logger);

    test_renamed_directory_is_found_by_identity(&ctx, root);
    test_deleted_directory_is_not_found(&ctx, root);
    test_empty_queue_is_noop(&ctx, root);

    fclose(logger.event);
    watcher_destroy(&runtime.watchers);
    backend_lost_scope_queue_destroy(&runtime.lost_scopes);

    return 0;
}
