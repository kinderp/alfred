/* ============================================================================
 * alfred_backend_ops.h - Backend API v0 operations contract
 *
 * This header defines the first compile-tested shape of Backend API v0. It is
 * a contract skeleton only: current runtime wiring still uses the inotify
 * backend directly, and future commits will adapt that backend to this shape.
 * ========================================================================== */

#ifndef ALFRED_BACKEND_OPS_H
#define ALFRED_BACKEND_OPS_H

#include "alfred_backend_capabilities.h"
#include "alfred_record.h"

#include <stdint.h>

typedef struct alfred_backend alfred_backend_t;
typedef struct alfred_backend_config alfred_backend_config_t;
typedef struct alfred_backend_target alfred_backend_target_t;

typedef int (*alfred_backend_emit_fn)(
    const alfred_record_t *record,
    void *userdata
);

/*
 * alfred_backend_emit_t - caller-owned backend emit envelope
 * @emit: callback used by a backend to emit one borrowed record
 * @userdata: opaque caller-owned context passed back to @emit
 *
 * The caller owns this envelope and the object referenced by @userdata. A
 * backend may copy @emit and @userdata into its own runtime during init, but it
 * must not store the alfred_backend_emit_t pointer itself. This prevents a
 * backend from keeping a pointer to a stack-local envelope created by the
 * caller.
 *
 * The backend does not own @userdata and must not free it. The caller must keep
 * @userdata valid while the backend can still emit records, normally until the
 * backend has been stopped and destroyed.
 *
 * Records passed to @emit are borrowed and valid only for the duration of the
 * callback unless the receiver explicitly clones them.
 */
typedef struct alfred_backend_emit {
    alfred_backend_emit_fn emit;
    void *userdata;
} alfred_backend_emit_t;

typedef struct alfred_backend_ops {
    const char *name;
    uint32_t api_version;
    const alfred_backend_capabilities_t *capabilities;

    int (*init)(
        alfred_backend_t *backend,
        const alfred_backend_config_t *config,
        const alfred_backend_emit_t *emit
    );

    int (*start)(alfred_backend_t *backend);

    int (*add_target)(
        alfred_backend_t *backend,
        const alfred_backend_target_t *target
    );

    int (*remove_target)(
        alfred_backend_t *backend,
        const alfred_backend_target_t *target
    );

    int (*poll)(alfred_backend_t *backend, int timeout_ms);

    int (*stop)(alfred_backend_t *backend);

    void (*destroy)(alfred_backend_t *backend);
} alfred_backend_ops_t;

/*
 * alfred_backend_ops_is_minimally_valid - validate a Backend API v0 descriptor
 * @ops: backend operations descriptor to validate
 *
 * This helper validates static contract metadata and required lifecycle
 * callbacks. It is intended for wiring, tests, and future backend registry
 * checks; it is not part of the per-event hot path.
 *
 * A valid v0 descriptor must have:
 *
 * - a non-empty backend name;
 * - Backend API version v0;
 * - a non-NULL capabilities descriptor;
 * - matching ops/capabilities backend name and API version;
 * - at least one capability flag;
 * - all lifecycle callbacks populated.
 *
 * Therefore this helper rejects these contract violations explicitly:
 *
 * - @ops is NULL;
 * - @ops->name is NULL or empty;
 * - @ops->api_version is not ALFRED_BACKEND_API_VERSION_V0;
 * - @ops->capabilities is NULL;
 * - @ops->capabilities->backend_name is NULL or empty;
 * - @ops->capabilities->backend_name differs from @ops->name;
 * - @ops->capabilities->api_version differs from @ops->api_version;
 * - @ops->capabilities->flags is zero;
 * - any lifecycle callback is NULL.
 *
 * Return: 1 when @ops is valid enough to register, 0 otherwise.
 */
int alfred_backend_ops_is_minimally_valid(
    const alfred_backend_ops_t *ops
);

#endif /* ALFRED_BACKEND_OPS_H */
