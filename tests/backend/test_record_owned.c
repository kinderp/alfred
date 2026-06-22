/*
 * test_record_owned.c - owned clone tests for Event Model v0 records
 *
 * This test fixes the contract needed before Alfred can enqueue records. The
 * runtime bridge currently consumes alfred_record_t synchronously, so borrowed
 * strings are safe. A queue or dispatcher will read records later, after backend
 * stack frames and temporary buffers may be gone; those records must own their
 * strings or use an equivalent lifetime strategy.
 *
 * Expected ownership contract:
 *
 * raw record clone:
 * - scalar fields are preserved
 * - path text is equal to the source path
 * - cloned path pointer is different from the borrowed source pointer
 *
 * semantic move clone:
 * - old_path and new_path are both deep-copied
 *
 * diagnostic clone:
 * - backend, path, os_error fields, watch fields, and recovery detail_path are
 *   deep-copied
 *
 * destroy:
 * - frees owned strings
 * - clears the record so stale owned pointers cannot be reused accidentally
 *
 * Meaning:
 * This is deliberately not connected to the backend hot path yet. It validates
 * the simple v0 ownership API before we decide whether the dispatcher will use
 * per-record deep copies, inline storage, pools/arenas, or a more advanced path
 * table after benchmarks.
 */

#include "alfred_record_owned.h"

#include <assert.h>
#include <string.h>

static void test_clone_raw_record_owns_path(void)
{
    alfred_record_t src;
    alfred_record_t owned;

    memset(&src, 0, sizeof(src));
    memset(&owned, 0, sizeof(owned));

    src.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    src.ts_ns = 42u;
    src.layer = ALFRED_RECORD_LAYER_NORMALIZED_RAW;
    src.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    src.type = ALFRED_RECORD_TYPE_RAW_CREATE;
    src.source = 1u;
    src.raw_mask = 257u;
    src.path = "/tmp/root/dir";

    assert(alfred_record_clone_owned(&src, &owned) == 0);

    assert(owned.schema_version == src.schema_version);
    assert(owned.ts_ns == src.ts_ns);
    assert(owned.layer == src.layer);
    assert(owned.category == src.category);
    assert(owned.type == src.type);
    assert(owned.source == src.source);
    assert(owned.raw_mask == src.raw_mask);
    assert(strcmp(owned.path, src.path) == 0);
    assert(owned.path != src.path);

    alfred_record_destroy_owned(&owned);
    assert(owned.path == NULL);
    assert(owned.type == 0);
}

static void test_clone_semantic_move_owns_both_paths(void)
{
    alfred_record_t src;
    alfred_record_t owned;

    memset(&src, 0, sizeof(src));
    memset(&owned, 0, sizeof(owned));

    src.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    src.layer = ALFRED_RECORD_LAYER_SEMANTIC;
    src.category = ALFRED_RECORD_CATEGORY_FILESYSTEM;
    src.type = ALFRED_RECORD_TYPE_FILE_RENAMED;
    src.old_path = "/tmp/root/old.txt";
    src.new_path = "/tmp/root/new.txt";

    assert(alfred_record_clone_owned(&src, &owned) == 0);

    assert(strcmp(owned.old_path, src.old_path) == 0);
    assert(strcmp(owned.new_path, src.new_path) == 0);
    assert(owned.old_path != src.old_path);
    assert(owned.new_path != src.new_path);
    assert(owned.path == NULL);

    alfred_record_destroy_owned(&owned);
}

static void test_clone_diagnostic_owns_nested_strings(void)
{
    alfred_record_t src;
    alfred_record_t owned;

    memset(&src, 0, sizeof(src));
    memset(&owned, 0, sizeof(owned));

    src.schema_version = ALFRED_RECORD_SCHEMA_VERSION;
    src.layer = ALFRED_RECORD_LAYER_DIAGNOSTIC;
    src.category = ALFRED_RECORD_CATEGORY_RECOVERY;
    src.type = ALFRED_RECORD_TYPE_WATCH_RESYNC_FAILED;
    src.backend = "inotify";
    src.path = "/tmp/root/watched";
    src.os_error.code = 2;
    src.os_error.name = "ENOENT";
    src.os_error.message = "No such file or directory";
    src.watch.watch_id = 7;
    src.watch.state = "stale";
    src.watch.reason = "IN_MOVE_SELF";
    src.watch.error = "path-unreachable";
    src.recovery.detail_path = "/tmp/root/watched/a";
    src.recovery.related_watch_id = 101;

    assert(alfred_record_clone_owned(&src, &owned) == 0);

    assert(strcmp(owned.backend, src.backend) == 0);
    assert(strcmp(owned.path, src.path) == 0);
    assert(strcmp(owned.os_error.name, src.os_error.name) == 0);
    assert(strcmp(owned.os_error.message, src.os_error.message) == 0);
    assert(strcmp(owned.watch.state, src.watch.state) == 0);
    assert(strcmp(owned.watch.reason, src.watch.reason) == 0);
    assert(strcmp(owned.watch.error, src.watch.error) == 0);
    assert(strcmp(owned.recovery.detail_path, src.recovery.detail_path) == 0);

    assert(owned.backend != src.backend);
    assert(owned.path != src.path);
    assert(owned.os_error.name != src.os_error.name);
    assert(owned.os_error.message != src.os_error.message);
    assert(owned.watch.state != src.watch.state);
    assert(owned.watch.reason != src.watch.reason);
    assert(owned.watch.error != src.watch.error);
    assert(owned.recovery.detail_path != src.recovery.detail_path);
    assert(owned.os_error.code == src.os_error.code);
    assert(owned.watch.watch_id == src.watch.watch_id);
    assert(owned.recovery.related_watch_id == src.recovery.related_watch_id);

    alfred_record_destroy_owned(&owned);
    assert(owned.backend == NULL);
    assert(owned.path == NULL);
    assert(owned.watch.reason == NULL);
    assert(owned.recovery.detail_path == NULL);
}

static void test_invalid_clone_input_fails(void)
{
    alfred_record_t src;
    alfred_record_t owned;

    memset(&src, 0, sizeof(src));
    memset(&owned, 0, sizeof(owned));

    assert(alfred_record_clone_owned(NULL, &owned) != 0);
    assert(alfred_record_clone_owned(&src, NULL) != 0);
    alfred_record_destroy_owned(NULL);
}

int main(void)
{
    test_clone_raw_record_owns_path();
    test_clone_semantic_move_owns_both_paths();
    test_clone_diagnostic_owns_nested_strings();
    test_invalid_clone_input_fails();

    return 0;
}
