/* ============================================================================
 * move_cache.h - legacy MOVED_FROM cache for shadow dispatcher
 *
 * This cache belongs to modules/inotify/src/events.c, the legacy semantic
 * dispatcher used only by shadow mode. The core correlator has independent move
 * state and does not use this API.
 * ========================================================================== */

#ifndef MOVE_CACHE_H
#define MOVE_CACHE_H

#include <stdint.h>
#include <limits.h>
#include <stddef.h>

/*
 * move_slot_t - one cached legacy MOVED_FROM half
 * @cookie: inotify move cookie used to match MOVED_TO
 * @src_wd: source watch descriptor
 * @src_name: source basename reported by inotify
 * @used: nonzero when this slot contains a pending move
 */
typedef struct {
    uint32_t cookie;
    int src_wd;
    char src_name[NAME_MAX];
    int used;
} move_slot_t;

/*
 * move_cache_t - bounded array of legacy move slots
 * @slots: allocated slot array
 * @size: number of slots in @slots
 */
typedef struct {
    move_slot_t *slots;
    size_t size;
} move_cache_t;

/*
 * move_cache_init - allocate a legacy move cache
 * @mc: cache to initialize
 * @size: requested slot count, or 0 for default
 *
 * Return: 0 on success, -1 on allocation failure.
 */
int move_cache_init(move_cache_t *mc, size_t size);

/*
 * move_cache_destroy - release memory owned by a legacy move cache
 * @mc: cache to destroy
 */
void move_cache_destroy(move_cache_t *mc);

/*
 * move_cache_store - store one MOVED_FROM half
 * @mc: initialized cache
 * @cookie: inotify move cookie
 * @src_wd: source watch descriptor
 * @src_name: source basename
 *
 * Return: 0 on success, -1 on invalid input.
 */
int move_cache_store(move_cache_t *mc,
                     uint32_t cookie,
                     int src_wd,
                     const char *src_name);

/*
 * move_cache_find - find a cached MOVED_FROM by cookie
 * @mc: initialized cache
 * @cookie: inotify move cookie
 *
 * Return: slot pointer on success, NULL when not found.
 */
move_slot_t* move_cache_find(move_cache_t *mc, uint32_t cookie);

/*
 * move_cache_clear - clear a consumed or stale move cookie
 * @mc: initialized cache
 * @cookie: inotify move cookie
 */
void move_cache_clear(move_cache_t *mc, uint32_t cookie);

#endif
