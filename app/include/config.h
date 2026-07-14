/* ============================================================================
 * config.h - runtime configuration API
 *
 * The configuration layer owns user-tunable runtime values and log file paths.
 * Backend-specific groups, such as the inotify configuration, are nested here
 * so subsystem boundaries receive only the fields they need.
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "errors.h"
#include "inotify_config.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define OUTPUT_BUFFER_SIZE_DEFAULT (64u * 1024u)
/*
 * The runtime JSONL writer uses a separate scratch buffer for one formatted
 * object. The output buffer must be large enough to hold any object that fits in
 * that scratch buffer plus the trailing JSONL newline. app.c currently uses an
 * 8192-byte scratch buffer, so v0 rejects smaller configured output buffers.
 */
#define OUTPUT_BUFFER_SIZE_MIN 8192u

/*
 * Workspace/session identifiers are v0 runtime context strings, not Event
 * Model record fields. They are stored inline to keep configuration ownership
 * simple and to avoid per-record allocation or queue clone costs.
 */
#define WORKSPACE_SESSION_ID_MAX 128u

/*
 * output_format_t - configured format for the runtime writer path
 *
 * The enum describes the writer format requested by configuration. It does not
 * mean every format is a public output format: output.enabled controls whether
 * the runtime writer path should be active. JSONL is the user-facing v0 writer;
 * counter is a benchmark/no-op sink used to measure queue and dispatcher cost
 * without serialization or I/O.
 */
typedef enum {
    OUTPUT_FORMAT_TEXT = 0,
    OUTPUT_FORMAT_JSONL = 1,
    OUTPUT_FORMAT_COUNTER = 2
} output_format_t;

/*
 * output_config_t - top-level output runtime configuration
 * @enabled: opt-in flag for the record -> queue -> dispatcher -> writer path.
 *           When false, Alfred keeps only the current compatibility log path.
 * @format: requested writer format for the runtime output path
 * @buffer_size: bytes reserved for buffered writers such as JSONL
 *
 * This is application-level configuration, not backend configuration. The
 * inotify backend must not receive this struct and must not decide output
 * format, buffering, or writer policy.
 */
typedef struct {
    int enabled;
    output_format_t format;
    size_t buffer_size;
} output_config_t;

/*
 * workspace_session_config_t - optional workspace/session runtime context
 * @has_workspace_root: true when workspace_root was explicitly configured
 * @has_workspace_id: true when workspace_id was explicitly configured
 * @has_ledger_session_id: true when ledger_session_id was explicitly configured
 * @workspace_root: declared filesystem observation boundary for the run
 * @workspace_id: opaque workspace correlation label
 * @ledger_session_id: opaque Alfred ledger/run correlation label
 *
 * These values describe declared runtime context. They are not observed by the
 * backend, do not prove agent/process attribution, and are not emitted in JSONL
 * by this config contract. Present-empty values are rejected by config_load()
 * because an empty configured string would be ambiguous with an absent field.
 */
typedef struct {
    int has_workspace_root;
    int has_workspace_id;
    int has_ledger_session_id;

    char workspace_root[PATH_MAX];
    char workspace_id[WORKSPACE_SESSION_ID_MAX];
    char ledger_session_id[WORKSPACE_SESSION_ID_MAX];
} workspace_session_config_t;

/*
 * config_t - runtime configuration values
 *
 * The struct intentionally stores plain values instead of owning dynamic
 * memory. This keeps initialization and cleanup simple: config_defaults() can
 * reset the whole object and path fields are stored in fixed-size buffers.
 */
typedef struct {

    /* Reserved for a future epoll-based event loop. */
    int use_epoll;

    /* Flush log streams after each write for better crash visibility. */
    int flush_immediately;

    /* Linux inotify backend configuration. */
    inotify_config_t inotify;

    /* Optional record output runtime configuration. */
    output_config_t output;

    /* Optional app-owned workspace/session runtime context configuration. */
    workspace_session_config_t workspace_session;

    /* Log file paths. Stored inline to avoid configuration-owned allocation. */
    char raw_log[PATH_MAX];
    char event_log[PATH_MAX];
    char error_log[PATH_MAX];
    char output_log[PATH_MAX];

} config_t;

/*
 * config_load - load runtime configuration from a key-value file
 * @cfg: configuration object to update
 * @path: path to the configuration file
 *
 * Reads simple `key=value` lines and updates known fields in @cfg. Unknown
 * keys are ignored so newer config files remain mostly compatible with older
 * binaries.
 *
 * Return: ERR_OK on success, ERR_INVALID_ARG or ERR_CONFIG on failure.
 */
error_t config_load(config_t *cfg, const char *path);

/*
 * config_set_event_engine - validate the semantic event engine option
 * @cfg: configuration object to update
 * @value: expected value, currently only "core"
 *
 * The runtime no longer stores an engine selector because the core is the only
 * supported engine. This helper remains so config files and ALFRED_EVENT_ENGINE
 * can reject stale values such as "shadow" explicitly.
 *
 * Return: ERR_OK on success, ERR_INVALID_ARG or ERR_CONFIG on failure.
 */
error_t config_set_event_engine(config_t *cfg, const char *value);

/*
 * config_defaults - initialize configuration with safe defaults
 * @cfg: configuration object to initialize
 *
 * Resets @cfg and fills default values for all runtime options.
 */
void config_defaults(config_t *cfg);

#endif
