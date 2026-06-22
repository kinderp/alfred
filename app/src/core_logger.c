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
#include "alfred_record_sink.h"
#include "alfred_record_text_sink.h"

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
 * write_event_payload - bridge a text-sink payload to the application logger
 * @userdata: logger_t destination
 * @payload: formatted Event Model v0 payload
 *
 * The text sink deliberately does not know logger_t. This tiny adapter keeps
 * logger ownership in the application layer while allowing core_logger to use
 * the generic record -> sink -> writer shape.
 *
 * Return: 0 on success, -1 on invalid input.
 */
static int write_event_payload(void *userdata, const char *payload)
{
    logger_t *logger = userdata;

    if (logger == NULL || payload == NULL) {
        return -1;
    }

    logger_event(logger, "%s", payload);
    return 0;
}

/*
 * log_record_event - emit a semantic event through Event Model v0 records
 * @logger: destination application logger
 * @ev: semantic event emitted by the core
 *
 * This is the first runtime use of the record sink boundary on the core output
 * side. The semantic event is converted to alfred_record_t, emitted to a text
 * sink, formatted as the same payload as before, and finally bridged back to
 * logger_event(). The output must remain byte-for-byte compatible with the old
 * event payloads, so tests still see FILE_CREATED path=... and FILE_RENAMED
 * from=... to=....
 *
 * Return: 0 on success, -1 when conversion, sink setup, formatting, or logging
 * bridge fails.
 */
static int log_record_event(logger_t *logger, const alfred_event_t *ev)
{
    alfred_record_t record;
    alfred_record_text_sink_t text_sink;
    alfred_record_sink_t sink;
    char payload[1024];

    if (alfred_record_from_event(ev, &record) != 0) {
        return -1;
    }

    text_sink.write = write_event_payload;
    text_sink.userdata = logger;
    text_sink.buffer = payload;
    text_sink.buffer_size = sizeof(payload);

    if (alfred_record_text_sink_init(&text_sink, &sink) != 0) {
        return -1;
    }

    if (alfred_record_sink_emit(&sink, &record) != 0) {
        return -1;
    }

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
