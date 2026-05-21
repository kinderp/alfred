/* ============================================================================
 * move_cache.c - legacy move correlation cache
 *
 * This cache supports the legacy shadow dispatcher in events.c. It stores the
 * first half of an inotify move pair:
 *
 *   IN_MOVED_FROM + cookie + source wd/name
 *
 * and later lets events.c correlate it with:
 *
 *   IN_MOVED_TO + same cookie + destination wd/name
 *
 * The default core runtime does not use this file. The core has its own move
 * table and emits RELOCATED for the mixed move+rename case, while legacy events
 * emit MOVED and then RENAMED.
 * ========================================================================== */

#include "move_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * slot_meta_t - side metadata for legacy move slots
 * @created_at: wall-clock insertion time used for stale cleanup
 *
 * Metadata is kept parallel to move_cache_t.slots so the public legacy struct
 * remains small. This design is acceptable for temporary shadow support but is
 * not the architecture used by the core.
 */
typedef struct {
    time_t created_at;
} slot_meta_t;

/*
 * File-local metadata for the single legacy dispatcher cache. This is another
 * reason the cache should stay confined to shadow mode.
 */
static slot_meta_t *g_meta = NULL;

/* ============================================================================
 * INTERNAL HELPERS
 * ========================================================================== */

/*
 * cache_find_free - find an unused legacy move slot
 * @mc: initialized cache
 *
 * Return: slot index on success, -1 when full.
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
 * cache_find_oldest - choose the oldest slot for bounded replacement
 * @mc: initialized cache
 *
 * Return: slot index on success, -1 when the cache has no slots.
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
 * cache_find_cookie - find a slot by inotify move cookie
 * @mc: initialized cache
 * @cookie: inotify move cookie to search
 *
 * Return: slot index on success, -1 when absent.
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
 * move_cache_init - allocate a bounded legacy move cache
 * @mc: cache to initialize
 * @size: requested slot count, or 0 for default
 *
 * Return: 0 on success, -1 on invalid input or allocation failure.
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
 * move_cache_destroy - release memory owned by a legacy move cache
 * @mc: cache to destroy
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
 * move_cache_store - store or replace one MOVED_FROM half
 * @mc: initialized cache
 * @cookie: inotify move cookie
 * @src_wd: source watch descriptor
 * @src_name: source basename
 *
 * If the cache is full, the oldest slot is overwritten. This bounded behavior
 * prevents unbounded memory growth in the legacy path.
 *
 * Return: 0 on success, -1 on invalid input.
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

    /* Prefer updating an existing cookie before allocating a new slot. */
    if (idx < 0)
        idx = cache_find_free(mc);

    /* Bounded cache: replace the oldest pending source if every slot is used. */
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
 * move_cache_find - return a cached MOVED_FROM slot by cookie
 * @mc: initialized cache
 * @cookie: inotify move cookie
 *
 * Return: slot pointer on success, NULL when absent.
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
 * move_cache_clear - clear one cached move slot
 * @mc: initialized cache
 * @cookie: inotify move cookie
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
 * move_cache_cleanup - remove stale MOVED_FROM halves
 * @mc: initialized cache
 * @ttl_seconds: maximum age before removal
 *
 * This helper is legacy support for MOVED_FROM events that never receive a
 * matching MOVED_TO.
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
 * move_cache_count - count active legacy move slots
 * @mc: initialized cache
 *
 * Return: active slot count, or 0 for NULL.
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
