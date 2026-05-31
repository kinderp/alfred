/* ============================================================================
 * fs_scanner.c - controlled filesystem tree scanner
 *
 * This module performs filesystem traversal only. It does not add inotify
 * watches, does not emit Alfred raw events, and does not decide semantic event
 * types. Those responsibilities belong to the inotify backend and core.
 * ============================================================================
 */

#include "fs_scanner.h"
#include "utils.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ============================================================================
 * Scan State
 * ============================================================================
 */

/*
 * fs_scan_state_t - private state for one scan operation
 *
 * The public API keeps traversal reentrant by storing all mutable counters and
 * callback state in this stack-allocated object. The scanner does not keep a
 * global cache and does not own semantic state; this is important because the
 * same helper must later be usable both by resync code and by a possible
 * command-line indexing mode.
 */
typedef struct fs_scan_state {
    /* Effective options for this scan, copied from the caller or defaults. */
    fs_scan_options_t opts;

    /* Caller callback invoked for each selected entry. */
    fs_scan_fn on_entry;

    /* Opaque callback context; borrowed and never interpreted by the scanner. */
    void *userdata;

    /* Number of entries delivered to the callback. */
    size_t emitted;

    /* Set when max_entries is reached or the callback asks to stop. */
    int stop;
} fs_scan_state_t;

/* ============================================================================
 * Type and Emission Helpers
 * ============================================================================
 */

/*
 * mode_to_type - translate POSIX mode bits to the scanner's coarse type
 * @mode: st_mode value returned by stat/fstatat
 *
 * The scanner intentionally collapses many filesystem kinds into
 * FS_SCAN_OTHER. Detailed semantic interpretation belongs above this layer.
 *
 * Return: scanner entry type matching @mode.
 */
static fs_scan_type_t mode_to_type(mode_t mode)
{
    if (S_ISDIR(mode))
        return FS_SCAN_DIR;
    if (S_ISREG(mode))
        return FS_SCAN_FILE;
    if (S_ISLNK(mode))
        return FS_SCAN_SYMLINK;

    return FS_SCAN_OTHER;
}

/*
 * should_emit - check whether a discovered entry should reach the callback
 * @opts: scan policy
 * @type: discovered entry type
 *
 * Emission is not the same as traversal. A future scan may avoid emitting
 * files while still walking directories in order to add watches or rebuild
 * directory state.
 *
 * Return: nonzero when @type should be emitted.
 */
static int should_emit(const fs_scan_options_t *opts, fs_scan_type_t type)
{
    switch (type) {
        case FS_SCAN_DIR:
            return opts->include_dirs;
        case FS_SCAN_FILE:
            return opts->include_files;
        case FS_SCAN_SYMLINK:
            return opts->include_symlinks;
        case FS_SCAN_OTHER:
            return opts->include_other;
    }

    return 0;
}

/*
 * emit_entry - deliver one selected entry to the caller
 * @state: active scan state
 * @path: full path for the entry
 * @type: coarse filesystem type
 * @depth: depth relative to the scan root
 * @st: stat data for the entry
 *
 * This helper centralizes include filters, max_entries handling, and callback
 * early-stop semantics. A nonzero callback result does not mean I/O failure; it
 * asks the traversal loop to stop cleanly.
 *
 * Return: ERR_OK. The current helper does not perform I/O.
 */
static error_t emit_entry(fs_scan_state_t *state,
                          const char *path,
                          fs_scan_type_t type,
                          int depth,
                          const struct stat *st)
{
    if (!should_emit(&state->opts, type))
        return ERR_OK;

    if (state->opts.max_entries > 0 &&
        state->emitted >= state->opts.max_entries) {
        state->stop = 1;
        return ERR_OK;
    }

    fs_scan_entry_t entry;

    entry.path = path;
    entry.type = type;
    entry.depth = depth;
    entry.st = st;

    state->emitted++;

    if (state->on_entry(&entry, state->userdata) != 0)
        state->stop = 1;

    return ERR_OK;
}

/* ============================================================================
 * Directory Traversal Helpers
 * ============================================================================
 */

/*
 * directory_open_flags - build open/openat flags for directory traversal
 * @opts: scan policy
 *
 * O_NOFOLLOW keeps the default scanner from crossing a directory symlink. This
 * matches the first resync use case: rebuild knowledge of the watched tree
 * without silently scanning another subtree through a link.
 *
 * Return: OR-ed open(2) flags for directory descriptors.
 */
static int directory_open_flags(const fs_scan_options_t *opts)
{
    int flags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;

    if (!opts->follow_symlinks)
        flags |= O_NOFOLLOW;

    return flags;
}

/*
 * scan_dir_fd - recursively scan an already-open directory
 * @state: active scan state
 * @path: printable path corresponding to @fd
 * @fd: open directory file descriptor; ownership is transferred to this helper
 * @depth: depth of @path relative to the scan root
 *
 * The implementation uses fdopendir(), readdir(), fstatat(), and openat().
 * fdopendir() takes ownership of @fd, so every path through this function must
 * close the DIR stream instead of closing @fd directly after that point.
 * Children are opened relative to the current directory descriptor to reduce
 * repeated absolute path resolution and to keep traversal logic close to the
 * directory entry that was just read.
 *
 * Return: ERR_OK on success or requested early stop, ERR_IO on traversal error.
 */
static error_t scan_dir_fd(fs_scan_state_t *state,
                           const char *path,
                           int fd,
                           int depth)
{
    if (state->stop)
        return ERR_OK;

    if (state->opts.max_depth >= 0 && depth > state->opts.max_depth)
        return ERR_OK;

    DIR *dir = fdopendir(fd);

    if (dir == NULL) {
        close(fd);
        return ERR_IO;
    }

    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL) {
        if (state->stop)
            break;

        if (strcmp(ent->d_name, ".") == 0 ||
            strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char child[PATH_MAX];

        if (path_join(child,
                      sizeof(child),
                      path,
                      ent->d_name) != 0) {
            closedir(dir);
            return ERR_IO;
        }

        struct stat st;
        int stat_flags = state->opts.follow_symlinks
                             ? 0
                             : AT_SYMLINK_NOFOLLOW;

        /*
         * A filesystem can change while it is being scanned. If an entry
         * disappears between readdir() and fstatat(), skip it for now rather
         * than failing the whole scan. The broader partial-error policy is still
         * documented as a future resync decision.
         */
        if (fstatat(dirfd(dir), ent->d_name, &st, stat_flags) != 0)
            continue;

        fs_scan_type_t type = mode_to_type(st.st_mode);
        int child_depth = depth + 1;

        /*
         * max_depth limits both emission and recursion. This keeps
         * max_depth = 0 meaningful: emit only the root when emit_root is set.
         */
        if (state->opts.max_depth >= 0 &&
            child_depth > state->opts.max_depth) {
            continue;
        }

        if (emit_entry(state, child, type, child_depth, &st) != ERR_OK)
            break;

        if (type == FS_SCAN_DIR &&
            (state->opts.max_depth < 0 ||
             child_depth < state->opts.max_depth)) {

            int child_fd =
                openat(dirfd(dir),
                       ent->d_name,
                       directory_open_flags(&state->opts));

            if (child_fd < 0) {
                closedir(dir);
                return ERR_IO;
            }

            error_t rc = scan_dir_fd(state, child, child_fd, child_depth);

            if (rc != ERR_OK) {
                closedir(dir);
                return rc;
            }
        }
    }

    closedir(dir);

    return ERR_OK;
}

/* ============================================================================
 * Public API
 * ============================================================================
 */

/*
 * fs_scan_options_defaults - initialize conservative scanner options
 * @opts: options object to initialize
 *
 * The default profile matches the first resync need: discover directories,
 * include the root path, avoid symbolic-link traversal, and leave depth/entry
 * limits disabled. Callers opt into files, symlinks, or bounded scans
 * explicitly.
 */
void fs_scan_options_defaults(fs_scan_options_t *opts)
{
    if (opts == NULL)
        return;

    memset(opts, 0, sizeof(*opts));

    /*
     * The initial scanner contract is intentionally conservative: emit only
     * directories, include the root, and do not follow symbolic links. Later
     * resync/indexing steps can opt into files or other entry kinds explicitly.
     */
    opts->include_dirs = 1;
    opts->include_files = 0;
    opts->include_symlinks = 0;
    opts->include_other = 0;
    opts->follow_symlinks = 0;
    opts->emit_root = 1;
    opts->max_depth = -1;
    opts->max_entries = 0;
}

/*
 * fs_scan_tree - scan a filesystem tree
 * @root: root path to scan
 * @opts: optional scan policy; NULL selects fs_scan_options_defaults()
 * @on_entry: callback invoked for selected entries
 * @userdata: opaque pointer passed unchanged to @on_entry
 *
 * This is a fact-gathering API. It does not add watches, emit raw events, or
 * decide semantic filesystem events. The callback receives borrowed entry data
 * that remains valid only until the callback returns.
 *
 * Return: ERR_OK on success or callback-requested stop, ERR_INVALID_ARG for
 * bad arguments, ERR_IO for root/traversal I/O failures.
 */
error_t fs_scan_tree(const char *root,
                     const fs_scan_options_t *opts,
                     fs_scan_fn on_entry,
                     void *userdata)
{
    if (root == NULL || on_entry == NULL)
        return ERR_INVALID_ARG;

    fs_scan_state_t state;

    memset(&state, 0, sizeof(state));

    if (opts != NULL)
        state.opts = *opts;
    else
        fs_scan_options_defaults(&state.opts);

    state.on_entry = on_entry;
    state.userdata = userdata;

    struct stat st;
    int stat_flags = state.opts.follow_symlinks ? 0 : AT_SYMLINK_NOFOLLOW;

    /*
     * Use lstat-like behavior when symlink following is disabled so the root is
     * classified as a symlink instead of as the target. Child entries use the
     * same policy through fstatat() in scan_dir_fd().
     */
    if (fstatat(AT_FDCWD, root, &st, stat_flags) != 0)
        return ERR_IO;

    fs_scan_type_t root_type = mode_to_type(st.st_mode);

    if (state.opts.emit_root) {
        error_t rc =
            emit_entry(&state, root, root_type, 0, &st);

        if (rc != ERR_OK || state.stop)
            return rc;
    }

    if (root_type != FS_SCAN_DIR)
        return ERR_OK;

    int fd = open(root, directory_open_flags(&state.opts));

    if (fd < 0)
        return ERR_IO;

    return scan_dir_fd(&state, root, fd, 0);
}
