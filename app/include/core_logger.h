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

/*
 * core_logger_on_event - log one semantic event emitted by the core
 * @ev: semantic event emitted by the core
 * @userdata: logger_t pointer supplied when the core is created
 *
 * Formats @ev into the application event log. The callback does not take
 * ownership of @ev or of the path strings inside it; those values are valid
 * only for the duration of the callback.
 */
void core_logger_on_event(const alfred_event_t *ev, void *userdata);

#endif /* CORE_LOGGER_H */
