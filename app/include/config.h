/* ============================================================================
 * config.h - runtime configuration API
 *
 * The configuration layer owns user-tunable runtime values such as recursive
 * watching, table capacities, watch masks, and log file paths. Configuration is
 * loaded before subsystem initialization so later components can rely on this
 * structure being populated.
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "errors.h"

/*
 * event_engine_mode_t - semantic event engine selection
 *
 * CORE is the normal runtime: backend raw events are processed by the core and
 * logged as the official plain semantic stream. SHADOW is kept only as a
 * recognized legacy value so startup can reject it with a clear error instead
 * of silently accepting an unsupported mode.
 */
typedef enum {
    EVENT_ENGINE_SHADOW = 0,
    EVENT_ENGINE_CORE
} event_engine_mode_t;

/*
 * config_t - runtime configuration values
 *
 * The struct intentionally stores plain values instead of owning dynamic
 * memory. This keeps initialization and cleanup simple: config_defaults() can
 * reset the whole object and path fields are stored in fixed-size buffers.
 */
typedef struct {

    /* Add watches recursively when startup paths contain directories. */
    int recursive;

    /* Reserved for a future epoll-based event loop. */
    int use_epoll;

    /* Flush log streams after each write for better crash visibility. */
    int flush_immediately;

    /*
     * Initial capacity of the backend watcher table. The table can grow when
     * inotify returns a watch descriptor beyond the current capacity.
     */
    size_t watcher_capacity;

    /*
     * Backend-specific inotify event mask used by watch_manager_add(). It is
     * stored in config_t even though it is not yet configurable from the file.
     */
    uint32_t watch_mask;

    /* Selects the event engine; only core is currently supported at runtime. */
    event_engine_mode_t event_engine_mode;

    /* Log file paths. Stored inline to avoid configuration-owned allocation. */
    char raw_log[PATH_MAX];
    char event_log[PATH_MAX];
    char error_log[PATH_MAX];

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
 * config_set_event_engine - parse and set the semantic event engine mode
 * @cfg: configuration object to update
 * @value: expected value, either "shadow" or "core"
 *
 * Return: ERR_OK on success, ERR_INVALID_ARG or ERR_CONFIG on failure.
 */
error_t config_set_event_engine(config_t *cfg, const char *value);

/*
 * config_event_engine_name - render an event engine mode for logs/docs
 * @mode: event engine mode to render
 *
 * Return: stable string name for @mode.
 */
const char *config_event_engine_name(event_engine_mode_t mode);

/*
 * config_defaults - initialize configuration with safe defaults
 * @cfg: configuration object to initialize
 *
 * Resets @cfg and fills default values for all runtime options.
 */
void config_defaults(config_t *cfg);

#endif
