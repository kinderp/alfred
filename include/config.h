#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include <stddef.h>

typedef struct {

    int recursive;
    int use_epoll;
    int flush_immediately;

    size_t move_cache_size;
    size_t watcher_capacity;

    char raw_log[PATH_MAX];
    char event_log[PATH_MAX];
    char error_log[PATH_MAX];

} config_t;

int config_load(config_t *cfg, const char *path);
void config_defaults(config_t *cfg);

#endif
