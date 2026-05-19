/* ============================================================================
 * logger.c
 * Professional logging subsystem
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
 * INTERNAL HELPERS
 * ========================================================================== */

/*
 * Open file append mode.
 */
static FILE* open_log_file(const char *path)
{
    if (path == NULL)
        return NULL;

    return fopen(path, "a");
}

/*
 * Generic formatted writer.
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
 * PUBLIC API
 * ========================================================================== */

/*
 * Initialize all log streams.
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
 * Close all streams.
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
 * INFO
 * ========================================================================== */

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

/* ============================================================================
 * ERROR
 * ========================================================================== */

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

/* ============================================================================
 * RAW
 * ========================================================================== */

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

/* ============================================================================
 * EVENT
 * semantic filesystem event
 * ========================================================================== */

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
 * OPTIONAL HELPERS
 * ========================================================================== */

/*
 * Flush immediately all streams.
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
 * Banner startup.
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
