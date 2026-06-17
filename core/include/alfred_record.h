/* ============================================================================
 * alfred_record.h - common Event Model v0 record
 *
 * This header defines the common record shape used by the documented Event
 * Model v0 and Backend API v0. It is intentionally a data contract only: the
 * current runtime still uses alfred_raw_event_t at the backend/core boundary
 * and alfred_event_t at the core/application boundary.
 *
 * The purpose of alfred_record_t is to give future adapters, diagnostic
 * builders, text writers, JSONL writers, and binary writers a single typed
 * representation. A backend should not need to know whether a consumer writes
 * text logs, JSONL, MessagePack, protobuf, or a compact binary socket frame.
 * It should emit facts as records; writers decide the output format.
 *
 * Ownership rule:
 *   String fields are borrowed const char pointers. Unless a future function
 *   documents a stronger guarantee, a record is valid only during the callback
 *   that receives it. Consumers that need to retain a path, reason, state, or
 *   backend name must copy the string.
 * ========================================================================== */

#ifndef ALFRED_RECORD_H
#define ALFRED_RECORD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/types.h>

/*
 * Schema version for the first structured Alfred record shape.
 *
 * This value is meant for structured writers and external consumers. It is not
 * the same thing as the Alfred library version. The schema version changes
 * when the record contract changes in a way that a JSONL, binary, or plugin
 * consumer must understand.
 */
#define ALFRED_RECORD_SCHEMA_VERSION 0u

/*
 * alfred_record_layer_t - architectural layer that produced the record
 *
 * The layer prevents different meanings from being collapsed into one event
 * namespace. For example, WATCH_STALE is diagnostic backend state, not a
 * semantic DIR_MOVED event. FILE_CREATED can exist as normalized raw input and
 * as a semantic output, but those two records are produced at different layers
 * and have different stability guarantees.
 */
typedef enum {

    /*
     * Unset or invalid layer.
     *
     * Builders should avoid emitting this value. It is useful for zeroed test
     * fixtures and defensive validation.
     */
    ALFRED_RECORD_LAYER_UNKNOWN = 0,

    /*
     * Native backend observation before Alfred normalization.
     *
     * Example: an inotify event mask, a future fanotify record, or an eBPF
     * observation. This layer preserves backend truth when Alfred wants to
     * expose low-level facts for debugging or audit.
     */
    ALFRED_RECORD_LAYER_BACKEND_OBSERVED,

    /*
     * Backend fact translated into Alfred's raw vocabulary.
     *
     * This corresponds to today's alfred_raw_event_t stream. The core consumes
     * this layer to produce semantic events.
     */
    ALFRED_RECORD_LAYER_NORMALIZED_RAW,

    /*
     * Stable application-facing event emitted by the semantic core.
     *
     * This corresponds to today's alfred_event_t stream. It is the layer most
     * applications should consume when they care about filesystem meaning
     * instead of backend mechanics.
     */
    ALFRED_RECORD_LAYER_SEMANTIC,

    /*
     * Internal Alfred diagnostic state made observable.
     *
     * Examples: WATCH_ADDED, WATCH_STALE, WATCH_RESYNC_FAILED, and
     * WATCH_LOST_RECOVERY_END. Diagnostic records are part of the operational
     * contract, but they are not filesystem semantic events.
     */
    ALFRED_RECORD_LAYER_DIAGNOSTIC

} alfred_record_layer_t;

/*
 * alfred_record_category_t - domain family inside a record layer
 *
 * Categories group record types by the subsystem or problem space they
 * describe. The pair (layer, category) tells consumers how to interpret the
 * type value and which optional payload fields are likely to be meaningful.
 */
typedef enum {

    /*
     * Unset or invalid category.
     */
    ALFRED_RECORD_CATEGORY_UNKNOWN = 0,

    /*
     * Filesystem object event.
     *
     * Used for create, delete, modify, close-write/file-ready, move, rename,
     * relocated, attribute changes, and overflow records.
     */
    ALFRED_RECORD_CATEGORY_FILESYSTEM,

    /*
     * Watch lifecycle diagnostic.
     *
     * Used when Alfred adds, removes, marks stale, or drops events from a
     * backend watch. These records explain what Alfred is observing, not what
     * necessarily happened semantically in the filesystem.
     */
    ALFRED_RECORD_CATEGORY_WATCH,

    /*
     * Recovery and resynchronization diagnostic.
     *
     * Used by local resync and lost-scope recovery to describe scans,
     * reinstall attempts, retries, backoff, and final outcomes.
     */
    ALFRED_RECORD_CATEGORY_RECOVERY,

    /*
     * Backend operational diagnostic.
     *
     * Reserved for backend init/start/stop failures, unsupported features,
     * configuration issues, and future plugin lifecycle records.
     */
    ALFRED_RECORD_CATEGORY_BACKEND,

    /*
     * Policy or guardrail decision.
     *
     * Reserved for the AI agent runtime-security direction, where Alfred may
     * evaluate an action and allow, warn, or block it.
     */
    ALFRED_RECORD_CATEGORY_POLICY

} alfred_record_category_t;

/*
 * alfred_record_type_t - concrete record name inside layer/category
 *
 * These values intentionally mirror the names documented in Event Model v0.
 * The enum is not a bitmask: each record has exactly one type. Raw inotify or
 * Alfred raw flags remain in alfred_record_t.raw_mask.
 */
typedef enum {

    /*
     * Unset or invalid type.
     */
    ALFRED_RECORD_TYPE_UNKNOWN = 0,

    /*
     * A regular file appeared at path.
     *
     * In normalized raw, this usually comes from a backend create fact without
     * the directory bit. In semantic output, it is the core-approved file
     * creation event.
     */
    ALFRED_RECORD_TYPE_FILE_CREATED,

    /*
     * A regular file was closed after writing and is ready for consumers.
     */
    ALFRED_RECORD_TYPE_FILE_READY,

    /*
     * A regular file content or metadata change was observed.
     */
    ALFRED_RECORD_TYPE_FILE_MODIFIED,

    /*
     * A regular file disappeared from path.
     */
    ALFRED_RECORD_TYPE_FILE_DELETED,

    /*
     * A directory appeared at path.
     */
    ALFRED_RECORD_TYPE_DIR_CREATED,

    /*
     * A directory disappeared from path.
     */
    ALFRED_RECORD_TYPE_DIR_DELETED,

    /*
     * A regular file changed basename while staying in the same parent.
     */
    ALFRED_RECORD_TYPE_FILE_RENAMED,

    /*
     * A regular file changed parent directory while keeping the same basename.
     */
    ALFRED_RECORD_TYPE_FILE_MOVED,

    /*
     * A regular file changed both parent directory and basename.
     */
    ALFRED_RECORD_TYPE_FILE_RELOCATED,

    /*
     * A directory changed basename while staying in the same parent.
     */
    ALFRED_RECORD_TYPE_DIR_RENAMED,

    /*
     * A directory changed parent directory while keeping the same basename.
     */
    ALFRED_RECORD_TYPE_DIR_MOVED,

    /*
     * A directory changed both parent directory and basename.
     */
    ALFRED_RECORD_TYPE_DIR_RELOCATED,

    /*
     * Backend queue overflow was observed.
     *
     * Consumers should treat this as a signal that a full rescan or stronger
     * recovery path may be required.
     */
    ALFRED_RECORD_TYPE_OVERFLOW,

    /*
     * Alfred successfully installed a backend watch.
     */
    ALFRED_RECORD_TYPE_WATCH_ADDED,

    /*
     * Alfred removed or lost a backend watch.
     */
    ALFRED_RECORD_TYPE_WATCH_REMOVED,

    /*
     * A watch descriptor still exists but its path mapping is no longer fully
     * trustworthy.
     */
    ALFRED_RECORD_TYPE_WATCH_STALE,

    /*
     * Alfred received an event on a stale watch and deliberately did not
     * forward it as a raw/core event because the path could be wrong.
     */
    ALFRED_RECORD_TYPE_WATCH_STALE_EVENT_DROPPED,

    /*
     * Local resync started for a stale watch.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_BEGIN,

    /*
     * Local resync failed for a stale watch.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,

    /*
     * Local resync completed and the watch returned to a usable state.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_END,

    /*
     * A stale scope was queued for delayed lost-scope recovery.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED,

    /*
     * Lost-scope recovery found the watched identity at a new path.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_FOUND,

    /*
     * Lost-scope recovery completed successfully.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END,

    /*
     * Lost-scope recovery did not complete yet and was scheduled for retry.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED,

    /*
     * Lost-scope recovery exhausted its retry budget and stopped trying.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP

} alfred_record_type_t;

/*
 * alfred_record_identity_t - filesystem identity attached to a record
 * @device_id: device number from stat-like metadata, or 0 when unavailable
 * @inode_id: inode number from stat-like metadata, or 0 when unavailable
 *
 * Device plus inode identifies a filesystem object more reliably than a path
 * while the object remains on the same filesystem. Scanner, resync, and
 * lost-scope recovery use this pair to decide whether a stale textual path
 * still refers to the same object or whether the object must be searched under
 * monitored roots.
 */
typedef struct {
    dev_t device_id;
    ino_t inode_id;
} alfred_record_identity_t;

/*
 * alfred_record_watch_t - watch/recovery diagnostic payload
 * @watch_id: backend watch descriptor/id, or -1 when not applicable
 * @state: borrowed state string such as "valid", "stale", or "removed"
 * @reason: borrowed reason string such as "IN_MOVE_SELF" or "IN_DELETE_SELF"
 * @error: borrowed error string such as "identity-mismatch", or NULL
 * @retry_after_ns: monotonic timestamp before the next retry may run
 * @retry_count: number of recovery attempts already made
 *
 * This payload is meaningful mainly for diagnostic watch and recovery records.
 * Keeping it separate from the common filesystem fields makes it clear that
 * WATCH_* records describe Alfred backend state, not user-facing filesystem
 * semantics.
 */
typedef struct {
    int watch_id;
    const char *state;
    const char *reason;
    const char *error;
    uint64_t retry_after_ns;
    unsigned retry_count;
} alfred_record_watch_t;

/*
 * alfred_record_t - common structured Event Model v0 record
 * @schema_version: record schema version, initially ALFRED_RECORD_SCHEMA_VERSION
 * @seq: optional monotonic sequence number assigned by the producer/writer
 * @ts_ns: monotonic timestamp in nanoseconds, or 0 when unavailable
 * @layer: architectural layer that produced this record
 * @category: domain family inside @layer
 * @type: concrete record name inside (@layer, @category)
 * @source: backend/source identifier compatible with alfred_source_t
 * @raw_mask: raw ALFRED_RAW_* flags when representing normalized raw input
 * @cookie: backend move correlation cookie, or 0 when not applicable
 * @pid: process id if known, or 0 when unavailable
 * @backend: borrowed backend name such as "inotify", or NULL
 * @path: borrowed primary path for the record, or NULL
 * @old_path: borrowed previous path for move/rename/recovery records, or NULL
 * @new_path: borrowed new path for move/rename/recovery records, or NULL
 * @identity: optional filesystem identity evidence
 * @watch: optional watch/recovery diagnostic payload
 *
 * This struct is deliberately flat and conservative. It can represent today's
 * alfred_raw_event_t and alfred_event_t streams, plus WATCH_* diagnostics,
 * without forcing a JSON-specific shape into the C API. Optional fields are
 * zero or NULL when they do not apply.
 */
typedef struct alfred_record {
    uint32_t schema_version;

    uint64_t seq;
    uint64_t ts_ns;

    alfred_record_layer_t layer;
    alfred_record_category_t category;
    alfred_record_type_t type;

    uint32_t source;
    uint32_t raw_mask;
    uint32_t cookie;
    pid_t pid;

    const char *backend;
    const char *path;
    const char *old_path;
    const char *new_path;

    alfred_record_identity_t identity;
    alfred_record_watch_t watch;
} alfred_record_t;

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_H */
