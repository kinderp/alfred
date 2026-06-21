/* ============================================================================
 * alfred_record_sink.c - common sink boundary for Event Model v0 records
 * ========================================================================== */

#include "alfred_record_sink.h"

int alfred_record_sink_emit(const alfred_record_sink_t *sink,
                            const alfred_record_t *record)
{
    if (sink == NULL || sink->emit == NULL || record == NULL) {
        return -1;
    }

    return sink->emit(sink->userdata, record);
}
