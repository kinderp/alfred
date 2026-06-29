/*
 * test_backend_capabilities.c - Backend API v0 capabilities contract
 *
 * This test covers static backend metadata, not runtime event delivery. The
 * capability descriptor is intentionally cheap: it is a constant bitmask used
 * to document what a backend can provide before Alfred grows multiple backend
 * implementations.
 */

#include "alfred_backend_capabilities.h"
#include "inotify_backend.h"

#include <assert.h>
#include <string.h>

static void test_capability_helper_rejects_ambiguous_input(void)
{
    alfred_backend_capabilities_t capabilities = {
        .backend_name = "test",
        .api_version = ALFRED_BACKEND_API_VERSION_V0,
        .flags = ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS
    };

    assert(alfred_backend_capabilities_has(
               NULL,
               ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS) == 0);
    assert(alfred_backend_capabilities_has(&capabilities, 0) == 0);
    assert(alfred_backend_capabilities_has(
               &capabilities,
               ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS |
                   ALFRED_BACKEND_CAP_RECURSIVE_WATCH) == 0);
}

static void test_inotify_declares_observational_filesystem_capabilities(void)
{
    const alfred_backend_capabilities_t *capabilities =
        inotify_backend_capabilities();

    assert(capabilities != NULL);
    assert(strcmp(capabilities->backend_name, "inotify") == 0);
    assert(capabilities->api_version == ALFRED_BACKEND_API_VERSION_V0);

    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS) == 1);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_RECURSIVE_WATCH) == 1);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_METADATA_EVENTS) == 1);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_SELF_EVENTS) == 1);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_OVERFLOW_EVENTS) == 1);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_IDENTITY_TRACKING) == 1);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_LOST_SCOPE_RECOVERY) == 1);
}

static void test_inotify_does_not_claim_enforcement_or_context(void)
{
    const alfred_backend_capabilities_t *capabilities =
        inotify_backend_capabilities();

    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_PERMISSION_EVENTS) == 0);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_AUDIT_EVENTS) == 0);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_PROCESS_CONTEXT) == 0);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_NETWORK_CONTEXT) == 0);
    assert(alfred_backend_capabilities_has(
               capabilities,
               ALFRED_BACKEND_CAP_CAN_BLOCK) == 0);
}

int main(void)
{
    test_capability_helper_rejects_ambiguous_input();
    test_inotify_declares_observational_filesystem_capabilities();
    test_inotify_does_not_claim_enforcement_or_context();

    return 0;
}
