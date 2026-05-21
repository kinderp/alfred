/* ============================================================================
 * inotify_adapter.c - inotify to core raw-event adapter
 *
 * This file contains conversion-only logic between Linux inotify records and
 * the core alfred_raw_event_t API. It is a bridge between backend-specific
 * kernel details and backend-independent core input.
 *
 * It must not perform semantic classification, move correlation, debounce, or
 * logging. Those responsibilities belong to the core or application layers.
 * ============================================================================
 */

#include "inotify_adapter.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * Time Helpers
 * ============================================================================
 */

/*
 * adapter_now_ns - read the monotonic clock in nanoseconds
 *
 * The core uses monotonic timestamps for timeout and debounce decisions because
 * wall-clock time can jump due to NTP, manual changes, or timezone updates.
 *
 * Return: monotonic timestamp in nanoseconds.
 */
static uint64_t adapter_now_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) +
           (uint64_t)ts.tv_nsec;
}

/* ============================================================================
 * Mask Conversion
 * ============================================================================
 */

uint32_t inotify_adapter_mask_to_alfred(uint32_t mask)
{
    uint32_t out = 0;

    /*
     * Keep this mapping mechanical. If a future semantic rule needs a
     * combination of raw facts, add it to the core instead of hiding it here.
     */
    if (mask & IN_CREATE)
        out |= ALFRED_RAW_CREATE;
    if (mask & IN_DELETE)
        out |= ALFRED_RAW_DELETE;
    if (mask & IN_MODIFY)
        out |= ALFRED_RAW_MODIFY;
    if (mask & IN_ATTRIB)
        out |= ALFRED_RAW_ATTRIB;
    if (mask & IN_CLOSE_WRITE)
        out |= ALFRED_RAW_CLOSE_WRITE;
    if (mask & IN_MOVED_FROM)
        out |= ALFRED_RAW_MOVED_FROM;
    if (mask & IN_MOVED_TO)
        out |= ALFRED_RAW_MOVED_TO;
    if (mask & IN_Q_OVERFLOW)
        out |= ALFRED_RAW_OVERFLOW;
    if (mask & IN_ISDIR)
        out |= ALFRED_RAW_ISDIR;

    return out;
}

/* ============================================================================
 * Path Conversion
 * ============================================================================
 */

int inotify_adapter_build_path(const char *parent,
                               const char *name,
                               char *dst,
                               size_t dst_size)
{
    if (parent == NULL || dst == NULL || dst_size == 0)
        return -1;

    if (name == NULL || name[0] == '\0') {
        /*
         * Some inotify records refer to the watched directory itself and do
         * not carry a child name. In that case the parent path is already the
         * full event path expected by the core.
         */
        int written = snprintf(dst, dst_size, "%s", parent);

        if (written < 0 || (size_t)written >= dst_size)
            return -1;

        return 0;
    }

    return path_join(dst, dst_size, parent, name);
}

/* ============================================================================
 * Raw Event Conversion
 * ============================================================================
 */

int inotify_adapter_build_raw(const struct inotify_event *ev,
                              const char *parent_path,
                              char *full_path,
                              size_t full_path_size,
                              alfred_raw_event_t *out)
{
    if (ev == NULL || parent_path == NULL || full_path == NULL || out == NULL)
        return -1;

    const char *name = ev->len > 0 ? ev->name : "";

    /*
     * full_path is caller-owned storage. The raw event below only borrows it
     * until alfred_process() returns.
     */
    if (inotify_adapter_build_path(parent_path,
                                   name,
                                   full_path,
                                   full_path_size) != 0) {
        return -1;
    }

    memset(out, 0, sizeof(*out));

    out->ts_ns = adapter_now_ns();
    out->source = ALFRED_SRC_INOTIFY;
    out->mask = inotify_adapter_mask_to_alfred(ev->mask);
    out->cookie = ev->cookie;
    out->pid = 0;
    out->path = full_path;

    return 0;
}
