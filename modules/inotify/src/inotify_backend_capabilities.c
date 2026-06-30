#include "inotify_backend.h"

#include "alfred_backend_capabilities.h"

const alfred_backend_capabilities_t inotify_backend_capabilities_descriptor = {
    .backend_name = "inotify",
    .api_version = ALFRED_BACKEND_API_VERSION_V0,
    .flags = ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS |
             ALFRED_BACKEND_CAP_RECURSIVE_WATCH |
             ALFRED_BACKEND_CAP_METADATA_EVENTS |
             ALFRED_BACKEND_CAP_SELF_EVENTS |
             ALFRED_BACKEND_CAP_OVERFLOW_EVENTS |
             ALFRED_BACKEND_CAP_IDENTITY_TRACKING |
             ALFRED_BACKEND_CAP_LOST_SCOPE_RECOVERY
};

const alfred_backend_capabilities_t *inotify_backend_capabilities(void)
{
    return &inotify_backend_capabilities_descriptor;
}
