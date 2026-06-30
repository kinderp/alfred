#include "inotify_backend.h"

#include "errors.h"

/*
 * Keep the ops table itself const while reusing the same static capabilities
 * descriptor returned by inotify_backend_capabilities(). The public entry point
 * for callers remains the function, not this internal symbol.
 */
extern const alfred_backend_capabilities_t inotify_backend_capabilities_descriptor;

static int inotify_backend_ops_not_wired_init(
    alfred_backend_t *backend,
    const alfred_backend_config_t *config,
    const alfred_backend_emit_t *emit
)
{
    (void)backend;
    (void)config;
    (void)emit;
    return ERR_INVALID_ARG;
}

static int inotify_backend_ops_not_wired_start(alfred_backend_t *backend)
{
    (void)backend;
    return ERR_INVALID_ARG;
}

static int inotify_backend_ops_not_wired_add_target(
    alfred_backend_t *backend,
    const alfred_backend_target_t *target
)
{
    (void)backend;
    (void)target;
    return ERR_INVALID_ARG;
}

static int inotify_backend_ops_not_wired_remove_target(
    alfred_backend_t *backend,
    const alfred_backend_target_t *target
)
{
    (void)backend;
    (void)target;
    return ERR_INVALID_ARG;
}

static int inotify_backend_ops_not_wired_poll(
    alfred_backend_t *backend,
    int timeout_ms
)
{
    (void)backend;
    (void)timeout_ms;
    return ERR_INVALID_ARG;
}

static int inotify_backend_ops_not_wired_stop(alfred_backend_t *backend)
{
    (void)backend;
    return ERR_INVALID_ARG;
}

static void inotify_backend_ops_not_wired_destroy(alfred_backend_t *backend)
{
    (void)backend;
}

static const alfred_backend_ops_t INOTIFY_BACKEND_OPS = {
    .name = "inotify",
    .api_version = ALFRED_BACKEND_API_VERSION_V0,
    .capabilities = &inotify_backend_capabilities_descriptor,
    .init = inotify_backend_ops_not_wired_init,
    .start = inotify_backend_ops_not_wired_start,
    .add_target = inotify_backend_ops_not_wired_add_target,
    .remove_target = inotify_backend_ops_not_wired_remove_target,
    .poll = inotify_backend_ops_not_wired_poll,
    .stop = inotify_backend_ops_not_wired_stop,
    .destroy = inotify_backend_ops_not_wired_destroy
};

const alfred_backend_ops_t *inotify_backend_ops(void)
{
    return &INOTIFY_BACKEND_OPS;
}
