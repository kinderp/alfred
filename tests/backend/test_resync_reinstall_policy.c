/*
 * test_resync_reinstall_policy.c - unit test for resync reinstall rollback
 *
 * The shell backend diagnostics cover the successful IN_MOVE_SELF recovery
 * path end-to-end. This C test focuses on the policy that is hard to exercise
 * deterministically with the filesystem: one missing watch can be reinstalled,
 * the next reinstall can fail, and the helper must roll back the watch added
 * earlier in the same resync attempt.
 *
 * The test includes inotify_backend.c directly so it can call the static
 * backend_resync_reinstall_missing_watches() helper without exporting that
 * helper through the public backend header. The helper receives fake watch
 * operations, so no kernel inotify watch is installed.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../modules/inotify/src/inotify_backend.c"

/*
 * fake_watch_ops_state - observable state for fake resync watch operations
 * @add_calls: how many missing paths the helper tried to reinstall
 * @remove_calls: how many rollback removals the helper requested
 * @fail_on_add_call: 1-based add call that should fail, or 0 for no failure
 * @removed_wds: watch descriptors passed to fake_watch_remove()
 * @added_paths: missing paths passed to fake_watch_add()
 *
 * The production helper normally talks to the real watch manager and kernel
 * inotify watches. This test replaces those operations with deterministic fake
 * callbacks and then inspects this state to prove the policy decisions.
 */
typedef struct fake_watch_ops_state {
    int add_calls;
    int remove_calls;
    int fail_on_add_call;
    int removed_wds[8];
    const char *added_paths[8];
} fake_watch_ops_state_t;

static fake_watch_ops_state_t fake_state;

/*
 * fake_state_reset - prepare one isolated scenario
 * @fail_on_add_call: add attempt that should fail, or 0 to let all adds succeed
 *
 * Each test case configures the fake watch manager before calling the helper.
 * Resetting here prevents one scenario's recorded paths or rollback calls from
 * affecting the next scenario.
 */
static void fake_state_reset(int fail_on_add_call)
{
    memset(&fake_state, 0, sizeof(fake_state));
    fake_state.fail_on_add_call = fail_on_add_call;
}

/*
 * fake_watch_add - deterministic replacement for watch_manager_add()
 * @ctx: unused backend context, kept to match backend_resync_watch_ops_t
 * @path: missing path that the helper is trying to reinstall
 *
 * Records every requested path, optionally fails on a configured call number,
 * and otherwise returns predictable fake watch descriptors: 101, 102, ...
 * Predictable descriptors make rollback assertions precise.
 *
 * Return: fake wd on success, -1 on the configured failure call.
 */
static int fake_watch_add(inotify_backend_context_t *ctx, const char *path)
{
    (void)ctx;

    assert(path != NULL);
    assert(fake_state.add_calls < 8);

    fake_state.add_calls++;
    fake_state.added_paths[fake_state.add_calls - 1] = path;

    if (fake_state.add_calls == fake_state.fail_on_add_call)
        return -1;

    return 100 + fake_state.add_calls;
}

/*
 * fake_watch_remove - deterministic replacement for watch_manager_remove()
 * @ctx: unused backend context, kept to match backend_resync_watch_ops_t
 * @wd: fake watch descriptor that the helper wants to roll back
 *
 * Rollback correctness is the point of this test. The fake remove does not
 * mutate a real watcher table; it records which wd values would have been
 * removed by production code.
 *
 * Return: 0 to model a successful rollback removal.
 */
static int fake_watch_remove(inotify_backend_context_t *ctx, int wd)
{
    (void)ctx;

    assert(fake_state.remove_calls < 8);

    fake_state.removed_wds[fake_state.remove_calls] = wd;
    fake_state.remove_calls++;

    return 0;
}

/*
 * make_scan_context - build the minimal scan result consumed by the helper
 * @paths: missing path array owned by the caller/test case
 * @count: number of missing paths in @paths
 *
 * The real scanner fills backend_resync_scan_context_t through callbacks. This
 * unit test bypasses fs_scan_tree() and constructs only the fields needed by
 * backend_resync_reinstall_missing_watches(), keeping the test focused on
 * reinstall/rollback policy rather than traversal.
 *
 * Return: initialized scan context with borrowed test paths.
 */
static backend_resync_scan_context_t make_scan_context(char **paths,
                                                       size_t count)
{
    backend_resync_scan_context_t scan_context;

    memset(&scan_context, 0, sizeof(scan_context));
    scan_context.missing_paths = paths;
    scan_context.missing_paths_count = count;
    scan_context.missing_paths_capacity = count;

    return scan_context;
}

/*
 * make_backend_context - build the minimal backend context required by logging
 * @logger: logger whose event stream receives helper diagnostics
 *
 * The fake watch operations ignore fd/config/watcher state, but the helper logs
 * through ctx->logger. Keeping a real logger object preserves the same code
 * path used at runtime without starting Alfred or opening inotify.
 *
 * Return: initialized backend context borrowing @logger.
 */
static inotify_backend_context_t make_backend_context(logger_t *logger)
{
    inotify_backend_context_t ctx;

    memset(&ctx, 0, sizeof(ctx));
    ctx.logger = logger;

    return ctx;
}

/*
 * test_empty_missing_list_is_noop - covered scope needs no watch operations
 *
 * If the scanner found no missing paths, the helper must return success
 * without calling either operation. That keeps the resync path cheap for a
 * subtree that is already fully watched.
 */
static void test_empty_missing_list_is_noop(logger_t *logger)
{
    backend_resync_scan_context_t scan_context =
        make_scan_context(NULL, 0);
    inotify_backend_context_t ctx = make_backend_context(logger);
    backend_resync_watch_ops_t ops = {
        .add = fake_watch_add,
        .remove = fake_watch_remove
    };

    fake_state_reset(0);

    assert(backend_resync_reinstall_missing_watches(&ctx,
                                                    7,
                                                    "/tmp/root",
                                                    "IN_MOVE_SELF",
                                                    &scan_context,
                                                    &ops) == ERR_OK);
    assert(fake_state.add_calls == 0);
    assert(fake_state.remove_calls == 0);
}

/*
 * test_all_missing_paths_are_reinstalled - successful repair has no rollback
 *
 * This mirrors the positive shell scenario in a smaller unit test: every
 * missing path gets one add attempt, no add fails, and no rollback remove is
 * needed.
 */
static void test_all_missing_paths_are_reinstalled(logger_t *logger)
{
    char *paths[] = {
        "/tmp/root/a",
        "/tmp/root/b"
    };
    backend_resync_scan_context_t scan_context =
        make_scan_context(paths, 2);
    inotify_backend_context_t ctx = make_backend_context(logger);
    backend_resync_watch_ops_t ops = {
        .add = fake_watch_add,
        .remove = fake_watch_remove
    };

    fake_state_reset(0);

    assert(backend_resync_reinstall_missing_watches(&ctx,
                                                    7,
                                                    "/tmp/root",
                                                    "IN_MOVE_SELF",
                                                    &scan_context,
                                                    &ops) == ERR_OK);
    assert(fake_state.add_calls == 2);
    assert(fake_state.remove_calls == 0);
    assert(strcmp(fake_state.added_paths[0], "/tmp/root/a") == 0);
    assert(strcmp(fake_state.added_paths[1], "/tmp/root/b") == 0);
}

/*
 * test_failure_rolls_back_installed_watches - all-or-stale policy
 *
 * The second add attempt fails after the first path has already produced wd
 * 101. The helper must remove wd 101 and return ERR_INOTIFY so the caller
 * keeps the parent watch STALE.
 */
static void test_failure_rolls_back_installed_watches(logger_t *logger)
{
    char *paths[] = {
        "/tmp/root/a",
        "/tmp/root/b"
    };
    backend_resync_scan_context_t scan_context =
        make_scan_context(paths, 2);
    inotify_backend_context_t ctx = make_backend_context(logger);
    backend_resync_watch_ops_t ops = {
        .add = fake_watch_add,
        .remove = fake_watch_remove
    };

    fake_state_reset(2);

    assert(backend_resync_reinstall_missing_watches(&ctx,
                                                    7,
                                                    "/tmp/root",
                                                    "IN_MOVE_SELF",
                                                    &scan_context,
                                                    &ops) == ERR_INOTIFY);
    assert(fake_state.add_calls == 2);
    assert(fake_state.remove_calls == 1);
    assert(strcmp(fake_state.added_paths[0], "/tmp/root/a") == 0);
    assert(strcmp(fake_state.added_paths[1], "/tmp/root/b") == 0);
    assert(fake_state.removed_wds[0] == 101);
}

int main(void)
{
    logger_t logger;

    memset(&logger, 0, sizeof(logger));

    /*
     * The helper writes WATCH_RESYNC_* diagnostics with logger_event(). A
     * temporary stream lets those calls run normally while keeping the test
     * self-contained and avoiding files in the repository.
     */
    logger.event = tmpfile();
    assert(logger.event != NULL);

    test_empty_missing_list_is_noop(&logger);
    test_all_missing_paths_are_reinstalled(&logger);
    test_failure_rolls_back_installed_watches(&logger);

    fclose(logger.event);

    return 0;
}
