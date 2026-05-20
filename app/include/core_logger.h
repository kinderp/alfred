/* ============================================================================
 * core_logger.h - core semantic event logging callback
 *
 * This header exposes the callback used to connect the core correlator output
 * to the application logger. The callback is designed to be passed to
 * alfred_create() as an alfred_emit_fn.
 * ============================================================================
 */

#ifndef CORE_LOGGER_H
#define CORE_LOGGER_H

#include "alfred_correlator.h"
#include "config.h"
#include "logger.h"

/*
 * core_logger_context_t - application-owned core logging options
 * @logger: destination logger
 * @event_engine_mode: selected event stream mode
 *
 * The core itself emits structured alfred_event_t values. This context lets the
 * application format those values differently while integration is in progress:
 * shadow mode uses an explicit `core ...` prefix, while core mode writes the
 * plain official event stream.
 */
typedef struct {
    logger_t *logger;
    event_engine_mode_t event_engine_mode;
} core_logger_context_t;

/*
 * core_logger_on_event - log one semantic event emitted by the core
 * @ev: semantic event emitted by the core
 * @userdata: core_logger_context_t pointer supplied when the core is created
 *
 * Formats @ev into the application event log. The callback does not take
 * ownership of @ev or of the path strings inside it; those values are valid
 * only for the duration of the callback.
 */
void core_logger_on_event(const alfred_event_t *ev, void *userdata);

#endif /* CORE_LOGGER_H */
