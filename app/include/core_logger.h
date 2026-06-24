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
#include "alfred_record.h"
#include "logger.h"

typedef int (*core_logger_record_emit_fn)(const alfred_record_t *record,
                                          void *userdata);

/*
 * core_logger_context_t - application-owned core logging options
 * @logger: destination logger
 * @emit_record: optional structured-record output callback
 * @emit_record_userdata: opaque context passed to @emit_record
 *
 * The core itself emits structured alfred_event_t values. The application owns
 * the destination logger and optional output pipeline. This context adapts the
 * core callback shape to logger_event() and, when configured, to the generic
 * record output path without making core_logger depend on app_t.
 */
typedef struct {
    logger_t *logger;
    core_logger_record_emit_fn emit_record;
    void *emit_record_userdata;
} core_logger_context_t;

/*
 * core_logger_on_event - log one semantic event emitted by the core
 * @ev: semantic event emitted by the core
 * @userdata: core_logger_context_t pointer supplied when the core is created
 *
 * Converts @ev to an Event Model v0 record and, when @emit_record is
 * configured, offers that structured record to the application-owned output
 * path before compatibility text formatting. The compatibility text sink then
 * writes the human-readable payload through the application logger.
 *
 * The ordering is intentional: a fixed-size text payload buffer must not decide
 * whether structured JSONL output receives a valid semantic record. The
 * callback does not take ownership of @ev or of the path strings inside it;
 * those values are valid only for the duration of the callback.
 */
void core_logger_on_event(const alfred_event_t *ev, void *userdata);

#endif /* CORE_LOGGER_H */
