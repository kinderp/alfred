/* ============================================================================
 * fs_scanner.h - filesystem tree scanner API
 *
 * The scanner is a backend-neutral helper for controlled filesystem walks. It
 * is intended for future resync/indexing work, not for semantic event
 * classification. Callers receive observed filesystem entries through a
 * callback and decide what those facts mean.
 * ============================================================================
 */

#ifndef FS_SCANNER_H
#define FS_SCANNER_H

#include "errors.h"

#include <stddef.h>
#include <sys/stat.h>

/*
 * fs_scan_type_t - coarse filesystem entry class
 *
 * The scanner reports filesystem facts, not Alfred events. These values are
 * deliberately simpler than inotify masks and semantic event kinds: they only
 * describe what was found while walking the tree. The caller decides whether a
 * directory should become a watch, whether a file should be indexed, or whether
 * an entry should be ignored.
 */
typedef enum {
    FS_SCAN_DIR = 1,
    FS_SCAN_FILE,
    FS_SCAN_SYMLINK,
    FS_SCAN_OTHER
} fs_scan_type_t;

/*
 * fs_scan_entry_t - entry observed during a filesystem scan
 * @path: full path assembled by the scanner for this entry
 * @type: coarse entry type derived from struct stat mode bits
 * @depth: distance from the scan root; the root itself has depth 0
 * @st: stat data collected for the entry
 *
 * The callback receives borrowed pointers. @path and @st are valid only while
 * the callback is running; callers that need to keep them must copy the data.
 */
typedef struct {
    const char *path;
    fs_scan_type_t type;
    int depth;
    const struct stat *st;
} fs_scan_entry_t;

/*
 * fs_scan_fn - scanner callback
 * @entry: selected filesystem entry
 * @userdata: opaque pointer supplied to fs_scan_tree()
 *
 * Return zero to continue the scan. Return nonzero to request an early stop.
 * Early stop is reported as ERR_OK by fs_scan_tree(), because bounded scans are
 * a valid use case for resync and future indexing commands.
 */
typedef int (*fs_scan_fn)(const fs_scan_entry_t *entry, void *userdata);

/*
 * fs_scan_options_t - filesystem scan policy
 *
 * The include_* fields select which entry classes are delivered to the
 * callback. Traversal is separate from emission: directories may still be
 * opened to continue the walk even when the caller does not ask to emit every
 * possible entry type.
 */
typedef struct {
    /* Emit directory entries. Enabled by default for recursive watch setup. */
    int include_dirs;

    /* Emit regular file entries. Disabled by default in the first resync step. */
    int include_files;

    /* Emit symbolic link entries. Disabled by default to avoid link surprises. */
    int include_symlinks;

    /* Emit sockets, fifos, devices, and other non-regular entries. */
    int include_other;

    /*
     * Follow symbolic links while statting/opening entries. Disabled by default
     * so a scan remains inside the physical tree chosen by the caller.
     */
    int follow_symlinks;

    /*
     * Treat child traversal errors as scan failures. Disabled by default for
     * generic/indexing scans, but enabled by resync callers that need complete
     * subtree coverage before trusting the result.
     */
    int strict_child_errors;

    /* Emit the root path before scanning its children. Enabled by default. */
    int emit_root;

    /*
     * Maximum depth to emit and traverse. Use -1 for unlimited depth; use 0 to
     * emit only the root when emit_root is enabled.
     */
    int max_depth;

    /*
     * Maximum number of callback entries. Use 0 for no limit. Reaching the
     * limit stops the scan successfully before emitting an extra entry.
     */
    size_t max_entries;
} fs_scan_options_t;

/*
 * fs_scan_options_defaults - initialize conservative scanner options
 * @opts: options object to initialize
 *
 * Defaults are tuned for the first resync use case: visit directories only,
 * include the root, do not follow symlinks, and apply no depth/entry limit.
 */
void fs_scan_options_defaults(fs_scan_options_t *opts);

/*
 * fs_scan_tree - scan a filesystem tree
 * @root: root path to scan
 * @opts: scanner options, or NULL for defaults
 * @on_entry: callback invoked for selected entries
 * @userdata: opaque callback context
 *
 * The callback may return nonzero to stop the scan early. Early stop is treated
 * as success because it is useful for bounded indexing and future resync
 * policies.
 *
 * Return: ERR_OK on success or early stop, ERR_INVALID_ARG or ERR_IO on error.
 */
error_t fs_scan_tree(const char *root,
                     const fs_scan_options_t *opts,
                     fs_scan_fn on_entry,
                     void *userdata);

#endif
