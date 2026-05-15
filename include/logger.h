#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef struct {
    FILE *raw;
    FILE *event;
    FILE *error;
} logger_t;

int logger_init(logger_t *lg,
                const char *raw_path,
                const char *event_path,
                const char *error_path);

void logger_close(logger_t *lg);

void logger_info(logger_t *lg, const char *fmt, ...);
void logger_error(logger_t *lg, const char *fmt, ...);
void logger_raw(logger_t *lg, const char *fmt, ...);
void logger_event(logger_t *lg, const char *fmt, ...);

#endif
