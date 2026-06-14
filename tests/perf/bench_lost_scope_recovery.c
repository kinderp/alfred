/*
 * bench_lost_scope_recovery.c - manual lost-scope recovery benchmark
 *
 * This benchmark is intentionally not part of the correctness test suite. It
 * creates synthetic directory trees, moves a watched subtree, and measures how
 * long the synchronous lost-scope recovery path takes while scanning for the
 * saved filesystem identity and reinstalling missing watches with fake watch
 * operations.
 *
 * Output columns:
 *
 * - dirs: directories created inside the moved subtree, excluding the root
 * - result: recovery outcome returned by the backend helper
 * - elapsed_us: wall-clock monotonic time spent in the recovery helper
 * - fake_adds: number of missing watches the recovery tried to reinstall
 * - fake_removes: rollback removals performed by the fake watch layer
 * - queue_after: pending lost-scope entries after the attempt
 *
 * The numbers are useful for comparing implementations on the same machine.
 * They are not stable correctness thresholds and should not gate CI.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../../modules/inotify/src/inotify_backend.c"

typedef struct fake_watch_ops_state {
    size_t add_calls;
    size_t remove_calls;
} fake_watch_ops_state_t;

static fake_watch_ops_state_t fake_watch_state;

/*
 * fake_watch_add - record a synthetic watch installation
 * @ctx: backend context whose watcher table receives the fake watch
 * @path: missing path found by recovery coverage scan
 *
 * Real watch installation would make this benchmark depend on kernel watch
 * descriptor allocation and system limits. The fake keeps the measurement
 * focused on scan, path update, coverage accounting, and recovery control
 * flow.
 *
 * Return: positive fake watch descriptor on success.
 */
static int fake_watch_add(inotify_backend_context_t *ctx, const char *path)
{
    int wd;

    assert(ctx != NULL);
    assert(ctx->runtime != NULL);
    assert(path != NULL);

    fake_watch_state.add_calls++;
    wd = 100000 + (int)fake_watch_state.add_calls;

    assert(watcher_store(&ctx->runtime->watchers, wd, path) == 0);

    return wd;
}

/*
 * fake_watch_remove - record a synthetic rollback removal
 * @ctx: backend context whose watcher table loses the fake watch
 * @wd: fake watch descriptor to remove
 *
 * Return: 0 to model successful rollback.
 */
static int fake_watch_remove(inotify_backend_context_t *ctx, int wd)
{
    assert(ctx != NULL);
    assert(ctx->runtime != NULL);

    fake_watch_state.remove_calls++;
    watcher_remove(&ctx->runtime->watchers, wd);

    return 0;
}

static const backend_resync_watch_ops_t FAKE_WATCH_OPS = {
    .add = fake_watch_add,
    .remove = fake_watch_remove
};

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s ROOT DIRS...\n"
            "example: %s /tmp/alfred_perf_lost_scope 10 100 1000\n",
            argv0,
            argv0);
}

static uint64_t now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

static void join_path(char *dest,
                      size_t dest_size,
                      const char *parent,
                      const char *name)
{
    int written;

    written = snprintf(dest, dest_size, "%s/%s", parent, name);
    assert(written > 0);
    assert((size_t)written < dest_size);
}

static void mkdir_checked(const char *path)
{
    if (mkdir(path, 0700) != 0) {
        fprintf(stderr, "mkdir failed path=%s errno=%d\n", path, errno);
        exit(1);
    }
}

static void remove_tree(const char *path)
{
    char command[PATH_MAX + 32];
    int written;

    /*
     * The benchmark owns paths below the caller-provided root. A shell rm keeps
     * this harness small; production scanner code does not use this helper.
     */
    written = snprintf(command, sizeof(command), "rm -rf -- '%s'", path);
    assert(written > 0);
    assert((size_t)written < sizeof(command));
    assert(system(command) == 0);
}

static void make_tree(const char *subtree, size_t dirs)
{
    char path[PATH_MAX];

    for (size_t i = 0; i < dirs; i++) {
        int written =
            snprintf(path, sizeof(path), "%s/dir-%05zu", subtree, i);

        assert(written > 0);
        assert((size_t)written < sizeof(path));
        mkdir_checked(path);
    }
}

static void reset_runtime(inotify_backend_t *runtime)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->fd = -1;

    assert(backend_lost_scope_queue_init(&runtime->lost_scopes, 1) == 0);
    assert(watcher_init(&runtime->watchers, 16) == 0);
}

static void destroy_runtime(inotify_backend_t *runtime)
{
    backend_configured_roots_destroy(runtime);
    backend_lost_scope_queue_destroy(&runtime->lost_scopes);
    watcher_destroy(&runtime->watchers);
}

static int run_case(const char *root, size_t dirs)
{
    char primary_root[PATH_MAX];
    char fallback_root[PATH_MAX];
    char original[PATH_MAX];
    char moved[PATH_MAX];
    struct stat st;
    inotify_backend_t runtime;
    logger_t logger;
    inotify_backend_context_t ctx;
    uint64_t start_ns;
    uint64_t end_ns;
    size_t processed;
    const char *result_name;

    remove_tree(root);
    mkdir_checked(root);

    join_path(primary_root, sizeof(primary_root), root, "primary");
    join_path(fallback_root, sizeof(fallback_root), root, "fallback");
    join_path(original, sizeof(original), primary_root, "lost");
    join_path(moved, sizeof(moved), fallback_root, "lost");

    mkdir_checked(primary_root);
    mkdir_checked(fallback_root);
    mkdir_checked(original);
    make_tree(original, dirs);

    assert(stat(original, &st) == 0);

    reset_runtime(&runtime);
    memset(&logger, 0, sizeof(logger));
    logger.event = tmpfile();
    assert(logger.event != NULL);

    memset(&ctx, 0, sizeof(ctx));
    ctx.runtime = &runtime;
    ctx.logger = &logger;

    assert(backend_configured_roots_add(&runtime, primary_root) == 0);
    assert(backend_configured_roots_add(&runtime, fallback_root) == 0);
    assert(watcher_store_identity(&runtime.watchers,
                                  1,
                                  original,
                                  st.st_dev,
                                  st.st_ino) == 0);
    assert(watcher_set_state(&runtime.watchers,
                             1,
                             WATCHER_STATE_STALE) == 0);
    assert(rename(original, moved) == 0);
    assert(backend_lost_scope_queue_enqueue(&runtime.lost_scopes,
                                            1,
                                            original,
                                            st.st_dev,
                                            st.st_ino,
                                            primary_root,
                                            "IN_MOVE_SELF",
                                            1,
                                            1) == 0);

    memset(&fake_watch_state, 0, sizeof(fake_watch_state));
    start_ns = now_ns();
    processed = backend_lost_scope_process_due_with_ops(&ctx,
                                                        primary_root,
                                                        1,
                                                        1,
                                                        &FAKE_WATCH_OPS);
    end_ns = now_ns();

    if (processed == 1 &&
        backend_lost_scope_queue_count(&runtime.lost_scopes) == 0 &&
        watcher_get_state(&runtime.watchers, 1) == WATCHER_STATE_VALID &&
        strcmp(watcher_get_path(&runtime.watchers, 1), moved) == 0) {
        result_name = "found";
    }
    else {
        result_name = "failed";
    }

    printf("%zu,%s,%llu,%zu,%zu,%zu\n",
           dirs,
           result_name,
           (unsigned long long)((end_ns - start_ns) / 1000ULL),
           fake_watch_state.add_calls,
           fake_watch_state.remove_calls,
           backend_lost_scope_queue_count(&runtime.lost_scopes));

    fclose(logger.event);
    destroy_runtime(&runtime);

    return strcmp(result_name, "found") == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *root;
    int failed = 0;

    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    root = argv[1];

    printf("dirs,result,elapsed_us,fake_adds,fake_removes,queue_after\n");

    for (int i = 2; i < argc; i++) {
        char *end = NULL;
        unsigned long parsed = strtoul(argv[i], &end, 10);

        if (end == argv[i] || *end != '\0') {
            fprintf(stderr, "invalid directory count: %s\n", argv[i]);
            return 1;
        }

        if (run_case(root, (size_t)parsed) != 0)
            failed = 1;
    }

    remove_tree(root);

    return failed;
}
