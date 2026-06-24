/* ============================================================================
 * alfred_record_output_pipeline.h - single-writer output pipeline v0
 *
 * This module composes the already tested record queue, dispatcher, runtime
 * drain helper, and JSONL buffered writer into one small pipeline object. It is
 * still single-threaded. app.c can wire it behind output_enabled=true as an
 * additive JSONL path, while worker threads, per-sink queues, socket output, and
 * real backpressure policy remain future work.
 * ========================================================================== */

#ifndef ALFRED_RECORD_OUTPUT_PIPELINE_H
#define ALFRED_RECORD_OUTPUT_PIPELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "alfred_record.h"
#include "alfred_record_dispatcher.h"
#include "alfred_record_jsonl_writer.h"
#include "alfred_record_queue.h"
#include "alfred_record_runtime.h"

#include <stddef.h>

/*
 * alfred_record_output_pipeline_format_t - writer format supported by pipeline
 *
 * The first pipeline implementation supports only JSONL because the buffered
 * JSONL writer already has a documented and tested contract. Text remains a
 * valid top-level configuration value only for disabled/future configurations;
 * app.c currently accepts JSONL when output_enabled=true.
 */
typedef enum {
    ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_JSONL = 1
} alfred_record_output_pipeline_format_t;

/*
 * alfred_record_output_pipeline_config_t - setup for one output pipeline
 * @enabled: when false, init succeeds without allocating or registering sinks
 * @format: writer format to build when @enabled is true
 * @queue_capacity: maximum number of owned records in the pipeline queue
 * @drain_batch_size: maximum records consumed by one drain call
 * @write: output callback used by the configured writer
 * @userdata: opaque context passed to @write
 * @format_buffer: scratch buffer for formatting one record
 * @format_buffer_size: size of @format_buffer
 * @output_buffer: buffered writer output storage
 * @output_buffer_size: size of @output_buffer
 *
 * Buffers and callback are required only when @enabled is true. The pipeline
 * does not own the buffers; the caller must keep them alive until destroy.
 */
typedef struct {
    int enabled;
    alfred_record_output_pipeline_format_t format;
    size_t queue_capacity;
    size_t drain_batch_size;
    alfred_record_jsonl_writer_write_fn write;
    void *userdata;
    char *format_buffer;
    size_t format_buffer_size;
    char *output_buffer;
    size_t output_buffer_size;
} alfred_record_output_pipeline_config_t;

/*
 * alfred_record_output_pipeline_t - composed queue/dispatcher/writer pipeline
 * @enabled: no-op mode flag copied from configuration
 * @format: active writer format
 * @drain_batch_size: max records consumed by drain_once()
 * @queue: bounded owned-record queue
 * @dispatcher: bounded dispatcher with one sink in v0
 * @sink_storage: embedded storage for the single registered sink
 * @writer: JSONL buffered writer used when @format is JSONL
 *
 * The pipeline owns the queue allocation. It does not own writer buffers,
 * output callbacks, files, sockets, or threads.
 */
typedef struct {
    int enabled;
    alfred_record_output_pipeline_format_t format;
    size_t drain_batch_size;
    alfred_record_queue_t queue;
    alfred_record_dispatcher_t dispatcher;
    alfred_record_dispatcher_sink_t sink_storage[1];
    alfred_record_jsonl_writer_t writer;
} alfred_record_output_pipeline_t;

/*
 * alfred_record_output_pipeline_init - initialize a single-writer pipeline
 * @pipeline: zeroed or previously destroyed pipeline object
 * @config: pipeline setup
 *
 * Disabled configuration is valid and creates a no-op pipeline. Enabled JSONL
 * configuration builds queue -> dispatcher -> JSONL buffered writer. Reusing an
 * active pipeline without destroy is invalid.
 *
 * Return: 0 on success, -1 on invalid input or allocation/setup failure.
 */
int alfred_record_output_pipeline_init(
    alfred_record_output_pipeline_t *pipeline,
    const alfred_record_output_pipeline_config_t *config);

/*
 * alfred_record_output_pipeline_destroy - release owned queue state
 * @pipeline: pipeline initialized by alfred_record_output_pipeline_init()
 *
 * The function does not flush writer bytes. Runtime shutdown policy must decide
 * whether to flush before destroy.
 */
void alfred_record_output_pipeline_destroy(
    alfred_record_output_pipeline_t *pipeline);

/*
 * alfred_record_output_pipeline_enqueue - enqueue one borrowed record
 * @pipeline: initialized pipeline
 * @record: borrowed Event Model v0 record
 *
 * Disabled pipelines are successful no-ops. Enabled pipelines store an owned
 * clone in the queue.
 *
 * Return: 0 on success, -1 if enabled pipeline enqueue fails.
 */
int alfred_record_output_pipeline_enqueue(
    alfred_record_output_pipeline_t *pipeline,
    const alfred_record_t *record);

/*
 * alfred_record_output_pipeline_drain_once - drain one configured batch
 * @pipeline: initialized pipeline
 * @result: optional summary filled like alfred_record_runtime_drain_once()
 *
 * Disabled pipelines fill a zero-result and return success. Enabled pipelines
 * call the runtime drain helper with the configured batch size.
 *
 * Return: 0 on success, -1 on invalid input or first dispatch/write failure.
 */
int alfred_record_output_pipeline_drain_once(
    alfred_record_output_pipeline_t *pipeline,
    alfred_record_runtime_drain_result_t *result);

/*
 * alfred_record_output_pipeline_flush - flush buffered writer bytes
 * @pipeline: initialized pipeline
 *
 * Disabled pipelines are successful no-ops.
 *
 * Return: 0 on success, -1 on invalid input or writer flush failure.
 */
int alfred_record_output_pipeline_flush(
    alfred_record_output_pipeline_t *pipeline);

size_t alfred_record_output_pipeline_pending(
    const alfred_record_output_pipeline_t *pipeline);

size_t alfred_record_output_pipeline_buffered_bytes(
    const alfred_record_output_pipeline_t *pipeline);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_OUTPUT_PIPELINE_H */
