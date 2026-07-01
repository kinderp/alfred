#include "inotify_backend.h"

#include "alfred_record_adapter.h"
#include "errors.h"

static int inotify_backend_ops_emit_raw_record(
    const alfred_raw_event_t *raw,
    void *userdata
)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)userdata;
    alfred_record_t record;

    if (runtime == NULL ||
        runtime->context.emit_record == NULL) {
        return ERR_INVALID_ARG;
    }

    if (raw == NULL)
        return ERR_OK;

    if (alfred_record_from_raw(raw, &record) != 0)
        return ERR_INVALID_ARG;

    return runtime->context.emit_record(&record,
                                        runtime->context.emit_record_userdata);
}

static int inotify_backend_ops_init(
    alfred_backend_t *backend,
    const alfred_backend_config_t *config,
    const alfred_backend_emit_t *emit
)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)backend;
    const inotify_backend_ops_config_t *inotify_config =
        (const inotify_backend_ops_config_t *)config;
    int error;

    if (runtime == NULL ||
        inotify_config == NULL ||
        inotify_config->config == NULL ||
        inotify_config->logger == NULL ||
        runtime->initialized) {
        return ERR_INVALID_ARG;
    }

    runtime->context.runtime = &runtime->runtime;
    runtime->context.config = inotify_config->config;
    runtime->context.logger = inotify_config->logger;
    runtime->context.emit_record = emit != NULL ? emit->emit : NULL;
    runtime->context.emit_record_userdata = emit != NULL ? emit->userdata : NULL;

    error = inotify_backend_init(&runtime->context);
    if (error != ERR_OK) {
        runtime->context.runtime = NULL;
        runtime->context.config = NULL;
        runtime->context.logger = NULL;
        runtime->context.emit_record = NULL;
        runtime->context.emit_record_userdata = NULL;
        return error;
    }

    runtime->initialized = 1;
    runtime->started = 0;

    return ERR_OK;
}

static int inotify_backend_ops_start(alfred_backend_t *backend)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)backend;

    if (runtime == NULL ||
        !runtime->initialized ||
        runtime->context.runtime == NULL ||
        runtime->context.config == NULL ||
        runtime->context.logger == NULL) {
        return ERR_INVALID_ARG;
    }

    runtime->started = 1;

    return ERR_OK;
}

static int inotify_backend_ops_add_target(
    alfred_backend_t *backend,
    const alfred_backend_target_t *target
)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)backend;

    if (runtime == NULL ||
        !runtime->initialized ||
        runtime->context.runtime == NULL ||
        runtime->context.config == NULL ||
        runtime->context.logger == NULL ||
        target == NULL ||
        target->target_type != ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH ||
        target->path == NULL ||
        target->path[0] == '\0' ||
        target->flags != ALFRED_BACKEND_TARGET_FLAG_NONE ||
        target->backend_options != NULL) {
        return ERR_INVALID_ARG;
    }

    return inotify_backend_add_startup_watch(&runtime->context, target->path);
}

static int inotify_backend_ops_remove_target(
    alfred_backend_t *backend,
    const alfred_backend_target_t *target
)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)backend;

    if (runtime == NULL ||
        !runtime->initialized ||
        runtime->context.runtime == NULL ||
        runtime->context.config == NULL ||
        runtime->context.logger == NULL ||
        target == NULL ||
        target->target_type != ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH ||
        target->path == NULL ||
        target->path[0] == '\0' ||
        target->flags != ALFRED_BACKEND_TARGET_FLAG_NONE ||
        target->backend_options != NULL) {
        return ERR_INVALID_ARG;
    }

    return inotify_backend_remove_startup_watch(&runtime->context,
                                                target->path);
}

static int inotify_backend_ops_poll(
    alfred_backend_t *backend,
    int timeout_ms
)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)backend;

    if (runtime == NULL ||
        !runtime->initialized ||
        !runtime->started ||
        runtime->context.runtime == NULL ||
        runtime->context.config == NULL ||
        runtime->context.logger == NULL ||
        runtime->context.emit_record == NULL ||
        timeout_ms != 0) {
        return ERR_INVALID_ARG;
    }

    return inotify_backend_poll(&runtime->context,
                                inotify_backend_ops_emit_raw_record,
                                runtime);
}

static int inotify_backend_ops_stop(alfred_backend_t *backend)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)backend;

    if (runtime == NULL ||
        !runtime->initialized ||
        runtime->context.runtime == NULL ||
        runtime->context.config == NULL ||
        runtime->context.logger == NULL) {
        return ERR_INVALID_ARG;
    }

    runtime->started = 0;

    return ERR_OK;
}

static void inotify_backend_ops_destroy(alfred_backend_t *backend)
{
    inotify_backend_ops_runtime_t *runtime =
        (inotify_backend_ops_runtime_t *)backend;

    if (runtime == NULL || !runtime->initialized)
        return;

    inotify_backend_shutdown(&runtime->context);

    runtime->context.runtime = NULL;
    runtime->context.config = NULL;
    runtime->context.logger = NULL;
    runtime->context.emit_record = NULL;
    runtime->context.emit_record_userdata = NULL;
    runtime->initialized = 0;
    runtime->started = 0;
}

static const alfred_backend_ops_t INOTIFY_BACKEND_OPS_TEMPLATE = {
    .name = "inotify",
    .api_version = ALFRED_BACKEND_API_VERSION_V0,
    .capabilities = NULL,
    .init = inotify_backend_ops_init,
    .start = inotify_backend_ops_start,
    .add_target = inotify_backend_ops_add_target,
    .remove_target = inotify_backend_ops_remove_target,
    .poll = inotify_backend_ops_poll,
    .stop = inotify_backend_ops_stop,
    .destroy = inotify_backend_ops_destroy
};

const alfred_backend_ops_t *inotify_backend_ops(void)
{
    static alfred_backend_ops_t ops;
    static int initialized;

    if (!initialized) {
        ops = INOTIFY_BACKEND_OPS_TEMPLATE;
        ops.capabilities = inotify_backend_capabilities();
        initialized = 1;
    }

    return &ops;
}
