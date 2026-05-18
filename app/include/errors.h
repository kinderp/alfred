#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    ERR_OK = 0,
    ERR_ALLOC = -1,
    ERR_IO = -2,
    ERR_INOTIFY = -3,
    ERR_CONFIG = -4,
    ERR_INVALID_ARG = -5,
    ERR_UNKNOWN = -1000
} error_t;

#endif
