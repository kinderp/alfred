/* ============================================================================
 * core_logger.c - core semantic event logging callback
 *
 * The core emits alfred_event_t values through a callback. This file adapts
 * that callback interface to the application logger, keeping core output
 * formatting outside both the core and the inotify backend.
 * ============================================================================
 */

#include "core_logger.h"
/* ============================================================================
 * Local Formatting Helpers
 * ============================================================================
 */

static void log_shadow_event(logger_t *logger, const alfred_event_t *ev)
{
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

static void log_plain_event(logger_t *logger, const alfred_event_t *ev)
{
    if (ev->dst_path != NULL) {
        logger_event(logger,
                     "%s from=%s to=%s",
                     alfred_event_name(ev->type),
                     ev->src_path ? ev->src_path : "",
                     ev->dst_path);
        return;
    }

    logger_event(logger,
                 "%s path=%s",
                 alfred_event_name(ev->type),
                 ev->src_path ? ev->src_path : "");
}

/* ============================================================================
 * Callback Implementation
 * ============================================================================
 */

void core_logger_on_event(const alfred_event_t *ev, void *userdata)
{
    core_logger_context_t *context = userdata;

    if (ev == NULL || context == NULL || context->logger == NULL)
        return;

    if (context->event_engine_mode == EVENT_ENGINE_CORE) {
        log_plain_event(context->logger, ev);
        return;
    }

    log_shadow_event(context->logger, ev);
}
