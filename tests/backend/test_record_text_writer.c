/*
 * test_record_text_writer.c - focused test for Event Model v0 text formatting
 *
 * This test verifies the behavior-neutral text writer foundation. The current
 * runtime still calls logger_event() directly, but future writers need to turn
 * structured alfred_record_t values into the same payloads that tests and
 * humans already understand.
 *
 * Expected payload contract:
 *
 * semantic single-path event:
 * - FILE_CREATED path=/tmp/root/a.txt
 *
 * semantic from/to event:
 * - FILE_RENAMED from=/tmp/root/old.txt to=/tmp/root/new.txt
 *
 * diagnostic watch event:
 * - WATCH_ADDED wd=7 path=/tmp/root/watched
 * - WATCH_REMOVED wd=7 path=/tmp/root/watched
 * - WATCH_STALE wd=7 path=/tmp/root/watched reason=IN_MOVE_SELF
 *
 * diagnostic recovery event:
 * - WATCH_RESYNC_FAILED wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF error=identity-mismatch
 * - WATCH_RESYNC_FAILED wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF error=path-unreachable
 *   errno=2 (No such file or directory)
 * - WATCH_RESYNC_FAILED wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF error=path-unreachable errno=13
 * - WATCH_RESYNC_SCAN_DONE wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF dirs=3 watched=1 missing=2
 * - WATCH_RESYNC_SCAN_CLASS wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF result=needs-reinstall
 * - WATCH_RESYNC_SCAN_MISSING wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF missing_path=/tmp/root/watched/a
 * - WATCH_RESYNC_REINSTALLED wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF installed_path=/tmp/root/watched/a
 * - WATCH_RESYNC_REINSTALL_FAILED wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF missing_path=/tmp/root/watched/b
 * - WATCH_RESYNC_ROLLBACK wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF removed_wd=101
 * - WATCH_RESYNC_END wd=7 path=/tmp/root/watched
 *   reason=IN_MOVE_SELF result=valid
 * - WATCH_LOST_QUEUED wd=7 path=/tmp/root/lost
 *   reason=IN_MOVE_SELF error=path-unreachable pending=1
 * - WATCH_LOST_QUEUE_SKIPPED wd=7 path=/tmp/root/lost
 *   reason=IN_MOVE_SELF error=missing-identity
 * - WATCH_LOST_QUEUE_FAILED wd=7 path=/tmp/root/lost
 *   reason=IN_MOVE_SELF error=path-unreachable
 * - WATCH_LOST_SCAN_BEGIN root=/tmp/root pending=0
 * - WATCH_LOST_FOUND wd=7 old_path=/tmp/root/lost
 *   new_path=/tmp/other/lost reason=IN_MOVE_SELF
 * - WATCH_LOST_PREFIX_UPDATED wd=7 old_prefix=/tmp/root/lost
 *   new_prefix=/tmp/other/lost children=2
 * - WATCH_LOST_COVERAGE_DONE wd=7 path=/tmp/other/lost
 *   reason=IN_MOVE_SELF dirs=3 watched=2 missing=1
 * - WATCH_LOST_COVERAGE_MISSING wd=7 path=/tmp/other/lost
 *   reason=IN_MOVE_SELF missing_path=/tmp/other/lost/a
 * - WATCH_LOST_COVERAGE_CLASS wd=7 path=/tmp/other/lost
 *   reason=IN_MOVE_SELF result=needs-reinstall
 * - WATCH_LOST_REINSTALLED wd=7 path=/tmp/other/lost
 *   reason=IN_MOVE_SELF installed_path=/tmp/other/lost/a
 * - WATCH_LOST_REINSTALL_FAILED wd=7 path=/tmp/other/lost
 *   reason=IN_MOVE_SELF missing_path=/tmp/other/lost/b
 * - WATCH_LOST_ROLLBACK wd=7 path=/tmp/other/lost
 *   reason=IN_MOVE_SELF removed_wd=101
 * - WATCH_LOST_NOT_FOUND wd=7 path=/tmp/root/lost
 *   reason=IN_MOVE_SELF retry=0
 * - WATCH_LOST_RETRY_SCHEDULED wd=7 path=/tmp/root/lost
 *   reason=IN_MOVE_SELF result=not-found retry=1 delay_ms=100 pending=1
 * - WATCH_LOST_RECOVERY_GAVE_UP wd=7 path=/tmp/root/lost
 *   reason=IN_MOVE_SELF result=not-found retries=8
 * - WATCH_LOST_RECOVERY_FAILED wd=7 path=/tmp/root/lost
 *   reason=IN_MOVE_SELF error=scan-failed
 * - WATCH_LOST_RECOVERY_END wd=7 path=/tmp/other/lost
 *   reason=IN_MOVE_SELF result=valid watches=3
 *
 * normalized raw event:
 * - RAW_CREATE path=/tmp/root/dir mask=<ALFRED_RAW_CREATE|ALFRED_RAW_ISDIR>
 *
 * Meaning:
 * The text writer formats only the payload. It does not write timestamps,
 * levels, files, or newlines; those concerns remain owned by the logger or by
 * a future output device.
 */

#include "alfred_record_adapter.h"
#include "alfred_record_diagnostic.h"
#include "alfred_record_text.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_semantic_single_path_payload(void)
{
    alfred_record_t record;
    char text[128];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/a.txt";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text, "FILE_CREATED path=/tmp/root/a.txt") == 0);
}

static void test_semantic_from_to_payload(void)
{
    alfred_record_t record;
    char text[160];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_RENAMED;
    record.old_path = "/tmp/root/old.txt";
    record.new_path = "/tmp/root/new.txt";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "FILE_RENAMED from=/tmp/root/old.txt to=/tmp/root/new.txt") ==
           0);
}

static void test_diagnostic_watch_payload(void)
{
    alfred_record_t record;
    char text[160];

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_ADDED,
               "inotify",
               7,
               "/tmp/root/watched",
               NULL,
               NULL,
               NULL,
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_ADDED wd=7 path=/tmp/root/watched") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_REMOVED,
               "inotify",
               7,
               "/tmp/root/watched",
               NULL,
               NULL,
               NULL,
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_REMOVED wd=7 path=/tmp/root/watched") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_STALE,
               "inotify",
               7,
               "/tmp/root/watched",
               "stale",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_STALE wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF") == 0);
}

static void test_diagnostic_recovery_payload(void)
{
    alfred_record_t record;
    char text[256];

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,
               "inotify",
               7,
               "/tmp/root/watched",
               "stale",
               "IN_MOVE_SELF",
               "identity-mismatch",
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_FAILED wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF error=identity-mismatch") == 0);

    assert(alfred_record_build_watch_diagnostic_with_os_error(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,
               "inotify",
               7,
               "/tmp/root/watched",
               "stale",
               "IN_MOVE_SELF",
               "path-unreachable",
               2,
               "ENOENT",
               "No such file or directory",
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_FAILED wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF error=path-unreachable "
                  "errno=2 (No such file or directory)") == 0);

    assert(alfred_record_build_watch_diagnostic_with_os_error(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,
               "inotify",
               7,
               "/tmp/root/watched",
               "stale",
               "IN_MOVE_SELF",
               "path-unreachable",
               13,
               "EACCES",
               NULL,
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_FAILED wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF error=path-unreachable errno=13") == 0);
}

static void test_diagnostic_resync_detail_payload(void)
{
    alfred_record_t record;
    char text[256];

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_DONE,
               "inotify",
               7,
               "/tmp/root/watched",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.directories_seen = 3u;
    record.recovery.directories_watched = 1u;
    record.recovery.directories_missing = 2u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_SCAN_DONE wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF dirs=3 watched=1 missing=2") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_CLASS,
               "inotify",
               7,
               "/tmp/root/watched",
               "needs-reinstall",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_SCAN_CLASS wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF result=needs-reinstall") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_SCAN_MISSING,
               "inotify",
               7,
               "/tmp/root/watched",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.detail_path = "/tmp/root/watched/a";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_SCAN_MISSING wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF missing_path=/tmp/root/watched/a") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALLED,
               "inotify",
               7,
               "/tmp/root/watched",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.detail_path = "/tmp/root/watched/a";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_REINSTALLED wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF installed_path=/tmp/root/watched/a") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_REINSTALL_FAILED,
               "inotify",
               7,
               "/tmp/root/watched",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.detail_path = "/tmp/root/watched/b";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_REINSTALL_FAILED wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF missing_path=/tmp/root/watched/b") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_ROLLBACK,
               "inotify",
               7,
               "/tmp/root/watched",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.related_watch_id = 101;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_ROLLBACK wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF removed_wd=101") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_END,
               "inotify",
               7,
               "/tmp/root/watched",
               "valid",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_RESYNC_END wd=7 path=/tmp/root/watched "
                  "reason=IN_MOVE_SELF result=valid") == 0);
}

static void test_diagnostic_lost_scope_payload(void)
{
    alfred_record_t record;
    char text[384];

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_QUEUED,
               "inotify",
               7,
               "/tmp/root/lost",
               NULL,
               "IN_MOVE_SELF",
               "path-unreachable",
               &record) == 0);
    record.recovery.pending_count = 1u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_QUEUED wd=7 path=/tmp/root/lost "
                  "reason=IN_MOVE_SELF error=path-unreachable pending=1") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_SKIPPED,
               "inotify",
               7,
               "/tmp/root/lost",
               NULL,
               "IN_MOVE_SELF",
               "missing-identity",
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_QUEUE_SKIPPED wd=7 path=/tmp/root/lost "
                  "reason=IN_MOVE_SELF error=missing-identity") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_QUEUE_FAILED,
               "inotify",
               7,
               "/tmp/root/lost",
               NULL,
               "IN_MOVE_SELF",
               "path-unreachable",
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_QUEUE_FAILED wd=7 path=/tmp/root/lost "
                  "reason=IN_MOVE_SELF error=path-unreachable") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_SCAN_BEGIN,
               "inotify",
               -1,
               "/tmp/root",
               NULL,
               NULL,
               NULL,
               &record) == 0);
    record.recovery.pending_count = 0u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text, "WATCH_LOST_SCAN_BEGIN root=/tmp/root pending=0") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_FOUND,
               "inotify",
               7,
               NULL,
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.old_path = "/tmp/root/lost";
    record.new_path = "/tmp/other/lost";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_FOUND wd=7 old_path=/tmp/root/lost "
                  "new_path=/tmp/other/lost reason=IN_MOVE_SELF") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_PREFIX_UPDATED,
               "inotify",
               7,
               NULL,
               NULL,
               NULL,
               NULL,
               &record) == 0);
    record.old_path = "/tmp/root/lost";
    record.new_path = "/tmp/other/lost";
    record.recovery.children_count = 2u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_PREFIX_UPDATED wd=7 old_prefix=/tmp/root/lost "
                  "new_prefix=/tmp/other/lost children=2") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_DONE,
               "inotify",
               7,
               "/tmp/other/lost",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.directories_seen = 3u;
    record.recovery.directories_watched = 2u;
    record.recovery.directories_missing = 1u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_COVERAGE_DONE wd=7 path=/tmp/other/lost "
                  "reason=IN_MOVE_SELF dirs=3 watched=2 missing=1") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_MISSING,
               "inotify",
               7,
               "/tmp/other/lost",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.detail_path = "/tmp/other/lost/a";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_COVERAGE_MISSING wd=7 path=/tmp/other/lost "
                  "reason=IN_MOVE_SELF missing_path=/tmp/other/lost/a") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_COVERAGE_CLASS,
               "inotify",
               7,
               "/tmp/other/lost",
               "needs-reinstall",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_COVERAGE_CLASS wd=7 path=/tmp/other/lost "
                  "reason=IN_MOVE_SELF result=needs-reinstall") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALLED,
               "inotify",
               7,
               "/tmp/other/lost",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.detail_path = "/tmp/other/lost/a";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_REINSTALLED wd=7 path=/tmp/other/lost "
                  "reason=IN_MOVE_SELF installed_path=/tmp/other/lost/a") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_REINSTALL_FAILED,
               "inotify",
               7,
               "/tmp/other/lost",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.detail_path = "/tmp/other/lost/b";

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_REINSTALL_FAILED wd=7 path=/tmp/other/lost "
                  "reason=IN_MOVE_SELF missing_path=/tmp/other/lost/b") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_ROLLBACK,
               "inotify",
               7,
               "/tmp/other/lost",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.related_watch_id = 101;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_ROLLBACK wd=7 path=/tmp/other/lost "
                  "reason=IN_MOVE_SELF removed_wd=101") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_NOT_FOUND,
               "inotify",
               7,
               "/tmp/root/lost",
               NULL,
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.watch.retry_count = 0u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_NOT_FOUND wd=7 path=/tmp/root/lost "
                  "reason=IN_MOVE_SELF retry=0") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_RETRY_SCHEDULED,
               "inotify",
               7,
               "/tmp/root/lost",
               "not-found",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.watch.retry_count = 1u;
    record.recovery.delay_ms = 100u;
    record.recovery.pending_count = 1u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_RETRY_SCHEDULED wd=7 path=/tmp/root/lost "
                  "reason=IN_MOVE_SELF result=not-found retry=1 "
                  "delay_ms=100 pending=1") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_GAVE_UP,
               "inotify",
               7,
               "/tmp/root/lost",
               "not-found",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.watch.retry_count = 8u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_RECOVERY_GAVE_UP wd=7 path=/tmp/root/lost "
                  "reason=IN_MOVE_SELF result=not-found retries=8") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_FAILED,
               "inotify",
               7,
               "/tmp/root/lost",
               NULL,
               "IN_MOVE_SELF",
               "scan-failed",
               &record) == 0);

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_RECOVERY_FAILED wd=7 path=/tmp/root/lost "
                  "reason=IN_MOVE_SELF error=scan-failed") == 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_LOST_RECOVERY_END,
               "inotify",
               7,
               "/tmp/other/lost",
               "valid",
               "IN_MOVE_SELF",
               NULL,
               &record) == 0);
    record.recovery.watches_count = 3u;

    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);
    assert(strcmp(text,
                  "WATCH_LOST_RECOVERY_END wd=7 path=/tmp/other/lost "
                  "reason=IN_MOVE_SELF result=valid watches=3") == 0);
}

static void test_normalized_raw_payload(void)
{
    alfred_raw_event_t raw;
    alfred_record_t record;
    char expected[128];
    char text[128];

    memset(&raw, 0, sizeof(raw));
    raw.ts_ns = 42u;
    raw.source = ALFRED_SRC_INOTIFY;
    raw.mask = ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR;
    raw.path = "/tmp/root/dir";

    assert(alfred_record_from_raw(&raw, &record) == 0);
    assert(alfred_record_format_text(&record, text, sizeof(text)) > 0);

    snprintf(expected,
             sizeof(expected),
             "RAW_CREATE path=/tmp/root/dir mask=%u",
             raw.mask);
    assert(strcmp(text, expected) == 0);
}

static void test_invalid_or_truncated_output_fails(void)
{
    alfred_record_t record;
    char too_small[4];

    memset(&record, 0, sizeof(record));
    record.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    record.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    record.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    record.type = ALFRED_RECORD_TYPE_FILE_CREATED;
    record.path = "/tmp/root/a.txt";

    assert(alfred_record_format_text(NULL, too_small, sizeof(too_small)) != 0);
    assert(alfred_record_format_text(&record, NULL, sizeof(too_small)) != 0);
    assert(alfred_record_format_text(&record, too_small, 0u) != 0);
    assert(alfred_record_format_text(&record,
                                     too_small,
                                     sizeof(too_small)) != 0);
}

int main(void)
{
    test_semantic_single_path_payload();
    test_semantic_from_to_payload();
    test_diagnostic_watch_payload();
    test_diagnostic_recovery_payload();
    test_diagnostic_resync_detail_payload();
    test_diagnostic_lost_scope_payload();
    test_normalized_raw_payload();
    test_invalid_or_truncated_output_fails();

    return 0;
}
