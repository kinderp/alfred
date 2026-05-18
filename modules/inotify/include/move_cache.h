#ifndef MOVE_CACHE_H
#define MOVE_CACHE_H

#include <stdint.h>
#include <limits.h>
#include <stddef.h>

typedef struct {
    uint32_t cookie;
    int src_wd;
    char src_name[NAME_MAX];
    int used;
} move_slot_t;

typedef struct {
    move_slot_t *slots;
    size_t size;
} move_cache_t;

int move_cache_init(move_cache_t *mc, size_t size);
void move_cache_destroy(move_cache_t *mc);

int move_cache_store(move_cache_t *mc,
                     uint32_t cookie,
                     int src_wd,
                     const char *src_name);

move_slot_t* move_cache_find(move_cache_t *mc, uint32_t cookie);
void move_cache_clear(move_cache_t *mc, uint32_t cookie);

#endif
