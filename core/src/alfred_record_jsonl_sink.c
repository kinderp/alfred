/* ============================================================================
 * alfred_record_jsonl_sink.c - JSONL sink for Event Model v0 records
 * ========================================================================== */

#include "alfred_record_jsonl_sink.h"

#include "alfred_record_jsonl.h"

int alfred_record_jsonl_sink_init(alfred_record_jsonl_sink_t *jsonl_sink,
                                  alfred_record_sink_t *sink)
{
    if (jsonl_sink == NULL || sink == NULL) {
        return -1;
    }

    if (jsonl_sink->write == NULL ||
        jsonl_sink->buffer == NULL ||
        jsonl_sink->buffer_size == 0u) {
        return -1;
    }

    sink->emit = alfred_record_jsonl_sink_emit;
    sink->userdata = jsonl_sink;
    return 0;
}

int alfred_record_jsonl_sink_emit(void *userdata,
                                  const alfred_record_t *record)
{
    alfred_record_jsonl_sink_t *jsonl_sink = userdata;

    if (jsonl_sink == NULL ||
        jsonl_sink->write == NULL ||
        jsonl_sink->buffer == NULL ||
        jsonl_sink->buffer_size == 0u ||
        record == NULL) {
        return -1;
    }

    if (alfred_record_format_jsonl(record,
                                   jsonl_sink->buffer,
                                   jsonl_sink->buffer_size) < 0) {
        return -1;
    }

    if (jsonl_sink->write(jsonl_sink->userdata, jsonl_sink->buffer) != 0) {
        return -1;
    }

    return 0;
}
