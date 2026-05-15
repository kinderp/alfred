/* ============================================================================
 * move_cache.c
 * Professional move / rename correlation cache
 *
 * Purpose:
 *   inotify emits:
 *
 *      IN_MOVED_FROM   + cookie
 *      IN_MOVED_TO     + same cookie
 *
 * We temporarily store MOVED_FROM and later correlate MOVED_TO.
 *
 * Used by:
 *   event_engine.c
 *
 * Design goals:
 *   - fast lookup
 *   - overwrite stale entries
 *   - bounded memory
 *   - production readable
 * ========================================================================== */

#include "move_cache.h"

#include <stdio.h>	// snprintf()
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================================
 * INTERNAL STRUCTURE NOTE
 *
 * move_slot_t already declared in header:
 *
 * typedef struct {
 *      uint32_t cookie;
 *      int src_wd;
 *      char src_name[NAME_MAX];
 *      int used;
 * } move_slot_t;
 *
 * We extend freshness using parallel timestamps here.
 * ========================================================================== */

typedef struct {
    time_t created_at;
} slot_meta_t;

/* private side metadata */
static slot_meta_t *g_meta = NULL;

/* ============================================================================
 * INTERNAL HELPERS
 * ========================================================================== */

/*
 * Find free slot.
 */
static int cache_find_free(move_cache_t *mc)
{
    for (size_t i = 0; i < mc->size; i++) {
        if (!mc->slots[i].used)
            return (int)i;
    }

    return -1;
}

/*
 * Find oldest slot for replacement.
 */
static int cache_find_oldest(move_cache_t *mc)
{
    if (mc->size == 0)
        return -1;

    size_t oldest = 0;
    time_t min_ts = g_meta[0].created_at;

    for (size_t i = 1; i < mc->size; i++) {
        if (g_meta[i].created_at < min_ts) {
            min_ts = g_meta[i].created_at;
            oldest = i;
        }
    }

    return (int)oldest;
}

/*
 * Find exact cookie.
 */
static int cache_find_cookie(move_cache_t *mc,
                             uint32_t cookie)
{
    for (size_t i = 0; i < mc->size; i++) {

        if (!mc->slots[i].used)
            continue;

        if (mc->slots[i].cookie == cookie)
            return (int)i;
    }

    return -1;
}

/* ============================================================================
 * PUBLIC API
 * ========================================================================== */

/*
 * Allocate cache.
 */
int move_cache_init(move_cache_t *mc,
                    size_t size)
{
    if (mc == NULL)
        return -1;

    if (size == 0)
        size = 64;

    memset(mc, 0, sizeof(*mc));

    mc->slots =
        calloc(size, sizeof(move_slot_t));

    if (mc->slots == NULL)
        return -1;

    g_meta =
        calloc(size, sizeof(slot_meta_t));

    if (g_meta == NULL) {
        free(mc->slots);
        mc->slots = NULL;
        return -1;
    }

    mc->size = size;

    return 0;
}

/*
 * Free cache.
 */
void move_cache_destroy(move_cache_t *mc)
{
    if (mc == NULL)
        return;

    free(mc->slots);
    mc->slots = NULL;

    free(g_meta);
    g_meta = NULL;

    mc->size = 0;
}

/*
 * Store MOVED_FROM piece.
 *
 * If full:
 *   oldest entry overwritten.
 */
int move_cache_store(move_cache_t *mc,
                     uint32_t cookie,
                     int src_wd,
                     const char *src_name)
{
    if (mc == NULL || src_name == NULL)
        return -1;

    if (cookie == 0)
        return -1;

    int idx = cache_find_cookie(mc, cookie);

    /* update existing cookie */
    if (idx < 0)
        idx = cache_find_free(mc);

    /* replace oldest if full */
    if (idx < 0)
        idx = cache_find_oldest(mc);

    if (idx < 0)
        return -1;

    move_slot_t *slot = &mc->slots[idx];

    slot->used   = 1;
    slot->cookie = cookie;
    slot->src_wd = src_wd;

    snprintf(slot->src_name,
             sizeof(slot->src_name),
             "%s",
             src_name);

    g_meta[idx].created_at = time(NULL);

    return 0;
}

/*
 * Return matching slot pointer.
 *
 * NULL if not found.
 */
move_slot_t* move_cache_find(move_cache_t *mc,
                             uint32_t cookie)
{
    if (mc == NULL)
        return NULL;

    if (cookie == 0)
        return NULL;

    int idx = cache_find_cookie(mc, cookie);

    if (idx < 0)
        return NULL;

    return &mc->slots[idx];
}

/*
 * Clear consumed cookie.
 */
void move_cache_clear(move_cache_t *mc,
                      uint32_t cookie)
{
    if (mc == NULL)
        return;

    int idx = cache_find_cookie(mc, cookie);

    if (idx < 0)
        return;

    memset(&mc->slots[idx],
           0,
           sizeof(move_slot_t));

    g_meta[idx].created_at = 0;
}

/*
 * Remove stale entries older than ttl_seconds.
 *
 * Useful for:
 *   MOVED_FROM without MOVED_TO
 */
void move_cache_cleanup(move_cache_t *mc,
                        int ttl_seconds)
{
    if (mc == NULL)
        return;

    if (ttl_seconds <= 0)
        ttl_seconds = 10;

    time_t now = time(NULL);

    for (size_t i = 0; i < mc->size; i++) {

        if (!mc->slots[i].used)
            continue;

        if ((now - g_meta[i].created_at)
            > ttl_seconds) {

            memset(&mc->slots[i],
                   0,
                   sizeof(move_slot_t));

            g_meta[i].created_at = 0;
        }
    }
}

/*
 * Debug helper.
 */
size_t move_cache_count(move_cache_t *mc)
{
    if (mc == NULL)
        return 0;

    size_t count = 0;

    for (size_t i = 0; i < mc->size; i++) {
        if (mc->slots[i].used)
            count++;
    }

    return count;
}
