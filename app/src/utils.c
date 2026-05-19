/* ============================================================================
 * utils.c - shared application utility helpers
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
 *   modules/inotify event handling
 * ========================================================================== */

#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <sys/inotify.h>

/* ============================================================================
 * Time Formatting
 * ========================================================================== */

/*
 * iso_timestamp - format the current wall-clock time
 * @buf: destination buffer
 * @len: destination buffer length
 *
 * Produces an ISO-like timestamp with millisecond precision and timezone
 * offset. The timestamp is intended for logs, not for monotonic timing.
 *
 * Example:
 *   2026-05-11T14:55:21.123+0200
 */
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
 * Path Helpers
 * ========================================================================== */

/*
 * path_join - concatenate a base path and child name
 * @dst: destination buffer
 * @len: destination buffer length
 * @base: parent path
 * @name: child name
 *
 * Joins `/tmp` and `file.txt` as `/tmp/file.txt`. If @base already ends in
 * '/', no extra slash is inserted.
 *
 * Return: 0 on success, -1 on invalid input or truncation.
 */
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

/*
 * path_trim_slashes - remove trailing slashes from a path in place
 * @path: mutable path string
 *
 * Converts `/tmp///` to `/tmp` while preserving the root path `/`.
 */
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

/*
 * path_basename - return the final path component
 * @path: path to inspect
 *
 * Return: pointer inside @path, or NULL when @path is NULL.
 */
const char *path_basename(const char *path)
{
    if (path == NULL)
        return NULL;

    const char *p = strrchr(path, '/');

    if (p == NULL)
        return path;

    return p + 1;
}

/*
 * path_dirname - copy a path's parent directory
 * @dst: destination buffer
 * @len: destination buffer length
 * @src: source path
 *
 * Copies `/tmp/a` when @src is `/tmp/a/b.txt`. Paths without a slash use ".".
 *
 * Return: 0 on success, -1 on invalid input.
 */
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
 * Generic Helpers
 * ========================================================================== */

/*
 * str_copy - copy a possibly NULL string into a fixed buffer
 * @dst: destination buffer
 * @len: destination buffer length
 * @src: source string, or NULL
 *
 * NULL input is converted to an empty string.
 */
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

/*
 * yesno - convert a boolean-like integer to text
 * @value: integer value to convert
 *
 * Return: "yes" when @value is nonzero, otherwise "no".
 */
const char *yesno(int value)
{
    return value ? "yes" : "no";
}

/*
 * filetype_name - convert a directory flag to text
 * @is_dir: nonzero for directory, zero for file
 *
 * Return: "dir" or "file".
 */
const char *filetype_name(int is_dir)
{
    return is_dir ? "dir" : "file";
}

/*
 * clamp_int - clamp an integer to a closed range
 * @value: input value
 * @min: minimum returned value
 * @max: maximum returned value
 *
 * Return: @value limited to [@min, @max].
 */
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

/*
 * mem_zero - zero a memory region when the pointer is valid
 * @ptr: memory region to clear
 * @size: number of bytes to clear
 */
void mem_zero(void *ptr, size_t size)
{
    if (ptr == NULL)
        return;

    memset(ptr, 0, size);
}

/* ============================================================================
 * inotify Formatting
 * ========================================================================== */

/*
 * raw_event_name_from_mask - render an inotify mask as text
 * @mask: inotify event mask
 * @dest: destination buffer
 * @dest_size: destination buffer length
 *
 * Appends known inotify flag names into @dest. Unknown masks are rendered as
 * "UNRECOGNIZED".
 *
 * TODO(core-integration): this helper belongs in modules/inotify because it is
 * specific to Linux inotify masks.
 */
void raw_event_name_from_mask(uint32_t mask,
                              char *dest,
                              size_t dest_size)
{
    if (dest == NULL || dest_size == 0)
        return;

    dest[0] = '\0';

    if (mask & IN_CREATE)
        strncat(dest, "IN_CREATE ", dest_size - strlen(dest) - 1);
    if (mask & IN_DELETE)
        strncat(dest, "IN_DELETE ", dest_size - strlen(dest) - 1);
    if (mask & IN_MOVED_FROM)
        strncat(dest, "IN_MOVED_FROM ", dest_size - strlen(dest) - 1);
    if (mask & IN_MOVED_TO)
        strncat(dest, "IN_MOVED_TO ", dest_size - strlen(dest) - 1);
    if (mask & IN_ISDIR)
        strncat(dest, "IN_ISDIR ", dest_size - strlen(dest) - 1);
    if (mask & IN_DELETE_SELF)
        strncat(dest, "IN_DELETE_SELF ", dest_size - strlen(dest) - 1);
    if (mask & IN_IGNORED)
        strncat(dest, "IN_IGNORED ", dest_size - strlen(dest) - 1);
    if (mask & IN_Q_OVERFLOW)
        strncat(dest, "IN_Q_OVERFLOW ", dest_size - strlen(dest) - 1);

    if (dest[0] == '\0') {
        strncpy(dest, "UNRECOGNIZED", dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
}
