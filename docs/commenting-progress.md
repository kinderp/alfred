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

1. `modules/inotify/src/watch_manager.c`
2. `modules/inotify/src/inotify_adapter.c`
3. `app/src/config.c`
4. `modules/inotify/src/events.c`
5. `modules/inotify/src/move_cache.c`

Legacy files should be commented as legacy/shadow support, not as the future
runtime architecture.

Completed in the first heavy pass:

- `modules/inotify/src/inotify_backend.c`
- `modules/inotify/include/inotify_backend.h`
- `core/src/alfred_correlator.c`
- `core/include/alfred_correlator.h`
- `app/src/app.c` comment refresh only

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
| Pending | `modules/inotify/include/events.h` |
| Done | `modules/inotify/include/inotify_adapter.h` |
| Done | `modules/inotify/include/inotify_backend.h` |
| Pending | `modules/inotify/include/move_cache.h` |
| Pending | `modules/inotify/include/watch_manager.h` |
| Pending | `modules/inotify/include/watcher.h` |
| Pending | `modules/inotify/src/events.c` |
| Done | `modules/inotify/src/inotify_adapter.c` |
| Done | `modules/inotify/src/inotify_backend.c` |
| Pending | `modules/inotify/src/move_cache.c` |
| Pending | `modules/inotify/src/watch_manager.c` |
| Pending | `modules/inotify/src/watcher.c` |
