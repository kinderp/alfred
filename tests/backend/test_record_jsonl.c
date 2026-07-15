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
 * - includes escaped path text for quote, backslash, newline, tab, carriage
 *   return, and generic control characters
 *
 * semantic rename record:
 * - includes old_path and new_path
 * - does not include raw-only fields when they are zero
 *
 * diagnostic recovery record:
 * - includes nested os_error, watch, and recovery objects only when populated
 *
 * metadata/session lifecycle record:
 * - admits diagnostic + lifecycle + SESSION_CONTEXT as stable public names
 * - does not imply that workspace/session payload fields or runtime emission
 *   are implemented yet
 * - serializes workspace and ledger payload objects only when their fields are
 *   present
 * - rejects session payload fields on non-SESSION_CONTEXT records
 *
 * identity:
 * - emits device_id and inode_id only as a complete pair
 * - omits partial identity evidence because one side alone is not a stable
 *   filesystem object identity
 *
 * Meaning:
 * The formatter writes one JSON object without a trailing newline. A later file,
 * socket, or ledger writer will decide line framing and I/O policy.
 */

#include "alfred_record_diagnostic.h"
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
    record.path = "/tmp/root/a\"b\\c\n\t\r\001file";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"normalized_raw\","
                  "\"category\":\"filesystem\","
                  "\"type\":\"RAW_MOVED_FROM\","
                  "\"source\":1,"
                  "\"raw_mask\":64,"
                  "\"cookie\":123,"
                  "\"path\":\"/tmp/root/a\\\"b\\\\c\\n\\t\\r\\u0001file\"}") == 0);
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

static void test_jsonl_formats_stale_event_dropped(void)
{
    alfred_record_t record;
    char buffer[1024];

    assert(alfred_record_build_stale_event_dropped("inotify",
                                                   7,
                                                   "/tmp/root/watched",
                                                   "IN_CREATE",
                                                   "a.txt",
                                                   &record) == 0);

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"diagnostic\","
                  "\"category\":\"watch\","
                  "\"type\":\"WATCH_STALE_EVENT_DROPPED\","
                  "\"backend\":\"inotify\","
                  "\"path\":\"/tmp/root/watched\","
                  "\"watch\":{\"watch_id\":7,"
                  "\"event_mask\":\"IN_CREATE\","
                  "\"event_name\":\"a.txt\"}}") == 0);
}

static void test_jsonl_formats_session_context_name_contract(void)
{
    alfred_record_t record;
    char buffer[512];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    record.category = ALFRED_RECORD_CATEGORY_LIFECYCLE;
    record.type = ALFRED_RECORD_TYPE_SESSION_CONTEXT;

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"diagnostic\","
                  "\"category\":\"lifecycle\","
                  "\"type\":\"SESSION_CONTEXT\"}") == 0);
}

static void test_jsonl_rejects_invalid_session_context_tuples(void)
{
    alfred_record_t record;
    char buffer[512];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_SESSION_CONTEXT;

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) < 0);

    record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    record.category = ALFRED_RECORD_CATEGORY_LIFECYCLE;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) < 0);

    record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    record.category = ALFRED_RECORD_CATEGORY_WATCH;
    record.type = ALFRED_RECORD_TYPE_SESSION_CONTEXT;

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) < 0);
}

static void test_jsonl_formats_complete_session_context_payload(void)
{
    alfred_record_t record;
    char buffer[1024];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    record.category = ALFRED_RECORD_CATEGORY_LIFECYCLE;
    record.type = ALFRED_RECORD_TYPE_SESSION_CONTEXT;
    record.session.workspace_root = "/home/antonio/alfred";
    record.session.workspace_id = "ws-alfred-local";
    record.session.ledger_session_id = "ls-2026-07-15-local";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"diagnostic\","
                  "\"category\":\"lifecycle\","
                  "\"type\":\"SESSION_CONTEXT\","
                  "\"workspace\":{\"root\":\"/home/antonio/alfred\","
                  "\"id\":\"ws-alfred-local\"},"
                  "\"ledger\":{\"session_id\":\"ls-2026-07-15-local\"}}") == 0);
}

static void test_jsonl_formats_partial_session_context_payload(void)
{
    alfred_record_t record;
    char buffer[1024];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    record.category = ALFRED_RECORD_CATEGORY_LIFECYCLE;
    record.type = ALFRED_RECORD_TYPE_SESSION_CONTEXT;
    record.session.ledger_session_id = "ls-only";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"diagnostic\","
                  "\"category\":\"lifecycle\","
                  "\"type\":\"SESSION_CONTEXT\","
                  "\"ledger\":{\"session_id\":\"ls-only\"}}") == 0);

    record.session.ledger_session_id = NULL;
    record.session.workspace_id = "ws-only";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"diagnostic\","
                  "\"category\":\"lifecycle\","
                  "\"type\":\"SESSION_CONTEXT\","
                  "\"workspace\":{\"id\":\"ws-only\"}}") == 0);
}

static void test_jsonl_session_context_escapes_and_omits_empty_fields(void)
{
    alfred_record_t record;
    char buffer[1024];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    record.category = ALFRED_RECORD_CATEGORY_LIFECYCLE;
    record.type = ALFRED_RECORD_TYPE_SESSION_CONTEXT;
    record.session.workspace_root = "/tmp/a\"b\\c\n";
    record.session.workspace_id = "";
    record.session.ledger_session_id = "";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"diagnostic\","
                  "\"category\":\"lifecycle\","
                  "\"type\":\"SESSION_CONTEXT\","
                  "\"workspace\":{\"root\":\"/tmp/a\\\"b\\\\c\\n\"}}") == 0);
    assert(strstr(buffer, "\"id\"") == NULL);
    assert(strstr(buffer, "\"ledger\"") == NULL);
    assert(strstr(buffer, "\"agent\"") == NULL);
    assert(strstr(buffer, "\"policy\"") == NULL);
    assert(strstr(buffer, "\"process\"") == NULL);
}

static void test_jsonl_rejects_session_payload_on_other_records(void)
{
    alfred_record_t record;
    char buffer[512];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/a.txt";
    record.session.workspace_id = "ws-not-allowed-here";

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) < 0);
}

static void test_jsonl_formats_complete_identity_only(void)
{
    alfred_record_t record;
    char buffer[512];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/a.txt";
    record.identity.device_id = 8;
    record.identity.inode_id = 123;

    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strcmp(buffer,
                  "{\"schema_version\":0,"
                  "\"layer\":\"semantic\","
                  "\"category\":\"filesystem\","
                  "\"type\":\"FILE_CREATED\","
                  "\"path\":\"/tmp/root/a.txt\","
                  "\"identity\":{\"device_id\":8,"
                  "\"inode_id\":123}}") == 0);
}

static void test_jsonl_omits_partial_identity(void)
{
    alfred_record_t record;
    char buffer[512];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/a.txt";

    record.identity.device_id = 0;
    record.identity.inode_id = 123;
    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strstr(buffer, "\"identity\"") == NULL);

    record.identity.device_id = 8;
    record.identity.inode_id = 0;
    assert(alfred_record_format_jsonl(&record, buffer, sizeof(buffer)) > 0);
    assert(strstr(buffer, "\"identity\"") == NULL);
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
    test_jsonl_formats_stale_event_dropped();
    test_jsonl_formats_session_context_name_contract();
    test_jsonl_rejects_invalid_session_context_tuples();
    test_jsonl_formats_complete_session_context_payload();
    test_jsonl_formats_partial_session_context_payload();
    test_jsonl_session_context_escapes_and_omits_empty_fields();
    test_jsonl_rejects_session_payload_on_other_records();
    test_jsonl_formats_complete_identity_only();
    test_jsonl_omits_partial_identity();
    test_jsonl_rejects_invalid_input_and_truncation();

    return 0;
}
