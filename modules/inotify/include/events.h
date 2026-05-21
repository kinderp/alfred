/* ============================================================================
 * events.h - legacy inotify semantic dispatcher API
 *
 * This header belongs to the legacy shadow path. The default runtime sends
 * alfred_raw_event_t records to the core and does not use these event types as
 * the target architecture. Keep this interface only while shadow comparison is
 * still useful.
 * ========================================================================== */

#ifndef EVENTS_H
#define EVENTS_H

#include <limits.h>
#include <stddef.h>
#include <time.h>
#include <sys/inotify.h>

struct app;

typedef enum {

    EVT_UNKNOWN = 0,

    EVT_FILE_CREATED,
    EVT_FILE_DELETED,
    EVT_FILE_MOVED,
    EVT_FILE_RENAMED,

    EVT_DIR_CREATED,
    EVT_DIR_DELETED,
    EVT_DIR_MOVED,
    EVT_DIR_RENAMED,

    EVT_WATCH_REMOVED,
    EVT_QUEUE_OVERFLOW

} event_type_t;

/*
 * fs_event_t - legacy semantic event record
 *
 * This struct is retained for the historical dispatcher interface. The current
 * core path uses alfred_event_t instead.
 */
typedef struct {

    event_type_t type;

    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];

    struct timespec ts;

} fs_event_t;

const char* event_type_str(event_type_t type);

/*
 * legacy_events_init - initialize legacy shadow dispatcher state
 * @move_cache_size: number of move slots for legacy MOVED_FROM storage
 *
 * Return: 0 on success, -1 on allocation failure.
 */
int legacy_events_init(size_t move_cache_size);

/*
 * legacy_events_shutdown - release legacy shadow dispatcher state
 */
void legacy_events_shutdown(void);

/*
 * legacy_events_dispatch - process one inotify event through legacy semantics
 * @app: application context used for watcher lookup and logging
 * @ev: inotify event to classify
 *
 * This function is used only by shadow mode. It emits the historical event log
 * stream and intentionally does not define the final core semantics.
 */
void legacy_events_dispatch(struct app *app,
                            const struct inotify_event *ev);

#endif
