/* ============================================================================
 * core_logger.c - core semantic event logging callback
 *
 * The core emits alfred_event_t values through a callback. This file adapts
 * that callback interface to the application logger, keeping core output
 * formatting outside both the core and the inotify backend.
 * ============================================================================
 */

#include "core_logger.h"
#include "logger.h"

/* ============================================================================
 * Callback Implementation
 * ============================================================================
 */

void core_logger_on_event(const alfred_event_t *ev, void *userdata)
{
    logger_t *logger = userdata;

    if (ev == NULL || logger == NULL)
        return;

    if (ev->dst_path != NULL) {
        logger_event(logger,
                     "core seq=%llu type=%s from=%s to=%s pid=%d",
                     (unsigned long long)ev->seq,
                     alfred_event_name(ev->type),
                     ev->src_path ? ev->src_path : "",
                     ev->dst_path,
                     (int)ev->pid);
        return;
    }

    logger_event(logger,
                 "core seq=%llu type=%s path=%s pid=%d",
                 (unsigned long long)ev->seq,
                 alfred_event_name(ev->type),
                 ev->src_path ? ev->src_path : "",
                 (int)ev->pid);
}
