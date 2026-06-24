/*
 * test_record_diagnostic_builder.c - focused test for WATCH_* record builders
 *
 * This test verifies the structured equivalent of the current WATCH_* text
 * diagnostics. Runtime still writes logger_event() lines, but Event Model v0
 * needs a typed representation for future text, JSONL, and binary writers.
 *
 * Expected record contract:
 *
 * WATCH_STALE diagnostic:
 * - layer=ALFRED_RECORD_LAYER_DIAGNOSTIC
 * - category=ALFRED_RECORD_CATEGORY_WATCH
 * - type=ALFRED_RECORD_TYPE_WATCH_STALE
 * - backend=<borrowed backend pointer>
 * - path=<borrowed path pointer>
 * - watch.watch_id=<wd>
 * - watch.state=<borrowed state pointer>
 * - watch.reason=<borrowed reason pointer>
 * - watch.error=NULL
 * - os_error.code=0
 * - os_error.name=NULL
 * - os_error.message=NULL
 *
 * WATCH_RESYNC_FAILED diagnostic:
 * - layer=ALFRED_RECORD_LAYER_DIAGNOSTIC
 * - category=ALFRED_RECORD_CATEGORY_RECOVERY
 * - type=ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED
 * - watch.error=<borrowed error pointer>
 * - optional os_error fields can preserve errno-style evidence separately
 *
 * Invalid type:
 * - semantic or raw record types are rejected
 *
 * Meaning:
 * WATCH_* records describe Alfred backend state. They must not become semantic
 * FILE_* or DIR_* events, and the builder must preserve borrowed string
 * ownership so it can be used on hot paths without allocation.
 */

#include "alfred_record_diagnostic.h"

#include <assert.h>
#include <stddef.h>

static void test_watch_stale_builds_watch_diagnostic(void)
{
    const char *backend = "inotify";
    const char *path = "/tmp/root/watched";
    const char *state = "stale";
    const char *reason = "IN_MOVE_SELF";
    alfred_record_t record;

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_STALE,
               backend,
               7,
               path,
               state,
               reason,
               NULL,
               &record) == 0);

    assert(record.schema_version == ALFRED_RECORD_SCHEMA_VERSION);
    assert(record.layer == ALFRED_RECORD_LAYER_DIAGNOSTIC);
    assert(record.category == ALFRED_RECORD_CATEGORY_WATCH);
    assert(record.type == ALFRED_RECORD_TYPE_WATCH_STALE);
    assert(record.backend == backend);
    assert(record.path == path);
    assert(record.watch.watch_id == 7);
    assert(record.watch.state == state);
    assert(record.watch.reason == reason);
    assert(record.watch.error == NULL);
    assert(record.os_error.code == 0);
    assert(record.os_error.name == NULL);
    assert(record.os_error.message == NULL);
}

static void test_stale_event_dropped_builds_watch_diagnostic(void)
{
    const char *backend = "inotify";
    const char *path = "/tmp/root/watched";
    const char *event_mask = "IN_CREATE";
    const char *event_name = "a.txt";
    alfred_record_t record;

    assert(alfred_record_build_stale_event_dropped(backend,
                                                   7,
                                                   path,
                                                   event_mask,
                                                   event_name,
                                                   &record) == 0);

    assert(record.schema_version == ALFRED_RECORD_SCHEMA_VERSION);
    assert(record.layer == ALFRED_RECORD_LAYER_DIAGNOSTIC);
    assert(record.category == ALFRED_RECORD_CATEGORY_WATCH);
    assert(record.type == ALFRED_RECORD_TYPE_WATCH_STALE_EVENT_DROPPED);
    assert(record.backend == backend);
    assert(record.path == path);
    assert(record.watch.watch_id == 7);
    assert(record.watch.event_mask == event_mask);
    assert(record.watch.event_name == event_name);
}

static void test_resync_failed_builds_recovery_diagnostic(void)
{
    const char *backend = "inotify";
    const char *path = "/tmp/root/watched";
    const char *reason = "IN_MOVE_SELF";
    const char *error = "identity-mismatch";
    alfred_record_t record;

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,
               backend,
               7,
               path,
               "stale",
               reason,
               error,
               &record) == 0);

    assert(record.layer == ALFRED_RECORD_LAYER_DIAGNOSTIC);
    assert(record.category == ALFRED_RECORD_CATEGORY_RECOVERY);
    assert(record.type == ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED);
    assert(record.backend == backend);
    assert(record.path == path);
    assert(record.watch.watch_id == 7);
    assert(record.watch.reason == reason);
    assert(record.watch.error == error);
}

static void test_resync_failed_can_carry_os_error(void)
{
    const char *backend = "inotify";
    const char *path = "/tmp/root/watched";
    const char *reason = "IN_MOVE_SELF";
    const char *error = "path-unreachable";
    const char *os_error_name = "ENOENT";
    const char *os_error_message = "No such file or directory";
    alfred_record_t record;

    assert(alfred_record_build_watch_diagnostic_with_os_error(
               ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED,
               backend,
               7,
               path,
               "stale",
               reason,
               error,
               2,
               os_error_name,
               os_error_message,
               &record) == 0);

    assert(record.layer == ALFRED_RECORD_LAYER_DIAGNOSTIC);
    assert(record.category == ALFRED_RECORD_CATEGORY_RECOVERY);
    assert(record.type == ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED);
    assert(record.backend == backend);
    assert(record.path == path);
    assert(record.watch.watch_id == 7);
    assert(record.watch.reason == reason);
    assert(record.watch.error == error);
    assert(record.os_error.code == 2);
    assert(record.os_error.name == os_error_name);
    assert(record.os_error.message == os_error_message);
}

static void test_invalid_inputs_are_rejected(void)
{
    alfred_record_t record;

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_RAW_CREATE,
               "inotify",
               1,
               "/tmp/root",
               NULL,
               NULL,
               NULL,
               &record) != 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_FILE_CREATED,
               "inotify",
               1,
               "/tmp/root",
               NULL,
               NULL,
               NULL,
               &record) != 0);

    assert(alfred_record_build_watch_diagnostic(
               ALFRED_RECORD_TYPE_WATCH_STALE,
               "inotify",
               1,
               "/tmp/root",
               NULL,
               NULL,
               NULL,
               NULL) != 0);
}

int main(void)
{
    test_watch_stale_builds_watch_diagnostic();
    test_stale_event_dropped_builds_watch_diagnostic();
    test_resync_failed_builds_recovery_diagnostic();
    test_resync_failed_can_carry_os_error();
    test_invalid_inputs_are_rejected();

    return 0;
}
