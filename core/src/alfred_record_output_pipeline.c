/* ============================================================================
 * alfred_record_output_pipeline.c - single-writer output pipeline v0
 * ========================================================================== */

#include "alfred_record_output_pipeline.h"

#include <string.h>

static void fill_disabled_result(alfred_record_runtime_drain_result_t *result)
{
    if (result == NULL) {
        return;
    }

    result->max_records = 0u;
    result->dispatched = 0u;
    result->remaining = 0u;
    result->status = 0;
}

static int config_is_valid_for_jsonl(
    const alfred_record_output_pipeline_config_t *config)
{
    return config != NULL &&
           config->format == ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_JSONL &&
           config->queue_capacity > 0u &&
           config->drain_batch_size > 0u &&
           config->write != NULL &&
           config->format_buffer != NULL &&
           config->format_buffer_size > 0u &&
           config->output_buffer != NULL &&
           config->output_buffer_size > 0u;
}

static int config_is_valid_for_counter(
    const alfred_record_output_pipeline_config_t *config)
{
    return config != NULL &&
           config->format == ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_COUNTER &&
           config->queue_capacity > 0u &&
           config->drain_batch_size > 0u;
}

int alfred_record_output_pipeline_init(
    alfred_record_output_pipeline_t *pipeline,
    const alfred_record_output_pipeline_config_t *config)
{
    alfred_record_sink_t sink;

    if (pipeline == NULL || config == NULL) {
        return -1;
    }

    if (pipeline->queue.items != NULL) {
        return -1;
    }

    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->enabled = config->enabled ? 1 : 0;
    pipeline->format = config->format;

    if (!pipeline->enabled) {
        return 0;
    }

    if (!config_is_valid_for_jsonl(config) &&
        !config_is_valid_for_counter(config)) {
        memset(pipeline, 0, sizeof(*pipeline));
        return -1;
    }

    pipeline->drain_batch_size = config->drain_batch_size;

    if (alfred_record_queue_init(&pipeline->queue,
                                 config->queue_capacity) != 0) {
        memset(pipeline, 0, sizeof(*pipeline));
        return -1;
    }

    if (alfred_record_dispatcher_init(&pipeline->dispatcher,
                                      pipeline->sink_storage,
                                      1u) != 0) {
        alfred_record_output_pipeline_destroy(pipeline);
        return -1;
    }

    if (config->format == ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_COUNTER) {
        if (alfred_record_counter_sink_init(&pipeline->counter, &sink) != 0) {
            alfred_record_output_pipeline_destroy(pipeline);
            return -1;
        }

        if (alfred_record_dispatcher_add_sink(
                &pipeline->dispatcher,
                "counter",
                ALFRED_RECORD_DISPATCHER_SINK_DEBUG,
                &sink) != 0) {
            alfred_record_output_pipeline_destroy(pipeline);
            return -1;
        }

        return 0;
    }

    pipeline->writer.write = config->write;
    pipeline->writer.userdata = config->userdata;
    pipeline->writer.format_buffer = config->format_buffer;
    pipeline->writer.format_buffer_size = config->format_buffer_size;
    pipeline->writer.buffer = config->output_buffer;
    pipeline->writer.buffer_size = config->output_buffer_size;

    if (alfred_record_jsonl_writer_init(&pipeline->writer) != 0) {
        alfred_record_output_pipeline_destroy(pipeline);
        return -1;
    }

    if (alfred_record_jsonl_writer_init_sink(&pipeline->writer, &sink) != 0) {
        alfred_record_output_pipeline_destroy(pipeline);
        return -1;
    }

    if (alfred_record_dispatcher_add_sink(
            &pipeline->dispatcher,
            "jsonl",
            ALFRED_RECORD_DISPATCHER_SINK_CRITICAL,
            &sink) != 0) {
        alfred_record_output_pipeline_destroy(pipeline);
        return -1;
    }

    return 0;
}

void alfred_record_output_pipeline_destroy(
    alfred_record_output_pipeline_t *pipeline)
{
    if (pipeline == NULL) {
        return;
    }

    alfred_record_queue_destroy(&pipeline->queue);
    memset(pipeline, 0, sizeof(*pipeline));
}

int alfred_record_output_pipeline_enqueue(
    alfred_record_output_pipeline_t *pipeline,
    const alfred_record_t *record)
{
    if (pipeline == NULL) {
        return -1;
    }

    if (!pipeline->enabled) {
        return 0;
    }

    return alfred_record_queue_push(&pipeline->queue, record);
}

int alfred_record_output_pipeline_drain_once(
    alfred_record_output_pipeline_t *pipeline,
    alfred_record_runtime_drain_result_t *result)
{
    if (pipeline == NULL) {
        if (result != NULL) {
            result->max_records = 0u;
            result->dispatched = 0u;
            result->remaining = 0u;
            result->status = -1;
        }
        return -1;
    }

    if (!pipeline->enabled) {
        fill_disabled_result(result);
        return 0;
    }

    return alfred_record_runtime_drain_once(&pipeline->dispatcher,
                                           &pipeline->queue,
                                           pipeline->drain_batch_size,
                                           result);
}

int alfred_record_output_pipeline_flush(
    alfred_record_output_pipeline_t *pipeline)
{
    if (pipeline == NULL) {
        return -1;
    }

    if (!pipeline->enabled) {
        return 0;
    }

    if (pipeline->format == ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_COUNTER) {
        return 0;
    }

    return alfred_record_jsonl_writer_flush(&pipeline->writer);
}

size_t alfred_record_output_pipeline_pending(
    const alfred_record_output_pipeline_t *pipeline)
{
    if (pipeline == NULL || !pipeline->enabled) {
        return 0u;
    }

    return alfred_record_queue_count(&pipeline->queue);
}

size_t alfred_record_output_pipeline_buffered_bytes(
    const alfred_record_output_pipeline_t *pipeline)
{
    if (pipeline == NULL || !pipeline->enabled) {
        return 0u;
    }

    if (pipeline->format == ALFRED_RECORD_OUTPUT_PIPELINE_FORMAT_COUNTER) {
        return 0u;
    }

    return alfred_record_jsonl_writer_buffered_size(&pipeline->writer);
}
