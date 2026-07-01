/*
 * test_backend_inotify_ops.c - inotify Backend API v0 ops skeleton contract
 *
 * This test locks down the first inotify-specific operations descriptor without
 * migrating app.c. The descriptor must be valid metadata. init/destroy are the
 * first real lifecycle callbacks; target management, polling, start and stop
 * remain staged fail-fast placeholders.
 */

#include "errors.h"
#include "inotify_backend.h"
#include "logger.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct emit_capture {
    int calls;
    int watch_added;
    int watch_removed;
    int fail_watch_added_on;
    int fail_watch_removed_on;
} emit_capture_t;

static int capture_emit(const alfred_record_t *record, void *userdata)
{
    emit_capture_t *capture = userdata;

    assert(record != NULL);

    if (capture != NULL)
        capture->calls++;

    if (capture != NULL && record->type == ALFRED_RECORD_TYPE_WATCH_ADDED)
        capture->watch_added++;

    if (capture != NULL && record->type == ALFRED_RECORD_TYPE_WATCH_REMOVED)
        capture->watch_removed++;

    if (capture != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_REMOVED &&
        capture->fail_watch_removed_on > 0 &&
        capture->watch_removed == capture->fail_watch_removed_on) {
        return ERR_IO;
    }

    if (capture != NULL &&
        record->type == ALFRED_RECORD_TYPE_WATCH_ADDED &&
        capture->fail_watch_added_on > 0 &&
        capture->watch_added == capture->fail_watch_added_on) {
        return ERR_IO;
    }

    return ERR_OK;
}

static void assert_runtime_initialized(
    const inotify_backend_ops_runtime_t *runtime,
    const inotify_config_t *config,
    const logger_t *logger,
    emit_capture_t *capture)
{
    assert(runtime != NULL);
    assert(runtime->initialized == 1);
    assert(runtime->runtime.fd >= 0);
    assert(runtime->context.runtime == &runtime->runtime);
    assert(runtime->context.config == config);
    assert(runtime->context.logger == logger);
    assert(runtime->context.emit_record == capture_emit);
    assert(runtime->context.emit_record_userdata == capture);
}

static void assert_runtime_destroyed(const inotify_backend_ops_runtime_t *runtime)
{
    assert(runtime != NULL);
    assert(runtime->initialized == 0);
    assert(runtime->runtime.fd == -1);
    assert(runtime->context.runtime == NULL);
    assert(runtime->context.config == NULL);
    assert(runtime->context.logger == NULL);
    assert(runtime->context.emit_record == NULL);
    assert(runtime->context.emit_record_userdata == NULL);
}

static void assert_add_target_rejected_without_watch(
    const alfred_backend_ops_t *ops,
    inotify_backend_ops_runtime_t *runtime,
    const alfred_backend_target_t *target)
{
    size_t before;
    size_t configured_roots_before;

    assert(ops != NULL);
    assert(runtime != NULL);

    before = watcher_count(&runtime->runtime.watchers);
    configured_roots_before = runtime->runtime.configured_roots_count;
    assert(ops->add_target((alfred_backend_t *)runtime, target) ==
           ERR_INVALID_ARG);
    assert(watcher_count(&runtime->runtime.watchers) == before);
    assert(runtime->runtime.configured_roots_count == configured_roots_before);
}

static void assert_remove_target_rejected_without_watch(
    const alfred_backend_ops_t *ops,
    inotify_backend_ops_runtime_t *runtime,
    const alfred_backend_target_t *target)
{
    size_t before;

    assert(ops != NULL);
    assert(runtime != NULL);

    before = watcher_count(&runtime->runtime.watchers);
    assert(ops->remove_target((alfred_backend_t *)runtime, target) ==
           ERR_INVALID_ARG);
    assert(watcher_count(&runtime->runtime.watchers) == before);
}

static void test_inotify_ops_rejects_invalid_add_target_arguments(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    int backend_option = 1;
    char too_long_path[PATH_MAX + 1u];
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_invalid.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_invalid.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_invalid.errors.log";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;

    target.path = "/tmp";
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;

    assert(ops->add_target(NULL, NULL) == ERR_INVALID_ARG);
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);

    /*
     * `initialized` alone is not a valid runtime boundary. The ops adapter
     * needs the context pointers installed by init before it may delegate to
     * the watch-manager path.
     */
    runtime.initialized = 1;
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);

    memset(&runtime, 0, sizeof(runtime));

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert_add_target_rejected_without_watch(ops, &runtime, NULL);

    target.target_type = 999u;
    assert_add_target_rejected_without_watch(ops, &runtime, &target);

    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.path = NULL;
    assert_add_target_rejected_without_watch(ops, &runtime, &target);

    target.path = "";
    assert_add_target_rejected_without_watch(ops, &runtime, &target);

    target.path = "/tmp";
    target.flags = 1u;
    assert_add_target_rejected_without_watch(ops, &runtime, &target);

    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;
    target.backend_options = &backend_option;
    assert_add_target_rejected_without_watch(ops, &runtime, &target);

    /*
     * Inotify v0 stores configured roots in fixed PATH_MAX buffers. A string
     * whose length is PATH_MAX cannot fit with the terminating NUL byte, so it
     * is a target validation error, not an allocation failure.
     */
    too_long_path[0] = '/';
    memset(too_long_path + 1, 'a', PATH_MAX - 1u);
    too_long_path[PATH_MAX] = '\0';

    target.backend_options = NULL;
    target.path = too_long_path;
    assert_add_target_rejected_without_watch(ops, &runtime, &target);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
}

static void test_inotify_ops_rejects_invalid_remove_target_arguments(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    int backend_option = 1;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_invalid.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_invalid.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_invalid.errors.log";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;

    target.path = "/tmp";
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;

    assert(ops->remove_target(NULL, NULL) == ERR_INVALID_ARG);
    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);

    runtime.initialized = 1;
    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);

    memset(&runtime, 0, sizeof(runtime));

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert_remove_target_rejected_without_watch(ops, &runtime, NULL);

    target.target_type = 999u;
    assert_remove_target_rejected_without_watch(ops, &runtime, &target);

    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.path = NULL;
    assert_remove_target_rejected_without_watch(ops, &runtime, &target);

    target.path = "";
    assert_remove_target_rejected_without_watch(ops, &runtime, &target);

    target.path = "/tmp";
    target.flags = 1u;
    assert_remove_target_rejected_without_watch(ops, &runtime, &target);

    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;
    target.backend_options = &backend_option;
    assert_remove_target_rejected_without_watch(ops, &runtime, &target);

    target.backend_options = NULL;
    target.path = "/tmp/alfred-not-currently-watched";
    assert_remove_target_rejected_without_watch(ops, &runtime, &target);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
}

static void test_inotify_ops_descriptor_is_valid(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();

    assert(ops != NULL);
    assert(strcmp(ops->name, "inotify") == 0);
    assert(ops->api_version == ALFRED_BACKEND_API_VERSION_V0);
    assert(ops->capabilities == inotify_backend_capabilities());
    assert(alfred_backend_ops_is_minimally_valid(ops) == 1);
}

static void test_inotify_ops_uses_inotify_capabilities(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();

    assert(alfred_backend_capabilities_has(
               ops->capabilities,
               ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS) == 1);
    assert(alfred_backend_capabilities_has(
               ops->capabilities,
               ALFRED_BACKEND_CAP_RECURSIVE_WATCH) == 1);
    assert(alfred_backend_capabilities_has(
               ops->capabilities,
               ALFRED_BACKEND_CAP_CAN_BLOCK) == 0);
}

static void test_inotify_ops_rejects_invalid_init_arguments(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    inotify_config_defaults(&inotify_config);

    assert(ops->init(NULL, NULL, NULL) == ERR_INVALID_ARG);
    assert(ops->init((alfred_backend_t *)&runtime, NULL, NULL) ==
           ERR_INVALID_ARG);

    config.logger = &logger;
    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     NULL) == ERR_INVALID_ARG);

    config.config = &inotify_config;
    config.logger = NULL;
    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     NULL) == ERR_INVALID_ARG);
    assert(runtime.initialized == 0);
}

static void test_inotify_ops_init_destroy_lifecycle(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log = "/tmp/alfred_test_backend_inotify_ops.raw.log";
    const char *event_log = "/tmp/alfred_test_backend_inotify_ops.events.log";
    const char *error_log = "/tmp/alfred_test_backend_inotify_ops.errors.log";
    char watch_root[] = "/tmp/alfred_test_backend_inotify_ops_watch.XXXXXX";

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(mkdtemp(watch_root) != NULL);

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);
    assert_runtime_initialized(&runtime, &inotify_config, &logger, &capture);

    /*
     * init/destroy/add_target/remove_target are real in this micro-step
     * sequence. The rest of the lifecycle still rejects accidental use until
     * each step is migrated.
     */
    assert(ops->start(NULL) == ERR_INVALID_ARG);
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 1);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.calls == 1);
    assert(capture.watch_added == 1);
    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 0);
    assert(runtime.runtime.configured_roots_count == 0);
    assert(capture.calls == 2);
    assert(capture.watch_removed == 1);
    assert(ops->poll((alfred_backend_t *)&runtime, 0) == ERR_INVALID_ARG);
    assert(ops->stop((alfred_backend_t *)&runtime) == ERR_INVALID_ARG);

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_INVALID_ARG);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);
    assert_runtime_initialized(&runtime, &inotify_config, &logger, &capture);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(runtime.runtime.configured_roots_count == 0);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    ops->destroy(NULL);
    ops->destroy((alfred_backend_t *)&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(watch_root);
}

static void test_inotify_ops_duplicate_nonrecursive_target_is_idempotent(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_nonrecursive.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_nonrecursive.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_nonrecursive.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_nonrecursive.XXXXXX";
    char trailing_slash_path[PATH_MAX];
    int written;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 0;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);
    assert(mkdtemp(watch_root) != NULL);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 1);

    written = snprintf(trailing_slash_path,
                       sizeof(trailing_slash_path),
                       "%s/",
                       watch_root);
    assert(written > 0 && (size_t)written < sizeof(trailing_slash_path));

    /*
     * V0 target identity is lexical but restricted: Alfred does not canonicalize
     * path aliases yet, so trailing-slash aliases are rejected except for "/".
     * This prevents /tmp/root and /tmp/root/ from becoming two configured API
     * targets that may share one kernel watch descriptor.
     */
    target.path = trailing_slash_path;
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);
    assert(watcher_count(&runtime.runtime.watchers) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 1);

    target.path = watch_root;

    /*
     * Exact duplicate target adds are an API-level idempotence rule, not a
     * recursive-watch detail. Non-recursive mode must therefore avoid another
     * inotify_add_watch() call and avoid another WATCH_ADDED diagnostic too.
     */
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 1);

    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(runtime.runtime.configured_roots_count == 0);
    assert(capture.watch_removed == 1);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(watch_root);
}

static void test_inotify_ops_duplicate_target_does_not_repair_missing_watch(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_missing.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_missing.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_missing.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_duplicate_missing.XXXXXX";
    int wd;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 0;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);
    assert(mkdtemp(watch_root) != NULL);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 1);

    wd = watcher_find_wd_by_path(&runtime.runtime.watchers, watch_root);
    assert(wd >= 0);

    /*
     * Duplicate add_target() is registry-idempotent in v0. If backend
     * maintenance has already cleared the active watch while configured_roots
     * still records the API target, a duplicate add returns ERR_OK but does not
     * reinstall kernel coverage. Forced reinstall is remove_target() + add_target().
     */
    watcher_remove(&runtime.runtime.watchers, wd);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(runtime.runtime.configured_roots_count == 1);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 1);

    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(runtime.runtime.configured_roots_count == 0);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(watch_root);
}

static void test_inotify_ops_remove_target_removes_recursive_subtree(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive.XXXXXX";
    char child_path[PATH_MAX];
    int written;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 1;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(mkdtemp(watch_root) != NULL);
    written = snprintf(child_path,
                       sizeof(child_path),
                       "%s/child",
                       watch_root);
    assert(written > 0 && (size_t)written < sizeof(child_path));
    assert(mkdir(child_path, 0700) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 2);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 1);
    assert(watcher_has_path(&runtime.runtime.watchers, child_path) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 2);

    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, child_path) == 0);
    assert(runtime.runtime.configured_roots_count == 0);
    assert(capture.watch_removed == 2);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(child_path);
    rmdir(watch_root);
}

static void test_inotify_ops_rejects_remove_of_recursive_child_watch(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_child.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_child.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_child.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_remove_child.XXXXXX";
    char child_path[PATH_MAX];
    int written;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 1;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(mkdtemp(watch_root) != NULL);
    written = snprintf(child_path,
                       sizeof(child_path),
                       "%s/child",
                       watch_root);
    assert(written > 0 && (size_t)written < sizeof(child_path));
    assert(mkdir(child_path, 0700) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 2);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 1);
    assert(watcher_has_path(&runtime.runtime.watchers, child_path) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 2);

    /*
     * The child watch is internal state created by the recursive parent target.
     * remove_target() may remove exact configured roots only; otherwise a
     * caller could punch a hole in the parent's recursive coverage.
     */
    target.path = child_path;
    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);
    assert(watcher_count(&runtime.runtime.watchers) == 2);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 1);
    assert(watcher_has_path(&runtime.runtime.watchers, child_path) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_removed == 0);

    target.path = watch_root;
    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(runtime.runtime.configured_roots_count == 0);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(child_path);
    rmdir(watch_root);
}

static void test_inotify_ops_remove_nonrecursive_target_without_active_watch(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_nonrecursive_missing.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_nonrecursive_missing.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_nonrecursive_missing.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_remove_nonrecursive_missing.XXXXXX";
    int wd;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 0;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);
    assert(mkdtemp(watch_root) != NULL);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(runtime.runtime.configured_roots_count == 1);
    wd = watcher_find_wd_by_path(&runtime.runtime.watchers, watch_root);
    assert(wd >= 0);

    /*
     * Model a backend maintenance path such as IN_IGNORED that has already
     * cleared the active watcher entry while the API target root is still
     * configured. remove_target() must still be able to unregister the target.
     */
    watcher_remove(&runtime.runtime.watchers, wd);
    assert(watcher_count(&runtime.runtime.watchers) == 0);

    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(runtime.runtime.configured_roots_count == 0);
    assert(capture.watch_removed == 0);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(watch_root);
}

static void test_inotify_ops_remove_recursive_target_without_active_watches(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive_missing.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive_missing.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive_missing.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_remove_recursive_missing.XXXXXX";
    char child_path[PATH_MAX];
    int root_wd;
    int child_wd;
    int written;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 1;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(mkdtemp(watch_root) != NULL);
    written = snprintf(child_path,
                       sizeof(child_path),
                       "%s/child",
                       watch_root);
    assert(written > 0 && (size_t)written < sizeof(child_path));
    assert(mkdir(child_path, 0700) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(runtime.runtime.configured_roots_count == 1);
    root_wd = watcher_find_wd_by_path(&runtime.runtime.watchers, watch_root);
    child_wd = watcher_find_wd_by_path(&runtime.runtime.watchers, child_path);
    assert(root_wd >= 0);
    assert(child_wd >= 0);

    /*
     * The configured root is the API target. If all active watches for that
     * target were already cleared, target removal still unregisters the root.
     */
    watcher_remove(&runtime.runtime.watchers, root_wd);
    watcher_remove(&runtime.runtime.watchers, child_wd);
    assert(watcher_count(&runtime.runtime.watchers) == 0);

    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(runtime.runtime.configured_roots_count == 0);
    assert(capture.watch_removed == 0);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(child_path);
    rmdir(watch_root);
}

static void test_inotify_ops_rolls_back_failed_recursive_add(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_add_rollback.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_add_rollback.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_add_rollback.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_add_rollback.XXXXXX";
    char child_path[PATH_MAX];
    int written;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 1;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(mkdtemp(watch_root) != NULL);
    written = snprintf(child_path,
                       sizeof(child_path),
                       "%s/child",
                       watch_root);
    assert(written > 0 && (size_t)written < sizeof(child_path));
    assert(mkdir(child_path, 0700) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    /*
     * Fail on the second WATCH_ADDED diagnostic. The root watch has already
     * been stored successfully, then the child add fails. add_target() must
     * roll back both configured_roots and the partial root watch before it
     * returns the error.
     */
    capture.fail_watch_added_on = 2;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INOTIFY);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, child_path) == 0);
    assert(runtime.runtime.configured_roots_count == 0);
    assert(capture.watch_added == 2);
    assert(capture.watch_removed == 1);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(child_path);
    rmdir(watch_root);
}

static void test_inotify_ops_completes_recursive_remove_after_emit_failure(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_failure.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_failure.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_remove_failure.errors.log";
    char watch_root[] =
        "/tmp/alfred_test_backend_inotify_ops_remove_failure.XXXXXX";
    char child_path[PATH_MAX];
    int written;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 1;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(mkdtemp(watch_root) != NULL);
    written = snprintf(child_path,
                       sizeof(child_path),
                       "%s/child",
                       watch_root);
    assert(written > 0 && (size_t)written < sizeof(child_path));
    assert(mkdir(child_path, 0700) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.path = watch_root;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 2);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 2);

    /*
     * watch_manager_remove() clears kernel/table state before emitting
     * WATCH_REMOVED. Even if that diagnostic fails, remove_target() must keep
     * removing the collected descriptors and clear the configured root so the
     * target state is not left half-removed.
     */
    capture.fail_watch_removed_on = 1;

    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INOTIFY);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, child_path) == 0);
    assert(runtime.runtime.configured_roots_count == 0);
    assert(capture.watch_removed == 2);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(child_path);
    rmdir(watch_root);
}

static void test_inotify_ops_rejects_overlapping_recursive_targets(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();
    inotify_backend_ops_runtime_t runtime;
    inotify_backend_ops_config_t config;
    inotify_config_t inotify_config;
    logger_t logger;
    emit_capture_t capture;
    alfred_backend_emit_t emit;
    alfred_backend_target_t target;
    const char *raw_log =
        "/tmp/alfred_test_backend_inotify_ops_overlap.raw.log";
    const char *event_log =
        "/tmp/alfred_test_backend_inotify_ops_overlap.events.log";
    const char *error_log =
        "/tmp/alfred_test_backend_inotify_ops_overlap.errors.log";
    char base[] = "/tmp/alfred_test_backend_inotify_ops_overlap.XXXXXX";
    char root_path[PATH_MAX];
    char child_path[PATH_MAX];
    char sibling_path[PATH_MAX];
    int written;

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);

    memset(&runtime, 0, sizeof(runtime));
    memset(&config, 0, sizeof(config));
    memset(&logger, 0, sizeof(logger));
    memset(&capture, 0, sizeof(capture));
    memset(&emit, 0, sizeof(emit));
    memset(&target, 0, sizeof(target));

    inotify_config_defaults(&inotify_config);
    inotify_config.watcher_capacity = 8;
    inotify_config.recursive = 1;

    assert(logger_init(&logger, raw_log, event_log, error_log) == 0);

    assert(mkdtemp(base) != NULL);
    written = snprintf(root_path,
                       sizeof(root_path),
                       "%s/root",
                       base);
    assert(written > 0 && (size_t)written < sizeof(root_path));
    written = snprintf(child_path,
                       sizeof(child_path),
                       "%s/child",
                       root_path);
    assert(written > 0 && (size_t)written < sizeof(child_path));
    written = snprintf(sibling_path,
                       sizeof(sibling_path),
                       "%s/root-old",
                       base);
    assert(written > 0 && (size_t)written < sizeof(sibling_path));

    assert(mkdir(root_path, 0700) == 0);
    assert(mkdir(child_path, 0700) == 0);
    assert(mkdir(sibling_path, 0700) == 0);

    config.config = &inotify_config;
    config.logger = &logger;
    emit.emit = capture_emit;
    emit.userdata = &capture;
    target.target_type = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH;
    target.flags = ALFRED_BACKEND_TARGET_FLAG_NONE;

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    target.path = root_path;
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 2);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 2);

    /*
     * Exact duplicates are idempotent. They must not reinstall watches or add
     * another configured root.
     */
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 2);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 2);

    target.path = child_path;
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);
    assert(watcher_count(&runtime.runtime.watchers) == 2);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 2);

    /*
     * Similar textual prefixes are allowed when the slash-boundary rule says
     * they are different subtrees.
     */
    target.path = sibling_path;
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 3);
    assert(runtime.runtime.configured_roots_count == 2);
    assert(capture.watch_added == 3);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    memset(&capture, 0, sizeof(capture));

    assert(ops->init((alfred_backend_t *)&runtime,
                     (const alfred_backend_config_t *)&config,
                     &emit) == ERR_OK);

    target.path = child_path;
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 1);

    target.path = root_path;
    assert(ops->add_target((alfred_backend_t *)&runtime, &target) ==
           ERR_INVALID_ARG);
    assert(watcher_count(&runtime.runtime.watchers) == 1);
    assert(runtime.runtime.configured_roots_count == 1);
    assert(capture.watch_added == 1);

    ops->destroy((alfred_backend_t *)&runtime);
    assert_runtime_destroyed(&runtime);

    logger_close(&logger);

    unlink(raw_log);
    unlink(event_log);
    unlink(error_log);
    rmdir(child_path);
    rmdir(root_path);
    rmdir(sibling_path);
    rmdir(base);
}

int main(void)
{
    test_inotify_ops_descriptor_is_valid();
    test_inotify_ops_uses_inotify_capabilities();
    test_inotify_ops_rejects_invalid_init_arguments();
    test_inotify_ops_rejects_invalid_add_target_arguments();
    test_inotify_ops_rejects_invalid_remove_target_arguments();
    test_inotify_ops_init_destroy_lifecycle();
    test_inotify_ops_duplicate_nonrecursive_target_is_idempotent();
    test_inotify_ops_duplicate_target_does_not_repair_missing_watch();
    test_inotify_ops_remove_target_removes_recursive_subtree();
    test_inotify_ops_rejects_remove_of_recursive_child_watch();
    test_inotify_ops_remove_nonrecursive_target_without_active_watch();
    test_inotify_ops_remove_recursive_target_without_active_watches();
    test_inotify_ops_rolls_back_failed_recursive_add();
    test_inotify_ops_completes_recursive_remove_after_emit_failure();
    test_inotify_ops_rejects_overlapping_recursive_targets();

    return 0;
}
