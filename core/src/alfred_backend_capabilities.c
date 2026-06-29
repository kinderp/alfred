#include "alfred_backend_capabilities.h"

static int alfred_backend_capability_is_single_bit(
    alfred_backend_capability_flags_t capability
)
{
    return capability != 0u && (capability & (capability - 1u)) == 0u;
}

int alfred_backend_capabilities_has(
    const alfred_backend_capabilities_t *capabilities,
    alfred_backend_capability_t capability
)
{
    alfred_backend_capability_flags_t flag =
        (alfred_backend_capability_flags_t)capability;

    if (capabilities == 0 || !alfred_backend_capability_is_single_bit(flag)) {
        return 0;
    }

    return (capabilities->flags & flag) != 0u;
}
