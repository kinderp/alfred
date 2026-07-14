/*
 * test_workspace_session_config.c - workspace/session config contract tests
 *
 * This test covers only configuration parsing for the first app-owned
 * workspace/session runtime context. It does not start Alfred, does not write
 * JSONL, and does not attach the context to alfred_record_t.
 *
 * Expected contract:
 *
 * defaults:
 * - workspace_root is absent
 * - workspace_id is absent
 * - ledger_session_id is absent
 *
 * accepted config:
 * - workspace_root=/tmp/project
 * - workspace_id=opaque-workspace
 * - ledger_session_id=opaque-ledger-run
 *
 * rejected config:
 * - workspace_root=
 * - workspace_id=
 * - ledger_session_id=
 * - workspace_id longer than the fixed identifier buffer
 * - ledger_session_id longer than the fixed identifier buffer
 *
 * Meaning:
 * These values are declared runtime context, not backend evidence and not
 * agent/process attribution. Present-empty strings are rejected because they
 * would make future consumers unable to distinguish "field absent" from
 * "configured but empty".
 */

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void inotify_config_defaults(inotify_config_t *cfg)
{
    assert(cfg != NULL);
    cfg->recursive = 1;
    cfg->watcher_capacity = 128u;
    cfg->watch_mask = 0x1u;
    cfg->audit_mask = 0u;
}

int inotify_config_set_watch_mask(inotify_config_t *cfg, const char *value)
{
    assert(cfg != NULL);
    assert(value != NULL);
    cfg->watch_mask = 0x2u;
    return 0;
}

int inotify_config_set_audit_events(inotify_config_t *cfg, const char *value)
{
    assert(cfg != NULL);
    assert(value != NULL);
    cfg->audit_mask = 0x4u;
    return 0;
}

static void write_config_file(const char *path, const char *content)
{
    FILE *fp = fopen(path, "w");

    assert(fp != NULL);
    assert(fputs(content, fp) >= 0);
    assert(fclose(fp) == 0);
}

static void fill_oversized_id(char *buffer, size_t size)
{
    size_t i;

    assert(buffer != NULL);
    assert(size > WORKSPACE_SESSION_ID_MAX);

    for (i = 0; i < WORKSPACE_SESSION_ID_MAX; i++)
        buffer[i] = 'x';

    buffer[WORKSPACE_SESSION_ID_MAX] = '\0';
}

static void test_workspace_session_defaults_are_absent(void)
{
    config_t cfg;

    config_defaults(&cfg);

    assert(cfg.workspace_session.has_workspace_root == 0);
    assert(cfg.workspace_session.has_workspace_id == 0);
    assert(cfg.workspace_session.has_ledger_session_id == 0);
    assert(cfg.workspace_session.workspace_root[0] == '\0');
    assert(cfg.workspace_session.workspace_id[0] == '\0');
    assert(cfg.workspace_session.ledger_session_id[0] == '\0');
}

static void test_workspace_session_config_accepts_values(void)
{
    config_t cfg;
    const char *path = "/tmp/alfred_workspace_session_config_valid.conf";

    write_config_file(path,
                      "workspace_root=/tmp/alfred-project\n"
                      "workspace_id=ws-local-test\n"
                      "ledger_session_id=ls-local-test\n");

    config_defaults(&cfg);
    assert(config_load(&cfg, path) == ERR_OK);
    assert(cfg.workspace_session.has_workspace_root == 1);
    assert(cfg.workspace_session.has_workspace_id == 1);
    assert(cfg.workspace_session.has_ledger_session_id == 1);
    assert(strcmp(cfg.workspace_session.workspace_root,
                  "/tmp/alfred-project") == 0);
    assert(strcmp(cfg.workspace_session.workspace_id,
                  "ws-local-test") == 0);
    assert(strcmp(cfg.workspace_session.ledger_session_id,
                  "ls-local-test") == 0);

    remove(path);
}

static void test_workspace_session_config_rejects_empty_values(void)
{
    config_t cfg;
    const char *empty_root = "/tmp/alfred_workspace_session_empty_root.conf";
    const char *empty_workspace_id =
        "/tmp/alfred_workspace_session_empty_workspace_id.conf";
    const char *empty_ledger_id =
        "/tmp/alfred_workspace_session_empty_ledger_id.conf";

    write_config_file(empty_root, "workspace_root=\n");
    write_config_file(empty_workspace_id, "workspace_id=\n");
    write_config_file(empty_ledger_id, "ledger_session_id=\n");

    config_defaults(&cfg);
    assert(config_load(&cfg, empty_root) == ERR_CONFIG);

    config_defaults(&cfg);
    assert(config_load(&cfg, empty_workspace_id) == ERR_CONFIG);

    config_defaults(&cfg);
    assert(config_load(&cfg, empty_ledger_id) == ERR_CONFIG);

    remove(empty_root);
    remove(empty_workspace_id);
    remove(empty_ledger_id);
}

static void test_workspace_session_config_rejects_oversized_ids(void)
{
    config_t cfg;
    char value[WORKSPACE_SESSION_ID_MAX + 1u];
    char line[WORKSPACE_SESSION_ID_MAX + 32u];
    const char *workspace_id_path =
        "/tmp/alfred_workspace_session_long_workspace_id.conf";
    const char *ledger_id_path =
        "/tmp/alfred_workspace_session_long_ledger_id.conf";

    fill_oversized_id(value, sizeof(value));

    snprintf(line, sizeof(line), "workspace_id=%s\n", value);
    write_config_file(workspace_id_path, line);

    snprintf(line, sizeof(line), "ledger_session_id=%s\n", value);
    write_config_file(ledger_id_path, line);

    config_defaults(&cfg);
    assert(config_load(&cfg, workspace_id_path) == ERR_CONFIG);

    config_defaults(&cfg);
    assert(config_load(&cfg, ledger_id_path) == ERR_CONFIG);

    remove(workspace_id_path);
    remove(ledger_id_path);
}

int main(void)
{
    test_workspace_session_defaults_are_absent();
    test_workspace_session_config_accepts_values();
    test_workspace_session_config_rejects_empty_values();
    test_workspace_session_config_rejects_oversized_ids();

    return 0;
}
