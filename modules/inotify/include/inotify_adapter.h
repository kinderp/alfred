/* ============================================================================
 * inotify_adapter.h - inotify to core raw-event adapter
 *
 * This header exposes conversion helpers for translating Linux
 * struct inotify_event records into alfred_raw_event_t values accepted by the
 * core correlator. The adapter is intentionally small and stateless.
 *
 * The adapter does not decide whether an event is a create, rename, move, or
 * delete at the semantic level. It only preserves raw backend facts.
 * ============================================================================
 */

#ifndef INOTIFY_ADAPTER_H
#define INOTIFY_ADAPTER_H

#include "alfred_correlator.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/inotify.h>

/*
 * inotify_adapter_mask_to_alfred - convert an inotify mask to core raw flags
 * @mask: Linux inotify event mask
 *
 * Maps backend-specific IN_* bits to ALFRED_RAW_* bits. Unknown inotify bits
 * are ignored so the core receives only facts it currently understands.
 *
 * Return: ALFRED_RAW_* bitmask.
 */
uint32_t inotify_adapter_mask_to_alfred(uint32_t mask);

/*
 * inotify_adapter_build_path - build a full event path
 * @parent: watched parent path associated with the inotify watch descriptor
 * @name: optional event name stored after struct inotify_event
 * @dst: destination buffer
 * @dst_size: destination buffer length
 *
 * Builds the path expected by alfred_raw_event_t.path. If @name is empty, the
 * watched parent path is copied as the full path.
 *
 * Return: 0 on success, -1 on invalid input or truncation.
 */
int inotify_adapter_build_path(const char *parent,
                               const char *name,
                               char *dst,
                               size_t dst_size);

/*
 * inotify_adapter_build_raw - convert one inotify event to a core raw event
 * @ev: inotify event to convert
 * @parent_path: watched parent path associated with ev->wd
 * @full_path: caller-owned storage for the raw event path
 * @full_path_size: size of @full_path
 * @out: destination raw event
 *
 * The resulting @out does not own path memory. Its path pointer references
 * @full_path, so @full_path must remain valid until alfred_process() returns.
 *
 * Return: 0 on success, -1 on invalid input or path truncation.
 */
int inotify_adapter_build_raw(const struct inotify_event *ev,
                              const char *parent_path,
                              char *full_path,
                              size_t full_path_size,
                              alfred_raw_event_t *out);

#endif /* INOTIFY_ADAPTER_H */
