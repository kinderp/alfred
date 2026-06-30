/*
 * test_backend_inotify_ops.c - inotify Backend API v0 ops skeleton contract
 *
 * This test locks down the first inotify-specific operations descriptor without
 * migrating the runtime. The descriptor must be valid metadata, but its
 * return-valued callbacks intentionally fail fast until app.c is wired through
 * Backend API v0. The destroy callback is a no-op placeholder because the
 * Backend API v0 destroy hook returns void and cannot report ERR_INVALID_ARG.
 */

#include "errors.h"
#include "inotify_backend.h"

#include <assert.h>
#include <string.h>

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

static void test_inotify_ops_callbacks_are_not_runtime_wired_yet(void)
{
    const alfred_backend_ops_t *ops = inotify_backend_ops();

    /*
     * Return-valued callbacks reject use before the runtime migration. destroy
     * is intentionally harmless because its API contract cannot return errors.
     */
    assert(ops->init(NULL, NULL, NULL) == ERR_INVALID_ARG);
    assert(ops->start(NULL) == ERR_INVALID_ARG);
    assert(ops->add_target(NULL, NULL) == ERR_INVALID_ARG);
    assert(ops->remove_target(NULL, NULL) == ERR_INVALID_ARG);
    assert(ops->poll(NULL, 0) == ERR_INVALID_ARG);
    assert(ops->stop(NULL) == ERR_INVALID_ARG);
    ops->destroy(NULL);
}

int main(void)
{
    test_inotify_ops_descriptor_is_valid();
    test_inotify_ops_uses_inotify_capabilities();
    test_inotify_ops_callbacks_are_not_runtime_wired_yet();

    return 0;
}
