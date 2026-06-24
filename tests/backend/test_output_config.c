/*
 * test_output_config.c - future writer output configuration tests
 *
 * This test covers only configuration parsing. It does not start Alfred, does
 * not enable the runtime writer path, and does not write JSONL. The expected
 * contract is:
 *
 * defaults:
 * - output.enabled = 0
 * - output.format = OUTPUT_FORMAT_JSONL
 * - output.buffer_size = OUTPUT_BUFFER_SIZE_DEFAULT
 *
 * accepted config:
 * - output_enabled=true
 * - output_format=text or output_format=jsonl
 * - output_buffer_size >= OUTPUT_BUFFER_SIZE_MIN
 *
 * rejected config:
 * - output_format=protobuf
 * - output_buffer_size=0
 * - output_buffer_size below OUTPUT_BUFFER_SIZE_MIN
 * - output_buffer_size with non-numeric suffixes
 *
 * Meaning:
 * output.enabled=true describes the future record -> queue -> dispatcher ->
 * writer path, but this micro-step only stores and validates the intent. With
 * output.enabled=false, Alfred keeps the current compatibility path through the
 * existing raw.log/events.log/errors.log loggers.
 */

#include "config.h"

#include <assert.h>
#include <stdint.h>
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

static void test_output_defaults_are_disabled(void)
{
    config_t cfg;

    config_defaults(&cfg);

    assert(cfg.output.enabled == 0);
    assert(cfg.output.format == OUTPUT_FORMAT_JSONL);
    assert(cfg.output.buffer_size == OUTPUT_BUFFER_SIZE_DEFAULT);
}

static void test_output_config_accepts_jsonl(void)
{
    config_t cfg;
    const char *path = "/tmp/alfred_output_config_jsonl.conf";

    write_config_file(path,
                      "output_enabled=true\n"
                      "output_format=jsonl\n"
                      "output_buffer_size=8192\n");

    config_defaults(&cfg);
    assert(config_load(&cfg, path) == ERR_OK);
    assert(cfg.output.enabled == 1);
    assert(cfg.output.format == OUTPUT_FORMAT_JSONL);
    assert(cfg.output.buffer_size == 8192u);
    remove(path);
}

static void test_output_config_accepts_text(void)
{
    config_t cfg;
    const char *path = "/tmp/alfred_output_config_text.conf";

    write_config_file(path,
                      "output_enabled=false\n"
                      "output_format=text\n"
                      "output_buffer_size=4096\n");

    config_defaults(&cfg);
    assert(config_load(&cfg, path) == ERR_OK);
    assert(cfg.output.enabled == 0);
    assert(cfg.output.format == OUTPUT_FORMAT_TEXT);
    assert(cfg.output.buffer_size == OUTPUT_BUFFER_SIZE_MIN);
    remove(path);
}

static void test_output_config_rejects_invalid_format(void)
{
    config_t cfg;
    const char *path = "/tmp/alfred_output_config_bad_format.conf";

    write_config_file(path, "output_format=protobuf\n");

    config_defaults(&cfg);
    assert(config_load(&cfg, path) == ERR_CONFIG);
    remove(path);
}

static void test_output_config_rejects_invalid_buffer_size(void)
{
    config_t cfg;
    const char *too_small = "/tmp/alfred_output_config_too_small.conf";
    const char *not_numeric = "/tmp/alfred_output_config_not_numeric.conf";

    write_config_file(too_small, "output_buffer_size=1024\n");
    write_config_file(not_numeric, "output_buffer_size=8192kb\n");

    config_defaults(&cfg);
    assert(config_load(&cfg, too_small) == ERR_CONFIG);

    config_defaults(&cfg);
    assert(config_load(&cfg, not_numeric) == ERR_CONFIG);

    remove(too_small);
    remove(not_numeric);
}

int main(void)
{
    test_output_defaults_are_disabled();
    test_output_config_accepts_jsonl();
    test_output_config_accepts_text();
    test_output_config_rejects_invalid_format();
    test_output_config_rejects_invalid_buffer_size();

    return 0;
}
