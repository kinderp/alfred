# C Commenting Style

This document defines the commenting style used by this codebase.
It is intended for contributors who are reading or modifying the
project from scratch.

The goal is to keep comments useful, consistent, and maintainable.
Comments should explain design intent, ownership, invariants, error
handling, and integration boundaries. They should not translate the
code line by line.

## Language

All code comments must be written in English.

The project APIs, type names, and technical vocabulary are already in
English. Keeping comments in English avoids mixing languages inside the
source and makes the code easier to share with future contributors.

## General Rules

Write comments for readers who already know C but do not yet know this
codebase.

Prefer comments that explain:

- why a module exists
- what a function owns
- what lifetime rules apply
- what invariants must hold
- which errors are expected
- which behavior is temporary during integration

Avoid comments that merely repeat obvious code:

```c
/* Bad: increment i. */
i++;
```

Prefer comments that explain a non-obvious rule:

```c
/*
 * The event loop owns the inotify descriptor for the process lifetime.
 * Cleanup is centralized in app_shutdown() so partial initialization
 * failures can use the same release path.
 */
```

## Line Width and Visual Alignment

Comments should normally be wrapped at 78 columns.

When practical, multi-line block comments may be visually justified so
their text reaches a similar right edge. This is a readability preference,
not a correctness requirement. Do not add awkward spacing that makes a
sentence harder to edit or harder to read.

Good:

```c
/*
 * Signal handlers run in a restricted context. They must not allocate,
 * log, lock, or call non async-signal-safe functions.
 */
```

Acceptable when manually aligned:

```c
/*
 * Signal handlers run in a restricted context. They must not allocate,
 * log, lock, or call non async-signal-safe functions.
 */
```

Avoid:

```c
/*
 * Signal handlers run in a restricted context. They must not allocate,
 * log,   lock,   or call non async-signal-safe functions.
 */
```

Artificial spacing inside prose is fragile. It tends to break as soon as
one word changes, and most C formatters do not preserve full justification.
Use it only if it remains natural.

## File Headers

Each source or public header file should start with a short file header.
The header must describe the responsibility of the file and, when useful,
what the file deliberately does not own.

```c
/* ============================================================================
 * app.c - application lifecycle and event loop
 *
 * This file owns process-level initialization, signal handling, the main
 * inotify read loop, and shutdown ordering. It wires subsystems together
 * but should not contain filesystem event semantics.
 * ============================================================================
 */
```

## Section Headers

Use section headers to divide large files into logical regions.

```c
/* ============================================================================
 * Signal Handling
 * ============================================================================
 */
```

Section names should be short and specific. Do not create a section for
every small helper; sections are for navigation.

## Function Comments

Public functions should use a kernel-doc-like format:

```c
/*
 * app_init - initialize the application runtime
 * @app: application state to initialize
 * @argc: command-line argument count
 * @argv: command-line argument vector
 *
 * Initializes configuration, logging, watcher state, the inotify file
 * descriptor, and signal handling. The function uses one failure path so
 * partially initialized resources are released by app_shutdown().
 *
 * Return: ERR_OK on success, a negative error_t value on failure.
 */
int app_init(app_t *app, int argc, char **argv);
```

Static functions should use the same style when they have non-trivial
behavior, ownership rules, or error handling:

```c
/*
 * setup_signals - install cooperative termination handlers
 *
 * SIGINT and SIGTERM are converted into a shutdown request by clearing
 * app->running. The handler intentionally avoids I/O and allocation.
 */
static void setup_signals(void);
```

Trivial static helpers do not need a function comment if their name and
body are self-explanatory.

## Kernel-Doc-Like Format

The style is inspired by Linux kernel-doc, but this project does not yet
require strict kernel-doc tooling.

The important conventions are:

- Start with the function name, a hyphen, and a short summary.
- Document each parameter with `@name: description`.
- Add a blank comment line before the longer description.
- End with `Return:` for non-void functions.
- Use concrete ownership and lifetime language when relevant.

Example:

```c
/*
 * watcher_store - associate an inotify watch descriptor with a path
 * @wt: watcher table to update
 * @wd: watch descriptor returned by inotify_add_watch()
 * @path: path owned by the caller and copied into the table
 *
 * Expands the table when needed and stores a private copy of @path.
 * Existing entries for @wd are replaced.
 *
 * Return: 0 on success, -1 on allocation failure.
 */
```

This format is useful because it is structured, easy to scan, and can be
validated later by documentation tools if the project adopts them.

## Struct Comments

Important structs should be documented near their definition:

```c
/*
 * app_t - process-wide application state
 *
 * This object is the ownership root for the runtime. Backend code should
 * receive narrowed contexts instead of the whole app_t whenever possible.
 */
typedef struct app {
    volatile sig_atomic_t running; /* cleared to request shutdown */
    config_t config;
    inotify_backend_t inotify;     /* owns fd and watcher table */
    alfred_engine_t *core;
} app_t;
```

Use field comments only for fields whose role, ownership, or lifetime is
not obvious.

## Inline Comments

Inline comments should be rare. Use them for local invariants or unusual
error handling:

```c
/*
 * EAGAIN is expected for the nonblocking inotify descriptor. Sleep briefly
 * to avoid a busy loop, then poll again.
 */
if (errno == EAGAIN || errno == EWOULDBLOCK) {
```

Avoid trailing comments unless they clarify compact struct fields or enum
values. Long trailing comments make diffs noisy.

## TODO Comments

TODO comments must include a category:

```c
/*
 * TODO(core-integration): route raw inotify events through alfred_process().
 */
```

Preferred categories:

- `TODO(core-integration)`
- `TODO(portability)`
- `TODO(error-handling)`
- `TODO(test)`
- `TODO(performance)`
- `TODO(cleanup)`

TODO comments should describe the intended direction, not just the problem.

## Integration Boundary Comments

During the core integration, comments should clearly identify temporary
boundaries:

```c
/*
 * TODO(core-integration): this module still emits semantic events directly.
 * It should eventually emit only alfred_raw_event_t records and leave move
 * correlation, debounce, and semantic classification to the core.
 */
```

Use these comments sparingly. They should mark real architectural seams,
not every place that might change later.

## Formatter and Linter Notes

The project does not currently enforce comment style with tooling.

Future options:

- `clang-format` for code layout
- `clang-tidy` for static checks
- `cppcheck` for additional C diagnostics
- `codespell` for comment spelling
- a custom script for line width and required function comment checks

If formatting tools are introduced, they should be configured before a
large comment pass so the style does not churn later.
