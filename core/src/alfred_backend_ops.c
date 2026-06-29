#include "alfred_backend_ops.h"

#include <string.h>

static int backend_ops_string_is_non_empty(const char *value)
{
    return value != NULL && value[0] != '\0';
}

static int backend_ops_have_required_callbacks(
    const alfred_backend_ops_t *ops
)
{
    return ops->init != NULL &&
           ops->start != NULL &&
           ops->add_target != NULL &&
           ops->remove_target != NULL &&
           ops->poll != NULL &&
           ops->stop != NULL &&
           ops->destroy != NULL;
}

int alfred_backend_ops_is_minimally_valid(
    const alfred_backend_ops_t *ops
)
{
    const alfred_backend_capabilities_t *capabilities;

    if (ops == NULL || !backend_ops_string_is_non_empty(ops->name)) {
        return 0;
    }

    if (ops->api_version != ALFRED_BACKEND_API_VERSION_V0) {
        return 0;
    }

    capabilities = ops->capabilities;
    if (capabilities == NULL ||
        !backend_ops_string_is_non_empty(capabilities->backend_name)) {
        return 0;
    }

    if (capabilities->api_version != ops->api_version) {
        return 0;
    }

    if (strcmp(capabilities->backend_name, ops->name) != 0) {
        return 0;
    }

    if (capabilities->flags == 0u) {
        return 0;
    }

    return backend_ops_have_required_callbacks(ops);
}
