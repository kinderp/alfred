/* ============================================================================
 * logger.c - multi-stream logging subsystem
 *
 * Streams:
 *   raw.log    -> raw kernel/inotify messages
 *   event.log  -> semantic high-level events
 *   error.log  -> errors / warnings / diagnostics
 *
 * Design goals:
 *   - centralized logging
 *   - timestamps ISO8601
 *   - fflush for crash visibility
 *   - variadic printf-style APIs
 * ========================================================================== */

#include "logger.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================================
 * Internal Helpers
 * ========================================================================== */

/*
 * open_log_file - open one log stream in append mode
 * @path: file path to open
 *
 * Append mode preserves previous runs, which is useful during manual testing.
 *
 * Return: opened FILE stream on success, NULL on invalid input or I/O failure.
 */
static FILE *open_log_file(const char *path)
{
    if (path == NULL)
        return NULL;

    return fopen(path, "a");
}

/*
 * write_line - write one timestamped log record
 * @fp: destination stream
 * @level: textual log level
 * @fmt: printf-style format string
 * @ap: variadic argument list matching @fmt
 *
 * Every record is flushed immediately so logs remain useful when the process
 * crashes during development or tests.
 */
static void write_line(FILE *fp,
                       const char *level,
                       const char *fmt,
                       va_list ap)
{
    if (fp == NULL || fmt == NULL)
        return;

    char ts[64];

    iso_timestamp(ts, sizeof(ts));

    fprintf(fp, "[%s] [%s] ", ts, level);

    vfprintf(fp, fmt, ap);

    fprintf(fp, "\n");

    fflush(fp);
}

/* ============================================================================
 * Public API
 * ========================================================================== */

/*
 * logger_init - open all log streams
 * @lg: logger object to initialize
 * @raw_path: raw event log path
 * @event_path: semantic event and info log path
 * @error_path: error log path
 *
 * Opens streams in dependency order and unwinds partially opened resources on
 * failure. The logger owns all successfully opened streams.
 *
 * Return: 0 on success, -1 on invalid input or I/O failure.
 */
int logger_init(logger_t *lg,
                const char *raw_path,
                const char *event_path,
                const char *error_path)
{
    if (lg == NULL)
        return -1;

    memset(lg, 0, sizeof(*lg));

    lg->raw = open_log_file(raw_path);
    if (lg->raw == NULL)
        return -1;

    lg->event = open_log_file(event_path);
    if (lg->event == NULL) {
        fclose(lg->raw);
        lg->raw = NULL;
        return -1;
    }

    lg->error = open_log_file(error_path);
    if (lg->error == NULL) {
        fclose(lg->raw);
        fclose(lg->event);

        lg->raw   = NULL;
        lg->event = NULL;

        return -1;
    }

    logger_info(lg, "logger initialized");

    return 0;
}

/*
 * logger_close - close all streams owned by a logger
 * @lg: logger object to close
 *
 * Safe to call with NULL. All stream pointers are reset after close so repeated
 * shutdown paths do not reuse stale FILE pointers.
 */
void logger_close(logger_t *lg)
{
    if (lg == NULL)
        return;

    if (lg->raw) {
        fclose(lg->raw);
        lg->raw = NULL;
    }

    if (lg->event) {
        fclose(lg->event);
        lg->event = NULL;
    }

    if (lg->error) {
        fclose(lg->error);
        lg->error = NULL;
    }
}

/* ============================================================================
 * Log Writers
 * ========================================================================== */

/*
 * logger_info - write an informational message
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_info(logger_t *lg,
                 const char *fmt,
                 ...)
{
    if (lg == NULL)
        return;

    va_list ap;
    va_start(ap, fmt);

    write_line(lg->event, "INFO", fmt, ap);

    va_end(ap);
}

/*
 * logger_error - write an error message
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_error(logger_t *lg,
                  const char *fmt,
                  ...)
{
    if (lg == NULL)
        return;

    va_list ap;
    va_start(ap, fmt);

    write_line(lg->error, "ERROR", fmt, ap);

    va_end(ap);
}

/*
 * logger_raw - write a raw backend event message
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_raw(logger_t *lg,
                const char *fmt,
                ...)
{
    if (lg == NULL)
        return;

    va_list ap;
    va_start(ap, fmt);

    write_line(lg->raw, "RAW", fmt, ap);

    va_end(ap);
}

/*
 * logger_event - write a semantic filesystem event message
 * @lg: initialized logger
 * @fmt: printf-style format string
 */
void logger_event(logger_t *lg,
                  const char *fmt,
                  ...)
{
    if (lg == NULL)
        return;

    va_list ap;
    va_start(ap, fmt);

    write_line(lg->event, "EVENT", fmt, ap);

    va_end(ap);
}

/* ============================================================================
 * Optional Helpers
 * ========================================================================== */

/*
 * logger_flush - flush all open streams
 * @lg: initialized logger
 *
 * This is useful before controlled shutdown or before tests inspect logs.
 */
void logger_flush(logger_t *lg)
{
    if (lg == NULL)
        return;

    if (lg->raw)
        fflush(lg->raw);

    if (lg->event)
        fflush(lg->event);

    if (lg->error)
        fflush(lg->error);
}

/*
 * logger_banner - write a startup banner to the event log
 * @lg: initialized logger
 */
void logger_banner(logger_t *lg)
{
    if (lg == NULL)
        return;

    logger_info(lg,
        "==================================================");

    logger_info(lg,
        "Alfred Filesystem Event Engine Started");

    logger_info(lg,
        "==================================================");
}
