/* ============================================================================
 * alfred_record_text_sink.c - compatibility text sink for records
 * ========================================================================== */

#include "alfred_record_text_sink.h"

#include "alfred_record_text.h"

int alfred_record_text_sink_init(alfred_record_text_sink_t *text_sink,
                                 alfred_record_sink_t *sink)
{
    if (text_sink == NULL || sink == NULL) {
        return -1;
    }

    if (text_sink->write == NULL ||
        text_sink->buffer == NULL ||
        text_sink->buffer_size == 0u) {
        return -1;
    }

    sink->emit = alfred_record_text_sink_emit;
    sink->userdata = text_sink;
    return 0;
}

int alfred_record_text_sink_emit(void *userdata,
                                 const alfred_record_t *record)
{
    alfred_record_text_sink_t *text_sink = userdata;

    if (text_sink == NULL ||
        text_sink->write == NULL ||
        text_sink->buffer == NULL ||
        text_sink->buffer_size == 0u ||
        record == NULL) {
        return -1;
    }

    if (alfred_record_format_text(record,
                                  text_sink->buffer,
                                  text_sink->buffer_size) < 0) {
        return -1;
    }

    if (text_sink->write(text_sink->userdata, text_sink->buffer) != 0) {
        return -1;
    }

    return 0;
}
