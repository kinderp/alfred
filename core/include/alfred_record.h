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

#include <stddef.h>
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
     * Runtime lifecycle metadata.
     *
     * Used for records that describe the Alfred run or output stream itself
     * rather than a filesystem object, backend watch, recovery attempt, or
     * policy decision. The first planned use is SESSION_CONTEXT, a one-shot
     * metadata/session record for workspace/session context.
     */
    ALFRED_RECORD_CATEGORY_LIFECYCLE,

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
 *
 * Important split:
 *   ALFRED_RECORD_TYPE_RAW_* values describe normalized raw backend facts.
 *   ALFRED_RECORD_TYPE_FILE_* and ALFRED_RECORD_TYPE_DIR_* values describe
 *   semantic core outcomes. Keeping both groups separate prevents an adapter
 *   from pretending that a low-level MOVED_FROM or MOVED_TO fact is already a
 *   stable move, rename, or relocated semantic event.
 */
typedef enum {

    /*
     * Unset or invalid type.
     */
    ALFRED_RECORD_TYPE_UNKNOWN = 0,

    /*
     * Raw backend create fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. Consumers must inspect
     * alfred_record_t.raw_mask to know whether the created object is a file or
     * directory. The semantic core later decides whether this becomes
     * FILE_CREATED, DIR_CREATED, or part of a more complex scenario.
     */
    ALFRED_RECORD_TYPE_RAW_CREATE,

    /*
     * Raw backend delete fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. The directory bit, if
     * present, remains in raw_mask. This is not yet a semantic FILE_DELETED or
     * DIR_DELETED decision.
     */
    ALFRED_RECORD_TYPE_RAW_DELETE,

    /*
     * Raw backend modify fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. The semantic core may
     * debounce or suppress these records before emitting FILE_MODIFIED.
     */
    ALFRED_RECORD_TYPE_RAW_MODIFY,

    /*
     * Raw backend attribute-change fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. Alfred currently keeps
     * IN_ATTRIB observable at the raw/backend level while the final semantic
     * policy for metadata changes remains deferred.
     */
    ALFRED_RECORD_TYPE_RAW_ATTRIB,

    /*
     * Raw backend close-after-write fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. The semantic core maps
     * this to FILE_READY when appropriate.
     */
    ALFRED_RECORD_TYPE_RAW_CLOSE_WRITE,

    /*
     * Raw backend move-from fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. This is one half of a
     * move pair and must not be treated as a complete move/rename by itself.
     */
    ALFRED_RECORD_TYPE_RAW_MOVED_FROM,

    /*
     * Raw backend move-to fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. The core correlates it
     * with RAW_MOVED_FROM through cookie before choosing moved, renamed, or
     * relocated semantics.
     */
    ALFRED_RECORD_TYPE_RAW_MOVED_TO,

    /*
     * Raw backend queue-overflow fact.
     *
     * Used only with ALFRED_RECORD_LAYER_NORMALIZED_RAW. It means the raw
     * stream is incomplete and higher layers should schedule recovery.
     */
    ALFRED_RECORD_TYPE_RAW_OVERFLOW,

    /*
     * A regular file appeared at path.
     *
     * Used with ALFRED_RECORD_LAYER_SEMANTIC after the core has accepted the
     * raw create fact as a stable file creation event.
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
     * Local resync could not complete the subtree coverage scan.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_FAILED,

    /*
     * Local resync completed a subtree coverage scan.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_DONE,

    /*
     * Local resync classified the coverage scan counters.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_CLASS,

    /*
     * Local resync found one directory missing a watch.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_MISSING,

    /*
     * Local resync successfully reinstalled one missing watch.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALLED,

    /*
     * Local resync failed to reinstall one missing watch.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALL_FAILED,

    /*
     * Local resync removed a watch installed by the same failed attempt.
     */
    ALFRED_RECORD_TYPE_WATCH_RESYNC_ROLLBACK,

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
     * A stale scope could not be queued because required identity was missing.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_SKIPPED,

    /*
     * A stale scope could not be queued because queue insertion failed.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED,

    /*
     * Lost-scope recovery started scanning one configured root.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN,

    /*
     * Lost-scope recovery found the watched identity at a new path.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_FOUND,

    /*
     * Lost-scope recovery updated the root path and known child prefixes.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED,

    /*
     * Lost-scope recovery completed a coverage scan of the recovered subtree.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE,

    /*
     * Lost-scope recovery found one directory missing a watch.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING,

    /*
     * Lost-scope recovery classified coverage scan counters.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS,

    /*
     * Lost-scope recovery successfully reinstalled one missing watch.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED,

    /*
     * Lost-scope recovery failed to reinstall one missing watch.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED,

    /*
     * Lost-scope recovery removed a watch installed by a failed attempt.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK,

    /*
     * Lost-scope recovery did not find the saved identity in one root.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND,

    /*
     * Lost-scope recovery failed before reaching a successful end state.
     */
    ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,

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
    ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP,

    /*
     * Runtime workspace/session context for the structured output stream.
     *
     * Used with ALFRED_RECORD_LAYER_DIAGNOSTIC and
     * ALFRED_RECORD_CATEGORY_LIFECYCLE. This type only admits the stable public
     * record name into Event Model v0; workspace/session payload fields,
     * builders, and runtime emission are separate contract steps.
     */
    ALFRED_RECORD_TYPE_SESSION_CONTEXT

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
 * alfred_record_os_error_t - operating-system error evidence
 * @code: numeric operating-system error code, or 0 when unavailable
 * @name: borrowed symbolic OS error name such as "ENOENT", or NULL
 * @message: borrowed human-readable OS error message, or NULL
 *
 * This payload is separate from alfred_record_watch_t.error on purpose.
 * watch.error is Alfred's stable diagnostic token, for example
 * "path-unreachable" or "identity-mismatch". The OS error fields preserve the
 * lower-level cause reported by the platform. On Linux, @code is usually errno,
 * @name is the errno symbol when the producer knows it, and @message can carry
 * the strerror-like text used by human-readable logs.
 *
 * Writers must not parse "errno=N (...)" text back into a record. Structured
 * outputs should serialize these fields directly when they are present.
 */
typedef struct {
    int code;
    const char *name;
    const char *message;
} alfred_record_os_error_t;

/*
 * alfred_record_watch_t - watch/recovery diagnostic payload
 * @watch_id: backend watch descriptor/id, or -1 when not applicable
 * @state: borrowed state string such as "valid", "stale", or "removed"
 * @reason: borrowed reason string such as "IN_MOVE_SELF" or "IN_DELETE_SELF"
 * @error: borrowed error string such as "identity-mismatch", or NULL
 * @event_mask: borrowed backend event-mask text for dropped-event diagnostics
 * @event_name: borrowed backend child name for dropped-event diagnostics
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
    const char *event_mask;
    const char *event_name;
    uint64_t retry_after_ns;
    unsigned retry_count;
} alfred_record_watch_t;

/*
 * alfred_record_recovery_t - resync/recovery diagnostic details
 * @directories_seen: number of directories discovered by a coverage scan
 * @directories_watched: number of scanned directories already covered
 * @directories_missing: number of scanned directories missing a watch
 * @detail_path: borrowed secondary path for missing/reinstalled watch records
 * @related_watch_id: related backend watch descriptor, such as rollback wd
 * @result_code: backend-local numeric result, such as a scan rc value
 * @pending_count: number of lost-scope entries pending after this diagnostic
 * @children_count: number of known child watch prefixes updated by recovery
 * @watches_count: number of watches repaired or marked valid by recovery
 * @delay_ms: retry/backoff delay in milliseconds, or 0 when not applicable
 *
 * These fields carry the technical payload of WATCH_RESYNC_* and WATCH_LOST_*
 * records. They are separate from alfred_record_watch_t because
 * watch.reason/error/state describe the high-level recovery decision, while
 * this payload describes scan counters, one affected child path, or one
 * related watch descriptor used by a concrete recovery step.
 */
typedef struct {
    size_t directories_seen;
    size_t directories_watched;
    size_t directories_missing;
    const char *detail_path;
    int related_watch_id;
    int result_code;
    size_t pending_count;
    size_t children_count;
    size_t watches_count;
    uint64_t delay_ms;
} alfred_record_recovery_t;

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
 * @os_error: optional operating-system error evidence
 * @watch: optional watch/recovery diagnostic payload
 * @recovery: optional resync/recovery diagnostic payload
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
    alfred_record_os_error_t os_error;
    alfred_record_watch_t watch;
    alfred_record_recovery_t recovery;
} alfred_record_t;

#ifdef __cplusplus
}
#endif

#endif /* ALFRED_RECORD_H */
