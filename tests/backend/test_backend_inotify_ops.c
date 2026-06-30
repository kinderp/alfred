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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct emit_capture {
    int calls;
    int watch_added;
    int watch_removed;
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

    assert(ops != NULL);
    assert(runtime != NULL);

    before = watcher_count(&runtime->runtime.watchers);
    assert(ops->add_target((alfred_backend_t *)runtime, target) ==
           ERR_INVALID_ARG);
    assert(watcher_count(&runtime->runtime.watchers) == before);
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
    assert(capture.calls == 1);
    assert(capture.watch_added == 1);
    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 0);
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
    assert(capture.watch_added == 2);

    assert(ops->remove_target((alfred_backend_t *)&runtime, &target) == ERR_OK);
    assert(watcher_count(&runtime.runtime.watchers) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, watch_root) == 0);
    assert(watcher_has_path(&runtime.runtime.watchers, child_path) == 0);
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

int main(void)
{
    test_inotify_ops_descriptor_is_valid();
    test_inotify_ops_uses_inotify_capabilities();
    test_inotify_ops_rejects_invalid_init_arguments();
    test_inotify_ops_rejects_invalid_add_target_arguments();
    test_inotify_ops_rejects_invalid_remove_target_arguments();
    test_inotify_ops_init_destroy_lifecycle();
    test_inotify_ops_remove_target_removes_recursive_subtree();

    return 0;
}
