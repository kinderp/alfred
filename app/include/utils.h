/* ============================================================================
 * utils.h - shared application utility helpers
 *
 * Utilities in this header are small, dependency-light helpers used by several
 * application modules. They should remain generic and should not accumulate
 * business logic from the inotify backend or the core correlator.
 * ============================================================================
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

/*
 * iso_timestamp - format the current wall-clock time
 * @buf: destination buffer
 * @len: destination buffer length
 *
 * Writes an ISO-like timestamp suitable for human-readable logs.
 */
void iso_timestamp(char *buf, size_t len);

/*
 * path_join - concatenate a base path and a child name
 * @dst: destination buffer
 * @len: destination buffer length
 * @base: parent path
 * @name: child name
 *
 * Adds exactly one slash between @base and @name when needed.
 *
 * Return: 0 on success, -1 on invalid input or truncation.
 */
int path_join(char *dst,
              size_t len,
              const char *base,
              const char *name);

/*
 * path_trim_slashes - remove trailing slashes from a path in place
 * @path: mutable path string
 *
 * Keeps "/" unchanged.
 */
void path_trim_slashes(char *path);

/*
 * path_basename - return a pointer to the final path component
 * @path: path to inspect
 *
 * Return: pointer inside @path, or NULL when @path is NULL.
 */
const char *path_basename(const char *path);

/*
 * path_dirname - copy the parent directory portion of a path
 * @dst: destination buffer
 * @len: destination buffer length
 * @src: source path
 *
 * Return: 0 on success, -1 on invalid input.
 */
int path_dirname(char *dst,
                 size_t len,
                 const char *src);

/*
 * str_copy - copy a possibly NULL string into a fixed buffer
 * @dst: destination buffer
 * @len: destination buffer length
 * @src: source string, or NULL
 */
void str_copy(char *dst,
              size_t len,
              const char *src);

/*
 * yesno - convert a boolean-like integer to text
 * @value: integer value to convert
 *
 * Return: "yes" when @value is nonzero, otherwise "no".
 */
const char *yesno(int value);

/*
 * filetype_name - convert a directory flag to text
 * @is_dir: nonzero for directory, zero for file
 *
 * Return: "dir" or "file".
 */
const char *filetype_name(int is_dir);

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
              int max);

/*
 * mem_zero - zero a memory region when the pointer is valid
 * @ptr: memory region to clear
 * @size: number of bytes to clear
 */
void mem_zero(void *ptr, size_t size);

#endif
