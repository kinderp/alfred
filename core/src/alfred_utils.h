/* ============================================================================
 * alfred_utils.h - internal utility helpers for the core
 *
 * These helpers are not part of the public Alfred API. They support time
 * calculations, hashing, string duplication, and path comparisons used by the
 * correlator and internal state tables.
 * ========================================================================== */

#ifndef ALFRED_UTILS_H
#define ALFRED_UTILS_H

#include <stddef.h>
#include <stdint.h>

/*
 * alfred_now_ns - return monotonic time in nanoseconds
 *
 * Return: nanoseconds from an unspecified monotonic epoch.
 */
uint64_t alfred_now_ns(void);

/*
 * alfred_ms_to_ns - convert milliseconds to nanoseconds
 * @ms: millisecond value
 *
 * Return: @ms expressed in nanoseconds.
 */
uint64_t alfred_ms_to_ns(uint64_t ms);

/*
 * alfred_strdup - duplicate a string or abort on allocation failure
 * @src: string to duplicate
 *
 * Return: newly allocated string, or NULL when @src is NULL.
 */
char *alfred_strdup(const char *src);

/*
 * alfred_hash_u32 - mix a 32-bit integer for hash table indexing
 * @x: integer key
 *
 * Return: mixed hash value.
 */
uint32_t alfred_hash_u32(uint32_t x);

/*
 * alfred_hash_str - hash a string path for table indexing
 * @s: null-terminated string
 *
 * Return: hash value.
 */
uint32_t alfred_hash_str(const char *s);

/*
 * alfred_basename_ptr - return a pointer to the basename inside a path
 * @path: filesystem path
 *
 * Return: pointer inside @path, or an empty string for NULL.
 */
const char *alfred_basename_ptr(const char *path);

/*
 * alfred_parent_dir - copy a path's parent directory into a buffer
 * @path: filesystem path
 * @out: destination buffer
 * @out_sz: size of @out
 */
void alfred_parent_dir(const char *path, char *out, size_t out_sz);

/*
 * alfred_same_parent - compare parent directories
 * @a: first path
 * @b: second path
 *
 * Return: 1 when both paths have the same parent, 0 otherwise.
 */
int alfred_same_parent(const char *a, const char *b);

/*
 * alfred_same_basename - compare basename portions of two paths
 * @a: first path
 * @b: second path
 *
 * Return: 1 when basenames match, 0 otherwise.
 */
int alfred_same_basename(const char *a, const char *b);

/*
 * alfred_streq - compare two possibly NULL strings
 * @a: first string
 * @b: second string
 *
 * Return: 1 when equal, 0 otherwise.
 */
int alfred_streq(const char *a, const char *b);

/*
 * alfred_version_string_impl - internal version string helper
 *
 * Return: static semantic version string.
 */
const char *alfred_version_string_impl(void);

#endif /* ALFRED_UTILS_H */
