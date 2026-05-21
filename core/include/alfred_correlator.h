/* ============================================================================
 * alfred_correlator.h - public API for the semantic event core
 *
 * The core accepts low-level Alfred raw events and emits high-level semantic
 * events. Backends such as inotify and future fanotify adapters are responsible
 * for translating their native kernel records into alfred_raw_event_t. The core
 * is responsible for correlation, debounce, move classification, and stable
 * application-facing event names.
 *
 * This header is intentionally backend-neutral. It must not expose inotify
 * watch descriptors, Linux masks, or module-specific state.
 * ========================================================================== */

#ifndef ALFRED_CORRELATOR_H
#define ALFRED_CORRELATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

/* ============================================================================
 * VERSION
 * ========================================================================== */

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

/* ============================================================================
 * FORWARD DECLARATION
 * ========================================================================== */

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

/* ============================================================================
 * RAW INPUT EVENT SOURCE
 * ========================================================================== */

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

/* ============================================================================
 * RAW INPUT MASK FLAGS
 * ========================================================================== */

/*
 * Raw masks represent low-level watcher facts.
 *
 * Multiple bits may be combined.
 *
 * Example:
 *   CREATE + ISDIR
 *
 * These flags are not semantic events. ALFRED_RAW_MOVED_FROM and
 * ALFRED_RAW_MOVED_TO are two raw facts; the core may combine them into one
 * FILE_MOVED, FILE_RENAMED, FILE_RELOCATED, DIR_MOVED, DIR_RENAMED, or
 * DIR_RELOCATED event.
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

/* ============================================================================
 * RAW INPUT EVENT
 * ========================================================================== */

/*
 * This structure is fed INTO the engine.
 *
 * Usually created by adapters:
 *
 *   inotify adapter  -> fills this struct
 *   fanotify adapter -> fills this struct
 *
 * Engine does NOT own string memory permanently. Path only needs to remain
 * valid during alfred_process(), except when the core stores a pending
 * MOVED_FROM path internally for later MOVED_TO correlation.
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

/* ============================================================================
 * HIGH LEVEL OUTPUT EVENT TYPES
 * ========================================================================== */

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
 *
 * Rename, move, and relocated are distinct semantic outcomes:
 *
 *   - RENAMED: same parent directory, different basename
 *   - MOVED: different parent directory, same basename
 *   - RELOCATED: both parent directory and basename changed
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

/* ============================================================================
 * HIGH LEVEL OUTPUT EVENT
 * ========================================================================== */

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

/* ============================================================================
 * USER CALLBACK
 * ========================================================================== */

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

/* ============================================================================
 * CONFIGURATION
 * ========================================================================== */

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

/* ============================================================================
 * API
 * ========================================================================== */

/*
 * alfred_config_default - fill config with sane defaults
 * @cfg: configuration object to initialize
 *
 * Safe to modify afterward.
 */
void alfred_config_default(alfred_config_t *cfg);

/*
 * alfred_create - create a new semantic event engine
 * @cfg: runtime configuration copied into the engine
 * @emit_cb: callback invoked for each semantic event
 * @userdata: opaque pointer passed to @emit_cb
 *
 * Return: pointer on success, NULL on failure.
 */
alfred_engine_t *alfred_create(
    const alfred_config_t *cfg,
    alfred_emit_fn emit_cb,
    void *userdata
);

/*
 * alfred_destroy - destroy an engine and free all internal state
 * @engine: engine returned by alfred_create()
 *
 * Safe with NULL.
 */
void alfred_destroy(alfred_engine_t *engine);

/*
 * alfred_process - feed one raw event into the engine
 * @engine: engine returned by alfred_create()
 * @raw: raw event produced by a backend or test harness
 *
 * The core is deterministic for an ordered raw input stream: the same raw
 * sequence produces the same semantic sequence.
 *
 * Return: 0 on success, -1 on invalid arguments.
 */
int alfred_process(
    alfred_engine_t *engine,
    const alfred_raw_event_t *raw
);

/*
 * alfred_tick - run periodic maintenance for time-based state
 * @engine: engine returned by alfred_create()
 *
 * Call regularly (example every 100 ms)
 * if your loop may become idle.
 *
 * Needed for move timeout expiration.
 *
 * Return: 0 on success, -1 on invalid arguments.
 */
int alfred_tick(alfred_engine_t *engine);

/*
 * alfred_flush - flush pending states immediately
 * @engine: engine returned by alfred_create()
 *
 * Example:
 *   before shutdown
 *   before snapshot
 *
 * Return: 0 on success, -1 on invalid arguments.
 */
int alfred_flush(alfred_engine_t *engine);

/*
 * alfred_event_name - return a human-readable semantic event name
 * @type: ALFRED_EV_* value
 *
 * Example:
 *   FILE_CREATED
 *
 * Return: static string for @type, or "UNKNOWN".
 */
const char *alfred_event_name(uint32_t type);

/*
 * alfred_version_string - return the public core version string
 *
 * Return: static semantic version string.
 *
 * Example:
 *   1.0.0
 */
const char *alfred_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_CORRELATOR_H */
