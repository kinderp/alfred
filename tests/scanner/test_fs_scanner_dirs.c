/*
 * test_fs_scanner_dirs.c - helper for the directory-only scanner scenario
 *
 * The shell wrapper builds a small tree, then this program verifies the first
 * scanner contract: default options emit root and directories only, without
 * following directory symlinks and without reporting regular files.
 */

#include "fs_scanner.h"

#include <stdio.h>
#include <string.h>

/* Compact set of observations collected from the scanner callback. */
typedef struct {
    int count;
    int saw_root;
    int saw_a;
    int saw_b;
    int saw_symlink_dir;
    int saw_file;
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

    if (strstr(entry->path, "link_to_a") != NULL)
        seen->saw_symlink_dir = 1;

    if (strstr(entry->path, "file.txt") != NULL)
        seen->saw_file = 1;

    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s ROOT\n", argv[0]);
        return 2;
    }

    fs_scan_options_t opts;

    fs_scan_options_defaults(&opts);

    scan_seen_t seen;

    memset(&seen, 0, sizeof(seen));

    error_t rc = fs_scan_tree(argv[1], &opts, on_entry, &seen);

    if (rc != ERR_OK) {
        fprintf(stderr, "scan failed rc=%d\n", rc);
        return 1;
    }

    if (!seen.saw_root ||
        !seen.saw_a ||
        !seen.saw_b ||
        seen.saw_symlink_dir ||
        seen.saw_file ||
        seen.count != 3) {

        fprintf(stderr,
                "unexpected scan result count=%d root=%d a=%d b=%d "
                "symlink=%d file=%d\n",
                seen.count,
                seen.saw_root,
                seen.saw_a,
                seen.saw_b,
                seen.saw_symlink_dir,
                seen.saw_file);
        return 1;
    }

    return 0;
}
