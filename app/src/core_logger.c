/* ============================================================================
 * core_logger.c - core semantic event logging callback
 *
 * The core emits alfred_event_t values through a callback. This file adapts
 * that callback interface to the application logger, keeping core output
 * formatting outside both the core and the inotify backend.
 * ============================================================================
 */

#include "core_logger.h"

#include "alfred_record_adapter.h"
#include "alfred_record_text.h"

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

/*
 * log_record_event - format a semantic event through Event Model v0 records
 * @logger: destination application logger
 * @ev: semantic event emitted by the core
 *
 * This is the first runtime use of alfred_record_t on the core output side.
 * The output must remain byte-for-byte compatible with the old event payloads,
 * so tests still see FILE_CREATED path=... and FILE_RENAMED from=... to=....
 *
 * Return: 0 on success, -1 when conversion or formatting fails.
 */
static int log_record_event(logger_t *logger, const alfred_event_t *ev)
{
    alfred_record_t record;
    char payload[1024];

    if (alfred_record_from_event(ev, &record) != 0) {
        return -1;
    }

    if (alfred_record_format_text(&record, payload, sizeof(payload)) < 0) {
        return -1;
    }

    logger_event(logger, "%s", payload);

    return 0;
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

    if (log_record_event(context->logger, ev) != 0) {
        log_plain_event(context->logger, ev);
    }
}
