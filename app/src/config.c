/* ============================================================================
 * config.c - runtime configuration manager
 *
 * The configuration manager provides defaults and an optional key-value file
 * parser. It deliberately stores values in a plain struct so application
 * startup can initialize configuration before any subsystem owns resources.
 * ========================================================================== */

#include "config.h"
#include "inotify_config.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Default Configuration
 * ========================================================================== */

/*
 * config_defaults - initialize configuration with safe defaults
 * @cfg: configuration object to initialize
 *
 * The defaults favor development visibility: recursive watching is enabled,
 * logs use predictable local filenames, and backend-specific defaults are
 * delegated to the owning backend configuration helper.
 */
void config_defaults(config_t *cfg)
{
    if (cfg == NULL)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->use_epoll          = 0;
    cfg->flush_immediately  = 1;

    inotify_config_defaults(&cfg->inotify);

    snprintf(cfg->raw_log,
             sizeof(cfg->raw_log),
             "raw.log");

    snprintf(cfg->event_log,
             sizeof(cfg->event_log),
             "events.log");

    snprintf(cfg->error_log,
             sizeof(cfg->error_log),
             "errors.log");
}

/* ============================================================================
 * Parser Helpers
 * ========================================================================== */

/*
 * trim_newline - remove line terminators from a mutable string
 * @s: string to modify
 *
 * Configuration lines come from fgets(), which keeps trailing newline
 * characters. Removing both LF and CR keeps Unix and Windows-style files
 * readable by the same parser.
 */
static void trim_newline(char *s)
{
    if (s == NULL)
        return;

    size_t len = strlen(s);

    while (len > 0 &&
          (s[len - 1] == '\n' ||
           s[len - 1] == '\r')) {

        s[len - 1] = '\0';
        len--;
    }
}

/*
 * parse_bool - parse a permissive boolean value
 * @value: string value to parse
 *
 * Recognizes the common true values used in simple config files. Unknown
 * values are treated as false.
 *
 * Return: 1 for true values, 0 otherwise.
 */
static int parse_bool(const char *value)
{
    if (value == NULL)
        return 0;

    if (strcmp(value, "1") == 0) return 1;
    if (strcmp(value, "true") == 0) return 1;
    if (strcmp(value, "yes") == 0) return 1;
    if (strcmp(value, "on") == 0) return 1;

    return 0;
}

static size_t parse_size_or_default(const char *value, size_t fallback)
{
    if (value == NULL)
        return fallback;

    /*
     * Capacity fields are size_t, while atoi() returns int and silently accepts
     * malformed input. Using strtoul() lets us reject negative values,
     * non-numeric suffixes, and conversion errors before assigning to size_t.
     */
    if (value[0] == '-')
        return fallback;

    errno = 0;

    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);

    if (errno != 0 || end == value || *end != '\0')
        return fallback;

    if (parsed > (unsigned long)SIZE_MAX)
        return fallback;

    return (size_t)parsed;
}

/*
 * config_set_event_engine - validate the event engine option
 * @cfg: configuration object to update
 * @value: expected value, currently only "core"
 *
 * Shadow mode is no longer a recognized configuration value. The runtime does
 * not store an engine selector because there is only one supported engine; this
 * helper exists only to reject stale config and environment values.
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
error_t config_set_event_engine(config_t *cfg, const char *value)
{
    if (cfg == NULL || value == NULL)
        return ERR_INVALID_ARG;

    if (strcmp(value, "core") == 0)
        return ERR_OK;

    return ERR_CONFIG;
}

/* ============================================================================
 * Config File Loading
 * ========================================================================== */

/*
 * config_load - load runtime configuration from a key-value file
 * @cfg: configuration object to update
 * @path: path to the configuration file
 *
 * Parses simple `key=value` records. Empty lines and lines beginning with '#'
 * are ignored. Unknown keys are skipped so configuration can evolve without
 * making older binaries fail immediately.
 *
 * Example:
 *
 *   recursive=true
 *   inotify_recursive=true
 *   use_epoll=false
 *   event_engine=core
 *   raw_log=myraw.log
 *
 * Return: ERR_OK on success, ERR_INVALID_ARG for invalid input, or ERR_CONFIG
 * for file open and parse failures.
 */
error_t config_load(config_t *cfg, const char *path)
{
    if (cfg == NULL || path == NULL)
        return ERR_INVALID_ARG;

    FILE *fp = fopen(path, "r");

    if (fp == NULL)
        return ERR_CONFIG;

    char line[512];

    while (fgets(line, sizeof(line), fp) != NULL) {

        trim_newline(line);

        /* Empty lines are allowed for readability. */
        if (line[0] == '\0')
            continue;

        /* Shell-style comments are ignored. */
        if (line[0] == '#')
            continue;

        char *eq = strchr(line, '=');

        if (eq == NULL)
            continue;

        *eq = '\0';

        char *key   = line;
        char *value = eq + 1;

        if (strcmp(key, "recursive") == 0 ||
            strcmp(key, "inotify_recursive") == 0) {

            cfg->inotify.recursive =
                parse_bool(value);
        }

        else if (strcmp(key, "use_epoll") == 0) {

            cfg->use_epoll =
                parse_bool(value);
        }

        else if (strcmp(key, "flush_immediately") == 0) {

            cfg->flush_immediately =
                parse_bool(value);
        }

        else if (strcmp(key, "watcher_capacity") == 0 ||
                 strcmp(key, "inotify_watcher_capacity") == 0) {

            cfg->inotify.watcher_capacity =
                parse_size_or_default(value, cfg->inotify.watcher_capacity);
        }

        else if (strcmp(key, "event_engine") == 0) {

            if (config_set_event_engine(cfg, value) != ERR_OK) {
                fclose(fp);
                return ERR_CONFIG;
            }
        }

        else if (strcmp(key, "raw_log") == 0) {

            snprintf(cfg->raw_log,
                     sizeof(cfg->raw_log),
                     "%s",
                     value);
        }

        else if (strcmp(key, "event_log") == 0) {

            snprintf(cfg->event_log,
                     sizeof(cfg->event_log),
                     "%s",
                     value);
        }

        else if (strcmp(key, "error_log") == 0) {

            snprintf(cfg->error_log,
                     sizeof(cfg->error_log),
                     "%s",
                     value);
        }
    }

    fclose(fp);

    return ERR_OK;
}
