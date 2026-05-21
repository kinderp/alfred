/* ============================================================================
 * alfred_utils.c - internal utility helpers for the core
 *
 * This file keeps small reusable operations out of the correlator: monotonic
 * time, millisecond conversion, hashing, safe string duplication, and path
 * component comparisons. They are internal helpers, not public API.
 * ========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/*
 * alfred_now_ns - return monotonic timestamp in nanoseconds
 *
 * Monotonic time is used for move timeouts, debounce windows, and event
 * ordering because wall-clock time can jump due to NTP, manual changes, DST, or
 * virtualization corrections.
 *
 * Return: nanoseconds from an unspecified monotonic epoch.
 */
uint64_t alfred_now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) +
           (uint64_t)ts.tv_nsec;
}

/*
 * alfred_ms_to_ns - convert milliseconds to nanoseconds
 * @ms: millisecond value
 *
 * Configuration uses milliseconds because they are human-readable. Internal
 * timing uses nanoseconds so all timeout math shares the same unit.
 *
 * Return: @ms expressed in nanoseconds.
 */
uint64_t alfred_ms_to_ns(uint64_t ms)
{
    return ms * 1000000ULL;
}

/*
 * alfred_strdup - duplicate a string for core-owned state
 * @src: string to duplicate
 *
 * The core uses this when it must retain a path after the backend's raw event
 * buffer goes out of scope. Allocation failure currently aborts the process;
 * production daemon policy could later return errors or drop events with
 * metrics instead.
 *
 * Return: allocated copy, or NULL when @src is NULL.
 */
char *alfred_strdup(const char *src)
{
    char *p;

    if (!src)
        return NULL;

    p = strdup(src);

    if (!p) {
        fprintf(stderr, "alfred: out of memory in strdup()\n");
        abort();
    }

    return p;
}

/*
 * alfred_hash_u32 - mix a 32-bit integer for hash table indexing
 * @x: integer key
 *
 * Raw modulo on sequential cookies can cluster badly. This small mixer improves
 * distribution before the bucket mask is applied.
 *
 * Return: mixed hash value.
 */
uint32_t alfred_hash_u32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;

    return x;
}

/*
 * alfred_hash_str - hash a string path for debounce table indexing
 * @s: null-terminated string
 *
 * Uses an FNV-1a style hash as a simple speed/readability tradeoff.
 *
 * Return: hash value.
 */
uint32_t alfred_hash_str(const char *s)
{
    uint32_t h = 2166136261U;

    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619U;
    }

    return h;
}

/*
 * alfred_basename_ptr - return a pointer to the basename portion of a path
 * @path: filesystem path
 *
 * No memory is allocated. The returned pointer refers to storage inside @path.
 *
 * Return: basename pointer, @path itself when no slash exists, or "" for NULL.
 */
const char *alfred_basename_ptr(const char *path)
{
    const char *p;

    if (!path)
        return "";

    p = strrchr(path, '/');

    if (!p)
        return path;

    return p + 1;
}

/*
 * alfred_parent_dir - copy the parent directory of a path
 * @path: filesystem path
 * @out: destination buffer
 * @out_sz: size of @out
 *
 * Used by move classification: same parent plus different basename means
 * rename, different parent plus same basename means move, and both different
 * means relocated.
 */
void alfred_parent_dir(
    const char *path,
    char *out,
    size_t out_sz)
{
    char *slash;

    if (!path || !out || out_sz == 0)
        return;

    /*
     * Copy path safely.
     */
    strncpy(out, path, out_sz - 1);
    out[out_sz - 1] = '\0';

    slash = strrchr(out, '/');

    /*
     * No slash means current directory style path.
     */
    if (!slash) {
        strncpy(out, ".", out_sz);
        return;
    }

    /*
     * Root case:
     * /file.txt -> /
     */
    if (slash == out) {
        out[1] = '\0';
        return;
    }

    /*
     * Truncate at last slash.
     */
    *slash = '\0';
}

/*
 * alfred_same_parent - compare the parent directories of two paths
 * @a: first path
 * @b: second path
 *
 * Return: 1 when parents match, 0 otherwise.
 */
int alfred_same_parent(
    const char *a,
    const char *b)
{
    char pa[4096];
    char pb[4096];

    alfred_parent_dir(a, pa, sizeof(pa));
    alfred_parent_dir(b, pb, sizeof(pb));

    return strcmp(pa, pb) == 0;
}

/*
 * alfred_same_basename - compare only leaf names of two paths
 * @a: first path
 * @b: second path
 *
 * Return: 1 when basenames match, 0 otherwise.
 */
int alfred_same_basename(
    const char *a,
    const char *b)
{
    return strcmp(
        alfred_basename_ptr(a),
        alfred_basename_ptr(b)
    ) == 0;
}

/*
 * alfred_streq - compare two possibly NULL strings
 * @a: first string
 * @b: second string
 *
 * Return: 1 when both strings are equal or both NULL, 0 otherwise.
 */
int alfred_streq(
    const char *a,
    const char *b)
{
    if (a == NULL && b == NULL)
        return 1;

    if (a == NULL || b == NULL)
        return 0;

    return strcmp(a, b) == 0;
}

/*
 * alfred_version_string_impl - return the internal core version string
 *
 * Return: static semantic version string.
 */
const char *alfred_version_string_impl(void)
{
    return "1.0.0";
}
