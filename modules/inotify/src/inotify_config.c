/* ============================================================================
 * inotify_config.c - inotify backend configuration defaults
 * ========================================================================== */

#include "inotify_config.h"
#include "watch_manager.h"

#include <sys/inotify.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/*
 * inotify_config_defaults - initialize inotify backend defaults
 * @cfg: inotify configuration object to initialize
 *
 * Defaults favor development visibility: recursive watching is enabled, the
 * watcher table starts small but can grow, and the watch mask comes from the
 * watch manager's backend-defined default.
 */
void inotify_config_defaults(inotify_config_t *cfg)
{
    if (cfg == NULL)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->recursive = 1;
    cfg->watcher_capacity = 128;
    cfg->watch_mask = watch_manager_default_mask();
}

typedef struct inotify_mask_name {
    const char *name;
    uint32_t flag;
} inotify_mask_name_t;

static const inotify_mask_name_t INOTIFY_MASK_NAMES[] = {
    { "IN_ATTRIB", IN_ATTRIB },
    { "IN_CLOSE_WRITE", IN_CLOSE_WRITE },
    { "IN_CREATE", IN_CREATE },
    { "IN_DELETE", IN_DELETE },
    { "IN_DELETE_SELF", IN_DELETE_SELF },
    { "IN_MODIFY", IN_MODIFY },
    { "IN_MOVE_SELF", IN_MOVE_SELF },
    { "IN_MOVED_FROM", IN_MOVED_FROM },
    { "IN_MOVED_TO", IN_MOVED_TO },
    { "IN_UNMOUNT", IN_UNMOUNT },
    { "IN_IGNORED", IN_IGNORED },
    { "IN_Q_OVERFLOW", IN_Q_OVERFLOW },
};

static char *trim_space(char *s)
{
    if (s == NULL)
        return NULL;

    while (isspace((unsigned char)*s))
        s++;

    char *end = s + strlen(s);

    while (end > s && isspace((unsigned char)end[-1])) {
        end--;
        *end = '\0';
    }

    return s;
}

static int lookup_mask_flag(const char *name, uint32_t *flag)
{
    if (name == NULL || flag == NULL)
        return -1;

    size_t count =
        sizeof(INOTIFY_MASK_NAMES) / sizeof(INOTIFY_MASK_NAMES[0]);

    for (size_t i = 0; i < count; i++) {
        if (strcmp(name, INOTIFY_MASK_NAMES[i].name) == 0) {
            *flag = INOTIFY_MASK_NAMES[i].flag;
            return 0;
        }
    }

    return -1;
}

/*
 * inotify_config_set_watch_mask - parse and assign a textual IN_* mask
 * @cfg: inotify configuration object to update
 * @value: comma-separated mask expression
 *
 * The parser uses a copy of @value so config_load() can pass pointers into its
 * line buffer safely. A plain list starts from an empty mask, while `default`
 * starts from watch_manager_default_mask(). `+TOKEN` and `-TOKEN` then modify
 * the current mask. Unknown tokens fail the whole parse to make typos visible.
 */
int inotify_config_set_watch_mask(inotify_config_t *cfg, const char *value)
{
    if (cfg == NULL || value == NULL || value[0] == '\0')
        return -1;

    char buffer[512];
    int written = snprintf(buffer, sizeof(buffer), "%s", value);

    if (written < 0 || written >= (int)sizeof(buffer))

        return -1;

    uint32_t mask = 0;
    int saw_token = 0;

    char *save = NULL;
    char *token = strtok_r(buffer, ",", &save);

    while (token != NULL) {
        token = trim_space(token);

        if (token[0] == '\0')
            return -1;

        char op = '\0';

        if (token[0] == '+' || token[0] == '-') {
            op = token[0];
            token = trim_space(token + 1);
        }

        if (strcmp(token, "default") == 0) {
            if (op == '-')
                return -1;

            mask = watch_manager_default_mask();
            saw_token = 1;
            token = strtok_r(NULL, ",", &save);
            continue;
        }

        uint32_t flag = 0;

        if (lookup_mask_flag(token, &flag) != 0)
            return -1;

        if (op == '-')
            mask &= ~flag;
        else
            mask |= flag;

        saw_token = 1;
        token = strtok_r(NULL, ",", &save);
    }

    if (!saw_token)
        return -1;

    cfg->watch_mask = mask;
    return 0;
}
