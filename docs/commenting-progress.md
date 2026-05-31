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

Latest refresh:

- added the first scanner comment pass for `app/include/fs_scanner.h`,
  `app/src/fs_scanner.c`, and the scanner C test helper. The comments document
  scanner boundaries, callback lifetimes, traversal options, early-stop
  behavior, and the `openat()`/`fstatat()` traversal policy
- added and refreshed reviewed comments for the inotify backend configuration
  files `modules/inotify/include/inotify_config.h` and
  `modules/inotify/src/inotify_config.c`, including the textual watch-mask
  parser
- reinforced the core-only inotify backend comments in
  `modules/inotify/src/inotify_backend.c`, especially the boundary between raw
  facts, recursive watch recovery, synthetic raw directory creates, and core
  semantic/deduplication policy
- reinforced `modules/inotify/src/watch_manager.c` and
  `modules/inotify/include/watch_manager.h` comments so WATCH_ADDED,
  WATCH_REMOVED, recursive discovery facts, synthetic raw events, and core
  semantics remain separate concepts
- strengthened app/core/backend boundary comments in `app/include/app.h` and
  `app/src/app.c`
- clarified raw path lifetime, sequence-number purpose, and private core state
  ownership in `core/include/alfred_correlator.h`
- expanded the rationale for the private move/debounce tables in
  `core/src/alfred_tables.h` and `core/src/alfred_tables.c`

Completed in the first heavy pass:

- `modules/inotify/src/inotify_backend.c`
- `modules/inotify/include/inotify_backend.h`
- `core/src/alfred_correlator.c`
- `core/include/alfred_correlator.h`
- `core/src/alfred_tables.c`
- `core/src/alfred_tables.h`
- `core/src/alfred_utils.c`
- `core/src/alfred_utils.h`
- `core/examples/main_demo.c`
- `app/src/app.c` comment refresh only
- `modules/inotify/src/watch_manager.c`
- `modules/inotify/include/watch_manager.h`
- `modules/inotify/src/watcher.c`
- `modules/inotify/include/watcher.h`
- `modules/inotify/src/inotify_adapter.c` comment refresh only
- `app/src/config.c` comment refresh only
- `app/include/config.h` comment refresh only
- `modules/inotify/src/events.c` (removed legacy file)
- `modules/inotify/include/events.h` (removed legacy file)
- `modules/inotify/src/move_cache.c` (removed legacy file)
- `modules/inotify/include/move_cache.h` (removed legacy file)

## App

| Status | File |
| --- | --- |
| Done | `app/include/app.h` |
| Done | `app/include/config.h` |
| Done | `app/include/core_logger.h` |
| Done | `app/include/errors.h` |
| Done | `app/include/fs_scanner.h` |
| Done | `app/include/logger.h` |
| Done | `app/include/utils.h` |
| Done | `app/src/app.c` |
| Done | `app/src/config.c` |
| Done | `app/src/core_logger.c` |
| Done | `app/src/fs_scanner.c` |
| Done | `app/src/logger.c` |
| Done | `app/src/main.c` |
| Done | `app/src/utils.c` |

## Core

| Status | File |
| --- | --- |
| Done | `core/examples/main_demo.c` |
| Done | `core/include/alfred_correlator.h` |
| Done | `core/src/alfred_correlator.c` |
| Done | `core/src/alfred_tables.c` |
| Done | `core/src/alfred_tables.h` |
| Done | `core/src/alfred_utils.c` |
| Done | `core/src/alfred_utils.h` |

## Modules

| Status | File |
| --- | --- |
| Done | `modules/inotify/include/inotify_adapter.h` |
| Done | `modules/inotify/include/inotify_backend.h` |
| Done | `modules/inotify/include/inotify_config.h` |
| Done | `modules/inotify/include/watch_manager.h` |
| Done | `modules/inotify/include/watcher.h` |
| Done | `modules/inotify/src/inotify_adapter.c` |
| Done | `modules/inotify/src/inotify_backend.c` |
| Done | `modules/inotify/src/inotify_config.c` |
| Done | `modules/inotify/src/watch_manager.c` |
| Done | `modules/inotify/src/watcher.c` |

Removed legacy files that were documented during the migration:

- `modules/inotify/include/events.h`
- `modules/inotify/include/move_cache.h`
- `modules/inotify/src/events.c`
- `modules/inotify/src/move_cache.c`
