# Commenting Progress

This file tracks which C source and header files have been reviewed and
commented according to `docs/commenting-style.md`.

Status values:

- `Done`: reviewed and commented according to the project style.
- `Partial`: contains useful comments but still needs a style pass.
- `Pending`: not yet reviewed for the project comment style.

## Next Heavy Pass

The next integration step is a documentation-heavy pass before more structural
changes. The priority is to document ownership, integration boundaries,
temporary legacy paths, and the raw-to-semantic event flow.

Recommended order:

The current heavy pass has covered the main integration files. Future passes
should revisit any file touched by structural changes and keep comments aligned
with the final core-only architecture.

Legacy files should be commented as legacy/shadow support, not as the future
runtime architecture.

Completed in the first heavy pass:

- `modules/inotify/src/inotify_backend.c`
- `modules/inotify/include/inotify_backend.h`
- `core/src/alfred_correlator.c`
- `core/include/alfred_correlator.h`
- `app/src/app.c` comment refresh only
- `modules/inotify/src/watch_manager.c`
- `modules/inotify/include/watch_manager.h`
- `modules/inotify/src/watcher.c`
- `modules/inotify/include/watcher.h`
- `modules/inotify/src/inotify_adapter.c` comment refresh only
- `app/src/config.c` comment refresh only
- `app/include/config.h` comment refresh only
- `modules/inotify/src/events.c`
- `modules/inotify/include/events.h`
- `modules/inotify/src/move_cache.c`
- `modules/inotify/include/move_cache.h`

## App

| Status | File |
| --- | --- |
| Done | `app/include/app.h` |
| Done | `app/include/config.h` |
| Done | `app/include/core_logger.h` |
| Done | `app/include/errors.h` |
| Done | `app/include/logger.h` |
| Done | `app/include/utils.h` |
| Done | `app/src/app.c` |
| Done | `app/src/config.c` |
| Done | `app/src/core_logger.c` |
| Done | `app/src/logger.c` |
| Done | `app/src/main.c` |
| Done | `app/src/utils.c` |

## Core

| Status | File |
| --- | --- |
| Pending | `core/examples/main_demo.c` |
| Done | `core/include/alfred_correlator.h` |
| Done | `core/src/alfred_correlator.c` |
| Pending | `core/src/alfred_tables.c` |
| Pending | `core/src/alfred_tables.h` |
| Pending | `core/src/alfred_utils.c` |
| Pending | `core/src/alfred_utils.h` |

## Modules

| Status | File |
| --- | --- |
| Done | `modules/inotify/include/events.h` |
| Done | `modules/inotify/include/inotify_adapter.h` |
| Done | `modules/inotify/include/inotify_backend.h` |
| Done | `modules/inotify/include/move_cache.h` |
| Done | `modules/inotify/include/watch_manager.h` |
| Done | `modules/inotify/include/watcher.h` |
| Done | `modules/inotify/src/events.c` |
| Done | `modules/inotify/src/inotify_adapter.c` |
| Done | `modules/inotify/src/inotify_backend.c` |
| Done | `modules/inotify/src/move_cache.c` |
| Done | `modules/inotify/src/watch_manager.c` |
| Done | `modules/inotify/src/watcher.c` |
