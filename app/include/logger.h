/* ============================================================================
 * logger.h - multi-stream logging API
 *
 * The logger centralizes all file output used by the application. Separate
 * streams keep raw backend events, semantic events, and diagnostics readable
 * during debugging and tests.
 * ============================================================================
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

/*
 * logger_t - owned log stream set
 *
 * Each field is a FILE stream opened by logger_init() and closed by
 * logger_close(). Callers should treat the fields as owned by the logger and
 * should not fclose() them directly.
 */
typedef struct {
    FILE *raw;
    FILE *event;
    FILE *error;
} logger_t;

/*
 * logger_init - open all log streams
 * @lg: logger object to initialize
 * @raw_path: path for raw backend event logs
 * @event_path: path for semantic event and info logs
 * @error_path: path for errors and diagnostics
 *
 * Opens all streams in append mode. If any stream fails to open, already-opened
 * streams are closed before returning.
 *
 * Return: 0 on success, -1 on invalid input or I/O failure.
 */
int logger_init(logger_t *lg,
                const char *raw_path,
                const char *event_path,
                const char *error_path);

/*
 * logger_close - close all streams owned by a logger
 * @lg: logger object to close
 *
 * Safe to call with NULL. Stream pointers are reset after close.
 */
void logger_close(logger_t *lg);

/*
 * logger_info - write an informational message to the event log
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_info(logger_t *lg, const char *fmt, ...);

/*
 * logger_error - write an error message to the error log
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_error(logger_t *lg, const char *fmt, ...);

/*
 * logger_raw - write a raw backend event message to the raw log
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_raw(logger_t *lg, const char *fmt, ...);

/*
 * logger_event - write a semantic event message to the event log
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_event(logger_t *lg, const char *fmt, ...);

/*
 * logger_flush - flush all open log streams
 * @lg: initialized logger
 */
void logger_flush(logger_t *lg);

/*
 * logger_banner - write a startup banner to the event log
 * @lg: initialized logger
 */
void logger_banner(logger_t *lg);

#endif
