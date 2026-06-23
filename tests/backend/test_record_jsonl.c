/*
 * test_record_jsonl.c - JSONL formatter tests for Event Model v0 records
 *
 * JSONL is Alfred's first structured output format. These tests intentionally
 * check exact strings because the JSON object shape is part of the public
 * writer contract that future golden tests, replay tools, and integrations will
 * consume.
 *
 * Expected JSONL contract:
 *
 * raw move record:
 * - includes schema_version, layer, category, type, source, raw_mask, cookie
 * - includes escaped path text
 *
 * semantic rename record:
 * - includes old_path and new_path
 * - does not include raw-only fields when they are zero
 *
 * diagnostic recovery record:
 * - includes nested os_error, watch, and recovery objects only when populated
 *
 * Meaning:
 * The formatter writes one JSON object without a trailing newline. A later file,
 * socket, or ledger writer will decide line framing and I/O policy.
 */

#include "alfred_record_jsonl.h"

#include <assert.h>
#include <string.h>

static void test_jsonl_formats_raw_move_with_escaping(void)
{
    alfred_record_t record;
    char buffer[512];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_NORMALIZED_RAW;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_RAW_MOVED_FROM;
    record.source = 1u;
    record.raw_mask = 64u;
    record.cookie = 123u;
    record.path = "/tmp/root/a\"b\\c\nfile";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"normalized_raw\","
                  "\"category\":\"filesystem\","
                  "\"type\":\"RAW_MOVED_FROM\","
                  "\"source\":1,"
                  "\"raw_mask\":64,"
                  "\"cookie\":123,"
                  "\"path\":\"/tmp/root/a\\\"b\\\\c\\nfile\"}") == 0);
}

static void test_jsonl_formats_semantic_rename(void)
{
    alfred_record_t record;
    char buffer[512];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_RENAMED;
    record.old_path = "/tmp/root/old.txt";
    record.new_path = "/tmp/root/new.txt";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"semantic\","
                  "\"category\":\"filesystem\","
                  "\"type\":\"FILE_RENAMED\","
                  "\"old_path\":\"/tmp/root/old.txt\","
                  "\"new_path\":\"/tmp/root/new.txt\"}") == 0);
}

static void test_jsonl_formats_diagnostic_payloads(void)
{
    alfred_record_t record;
    char buffer[1024];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    record.category = ALFRED_RECORD_CATEGORY_RECOVERY;
    record.type = ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED;
    record.backend = "inotify";
    record.path = "/tmp/root";
    record.os_error.code = 2;
    record.os_error.name = "ENOENT";
    record.os_error.message = "No such file or directory";
    record.watch.watch_id = 7;
    record.watch.state = "stale";
    record.watch.reason = "IN_MOVE_SELF";
    record.watch.error = "path-unreachable";
    record.watch.retry_count = 3u;
    record.recovery.detail_path = "/tmp/root/missing";
    record.recovery.result_code = -1;
    record.recovery.pending_count = 4u;

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"diagnostic\","
                  "\"category\":\"recovery\","
                  "\"type\":\"WATCH_RESYNC_FAILED\","
                  "\"backend\":\"inotify\","
                  "\"path\":\"/tmp/root\","
                  "\"os_error\":{\"code\":2,"
                  "\"name\":\"ENOENT\","
                  "\"message\":\"No such file or directory\"},"
                  "\"watch\":{\"watch_id\":7,"
                  "\"state\":\"stale\","
                  "\"reason\":\"IN_MOVE_SELF\","
                  "\"error\":\"path-unreachable\","
                  "\"retry_count\":3},"
                  "\"recovery\":{\"detail_path\":\"/tmp/root/missing\","
                  "\"result_code\":-1,"
                  "\"pending_count\":4}}") == 0);
}

static void test_jsonl_rejects_invalid_input_and_truncation(void)
{
    alfred_record_t record;
    char tiny[8];
    char buffer[128];

    memset(&record, 0, sizeof(record));

    assert(alfred_record_format_jsonl(NULL, buffer, sizeof(buffer)) < 0);
    assert(alfred_record_format_jsonl(&record, NULL, sizeof(buffer)) < 0);
    assert(alfred_record_format_jsonl(&record, buffer, 0u) < 0);

    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/a.txt";
    assert(alfred_record_format_jsonl(&record, tiny, sizeof(tiny)) < 0);
}

int main(void)
{
    test_jsonl_formats_raw_move_with_escaping();
    test_jsonl_formats_semantic_rename();
    test_jsonl_formats_diagnostic_payloads();
    test_jsonl_rejects_invalid_input_and_truncation();

    return 0;
}
