/* ============================================================================
 * alfred_record_sink.h - common sink boundary for Event Model v0 records
 *
 * A sink is the first small runtime boundary after alfred_record_t. Producers
 * emit structured records; sinks decide how those records are delivered to a
 * concrete writer or bridge. This keeps JSONL, text logs, binary sockets, and
 * future policy consumers behind the same record contract.
 * ========================================================================== */

#ifndef ALFRED_RECORD_SINK_H
#define ALFRED_RECORD_SINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"

/*
 * alfred_record_sink_emit_fn - emit one structured record to a sink
 * @userdata: caller-provided sink context
 * @record: borrowed Event Model v0 record
 *
 * Ownership rule:
 *   The sink must treat @record and all borrowed string fields as valid only
 *   during the call. A sink that buffers, queues, or retries output must copy
 *   every field it needs to keep.
 *
 * Return: 0 on success, -1 on invalid input or output failure.
 */
typedef int (*alfred_record_sink_emit_fn)(void *userdata,
                                          const alfred_record_t *record);

/*
 * alfred_record_sink_t - generic record consumer handle
 * @emit: function that receives one record
 * @userdata: opaque sink context passed to @emit
 *
 * This structure is intentionally tiny. It is a dispatch boundary, not a
 * writer implementation. A text sink, JSONL sink, binary sink, or test sink can
 * all implement the same shape.
 */
typedef struct {
    alfred_record_sink_emit_fn emit;
    void *userdata;
} alfred_record_sink_t;

/*
 * alfred_record_sink_emit - defensive wrapper around sink->emit()
 * @sink: sink to call
 * @record: borrowed Event Model v0 record
 *
 * Return: 0 on success, -1 if @sink, @sink->emit, or @record is NULL, or if
 * the sink implementation reports failure.
 */
int alfred_record_sink_emit(const alfred_record_sink_t *sink,
                            const alfred_record_t *record);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_SINK_H */
