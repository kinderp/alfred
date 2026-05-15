/* ============================================================================
 * config.c
 * Runtime configuration manager
 * ========================================================================== */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * DEFAULT CONFIGURATION
 * ========================================================================== */

void config_defaults(config_t *cfg)
{
    if (cfg == NULL)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->recursive          = 1;
    cfg->use_epoll          = 0;
    cfg->flush_immediately  = 1;

    cfg->move_cache_size    = 128;
    cfg->watcher_capacity   = 128;

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
 * INTERNAL PARSER HELPERS
 * ========================================================================== */

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

/* ============================================================================
 * LOAD CONFIG FILE
 *
 * Example:
 *
 * recursive=true
 * use_epoll=false
 * move_cache_size=256
 * raw_log=myraw.log
 * ========================================================================== */

int config_load(config_t *cfg, const char *path)
{
    if (cfg == NULL || path == NULL)
        return -1;

    FILE *fp = fopen(path, "r");

    if (fp == NULL)
        return -1;

    char line[512];

    while (fgets(line, sizeof(line), fp) != NULL) {

        trim_newline(line);

        /* skip empty */
        if (line[0] == '\0')
            continue;

        /* skip comments */
        if (line[0] == '#')
            continue;

        char *eq = strchr(line, '=');

        if (eq == NULL)
            continue;

        *eq = '\0';

        char *key   = line;
        char *value = eq + 1;

        /* ---------------------------------------------------------
         * recursive
         * ------------------------------------------------------- */
        if (strcmp(key, "recursive") == 0) {

            cfg->recursive =
                parse_bool(value);
        }

        /* ---------------------------------------------------------
         * use_epoll
         * ------------------------------------------------------- */
        else if (strcmp(key, "use_epoll") == 0) {

            cfg->use_epoll =
                parse_bool(value);
        }

        /* ---------------------------------------------------------
         * flush_immediately
         * ------------------------------------------------------- */
        else if (strcmp(key, "flush_immediately") == 0) {

            cfg->flush_immediately =
                parse_bool(value);
        }

        /* ---------------------------------------------------------
         * move_cache_size
         * ------------------------------------------------------- */
        else if (strcmp(key, "move_cache_size") == 0) {

            cfg->move_cache_size =
                atoi(value);
        }

        /* ---------------------------------------------------------
         * watcher_capacity
         * ------------------------------------------------------- */
        else if (strcmp(key, "watcher_capacity") == 0) {

            cfg->watcher_capacity =
                atoi(value);
        }

        /* ---------------------------------------------------------
         * raw_log
         * ------------------------------------------------------- */
        else if (strcmp(key, "raw_log") == 0) {

            snprintf(cfg->raw_log,
                     sizeof(cfg->raw_log),
                     "%s",
                     value);
        }

        /* ---------------------------------------------------------
         * event_log
         * ------------------------------------------------------- */
        else if (strcmp(key, "event_log") == 0) {

            snprintf(cfg->event_log,
                     sizeof(cfg->event_log),
                     "%s",
                     value);
        }

        /* ---------------------------------------------------------
         * error_log
         * ------------------------------------------------------- */
        else if (strcmp(key, "error_log") == 0) {

            snprintf(cfg->error_log,
                     sizeof(cfg->error_log),
                     "%s",
                     value);
        }
    }

    fclose(fp);

    return 0;
}
