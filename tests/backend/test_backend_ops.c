/*
 * test_backend_ops.c - Backend API v0 operations skeleton contract
 *
 * This test covers the static ops descriptor boundary. It does not start a
 * backend and does not exercise runtime event delivery. The goal is to lock
 * down what a backend must declare before future runtime wiring can register it
 * through a common Backend API v0 entry point.
 */

#include "alfred_backend_ops.h"

#include <assert.h>

typedef struct fake_backend_runtime {
    alfred_backend_emit_fn emit_fn;
    void *emit_userdata;
} fake_backend_runtime_t;

static const alfred_backend_capabilities_t TEST_CAPABILITIES = {
    .backend_name = "test",
    .api_version = ALFRED_BACKEND_API_VERSION_V0,
    .flags = ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS
};

static int dummy_init(
    alfred_backend_t *backend,
    const alfred_backend_config_t *config,
    const alfred_backend_emit_t *emit
)
{
    (void)backend;
    (void)config;
    (void)emit;
    return 0;
}

static int dummy_start(alfred_backend_t *backend)
{
    (void)backend;
    return 0;
}

static int dummy_add_target(
    alfred_backend_t *backend,
    const alfred_backend_target_t *target
)
{
    (void)backend;
    (void)target;
    return 0;
}

static int dummy_remove_target(
    alfred_backend_t *backend,
    const alfred_backend_target_t *target
)
{
    (void)backend;
    (void)target;
    return 0;
}

static int dummy_poll(alfred_backend_t *backend, int timeout_ms)
{
    (void)backend;
    (void)timeout_ms;
    return 0;
}

static int dummy_stop(alfred_backend_t *backend)
{
    (void)backend;
    return 0;
}

static void dummy_destroy(alfred_backend_t *backend)
{
    (void)backend;
}

static alfred_backend_ops_t valid_ops(void)
{
    alfred_backend_ops_t ops = {
        .name = "test",
        .api_version = ALFRED_BACKEND_API_VERSION_V0,
        .capabilities = &TEST_CAPABILITIES,
        .init = dummy_init,
        .start = dummy_start,
        .add_target = dummy_add_target,
        .remove_target = dummy_remove_target,
        .poll = dummy_poll,
        .stop = dummy_stop,
        .destroy = dummy_destroy
    };

    return ops;
}

static void test_ops_accepts_complete_v0_descriptor(void)
{
    alfred_backend_ops_t ops = valid_ops();

    assert(alfred_backend_ops_is_minimally_valid(&ops) == 1);
}

static void test_ops_rejects_missing_or_empty_name(void)
{
    alfred_backend_ops_t ops = valid_ops();

    assert(alfred_backend_ops_is_minimally_valid(NULL) == 0);

    ops.name = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    ops = valid_ops();
    ops.name = "";
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static void test_ops_rejects_invalid_version(void)
{
    alfred_backend_ops_t ops = valid_ops();

    ops.api_version = ALFRED_BACKEND_API_VERSION_V0 + 1u;

    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static void test_ops_rejects_missing_capabilities(void)
{
    alfred_backend_ops_t ops = valid_ops();

    ops.capabilities = NULL;

    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static void test_ops_rejects_missing_or_empty_capability_name(void)
{
    alfred_backend_capabilities_t capabilities = TEST_CAPABILITIES;
    alfred_backend_ops_t ops = valid_ops();

    capabilities.backend_name = NULL;
    ops.capabilities = &capabilities;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    capabilities = TEST_CAPABILITIES;
    ops = valid_ops();
    capabilities.backend_name = "";
    ops.capabilities = &capabilities;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static void test_ops_rejects_capability_name_mismatch(void)
{
    alfred_backend_capabilities_t capabilities = TEST_CAPABILITIES;
    alfred_backend_ops_t ops = valid_ops();

    capabilities.backend_name = "other";
    ops.capabilities = &capabilities;

    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static void test_ops_rejects_capability_version_mismatch(void)
{
    alfred_backend_capabilities_t capabilities = TEST_CAPABILITIES;
    alfred_backend_ops_t ops = valid_ops();

    capabilities.api_version = ALFRED_BACKEND_API_VERSION_V0 + 1u;
    ops.capabilities = &capabilities;

    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static void test_ops_rejects_zero_capabilities(void)
{
    alfred_backend_capabilities_t capabilities = TEST_CAPABILITIES;
    alfred_backend_ops_t ops = valid_ops();

    capabilities.flags = 0u;
    ops.capabilities = &capabilities;

    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static void test_ops_rejects_missing_callbacks(void)
{
    alfred_backend_ops_t ops = valid_ops();

    ops.init = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    ops = valid_ops();
    ops.start = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    ops = valid_ops();
    ops.add_target = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    ops = valid_ops();
    ops.remove_target = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    ops = valid_ops();
    ops.poll = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    ops = valid_ops();
    ops.stop = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);

    ops = valid_ops();
    ops.destroy = NULL;
    assert(alfred_backend_ops_is_minimally_valid(&ops) == 0);
}

static int fake_backend_init_copies_emit_values(
    fake_backend_runtime_t *runtime,
    const alfred_backend_emit_t *emit
)
{
    if (runtime == NULL || emit == NULL || emit->emit == NULL) {
        return -1;
    }

    runtime->emit_fn = emit->emit;
    runtime->emit_userdata = emit->userdata;

    return 0;
}

static int capture_emit_userdata(
    const alfred_record_t *record,
    void *userdata
)
{
    int *seen = userdata;

    (void)record;

    if (seen == NULL) {
        return -1;
    }

    *seen += 1;
    return 0;
}

static void test_backend_emit_contract_copies_values_not_envelope_pointer(void)
{
    fake_backend_runtime_t runtime = {0};
    int seen = 0;
    alfred_backend_emit_t emit = {
        .emit = capture_emit_userdata,
        .userdata = &seen
    };
    alfred_record_t record = {0};

    assert(fake_backend_init_copies_emit_values(&runtime, &emit) == 0);

    /*
     * Mutating the original envelope after init simulates a caller-owned
     * stack/local envelope going out of scope. The fake backend must keep using
     * the copied function pointer and userdata value, not the envelope address.
     */
    emit.emit = NULL;
    emit.userdata = NULL;

    assert(runtime.emit_fn != NULL);
    assert(runtime.emit_fn(&record, runtime.emit_userdata) == 0);
    assert(seen == 1);
}

int main(void)
{
    test_ops_accepts_complete_v0_descriptor();
    test_ops_rejects_missing_or_empty_name();
    test_ops_rejects_invalid_version();
    test_ops_rejects_missing_capabilities();
    test_ops_rejects_missing_or_empty_capability_name();
    test_ops_rejects_capability_name_mismatch();
    test_ops_rejects_capability_version_mismatch();
    test_ops_rejects_zero_capabilities();
    test_ops_rejects_missing_callbacks();
    test_backend_emit_contract_copies_values_not_envelope_pointer();

    return 0;
}
