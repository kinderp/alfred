/*======================================================================
 *
 * FILE: include/alfred_correlator.h
 *
 * PUBLIC API HEADER
 *
 * This file exposes the stable public interface of the Alfred
 * filesystem event correlation engine.
 *
 * PURPOSE:
 *   Convert noisy low-level raw watcher events into clean,
 *   semantic high-level events.
 *
 * INPUT EXAMPLES:
 *   RAW_CREATE
 *   RAW_MODIFY
 *   RAW_MOVED_FROM
 *   RAW_MOVED_TO
 *
 * OUTPUT EXAMPLES:
 *   FILE_CREATED
 *   FILE_READY
 *   FILE_MODIFIED
 *   FILE_RENAMED
 *   FILE_MOVED
 *
 * DESIGN GOALS:
 *   - portable C (POSIX)
 *   - no external dependencies
 *   - deterministic behavior
 *   - embeddable library
 *   - stable ABI-friendly structures
 *
 * NOTE:
 *   This header contains only declarations.
 *   Implementation lives in src/*.c
 *
 *======================================================================*/

#ifndef ALFRED_CORRELATOR_H
#define ALFRED_CORRELATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

/*======================================================================
 * VERSION
 *======================================================================*/

/*
 * Public semantic version of the engine.
 *
 * Increment rules:
 *
 * MAJOR:
 *   Breaking API change.
 *
 * MINOR:
 *   Backward compatible new features.
 *
 * PATCH:
 *   Bug fixes only.
 */
#define ALFRED_VERSION_MAJOR 1
#define ALFRED_VERSION_MINOR 0
#define ALFRED_VERSION_PATCH 0

/*======================================================================
 * FORWARD DECLARATION
 *======================================================================*/

/*
 * Opaque engine instance.
 *
 * Users only hold pointer references.
 * Internal fields remain private in .c files.
 *
 * This prevents accidental misuse and allows
 * internal redesign without changing public API.
 */
typedef struct alfred_engine alfred_engine_t;

/*======================================================================
 * RAW INPUT EVENT SOURCE
 *======================================================================*/

/*
 * Identifies where raw event originated.
 *
 * Why useful:
 *   Diagnostics, metrics, future source-specific logic.
 */
typedef enum {

    /* Unknown or user synthetic source */
    ALFRED_SRC_UNKNOWN = 0,

    /* Linux inotify adapter */
    ALFRED_SRC_INOTIFY = 1,

    /* Linux fanotify adapter */
    ALFRED_SRC_FANOTIFY = 2,

    /* Manual / tests / replay logs */
    ALFRED_SRC_USER = 3

} alfred_source_t;

/*======================================================================
 * RAW INPUT MASK FLAGS
 *======================================================================*/

/*
 * Raw masks represent low-level watcher facts.
 *
 * Multiple bits may be combined.
 *
 * Example:
 *   CREATE + ISDIR
 */
typedef enum {

    ALFRED_RAW_CREATE       = 1u << 0,
    ALFRED_RAW_DELETE       = 1u << 1,
    ALFRED_RAW_MODIFY       = 1u << 2,
    ALFRED_RAW_ATTRIB       = 1u << 3,
    ALFRED_RAW_CLOSE_WRITE  = 1u << 4,
    ALFRED_RAW_MOVED_FROM   = 1u << 5,
    ALFRED_RAW_MOVED_TO     = 1u << 6,
    ALFRED_RAW_OVERFLOW     = 1u << 7,
    ALFRED_RAW_ISDIR        = 1u << 8

} alfred_raw_mask_t;

/*======================================================================
 * RAW INPUT EVENT
 *======================================================================*/

/*
 * This structure is fed INTO the engine.
 *
 * Usually created by adapters:
 *
 *   inotify adapter  -> fills this struct
 *   fanotify adapter -> fills this struct
 *
 * Engine does NOT own string memory permanently.
 * Path only needs to remain valid during process() call.
 */
typedef struct {

    /*
     * Monotonic timestamp in nanoseconds.
     *
     * IMPORTANT:
     *   Use monotonic time, not wall clock.
     *
     * Why:
     *   Wall clock can jump due to NTP / user changes.
     *   Timeout logic would break.
     */
    uint64_t ts_ns;

    /*
     * Event source identifier.
     */
    uint32_t source;

    /*
     * Bitmask of ALFRED_RAW_* flags.
     */
    uint32_t mask;

    /*
     * Move correlation cookie.
     *
     * Linux inotify uses same cookie for:
     *
     *   MOVED_FROM
     *   MOVED_TO
     *
     * Zero means "not applicable".
     */
    uint32_t cookie;

    /*
     * Process ID if known.
     *
     * fanotify may provide this.
     * inotify usually does not.
     *
     * Zero means unavailable.
     */
    pid_t pid;

    /*
     * Absolute full path.
     *
     * Examples:
     *   /tmp/a.txt
     *   /home/user/docs
     *
     * UTF-8 / bytes accepted.
     */
    const char *path;

} alfred_raw_event_t;

/*======================================================================
 * HIGH LEVEL OUTPUT EVENT TYPES
 *======================================================================*/

/*
 * These are semantic events emitted by Alfred.
 *
 * Designed for applications.
 *
 * Example:
 *   Search indexers
 *   Backup daemons
 *   Security sensors
 *   Desktop UI refreshers
 */
typedef enum {

    ALFRED_EV_FILE_CREATED = 1,
    ALFRED_EV_FILE_READY,
    ALFRED_EV_FILE_MODIFIED,
    ALFRED_EV_FILE_DELETED,

    ALFRED_EV_DIR_CREATED,
    ALFRED_EV_DIR_DELETED,

    ALFRED_EV_FILE_RENAMED,
    ALFRED_EV_FILE_MOVED,
    ALFRED_EV_FILE_RELOCATED,

    ALFRED_EV_DIR_RENAMED,
    ALFRED_EV_DIR_MOVED,
    ALFRED_EV_DIR_RELOCATED,

    /*
     * Kernel queue overflow detected.
     *
     * Application should schedule full rescan.
     */
    ALFRED_EV_OVERFLOW

} alfred_event_type_t;

/*======================================================================
 * HIGH LEVEL OUTPUT EVENT
 *======================================================================*/

/*
 * This structure is emitted TO user callback.
 *
 * Lifetime:
 *   Valid only during callback unless copied.
 */
typedef struct {

    /*
     * Strictly increasing sequence number.
     *
     * Useful for logs and ordering.
     */
    uint64_t seq;

    /*
     * Emission timestamp (monotonic ns).
     */
    uint64_t ts_ns;

    /*
     * Semantic event type.
     */
    uint32_t type;

    /*
     * Process ID if known.
     */
    pid_t pid;

    /*
     * Source path.
     *
     * Used for:
     *   modify/delete/create -> src only
     *   rename/move         -> old path
     */
    const char *src_path;

    /*
     * Destination path.
     *
     * Used only for move/rename style events.
     *
     * Otherwise NULL.
     */
    const char *dst_path;

} alfred_event_t;

/*======================================================================
 * USER CALLBACK
 *======================================================================*/

/*
 * Called whenever engine emits high-level event.
 *
 * Parameters:
 *   ev        -> event data
 *   userdata  -> opaque pointer given at init
 *
 * Rules:
 *   - callback should return quickly
 *   - if heavy work needed, queue elsewhere
 *   - event memory expires after callback returns
 */
typedef void (*alfred_emit_fn)(
    const alfred_event_t *ev,
    void *userdata
);

/*======================================================================
 * CONFIGURATION
 *======================================================================*/

/*
 * Tunable runtime parameters.
 */
typedef struct {

    /*
     * How long to wait for MOVED_TO after MOVED_FROM.
     *
     * Example:
     *   250 ms default.
     *
     * If timeout expires:
     *   pending move treated as delete/move-out.
     */
    uint32_t move_timeout_ms;

    /*
     * Suppress repeated modify storms.
     *
     * Example:
     *   text editor may emit many MODIFY records.
     *
     * Engine merges them.
     */
    uint32_t modify_debounce_ms;

    /*
     * CREATE followed by CLOSE_WRITE inside this window
     * may emit FILE_READY.
     */
    uint32_t create_ready_ms;

} alfred_config_t;

/*======================================================================
 * API
 *======================================================================*/

/*
 * Fill config with sane defaults.
 *
 * Safe to modify afterward.
 */
void alfred_config_default(alfred_config_t *cfg);

/*
 * Create new engine instance.
 *
 * Returns:
 *   pointer on success
 *   NULL on failure
 */
alfred_engine_t *alfred_create(
    const alfred_config_t *cfg,
    alfred_emit_fn emit_cb,
    void *userdata
);

/*
 * Destroy engine and free all memory.
 *
 * Safe with NULL.
 */
void alfred_destroy(alfred_engine_t *engine);

/*
 * Feed one raw event into engine.
 *
 * Deterministic:
 *   same ordered input -> same output
 */
int alfred_process(
    alfred_engine_t *engine,
    const alfred_raw_event_t *raw
);

/*
 * Periodic maintenance tick.
 *
 * Call regularly (example every 100 ms)
 * if your loop may become idle.
 *
 * Needed for move timeout expiration.
 */
int alfred_tick(alfred_engine_t *engine);

/*
 * Flush pending states immediately.
 *
 * Example:
 *   before shutdown
 *   before snapshot
 */
int alfred_flush(alfred_engine_t *engine);

/*
 * Human readable event type string.
 *
 * Example:
 *   FILE_CREATED
 */
const char *alfred_event_name(uint32_t type);

/*
 * Library version string.
 *
 * Example:
 *   1.0.0
 */
const char *alfred_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_CORRELATOR_H */
