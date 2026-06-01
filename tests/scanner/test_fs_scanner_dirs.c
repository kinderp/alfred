/*
 * test_fs_scanner_dirs.c - helper for the directory-only scanner scenario
 *
 * The shell wrapper builds a small tree, then this program verifies the
 * scanner contracts that are easiest to check without starting the full Alfred
 * runtime: default directory-only output, root emission, depth limits, entry
 * limits, file inclusion, symlink exclusion, and explicit symlink emission.
 */

#include "fs_scanner.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Compact set of observations collected from the scanner callback. */
typedef struct {
    int count;
    int saw_root;
    int saw_a;
    int saw_b;
    int saw_c;
    int saw_volatile;
    int saw_symlink;
    int saw_symlink_as_dir;
    int saw_file;
    int remove_volatile;
    int callback_failed;
} scan_seen_t;

static int has_suffix(const char *s, const char *suffix)
{
    size_t s_len = strlen(s);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > s_len)
        return 0;

    return strcmp(s + s_len - suffix_len, suffix) == 0;
}

/* Record only the facts that define this scenario's expected output. */
static int on_entry(const fs_scan_entry_t *entry, void *userdata)
{
    scan_seen_t *seen = userdata;

    seen->count++;

    if (entry->depth == 0 && entry->type == FS_SCAN_DIR)
        seen->saw_root = 1;

    if (has_suffix(entry->path, "/a") &&
        entry->type == FS_SCAN_DIR) {
        seen->saw_a = 1;
    }

    if (has_suffix(entry->path, "/a/b") &&
        entry->type == FS_SCAN_DIR) {
        seen->saw_b = 1;
    }

    if (has_suffix(entry->path, "/c") &&
        entry->type == FS_SCAN_DIR) {
        seen->saw_c = 1;
    }

    if (has_suffix(entry->path, "/volatile") &&
        entry->type == FS_SCAN_DIR) {
        seen->saw_volatile = 1;

        if (seen->remove_volatile && rmdir(entry->path) != 0)
            seen->callback_failed = 1;
    }

    if (strstr(entry->path, "link_to_a") != NULL) {
        if (entry->type == FS_SCAN_SYMLINK)
            seen->saw_symlink = 1;
        if (entry->type == FS_SCAN_DIR)
            seen->saw_symlink_as_dir = 1;
    }

    if (strstr(entry->path, "file.txt") != NULL)
        seen->saw_file = 1;

    return 0;
}

static int run_scan(const char *root,
                    const fs_scan_options_t *opts,
                    scan_seen_t *seen)
{
    error_t rc = fs_scan_tree(root, opts, on_entry, seen);

    if (rc != ERR_OK) {
        fprintf(stderr, "scan failed rc=%d\n", rc);
        return 1;
    }

    if (seen->callback_failed) {
        fprintf(stderr, "callback failed\n");
        return 1;
    }

    return 0;
}

static int expect_default_dirs(const char *root)
{
    fs_scan_options_t opts;
    scan_seen_t seen;

    fs_scan_options_defaults(&opts);
    memset(&seen, 0, sizeof(seen));

    if (run_scan(root, &opts, &seen) != 0)
        return 1;

    if (!seen.saw_root || !seen.saw_a || !seen.saw_b || !seen.saw_c ||
        !seen.saw_volatile || seen.saw_symlink || seen.saw_symlink_as_dir ||
        seen.saw_file ||
        seen.count != 5) {

        fprintf(stderr,
                "default dirs mismatch count=%d root=%d a=%d b=%d c=%d "
                "volatile=%d symlink=%d symlink_dir=%d file=%d\n",
                seen.count,
                seen.saw_root,
                seen.saw_a,
                seen.saw_b,
                seen.saw_c,
                seen.saw_volatile,
                seen.saw_symlink,
                seen.saw_symlink_as_dir,
                seen.saw_file);
        return 1;
    }

    return 0;
}

static int expect_no_root(const char *root)
{
    fs_scan_options_t opts;
    scan_seen_t seen;

    fs_scan_options_defaults(&opts);
    opts.emit_root = 0;
    memset(&seen, 0, sizeof(seen));

    if (run_scan(root, &opts, &seen) != 0)
        return 1;

    if (seen.saw_root || !seen.saw_a || !seen.saw_b || !seen.saw_c ||
        !seen.saw_volatile || seen.count != 4) {
        fprintf(stderr,
                "emit_root=0 mismatch count=%d root=%d a=%d b=%d c=%d "
                "volatile=%d\n",
                seen.count,
                seen.saw_root,
                seen.saw_a,
                seen.saw_b,
                seen.saw_c,
                seen.saw_volatile);
        return 1;
    }

    return 0;
}

static int expect_max_depth_one(const char *root)
{
    fs_scan_options_t opts;
    scan_seen_t seen;

    fs_scan_options_defaults(&opts);
    opts.max_depth = 1;
    memset(&seen, 0, sizeof(seen));

    if (run_scan(root, &opts, &seen) != 0)
        return 1;

    if (!seen.saw_root || !seen.saw_a || seen.saw_b || !seen.saw_c ||
        !seen.saw_volatile || seen.count != 4) {
        fprintf(stderr,
                "max_depth=1 mismatch count=%d root=%d a=%d b=%d c=%d "
                "volatile=%d\n",
                seen.count,
                seen.saw_root,
                seen.saw_a,
                seen.saw_b,
                seen.saw_c,
                seen.saw_volatile);
        return 1;
    }

    return 0;
}

static int expect_max_entries_two(const char *root)
{
    fs_scan_options_t opts;
    scan_seen_t seen;

    fs_scan_options_defaults(&opts);
    opts.max_entries = 2;
    memset(&seen, 0, sizeof(seen));

    if (run_scan(root, &opts, &seen) != 0)
        return 1;

    if (seen.count != 2) {
        fprintf(stderr, "max_entries=2 mismatch count=%d\n", seen.count);
        return 1;
    }

    return 0;
}

static int expect_include_files(const char *root)
{
    fs_scan_options_t opts;
    scan_seen_t seen;

    fs_scan_options_defaults(&opts);
    opts.include_files = 1;
    memset(&seen, 0, sizeof(seen));

    if (run_scan(root, &opts, &seen) != 0)
        return 1;

    if (!seen.saw_root || !seen.saw_a || !seen.saw_b || !seen.saw_c ||
        !seen.saw_volatile || !seen.saw_file || seen.saw_symlink ||
        seen.saw_symlink_as_dir ||
        seen.count != 6) {
        fprintf(stderr,
                "include_files mismatch count=%d root=%d a=%d b=%d c=%d "
                "volatile=%d file=%d symlink=%d symlink_dir=%d\n",
                seen.count,
                seen.saw_root,
                seen.saw_a,
                seen.saw_b,
                seen.saw_c,
                seen.saw_volatile,
                seen.saw_file,
                seen.saw_symlink,
                seen.saw_symlink_as_dir);
        return 1;
    }

    return 0;
}

static int expect_include_symlinks(const char *root)
{
    fs_scan_options_t opts;
    scan_seen_t seen;

    fs_scan_options_defaults(&opts);
    opts.include_symlinks = 1;
    memset(&seen, 0, sizeof(seen));

    if (run_scan(root, &opts, &seen) != 0)
        return 1;

    if (!seen.saw_root || !seen.saw_a || !seen.saw_b || !seen.saw_c ||
        !seen.saw_volatile || !seen.saw_symlink ||
        seen.saw_symlink_as_dir || seen.saw_file || seen.count != 6) {
        fprintf(stderr,
                "include_symlinks mismatch count=%d root=%d a=%d b=%d c=%d "
                "volatile=%d symlink=%d symlink_dir=%d file=%d\n",
                seen.count,
                seen.saw_root,
                seen.saw_a,
                seen.saw_b,
                seen.saw_c,
                seen.saw_volatile,
                seen.saw_symlink,
                seen.saw_symlink_as_dir,
                seen.saw_file);
        return 1;
    }

    return 0;
}

static int expect_removed_child_directory_is_skipped(const char *root)
{
    fs_scan_options_t opts;
    scan_seen_t seen;

    fs_scan_options_defaults(&opts);
    memset(&seen, 0, sizeof(seen));
    seen.remove_volatile = 1;

    if (run_scan(root, &opts, &seen) != 0)
        return 1;

    if (!seen.saw_volatile) {
        fprintf(stderr, "volatile directory was not observed\n");
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s ROOT\n", argv[0]);
        return 2;
    }

    if (expect_default_dirs(argv[1]) != 0 ||
        expect_no_root(argv[1]) != 0 ||
        expect_max_depth_one(argv[1]) != 0 ||
        expect_max_entries_two(argv[1]) != 0 ||
        expect_include_files(argv[1]) != 0 ||
        expect_include_symlinks(argv[1]) != 0 ||
        expect_removed_child_directory_is_skipped(argv[1]) != 0) {
        return 1;
    }

    return 0;
}
