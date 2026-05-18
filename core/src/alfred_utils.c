/*======================================================================
 *
 * FILE: src/alfred_utils.c
 *
 * INTERNAL UTILITY FUNCTIONS
 *
 * This file contains small reusable helpers used by the engine.
 *
 * Why isolate utilities:
 *   - cleaner core logic
 *   - easier testing
 *   - easier portability
 *   - lower cognitive load
 *
 * NOTE:
 *   These are INTERNAL functions.
 *   They are not exposed in the public API.
 *
 *======================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/*======================================================================
 * alfred_now_ns()
 *
 * PURPOSE:
 *   Return monotonic timestamp in nanoseconds.
 *
 * WHY MONOTONIC CLOCK:
 *
 *   Wrong:
 *      wall clock time
 *
 *   Because wall clock may jump due to:
 *      - NTP sync
 *      - manual user clock change
 *      - DST changes
 *      - virtualization drift correction
 *
 *   Correct:
 *      monotonic time only increases.
 *
 * USED FOR:
 *   - move timeout logic
 *   - debounce timing
 *   - event ordering
 *
 * RETURNS:
 *   nanoseconds since unspecified boot-relative epoch
 *
 *======================================================================*/
uint64_t alfred_now_ns(void)
{
    struct timespec ts;

    /*
     * CLOCK_MONOTONIC:
     *   stable increasing timer.
     */
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) +
           (uint64_t)ts.tv_nsec;
}

/*======================================================================
 * alfred_ms_to_ns()
 *
 * PURPOSE:
 *   Convert milliseconds to nanoseconds.
 *
 * WHY:
 *   Human config uses ms.
 *   Internal engine uses ns.
 *
 * EXAMPLE:
 *   250 ms -> 250000000 ns
 *
 *======================================================================*/
uint64_t alfred_ms_to_ns(uint64_t ms)
{
    return ms * 1000000ULL;
}

/*======================================================================
 * alfred_strdup()
 *
 * PURPOSE:
 *   Safe strdup wrapper.
 *
 * WHY:
 *   strdup() may return NULL on OOM.
 *   Many projects forget checking.
 *
 * DESIGN CHOICE HERE:
 *   Fail-fast process termination.
 *
 * In daemon products you may instead:
 *   - return error codes
 *   - trigger degraded mode
 *   - drop event with metric
 *
 *======================================================================*/
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

/*======================================================================
 * alfred_hash_u32()
 *
 * PURPOSE:
 *   Fast integer hash mixer.
 *
 * USED FOR:
 *   cookie hash tables
 *
 * WHY NOT raw modulo(cookie)?
 *   Sequential values cluster badly.
 *
 * This bit-mixing improves distribution.
 *
 *======================================================================*/
uint32_t alfred_hash_u32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;

    return x;
}

/*======================================================================
 * alfred_hash_str()
 *
 * PURPOSE:
 *   Hash string path quickly.
 *
 * USED FOR:
 *   debounce table keyed by path
 *
 * ALGORITHM:
 *   FNV-1a style hash
 *
 * Good tradeoff:
 *   speed / simplicity / decent distribution
 *
 *======================================================================*/
uint32_t alfred_hash_str(const char *s)
{
    uint32_t h = 2166136261U;

    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619U;
    }

    return h;
}

/*======================================================================
 * alfred_basename_ptr()
 *
 * PURPOSE:
 *   Return pointer to basename portion of path.
 *
 * INPUT:
 *   /tmp/a.txt
 *
 * OUTPUT:
 *   a.txt
 *
 * NO MEMORY ALLOCATION.
 *
 * IMPORTANT:
 *   Returned pointer points inside original string.
 *
 *======================================================================*/
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

/*======================================================================
 * alfred_parent_dir()
 *
 * PURPOSE:
 *   Copy parent directory of path into output buffer.
 *
 * INPUT:
 *   /tmp/a.txt
 *
 * OUTPUT:
 *   /tmp
 *
 * INPUT:
 *   /a.txt
 *
 * OUTPUT:
 *   /
 *
 * WHY USED:
 *   classify rename vs move
 *
 * EXAMPLE:
 *
 *   /tmp/a.txt -> /tmp/b.txt
 *      same parent => rename
 *
 *   /tmp/a.txt -> /home/a.txt
 *      different parent => move
 *
 *======================================================================*/
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

/*======================================================================
 * alfred_same_parent()
 *
 * PURPOSE:
 *   Compare whether two paths share same parent dir.
 *
 * RETURNS:
 *   1 true
 *   0 false
 *
 *======================================================================*/
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

/*======================================================================
 * alfred_same_basename()
 *
 * PURPOSE:
 *   Compare leaf names only.
 *
 * INPUT:
 *   /tmp/a.txt
 *   /home/a.txt
 *
 * RESULT:
 *   same filename => true
 *
 *======================================================================*/
int alfred_same_basename(
    const char *a,
    const char *b)
{
    return strcmp(
        alfred_basename_ptr(a),
        alfred_basename_ptr(b)
    ) == 0;
}

/*======================================================================
 * alfred_streq()
 *
 * PURPOSE:
 *   NULL-safe string equality helper.
 *
 *======================================================================*/
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

/*======================================================================
 * alfred_version_string_impl()
 *
 * Internal helper.
 *
 * Public wrapper may call this.
 *
 *======================================================================*/
const char *alfred_version_string_impl(void)
{
    return "1.0.0";
}
