#ifndef WATCHER_H
#define WATCHER_H

#include <limits.h>
#include <stddef.h>

typedef struct {
    int wd;
    int active;
    char path[PATH_MAX];
} watcher_entry_t;

typedef struct {
    watcher_entry_t *items;
    size_t count;
    size_t capacity;
} watcher_table_t;

int watcher_init(watcher_table_t *wt, size_t capacity);
void watcher_destroy(watcher_table_t *wt);

int watcher_store(watcher_table_t *wt, int wd, const char *path);
void watcher_remove(watcher_table_t *wt, int wd);

const char* watcher_get_path(const watcher_table_t *wt, int wd);
int watcher_exists(const watcher_table_t *wt, int wd);

#endif
