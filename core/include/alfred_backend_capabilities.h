/* ============================================================================
 * alfred_backend_capabilities.h - Backend API v0 capability descriptor
 *
 * Backend capabilities describe what a backend can observe or enforce. They are
 * static metadata about the backend implementation, not per-event state and not
 * configuration flags. A capability being present means "this backend is able
 * to provide this kind of evidence or control when configured accordingly".
 * ========================================================================== */

#ifndef ALFRED_BACKEND_CAPABILITIES_H
#define ALFRED_BACKEND_CAPABILITIES_H

#include <stdint.h>

#define ALFRED_BACKEND_API_VERSION_V0 0u

typedef uint64_t alfred_backend_capability_flags_t;

typedef enum alfred_backend_capability {
    ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS = 1ULL << 0,
    ALFRED_BACKEND_CAP_RECURSIVE_WATCH = 1ULL << 1,
    ALFRED_BACKEND_CAP_AUDIT_EVENTS = 1ULL << 2,
    ALFRED_BACKEND_CAP_METADATA_EVENTS = 1ULL << 3,
    ALFRED_BACKEND_CAP_SELF_EVENTS = 1ULL << 4,
    ALFRED_BACKEND_CAP_OVERFLOW_EVENTS = 1ULL << 5,
    ALFRED_BACKEND_CAP_IDENTITY_TRACKING = 1ULL << 6,
    ALFRED_BACKEND_CAP_LOST_SCOPE_RECOVERY = 1ULL << 7,
    ALFRED_BACKEND_CAP_PERMISSION_EVENTS = 1ULL << 8,
    ALFRED_BACKEND_CAP_PROCESS_CONTEXT = 1ULL << 9,
    ALFRED_BACKEND_CAP_NETWORK_CONTEXT = 1ULL << 10,
    ALFRED_BACKEND_CAP_CAN_BLOCK = 1ULL << 11
} alfred_backend_capability_t;

typedef struct alfred_backend_capabilities {
    const char *backend_name;
    uint32_t api_version;
    alfred_backend_capability_flags_t flags;
} alfred_backend_capabilities_t;

/*
 * alfred_backend_capabilities_has - test one capability flag
 * @capabilities: backend descriptor to inspect
 * @capability: single capability flag to test
 *
 * This helper accepts only one capability bit at a time. Passing zero, multiple
 * bits, or a NULL descriptor returns 0. This keeps call sites explicit and
 * avoids ambiguous checks such as "has any of these flags" vs "has all flags".
 *
 * Return: 1 when @capabilities declares @capability, 0 otherwise.
 */
int alfred_backend_capabilities_has(
    const alfred_backend_capabilities_t *capabilities,
    alfred_backend_capability_t capability
);

#endif /* ALFRED_BACKEND_CAPABILITIES_H */
