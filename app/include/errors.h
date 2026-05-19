/* ============================================================================
 * errors.h - application error codes
 *
 * Public lifecycle functions return these values to describe high-level failure
 * categories. The codes are intentionally small and negative so ERR_OK remains
 * the only success value.
 * ============================================================================
 */

#ifndef ERRORS_H
#define ERRORS_H

/*
 * error_t - high-level application result code
 *
 * These values describe the failure category, not the full low-level cause.
 * Detailed diagnostics should be written through the logger when available.
 */
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
