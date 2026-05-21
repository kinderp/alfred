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

typedef struct {

    event_type_t type;

    char src_path[PATH_MAX];
    char dst_path[PATH_MAX];

    struct timespec ts;

} fs_event_t;

const char* event_type_str(event_type_t type);

int legacy_events_init(size_t move_cache_size);
void legacy_events_shutdown(void);
void app_dispatch_raw_event(struct app *app,
                            const struct inotify_event *ev);

#endif
