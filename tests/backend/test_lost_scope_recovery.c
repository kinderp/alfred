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
 * - WATCH_LOST_REINSTALLED ... installed_path=<root>/renamed/unwatched
 * - WATCH_LOST_RECOVERY_END ... result=valid watches=3
 * - WATCH_LOST_REINSTALL_FAILED ... missing_path=<root>/rollback-renamed/...
 * - WATCH_LOST_ROLLBACK ... removed_wd=101
 * - WATCH_LOST_RECOVERY_FAILED ... error=reinstall-failed
 * - WATCH_LOST_NOT_FOUND ... path=<root>/gone retry=0
 * - WATCH_LOST_RETRY_SCHEDULED ... result=not-found retry=1 delay_ms=100
 * - WATCH_LOST_RECOVERY_GAVE_UP ... result=not-found retries=8
 * - WATCH_LOST_SCAN_BEGIN root=<root>
 * - WATCH_LOST_SCAN_BEGIN root=<root>/fallback-root
 *
 * Forbidden events:
 * - FILE_*
 * - DIR_*
 *
 * Meaning:
 * The test verifies the first delayed recovery building block: consume one
 * queued stale scope and search one monitored root for the saved filesystem
 * identity. A renamed directory is found by st_dev/st_ino even though its old
 * textual path is stale. The helper updates watcher-table paths for the found
 * root and already-known children after a match, then scans the recovered
 * subtree to measure missing watch coverage. Fake watch operations reinstall
 * the one missing path, after which the recovered subtree can return VALID
 * without emitting semantic filesystem events. A second scenario forces a
 * reinstall failure after one fake watch was installed and verifies rollback.
 * A deleted directory is not found. The due-entry processor then proves that a
 * temporary miss is requeued with bounded backoff, while a retry-budget
 * exhaustion is logged and not scheduled again. The processor also proves it
 * scans the root stored in the queued entry, not an unrelated caller root. A
 * final scenario proves a clean NOT_FOUND on scan_root tries another configured
 * root before spending retry budget.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

typedef struct fake_lost_scope_watch_ops_state {
    int add_calls;
    int remove_calls;
    int fail_on_add_call;
    int removed_wds[8];
    const char *added_paths[8];
} fake_lost_scope_watch_ops_state_t;

static fake_lost_scope_watch_ops_state_t fake_watch_state;

static void fake_watch_state_reset(int fail_on_add_call)
{
    memset(&fake_watch_state, 0, sizeof(fake_watch_state));
    fake_watch_state.fail_on_add_call = fail_on_add_call;
}

/*
 * fake_lost_scope_watch_add - deterministic lost-scope reinstall operation
 * @ctx: backend context whose watcher table receives a fake installed watch
 * @path: missing path that recovery wants to watch
 *
 * The real runtime calls watch_manager_add(). This test uses a fake operation
 * so the lost-scope all-or-stale path can be tested without opening an inotify
 * descriptor or depending on kernel watch descriptor allocation.
 *
 * Return: predictable fake wd on success, -1 on the configured failure call.
 */
static int fake_lost_scope_watch_add(inotify_backend_context_t *ctx,
                                     const char *path)
{
    int new_wd;

    assert(ctx != NULL);
    assert(ctx->runtime != NULL);
    assert(path != NULL);
    assert(fake_watch_state.add_calls < 8);

    fake_watch_state.add_calls++;
    fake_watch_state.added_paths[fake_watch_state.add_calls - 1] = path;

    if (fake_watch_state.add_calls == fake_watch_state.fail_on_add_call)
        return -1;

    new_wd = 100 + fake_watch_state.add_calls;

    assert(watcher_store(&ctx->runtime->watchers, new_wd, path) == 0);

    return new_wd;
}

/*
 * fake_lost_scope_watch_remove - deterministic rollback operation
 * @ctx: backend context whose watcher table loses the fake watch
 * @wd: fake watch descriptor installed earlier in this recovery attempt
 *
 * Rollback should remove only watches installed by the current attempt. The
 * fake remove records those descriptors and mirrors watcher table cleanup.
 *
 * Return: 0 to model successful rollback.
 */
static int fake_lost_scope_watch_remove(inotify_backend_context_t *ctx, int wd)
{
    assert(ctx != NULL);
    assert(ctx->runtime != NULL);
    assert(fake_watch_state.remove_calls < 8);

    fake_watch_state.removed_wds[fake_watch_state.remove_calls] = wd;
    fake_watch_state.remove_calls++;

    watcher_remove(&ctx->runtime->watchers, wd);

    return 0;
}

static const backend_resync_watch_ops_t FAKE_LOST_SCOPE_WATCH_OPS = {
    .add = fake_lost_scope_watch_add,
    .remove = fake_lost_scope_watch_remove
};

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
 * test_raw_callback_noop - backend poll callback for idle runtime tests
 * @raw: raw Alfred event, expected to be NULL for an idle nonblocking fd
 * @userdata: unused test context
 *
 * The idle poll scenario uses a nonblocking pipe to force EAGAIN. No kernel
 * inotify record is read, so the callback should not receive a raw event.
 *
 * Return: ERR_OK to keep the poll path successful if it is called.
 */
static int test_raw_callback_noop(const alfred_raw_event_t *raw,
                                  void *userdata)
{
    (void)raw;
    (void)userdata;

    return ERR_OK;
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
 * assert_event_log_not_contains - verify one diagnostic fragment is absent
 * @logger: logger whose event stream is a temporary file
 * @needle: fragment that must not appear in the event stream
 */
static void assert_event_log_not_contains(logger_t *logger, const char *needle)
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

    assert(strstr(buffer, needle) == NULL);
}

/*
 * reset_event_log - isolate one scenario's expected diagnostics
 * @logger: logger whose event stream is a temporary file
 *
 * The test process reuses one temporary logger stream for several scenarios.
 * Truncating it before each scenario lets the assertions describe only the
 * current scenario and makes forbidden-log checks meaningful.
 */
static void reset_event_log(logger_t *logger)
{
    int fd;

    assert(logger != NULL);
    assert(logger->event != NULL);

    fflush(logger->event);
    fd = fileno(logger->event);
    assert(fd >= 0);
    assert(ftruncate(fd, 0) == 0);
    rewind(logger->event);
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
    char renamed_unwatched[PATH_MAX];
    char found[PATH_MAX];
    struct stat st;
    struct stat child_st;

    reset_event_log(ctx->logger);

    join_path(original, sizeof(original), root, "original");
    join_path(original_child, sizeof(original_child), original, "child");
    join_path(original_unwatched,
              sizeof(original_unwatched),
              original,
              "unwatched");
    join_path(renamed, sizeof(renamed), root, "renamed");
    join_path(renamed_child, sizeof(renamed_child), renamed, "child");
    join_path(renamed_unwatched,
              sizeof(renamed_unwatched),
              renamed,
              "unwatched");

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
                                            root,
                                            "IN_MOVE_SELF",
                                            1000,
                                            1000) == 0);

    fake_watch_state_reset(0);

    assert(backend_lost_scope_recover_next_with_ops(ctx,
                                                    root,
                                                    found,
                                                    sizeof(found),
                                                    &FAKE_LOST_SCOPE_WATCH_OPS) ==
           BACKEND_LOST_SCOPE_RECOVERY_FOUND);
    assert(strcmp(found, renamed) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 7),
                  renamed) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 9),
                  renamed_child) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 101),
                  renamed_unwatched) == 0);
    assert(watcher_get_state(&ctx->runtime->watchers, 7) ==
           WATCHER_STATE_VALID);
    assert(watcher_get_state(&ctx->runtime->watchers, 9) ==
           WATCHER_STATE_VALID);
    assert(watcher_get_state(&ctx->runtime->watchers, 101) ==
           WATCHER_STATE_VALID);
    assert(fake_watch_state.add_calls == 1);
    assert(fake_watch_state.remove_calls == 0);
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
    assert_event_log_contains(ctx->logger, "WATCH_LOST_REINSTALLED");
    assert_event_log_contains(ctx->logger, "installed_path=");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_RECOVERY_END");
    assert_event_log_contains(ctx->logger, "result=valid");
    assert_event_log_contains(ctx->logger, "watches=3");
    assert_event_log_contains(ctx->logger, "old_path=");
    assert_event_log_contains(ctx->logger, "new_path=");
}

/*
 * test_reinstall_failure_rolls_back_partial_lost_scope - all-or-stale failure
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root used as bounded scan scope
 *
 * The recovered subtree contains two missing child directories. Fake watch
 * operations install the first missing path and fail on the second one. Alfred
 * must roll back the first fake watch, keep the recovered subtree non-VALID,
 * and log the failed reinstall plus rollback diagnostics.
 */
static void test_reinstall_failure_rolls_back_partial_lost_scope(
    inotify_backend_context_t *ctx,
    const char *root
)
{
    char original[PATH_MAX];
    char original_child[PATH_MAX];
    char original_missing_one[PATH_MAX];
    char original_missing_two[PATH_MAX];
    char renamed[PATH_MAX];
    char renamed_child[PATH_MAX];
    char renamed_missing_one[PATH_MAX];
    char found[PATH_MAX];
    struct stat st;
    struct stat child_st;

    reset_event_log(ctx->logger);

    join_path(original, sizeof(original), root, "rollback-original");
    join_path(original_child, sizeof(original_child), original, "child");
    join_path(original_missing_one,
              sizeof(original_missing_one),
              original,
              "missing-one");
    join_path(original_missing_two,
              sizeof(original_missing_two),
              original,
              "missing-two");
    join_path(renamed, sizeof(renamed), root, "rollback-renamed");
    join_path(renamed_child, sizeof(renamed_child), renamed, "child");
    join_path(renamed_missing_one,
              sizeof(renamed_missing_one),
              renamed,
              "missing-one");

    assert(mkdir(original, 0700) == 0);
    assert(mkdir(original_child, 0700) == 0);
    assert(mkdir(original_missing_one, 0700) == 0);
    assert(mkdir(original_missing_two, 0700) == 0);
    assert(stat(original, &st) == 0);
    assert(stat(original_child, &child_st) == 0);

    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  17,
                                  original,
                                  st.st_dev,
                                  st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             17,
                             WATCHER_STATE_STALE) == 0);
    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  19,
                                  original_child,
                                  child_st.st_dev,
                                  child_st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             19,
                             WATCHER_STATE_STALE) == 0);
    assert(rename(original, renamed) == 0);

    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            17,
                                            original,
                                            st.st_dev,
                                            st.st_ino,
                                            root,
                                            "IN_MOVE_SELF",
                                            3000,
                                            3000) == 0);

    fake_watch_state_reset(2);

    assert(backend_lost_scope_recover_next_with_ops(ctx,
                                                    root,
                                                    found,
                                                    sizeof(found),
                                                    &FAKE_LOST_SCOPE_WATCH_OPS) ==
           BACKEND_LOST_SCOPE_RECOVERY_SCAN_FAILED);
    assert(strcmp(found, renamed) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 17),
                  renamed) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 19),
                  renamed_child) == 0);
    assert(watcher_get_path(&ctx->runtime->watchers, 101) == NULL);
    assert(watcher_get_state(&ctx->runtime->watchers, 17) ==
           WATCHER_STATE_STALE);
    assert(watcher_get_state(&ctx->runtime->watchers, 19) ==
           WATCHER_STATE_STALE);
    assert(fake_watch_state.add_calls == 2);
    assert(fake_watch_state.remove_calls == 1);
    assert(fake_watch_state.removed_wds[0] == 101);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);

    assert_event_log_contains(ctx->logger, "WATCH_LOST_COVERAGE_DONE");
    assert_event_log_contains(ctx->logger, "dirs=4 watched=2 missing=2");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_REINSTALLED");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_REINSTALL_FAILED");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_ROLLBACK");
    assert_event_log_contains(ctx->logger, "removed_wd=101");
    assert_event_log_contains(ctx->logger, renamed_missing_one);
    assert_event_log_contains(ctx->logger, "WATCH_LOST_RECOVERY_FAILED");
    assert_event_log_contains(ctx->logger, "error=reinstall-failed");
    assert_event_log_not_contains(ctx->logger, "WATCH_LOST_RECOVERY_END");
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

    reset_event_log(ctx->logger);

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
                                            root,
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
 * test_process_due_skips_future_head - delayed work is not consumed early
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root path borrowed by the due processor
 *
 * The first synchronous due processor must be safe before any worker thread
 * exists. If the FIFO head has retry_after_ns in the future, the processor
 * should stop immediately, leave the entry queued, and avoid recovery logs.
 */
static void test_process_due_skips_future_head(inotify_backend_context_t *ctx,
                                               const char *root)
{
    inotify_lost_scope_entry_t entry;

    reset_event_log(ctx->logger);

    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            27,
                                            "/tmp/future",
                                            1,
                                            2,
                                            root,
                                            "IN_MOVE_SELF",
                                            1000,
                                            5000) == 0);

    assert(backend_lost_scope_process_due_with_ops(ctx,
                                                   root,
                                                   4999,
                                                   4,
                                                   &FAKE_LOST_SCOPE_WATCH_OPS) == 0);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 1);
    assert_event_log_not_contains(ctx->logger, "WATCH_LOST_SCAN_BEGIN");

    assert(backend_lost_scope_queue_pop(&ctx->runtime->lost_scopes,
                                        &entry) == 0);
    assert(entry.wd == 27);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);
}

/*
 * test_process_due_requeues_not_found - temporary miss gets bounded backoff
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root that does not contain the queued identity
 *
 * A successful scan that does not find the saved identity is not proof that the
 * object is permanently gone. The due processor should consume the mature entry
 * once, log WATCH_LOST_NOT_FOUND, append a retry record to the queue tail, and
 * delay the next attempt instead of spinning immediately.
 */
static void test_process_due_requeues_not_found(inotify_backend_context_t *ctx,
                                                const char *root)
{
    inotify_lost_scope_entry_t entry;
    uint64_t now_ns = 10000;

    reset_event_log(ctx->logger);

    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            28,
                                            "/tmp/retry",
                                            999999,
                                            888888,
                                            root,
                                            "IN_MOVE_SELF",
                                            1000,
                                            now_ns) == 0);

    assert(backend_lost_scope_process_due_with_ops(ctx,
                                                   root,
                                                   now_ns,
                                                   4,
                                                   &FAKE_LOST_SCOPE_WATCH_OPS) == 1);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 1);
    assert_event_log_contains(ctx->logger, "WATCH_LOST_NOT_FOUND");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_RETRY_SCHEDULED");
    assert_event_log_contains(ctx->logger, "result=not-found retry=1");
    assert_event_log_contains(ctx->logger, "delay_ms=100");

    assert(backend_lost_scope_queue_pop(&ctx->runtime->lost_scopes,
                                        &entry) == 0);
    assert(entry.wd == 28);
    assert(entry.retry_count == 1);
    assert(entry.retry_after_ns == now_ns + 100 * 1000000ULL);
    assert(strcmp(entry.old_path, "/tmp/retry") == 0);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);
}

/*
 * test_process_due_gives_up_after_retry_budget - bounded retry exhaustion
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root that does not contain the queued identity
 *
 * The retry policy must never create an infinite diagnostic loop. An entry that
 * has already consumed the retry budget is attempted once more, logged as
 * WATCH_LOST_RECOVERY_GAVE_UP, and deliberately not requeued.
 */
static void test_process_due_gives_up_after_retry_budget(
    inotify_backend_context_t *ctx,
    const char *root
)
{
    inotify_lost_scope_entry_t entry;

    reset_event_log(ctx->logger);
    memset(&entry, 0, sizeof(entry));

    entry.wd = 29;
    entry.device_id = 999999;
    entry.inode_id = 777777;
    entry.first_seen_ns = 1000;
    entry.retry_after_ns = 10000;
    entry.retry_count = BACKEND_LOST_SCOPE_MAX_ATTEMPTS - 1;
    snprintf(entry.old_path, sizeof(entry.old_path), "%s", "/tmp/give-up");
    snprintf(entry.scan_root, sizeof(entry.scan_root), "%s", root);
    snprintf(entry.reason, sizeof(entry.reason), "%s", "IN_MOVE_SELF");

    assert(backend_lost_scope_queue_enqueue_entry(&ctx->runtime->lost_scopes,
                                                  &entry) == 0);

    assert(backend_lost_scope_process_due_with_ops(ctx,
                                                   root,
                                                   10000,
                                                   4,
                                                   &FAKE_LOST_SCOPE_WATCH_OPS) == 1);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);
    assert_event_log_contains(ctx->logger, "WATCH_LOST_NOT_FOUND");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_RECOVERY_GAVE_UP");
    assert_event_log_contains(ctx->logger, "result=not-found retries=8");
    assert_event_log_not_contains(ctx->logger, "WATCH_LOST_RETRY_SCHEDULED");
}

/*
 * test_process_due_uses_entry_scan_root - queued root drives recovery scan
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root that contains the moved identity
 *
 * Runtime lost-scope entries carry scan_root so the due processor does not need
 * to infer the bounded search scope later. The caller root below is
 * deliberately wrong: if the processor still used that argument, the identity
 * search would fail before finding the renamed directory under @root.
 */
static void test_process_due_uses_entry_scan_root(inotify_backend_context_t *ctx,
                                                  const char *root)
{
    char original[PATH_MAX];
    char renamed[PATH_MAX];
    char wrong_root[PATH_MAX];
    struct stat st;

    reset_event_log(ctx->logger);

    join_path(original, sizeof(original), root, "scan-root-original");
    join_path(renamed, sizeof(renamed), root, "scan-root-renamed");
    join_path(wrong_root, sizeof(wrong_root), root, "scan-root-wrong");

    assert(mkdir(original, 0700) == 0);
    assert(stat(original, &st) == 0);

    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  30,
                                  original,
                                  st.st_dev,
                                  st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             30,
                             WATCHER_STATE_STALE) == 0);
    assert(rename(original, renamed) == 0);

    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            30,
                                            original,
                                            st.st_dev,
                                            st.st_ino,
                                            root,
                                            "IN_MOVE_SELF",
                                            4000,
                                            4000) == 0);

    fake_watch_state_reset(0);

    assert(backend_lost_scope_process_due_with_ops(ctx,
                                                   wrong_root,
                                                   4000,
                                                   4,
                                                   &FAKE_LOST_SCOPE_WATCH_OPS) == 1);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 30),
                  renamed) == 0);
    assert(watcher_get_state(&ctx->runtime->watchers, 30) ==
           WATCHER_STATE_VALID);
    assert_event_log_contains(ctx->logger, "WATCH_LOST_SCAN_BEGIN");
    assert_event_log_contains(ctx->logger, root);
    assert_event_log_contains(ctx->logger, "WATCH_LOST_RECOVERY_END");
    assert_event_log_not_contains(ctx->logger, wrong_root);
}

/*
 * test_process_due_falls_back_to_configured_roots - second root can recover
 * @ctx: backend context with initialized queue and logger
 * @root: test root containing two configured recovery roots
 *
 * When scan_root does not contain the saved identity, the due processor should
 * try the other configured roots before scheduling a retry. This models a
 * directory moved from one monitored tree to another monitored tree.
 */
static void test_process_due_falls_back_to_configured_roots(
    inotify_backend_context_t *ctx,
    const char *root
)
{
    char primary_root[PATH_MAX];
    char fallback_root[PATH_MAX];
    char original[PATH_MAX];
    char moved[PATH_MAX];
    struct stat st;

    reset_event_log(ctx->logger);
    backend_configured_roots_destroy(ctx->runtime);

    join_path(primary_root, sizeof(primary_root), root, "primary-root");
    join_path(fallback_root, sizeof(fallback_root), root, "fallback-root");
    join_path(original, sizeof(original), primary_root, "lost");
    join_path(moved, sizeof(moved), fallback_root, "lost");

    assert(mkdir(primary_root, 0700) == 0);
    assert(mkdir(fallback_root, 0700) == 0);
    assert(mkdir(original, 0700) == 0);
    assert(stat(original, &st) == 0);

    assert(backend_configured_roots_add(ctx->runtime, primary_root) == 0);
    assert(backend_configured_roots_add(ctx->runtime, fallback_root) == 0);

    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  31,
                                  original,
                                  st.st_dev,
                                  st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             31,
                             WATCHER_STATE_STALE) == 0);
    assert(rename(original, moved) == 0);

    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            31,
                                            original,
                                            st.st_dev,
                                            st.st_ino,
                                            primary_root,
                                            "IN_MOVE_SELF",
                                            5000,
                                            5000) == 0);

    fake_watch_state_reset(0);

    assert(backend_lost_scope_process_due_with_ops(ctx,
                                                   primary_root,
                                                   5000,
                                                   4,
                                                   &FAKE_LOST_SCOPE_WATCH_OPS) == 1);
    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 31),
                  moved) == 0);
    assert(watcher_get_state(&ctx->runtime->watchers, 31) ==
           WATCHER_STATE_VALID);
    assert_event_log_contains(ctx->logger, primary_root);
    assert_event_log_contains(ctx->logger, fallback_root);
    assert_event_log_contains(ctx->logger, "WATCH_LOST_NOT_FOUND");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_RECOVERY_END");
    assert_event_log_not_contains(ctx->logger, "WATCH_LOST_RETRY_SCHEDULED");

    backend_configured_roots_destroy(ctx->runtime);
}

/*
 * test_idle_poll_processes_due_lost_scope - runtime poll drains one entry
 * @ctx: backend context with initialized queue and logger
 * @root: monitored root used as configured runtime fallback
 *
 * This scenario proves the lost-scope processor is connected to the real
 * backend poll path, not only to direct unit-test calls. A nonblocking pipe
 * stands in for an idle fd: read(2) returns EAGAIN, and the poll path should
 * spend one bounded recovery attempt before returning ERR_OK.
 */
static void test_idle_poll_processes_due_lost_scope(
    inotify_backend_context_t *ctx,
    const char *root
)
{
    char original[PATH_MAX];
    char renamed[PATH_MAX];
    int pipe_fds[2];
    int flags;
    struct stat st;

    reset_event_log(ctx->logger);
    backend_configured_roots_destroy(ctx->runtime);

    join_path(original, sizeof(original), root, "idle-original");
    join_path(renamed, sizeof(renamed), root, "idle-renamed");

    assert(mkdir(original, 0700) == 0);
    assert(stat(original, &st) == 0);
    assert(backend_configured_roots_add(ctx->runtime, root) == 0);
    assert(watcher_store_identity(&ctx->runtime->watchers,
                                  32,
                                  original,
                                  st.st_dev,
                                  st.st_ino) == 0);
    assert(watcher_set_state(&ctx->runtime->watchers,
                             32,
                             WATCHER_STATE_STALE) == 0);
    assert(rename(original, renamed) == 0);
    assert(backend_lost_scope_queue_enqueue(&ctx->runtime->lost_scopes,
                                            32,
                                            original,
                                            st.st_dev,
                                            st.st_ino,
                                            root,
                                            "IN_MOVE_SELF",
                                            1,
                                            1) == 0);

    assert(pipe(pipe_fds) == 0);
    flags = fcntl(pipe_fds[0], F_GETFL, 0);
    assert(flags >= 0);
    assert(fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == 0);

    ctx->runtime->fd = pipe_fds[0];
    assert(inotify_backend_poll(ctx, test_raw_callback_noop, NULL) == ERR_OK);
    ctx->runtime->fd = -1;

    close(pipe_fds[0]);
    close(pipe_fds[1]);

    assert(backend_lost_scope_queue_count(&ctx->runtime->lost_scopes) == 0);
    assert(strcmp(watcher_get_path(&ctx->runtime->watchers, 32),
                  renamed) == 0);
    assert(watcher_get_state(&ctx->runtime->watchers, 32) ==
           WATCHER_STATE_VALID);
    assert_event_log_contains(ctx->logger, "WATCH_LOST_SCAN_BEGIN");
    assert_event_log_contains(ctx->logger, "WATCH_LOST_RECOVERY_END");

    backend_configured_roots_destroy(ctx->runtime);
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
    test_reinstall_failure_rolls_back_partial_lost_scope(&ctx, root);
    test_deleted_directory_is_not_found(&ctx, root);
    test_process_due_skips_future_head(&ctx, root);
    test_process_due_requeues_not_found(&ctx, root);
    test_process_due_gives_up_after_retry_budget(&ctx, root);
    test_process_due_uses_entry_scan_root(&ctx, root);
    test_process_due_falls_back_to_configured_roots(&ctx, root);
    test_idle_poll_processes_due_lost_scope(&ctx, root);
    test_empty_queue_is_noop(&ctx, root);

    fclose(logger.event);
    watcher_destroy(&runtime.watchers);
    backend_lost_scope_queue_destroy(&runtime.lost_scopes);
    backend_configured_roots_destroy(&runtime);

    return 0;
}
