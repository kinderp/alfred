/* ============================================================================
 * utils.c
 * Shared utility helpers
 *
 * Responsibilities:
 *   - ISO8601 timestamps
 *   - safe path join
 *   - path normalization helpers
 *   - tiny reusable generic functions
 *
 * Used by:
 *   logger.c
 *   app.c
 *   event_engine.c
 * ========================================================================== */

#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>

/* ============================================================================
 * ISO TIMESTAMP
 *
 * Example:
 *   2026-05-11T14:55:21.123+0200
 * ========================================================================== */

void iso_timestamp(char *buf, size_t len)
{
    if (buf == NULL || len == 0)
        return;

    struct timeval tv;
    struct tm tmv;

    gettimeofday(&tv, NULL);

    localtime_r(&tv.tv_sec, &tmv);

    char date_part[32];
    char zone_part[8];

    strftime(date_part,
             sizeof(date_part),
             "%Y-%m-%dT%H:%M:%S",
             &tmv);

    strftime(zone_part,
             sizeof(zone_part),
             "%z",
             &tmv);

    snprintf(buf,
             len,
             "%s.%03ld%s",
             date_part,
             tv.tv_usec / 1000,
             zone_part);
}

/* ============================================================================
 * SAFE PATH JOIN
 *
 * Joins:
 *   /tmp + file.txt => /tmp/file.txt
 *
 * Returns:
 *   0 success
 *  -1 overflow
 * ========================================================================== */

int path_join(char *dst,
              size_t len,
              const char *base,
              const char *name)
{
    if (dst == NULL ||
        base == NULL ||
        name == NULL ||
        len == 0)
        return -1;

    size_t blen = strlen(base);

    if (blen == 0)
        return -1;

    int need_slash =
        (base[blen - 1] != '/');

    int written;

    if (need_slash) {
        written = snprintf(dst,
                           len,
                           "%s/%s",
                           base,
                           name);
    } else {
        written = snprintf(dst,
                           len,
                           "%s%s",
                           base,
                           name);
    }

    if (written < 0 ||
        (size_t)written >= len)
        return -1;

    return 0;
}

/* ============================================================================
 * NORMALIZE TRAILING SLASH
 *
 * "/tmp///" -> "/tmp"
 * except "/" remains "/"
 * ========================================================================== */

void path_trim_slashes(char *path)
{
    if (path == NULL)
        return;

    size_t n = strlen(path);

    if (n == 0)
        return;

    while (n > 1 && path[n - 1] == '/') {
        path[n - 1] = '\0';
        n--;
    }
}

/* ============================================================================
 * BASENAME POINTER
 *
 * "/tmp/a/b.txt" -> "b.txt"
 * ========================================================================== */

const char* path_basename(const char *path)
{
    if (path == NULL)
        return NULL;

    const char *p = strrchr(path, '/');

    if (p == NULL)
        return path;

    return p + 1;
}

/* ============================================================================
 * DIRECTORYNAME COPY
 *
 * "/tmp/a/b.txt" -> "/tmp/a"
 * ========================================================================== */

int path_dirname(char *dst,
                 size_t len,
                 const char *src)
{
    if (dst == NULL ||
        src == NULL ||
        len == 0)
        return -1;

    snprintf(dst, len, "%s", src);

    char *p = strrchr(dst, '/');

    if (p == NULL) {
        snprintf(dst, len, ".");
        return 0;
    }

    if (p == dst) {
        dst[1] = '\0'; /* root "/" */
        return 0;
    }

    *p = '\0';

    return 0;
}

/* ============================================================================
 * STRING SAFE COPY
 * ========================================================================== */

void str_copy(char *dst,
              size_t len,
              const char *src)
{
    if (dst == NULL || len == 0)
        return;

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, len, "%s", src);
}

/* ============================================================================
 * BOOL TEXT
 * ========================================================================== */

const char* yesno(int value)
{
    return value ? "yes" : "no";
}

/* ============================================================================
 * FILETYPE FROM MASK
 *
 * is_dir=1 => "dir"
 * else     => "file"
 * ========================================================================== */

const char* filetype_name(int is_dir)
{
    return is_dir ? "dir" : "file";
}

/* ============================================================================
 * CLAMP INT
 * ========================================================================== */

int clamp_int(int value,
              int min,
              int max)
{
    if (value < min)
        return min;

    if (value > max)
        return max;

    return value;
}

/* ============================================================================
 * ZERO MEMORY WRAPPER
 * ========================================================================== */

void mem_zero(void *ptr, size_t size)
{
    if (ptr == NULL)
        return;

    memset(ptr, 0, size);
}
