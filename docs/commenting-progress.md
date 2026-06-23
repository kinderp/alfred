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

- added `core/include/alfred_record_owned.h`,
  `core/src/alfred_record_owned.c`, and
  `tests/backend/test_record_owned.c` after introducing the preparatory owned
  record clone API. Comments now explain borrowed versus owned record lifetime,
  why asynchronous queues need an ownership boundary, and why the helper is not
  wired into the runtime hot path before dispatcher and benchmark decisions.
- added `core/include/alfred_record_queue.h`,
  `core/src/alfred_record_queue.c`, and
  `tests/backend/test_record_queue.c` after introducing the first bounded owned
  record queue. Comments now explain FIFO ownership transfer, bounded overflow,
  queue cleanup, and why the queue remains single-threaded and disconnected from
  the runtime path until dispatcher and benchmark policy are agreed.
- added `core/include/alfred_record_dispatcher.h`,
  `core/src/alfred_record_dispatcher.c`, and
  `tests/backend/test_record_dispatcher.c` after introducing the first bounded
  record dispatcher. Comments now explain bounded sink registration, synchronous
  v0 fan-out, sink class metadata reserved for future backpressure policy, and
  why the dispatcher does not own writer resources or run in the backend hot path.
- refreshed `core/include/alfred_record_dispatcher.h`,
  `core/src/alfred_record_dispatcher.c`, and added
  `tests/backend/test_record_dispatcher_drain.c` after introducing the first
  queue drain helper. Comments now explain the pop-dispatch-destroy lifecycle,
  the max_records batch limit, and why retry/requeue/dead-letter policy remains a
  future backpressure decision.
- refreshed `modules/inotify/src/watch_manager.c` and
  `tests/backend/test_record_text_writer.c` after routing the first simple
  runtime backend diagnostics, `WATCH_ADDED` and `WATCH_REMOVED`, through
  Event Model v0 records and the text formatter. Comments now explain why this
  is a behavior-neutral migration step toward Backend API v0 and why the
  fallback keeps the old payload stable.
- refreshed `modules/inotify/src/inotify_backend.c` after routing
  `WATCH_STALE` through Event Model v0 records and the text formatter. Comments
  now explain that WATCH_STALE remains backend watch-table reliability state,
  carries a reason such as IN_MOVE_SELF, and still preserves the old text
  payload for tests and users.
- refreshed `modules/inotify/src/inotify_backend.c` again after extracting
  `backend_log_watch_diagnostic_record()`. Comments now explain why the helper
  is a local bridge for record/formatter/fallback behavior, not the public
  Backend API v0 emit(record) boundary.
- refreshed `modules/inotify/src/inotify_backend.c` after routing logical
  `WATCH_RESYNC_FAILED` diagnostics through the shared record helper. Comments
  now document why the errno-bearing syscall branch remains on the direct text
  path until errno is represented in Event Model v0.
- documented the Event Model v0 OS-error policy in the Italian docs. The next
  C comment pass for `alfred_record_t` should describe separate Alfred error
  and OS error fields once they are added to the record structure.
- refreshed `core/include/alfred_record.h` and
  `tests/backend/test_record_diagnostic_builder.c` after adding
  `alfred_record_os_error_t`. Comments now explain why OS error evidence is
  separate from Alfred's stable diagnostic error token.
- refreshed `core/include/alfred_record_diagnostic.h`,
  `core/src/alfred_record_diagnostic.c`, and
  `tests/backend/test_record_diagnostic_builder.c` after adding
  `alfred_record_build_watch_diagnostic_with_os_error()`. Comments now explain
  how builders preserve OS error evidence separately from Alfred error tokens
  while keeping borrowed string ownership.
- refreshed `core/src/alfred_record_text.c` and
  `tests/backend/test_record_text_writer.c` after teaching the compatibility
  text formatter to render structured OS error fields as historical
  `errno=N` or `errno=N (message)` suffixes.
- refreshed `modules/inotify/src/inotify_backend.c` after routing
  errno-bearing `WATCH_RESYNC_FAILED` runtime diagnostics through the shared
  Event Model v0 diagnostic record bridge. Comments now explain why errno is
  OS evidence stored separately from Alfred's stable resync failure token.
- refreshed `core/include/alfred_record.h`, `core/src/alfred_record_text.c`,
  `core/src/alfred_record_diagnostic.c`,
  `modules/inotify/src/inotify_backend.c`, and
  `tests/backend/test_record_text_writer.c` after migrating local
  `WATCH_RESYNC_*` diagnostics to Event Model v0 records. Comments now explain
  the recovery payload fields used for scan counters, detail paths, rollback
  watch descriptors, and scan result codes.
- refreshed `core/include/alfred_record.h`, `core/src/alfred_record_text.c`,
  `core/src/alfred_record_diagnostic.c`, and
  `tests/backend/test_record_text_writer.c` after modeling all documented
  `WATCH_LOST_*` records. Comments now explain the recovery payload fields used
  for pending queue size, child prefix updates, watch counts, retry counters,
  and retry delays before the runtime lost-scope migration.
- refreshed `modules/inotify/src/inotify_backend.c` after adding
  `backend_log_lost_scope_record()` and routing `WATCH_LOST_QUEUED` plus
  `WATCH_LOST_SCAN_BEGIN` through Event Model v0 records. Comments now explain
  how the helper preserves the historical text contract while later call sites
  are migrated incrementally.
- refreshed `core/include/alfred_record.h`,
  `core/src/alfred_record_text.c`, `core/src/alfred_record_diagnostic.c`,
  `modules/inotify/src/inotify_backend.c`,
  `tests/backend/test_record_text_writer.c`, and Italian docs after routing
  the remaining runtime `WATCH_LOST_*` diagnostics through Event Model v0
  records. Comments now explain queue skipped/failed records and the
  lost-scope logging bridge fallback behavior.
- refreshed `core/include/alfred_record_adapter.h`,
  `core/src/alfred_record_adapter.c`, `app/src/core_logger.c`, and added
  `tests/backend/test_record_semantic_adapter.c` after wiring semantic core
  output through Event Model v0 records. Comments now explain why semantic
  events map to FILE_* / DIR_* record types, why core_logger keeps a fallback
  to the old formatter, and why output payloads must remain compatible.
- added `core/include/alfred_record_text.h`, `core/src/alfred_record_text.c`,
  and `tests/backend/test_record_text_writer.c` after introducing the first
  Event Model v0 text payload formatter. Comments now explain why the formatter
  owns only payload text, why timestamps and FILE streams remain logger/output
  device concerns, and how semantic, diagnostic, and normalized raw records are
  formatted without changing runtime behavior.
- added `core/include/alfred_record_diagnostic.h`,
  `core/src/alfred_record_diagnostic.c`, and
  `tests/backend/test_record_diagnostic_builder.c` after introducing the first
  Event Model v0 diagnostic builder. Comments now explain how WATCH_* types are
  classified as watch or recovery diagnostics, why raw/semantic types are
  rejected, and why borrowed string ownership is preserved for future hot-path
  writers.
- refreshed `core/include/alfred_record.h`, added
  `core/include/alfred_record_adapter.h`,
  `core/src/alfred_record_adapter.c`, and
  `tests/backend/test_record_raw_adapter.c` after introducing the first
  Event Model v0 raw adapter. Comments now explain why normalized raw record
  types use explicit `RAW_*` names, why the adapter must not promote
  `MOVED_FROM`/`MOVED_TO` to semantic move/rename outcomes, and why record
  string fields remain borrowed.
- refreshed `modules/inotify/src/inotify_backend.c`,
  `tests/backend/test_lost_scope_queue.c`, and
  `tests/backend/test_lost_scope_recovery.c` after the PR review fixes for
  lost-scope recovery. Comments now explain that queued filesystem identities are
  opaque data, and that runtime recovery can use an entry's scan_root for the
  first bounded scan even when configured roots are unavailable for fallback.
- refreshed `modules/inotify/src/inotify_backend.c`,
  `tests/backend/test_lost_scope_queue.c`, and
  `tests/backend/test_lost_scope_recovery.c` after adding the first
  synchronous due-entry processor. Comments now explain queue peeking,
  retry_after_ns maturity checks, FIFO stop-on-not-due behavior, and why
  retry/backoff requeue is intentionally left for the next step.
- refreshed `tests/backend/test_lost_scope_recovery.c` after adding the
  lost-scope reinstall failure scenario. The test comments now document the
  expected rollback diagnostics and why the scenario must keep the recovered
  subtree non-VALID when one missing watch cannot be installed.
- refreshed `modules/inotify/include/watcher.h`,
  `modules/inotify/src/watcher.c`,
  `modules/inotify/src/inotify_backend.c`,
  `tests/watcher/test_watcher_state.c`, and
  `tests/backend/test_lost_scope_recovery.c` after wiring positive
  all-or-stale lost-scope reinstall. Comments now explain subtree state repair,
  fake watch operations for deterministic tests, and why the runtime returns to
  VALID only after identity search, prefix repair, strict coverage scan, and
  complete watch reinstallation.
- refreshed `modules/inotify/src/inotify_backend.c` and
  `tests/backend/test_lost_scope_recovery.c` after adding strict coverage
  scanning to lost-scope recovery. Comments now explain the shared read-only
  subtree scan helper, the difference between local resync reinstallation and
  lost-scope measurement, and the expected WATCH_LOST_COVERAGE_* diagnostics,
  including per-path missing watch diagnostics.
- refreshed `modules/inotify/src/inotify_backend.c` and
  `tests/backend/test_lost_scope_recovery.c` after wiring
  watcher_update_path_prefix() into lost-scope recovery. Comments now explain
  why the runtime rewrites the recovered root and child prefixes in one
  validated table operation, why WATCH_LOST_PREFIX_UPDATED reports child count,
  and why the subtree still does not become VALID.
- refreshed `modules/inotify/include/watcher.h`,
  `modules/inotify/src/watcher.c`, and
  `tests/watcher/test_watcher_state.c` after adding
  watcher_update_path_prefix(). Comments now explain why subtree path repair
  must preserve state and identity, why prefix matching requires a slash
  boundary, and why the function validates all candidate rewrites before
  mutating the watcher table.
- refreshed `modules/inotify/src/inotify_backend.c` and
  `tests/backend/test_lost_scope_recovery.c` after documenting
  backend_lost_scope_recovery_result_t and backend_lost_scope_scan_context_t.
  Comments now explain each recovery outcome, every scan context field, and why
  WATCH_LOST_FOUND updates only the main watch path while leaving subtree
  validity for later steps.
- refreshed `modules/inotify/include/watcher.h`,
  `modules/inotify/src/watcher.c`, and
  `tests/watcher/test_watcher_state.c` after adding watcher_update_path().
  Comments now explain why lost-scope recovery needs a path-only update that
  preserves state and identity instead of reusing watcher_store_identity().
- refreshed `modules/inotify/src/inotify_backend.c` and added
  `tests/backend/test_lost_scope_recovery.c` after introducing synchronous
  lost-scope identity search. Comments document why the helper consumes one
  queued entry, why it scans exactly one caller-provided root, and why this
  micro-step reports FOUND/NOT_FOUND without updating watcher paths or
  reinstalling watches yet.
- refreshed `modules/inotify/src/inotify_backend.c`,
  `tests/backend/test_self_events_root_watch.sh`,
  `tests/backend/test_self_move_identity_match.sh`, and
  `tests/backend/test_self_move_identity_mismatch.sh` after wiring the
  lost-scope queue to local IN_MOVE_SELF recovery failures. Comments now
  explain which probe failures become delayed wide recovery, why successful
  identity-match resync must not enqueue anything, and why WATCH_LOST_QUEUED is
  diagnostic backend state rather than raw/core semantics.
- refreshed `modules/inotify/include/inotify_backend.h`,
  `modules/inotify/src/inotify_backend.c`, and
  `tests/backend/test_lost_scope_queue.c` after adding the first lost-scope
  recovery queue. Comments now explain why the queue is backend recovery state,
  why entries copy stale path/reason text, how FIFO growth preserves recovery
  order, and why the focused C test has no raw or event log output yet.
- refreshed `app/include/fs_scanner.h`, `app/src/fs_scanner.c`,
  `modules/inotify/src/inotify_backend.c`, and
  `tests/scanner/test_fs_scanner_dirs.c` after fixing the PR #7 scanner review
  findings. Comments now explain fd ownership on early stop and the
  `strict_child_errors` policy that lets generic scans remain best-effort while
  resync scans fail on partial subtree traversal.
- expanded `tests/backend/test_resync_reinstall_policy.c` with the same
  Expected log contract style used by shell backend diagnostics. The header now
  states the expected event stream, forbidden success log, and scenario meaning
  before the individual assertions.
- refreshed `modules/inotify/src/inotify_backend.c` and
  `tests/backend/test_resync_reinstall_policy.c` after adding
  WATCH_RESYNC_ROLLBACK diagnostics. Comments and tests now make clear that the
  rollback log is emitted before removing a watch installed during the failed
  resync attempt.
- expanded comments for the resync reinstall operation seam and
  `tests/backend/test_resync_reinstall_policy.c`. The code now documents why
  the internal helper type exists, why production still uses the real watch
  manager, how fake add/remove callbacks make rollback deterministic, and why
  the test uses a temporary logger stream instead of repository log files.
- refreshed `modules/inotify/src/inotify_backend.c` and added
  `tests/backend/test_resync_reinstall_policy.c` after introducing the
  resync watch operation seam. Comments explain why runtime uses the real
  watch manager, why the test passes fake operations, and how the rollback
  branch enforces the all-or-stale contract without a filesystem race.
- refreshed `modules/inotify/src/inotify_backend.c` after extracting the
  all-or-stale missing-watch reinstall policy into
  `backend_resync_reinstall_missing_watches()`. Comments now separate subtree
  scan orchestration from rollback/reinstall policy and document why rollback
  uses `watch_manager_remove()` diagnostics.
- refreshed `docs/it/21-roadmap-scanner-resync.md` with the explicit rollback
  test debt for multi-missing resync. The roadmap now records the untested
  failure branch, why a bash race test would be fragile, and why an isolated C
  helper is the preferred next option
- refreshed `modules/inotify/src/inotify_backend.c` after enabling full
  multi-missing watch reinstallation for the trusted IN_MOVE_SELF resync scope.
  Comments now document the all-or-stale policy, rollback of watches installed
  during a failed resync attempt, and why the parent returns VALID only after
  every missing watch has been restored
- refreshed `modules/inotify/src/inotify_backend.c` after changing the resync
  scan context from a single first-missing buffer to an owned dynamic list of
  missing paths. Comments now explain callback path lifetime, list ownership,
  and allocation-failure handling. This entry describes the preparatory step
  before the later full multi-missing reinstall loop
- refreshed `modules/inotify/src/inotify_backend.c` after documenting every
  `backend_resync_probe_result_t` value. The enum comments now explain when
  each stale-watch recovery outcome is used, why it remains backend diagnostic
  state, and how it relates to normalized WATCH_RESYNC_FAILED logs
- refreshed `modules/inotify/src/inotify_backend.c` after adding the first real
  watch reinstallation step. Comments document that the resync scan may now
  call `watch_manager_add()` only for the first missing path, while full
  multi-missing and partial-failure policy remains future work
- refreshed `modules/inotify/src/inotify_backend.c` after recording the first
  missing path observed by the dry-run resync scan. Comments and diagnostics now
  make clear that WATCH_RESYNC_SCAN_MISSING names a future reinstallation
  candidate, not a watch that has already been added
- refreshed `modules/inotify/src/inotify_backend.c` after adding read-only
  dry-run scan classification. Comments document the boundary between counting
  missing watches and deciding later whether watch reinstallation should run
- refreshed `modules/inotify/include/watcher.h`,
  `modules/inotify/src/watcher.c`, `modules/inotify/src/inotify_backend.c`,
  and `tests/watcher/test_watcher_state.c` after adding the read-only
  `watcher_has_path()` query. Comments document how scanner dry-runs can count
  already watched and missing directories without mutating kernel watches or the
  watcher table
- refreshed `modules/inotify/src/inotify_backend.c` after wiring the dry-run
  subtree scanner into the identity-match branch. Comments now explain that the
  scan runs only after the old path is proven to be the original object and
  remains diagnostic until watch reinstallation policy is implemented
- refreshed `modules/inotify/src/inotify_backend.c` after adding the dry-run
  scanner helper for future stale-watch subtree resync. Comments document why
  the helper is not wired into runtime yet, why it scans directories only with
  `emit_root = 0`, and why its callback only counts entries until a watch
  reinstallation policy is agreed
- refreshed `modules/inotify/src/inotify_backend.c` after separating the stale
  watch recovery wrapper from the identity probe. Comments now document that
  `backend_resync_watch()` is the future orchestration point, while
  `backend_probe_stale_watch_identity()` is only the current stat(2)-identity
  phase
- refreshed `modules/inotify/src/inotify_backend.c` after clarifying the
  identity-mismatch branch of the stale watch probe. Comments now explain that
  a reachable old path with a different `(st_dev, st_ino)` is a reused path, not
  proof that the stale watch can be repaired or that a replacement directory
  should be watched immediately
- refreshed `modules/inotify/src/watch_manager.c` after adding the pre-watch
  and post-watch stat(2) identity check. Comments now explain why the backend
  captures identity before inotify_add_watch(), validates it after the kernel
  watch is installed, and removes the watch if the path changed in between
- refreshed `modules/inotify/include/watcher.h`,
  `modules/inotify/src/watcher.c`,
  `modules/inotify/src/watch_manager.c`,
  `modules/inotify/src/inotify_backend.c`, and
  `tests/watcher/test_watcher_state.c` after adding watch filesystem identity.
  Comments document why `(st_dev, st_ino)` is captured, why path reachability is
  not enough after IN_MOVE_SELF, and how identity enables a guarded return to
  WATCHER_STATE_VALID
- refreshed `modules/inotify/src/inotify_backend.c` after splitting the resync
  probe result from the WATCH_RESYNC_FAILED formatter. Comments document the
  internal result enum, the normalized failure logger, and why these diagnostic
  tokens are not public semantic events
- refreshed `modules/inotify/src/inotify_backend.c` after adding the first
  conservative `backend_resync_watch()` probe. Comments document why the helper
  only verifies the old watched path, why failure keeps the watch STALE, and
  why the backend still avoids raw Alfred or semantic core events during resync
- refreshed `modules/inotify/include/watcher.h`,
  `modules/inotify/src/watcher.c`, and
  `tests/watcher/test_watcher_state.c` after adding
  `watcher_foreach_state()`. Comments document the read-only iterator contract,
  state filtering, early callback stop, and why REMOVED is not iterable
- refreshed `modules/inotify/include/watcher.h`,
  `modules/inotify/src/watcher.c`, and
  `tests/watcher/test_watcher_state.c` after adding
  `watcher_count_state()`. Comments now document that state counts are active
  watch diagnostics for future resync policy and that REMOVED slots are not
  counted as live watches
- refreshed `modules/inotify/src/inotify_backend.c` after wiring
  IN_DELETE_SELF to WATCHER_STATE_STALE. Comments document why delete-self is
  diagnostic state first, why child delete events are not invented, and why
  IN_IGNORED still owns final watch cleanup
- refreshed `modules/inotify/src/inotify_backend.c`,
  `modules/inotify/src/inotify_config.c`,
  `modules/inotify/include/inotify_config.h`, and
  `modules/inotify/src/watch_manager.c` after wiring IN_MOVE_SELF to
  WATCHER_STATE_STALE. Comments now document why the backend records stale
  state as diagnostics instead of inventing move/rename/relocation semantics
- expanded comments in `tests/watcher/test_watcher_state.c` so each watcher
  state transition is documented as part of the future IN_MOVE_SELF/resync
  contract, including the distinction between stale active mappings and removed
  slots
- refreshed `modules/inotify/include/watcher.h` and
  `modules/inotify/src/watcher.c` for the first stale-watch data model step.
  Comments now explain why reliability state lives next to the wd -> path
  mapping, how removed differs from stale, and why runtime semantics are still
  deferred to future backend policy
- removed the transitional watch-manager recursive discovery API from
  `modules/inotify/src/watch_manager.c` and
  `modules/inotify/include/watch_manager.h`. Comments now describe
  `fs_scan_tree()` as the single filesystem traversal primitive, with startup
  watch installation in the watch manager and runtime synthetic raw-event
  policy in the backend
- added the first scanner comment pass for `app/include/fs_scanner.h`,
  `app/src/fs_scanner.c`, and the scanner C test helper. The comments document
  scanner boundaries, callback lifetimes, traversal options, early-stop
  behavior, and the `openat()`/`fstatat()` traversal policy
- refreshed `app/src/fs_scanner.c` after the scanner options tests. The file
  now has explicit section headers, state-field comments, return contracts for
  non-trivial helpers, and local policy comments for disappearing entries and
  `max_depth` enforcement
- refreshed `app/src/fs_scanner.c` after the first partial-error policy step.
  The new child-open classifier documents which child directory failures are
  recoverable during resync and why root failures remain hard errors
- added and refreshed reviewed comments for the inotify backend configuration
  files `modules/inotify/include/inotify_config.h` and
  `modules/inotify/src/inotify_config.c`, including the textual watch-mask
  parser
- reinforced the core-only inotify backend comments in
  `modules/inotify/src/inotify_backend.c`, especially the boundary between raw
  facts, recursive watch recovery, synthetic raw directory creates, and core
  semantic/deduplication policy
- refreshed `modules/inotify/src/inotify_backend.c` after moving runtime
  recursive discovery to `fs_scan_tree()`. The new comments document why
  `emit_root = 0` preserves the real inotify create for the root and why only
  nested scanner directories receive synthetic raw create facts
- reinforced `modules/inotify/src/watch_manager.c` and
  `modules/inotify/include/watch_manager.h` comments so WATCH_ADDED,
  WATCH_REMOVED, recursive discovery facts, synthetic raw events, and core
  semantics remain separate concepts
- refreshed `modules/inotify/src/watch_manager.c` after the startup scanner
  adapter. The new comments describe how startup recursive watching consumes
  `fs_scan_tree()` directory facts without generating raw synthetic events,
  while runtime discovery remains on the existing callback path for now
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
| Done | `core/include/alfred_record_dispatcher.h` |
| Done | `core/include/alfred_record_owned.h` |
| Done | `core/include/alfred_record_queue.h` |
| Done | `core/src/alfred_correlator.c` |
| Done | `core/src/alfred_record_dispatcher.c` |
| Done | `core/src/alfred_record_owned.c` |
| Done | `core/src/alfred_record_queue.c` |
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

## Lost-Scope Retry Policy

Completed in this pass:

- expanded `modules/inotify/src/inotify_backend.c` comments around the
  due-entry processor, retry scheduling helper, retry delay helper, recovery
  result stringifier, and queue re-enqueue primitive
- updated `tests/backend/test_lost_scope_recovery.c` with expected log contract
  lines for `WATCH_LOST_RETRY_SCHEDULED` and
  `WATCH_LOST_RECOVERY_GAVE_UP`
- added scenario comments explaining why a `NOT_FOUND` result is retried with
  backoff and why retry-budget exhaustion is diagnostic rather than semantic

Completed in the scan-root pass:

- expanded `modules/inotify/include/inotify_backend.h` comments for
  `inotify_lost_scope_entry_t::scan_root`
- expanded `modules/inotify/src/inotify_backend.c` comments around the
  original temporary runtime fallback used before configured roots were owned by
  the backend

Completed in the configured-root pass:

- expanded `modules/inotify/include/inotify_backend.h` comments for
  `inotify_backend_t::configured_roots`,
  `configured_roots_count`, and `configured_roots_capacity`
- added detailed comments for backend configured-root registration, cleanup,
  prefix matching, and most-specific-root selection helpers
- updated `tests/backend/test_lost_scope_queue.c` comments to explain why
  configured roots are tested together with lost-scope queue primitives

Completed in the scan-root processor pass:

- expanded the `backend_lost_scope_process_due_with_ops()` comment to explain
  why due recovery uses each entry's stored `scan_root`
- added a dedicated `tests/backend/test_lost_scope_recovery.c` scenario comment
  for the case where the caller root is intentionally wrong but `scan_root`
  still drives a successful recovery

Completed in the configured-root fallback pass:

- expanded `backend_lost_scope_process_due_with_ops()` comments to explain the
  NOT_FOUND-only fallback across configured roots
- added a dedicated backend recovery test comment for a directory moved from one
  configured root to another configured root
- expanded `modules/inotify/src/watcher.c` comments in
  `watcher_update_path_prefix()` to explain why the suffix must be copied before
  rewriting `slot->path`

Completed in the runtime poll integration pass:

- updated `modules/inotify/include/inotify_backend.h` comments for
  `inotify_lost_scope_entry_t::scan_root` and `inotify_backend_poll()` so they
  describe the current configured-root fallback and synchronous poll batch
- added comments around `backend_lost_scope_process_due_runtime()` and its
  testable wrapper to explain why the first runtime integration is synchronous
  and why the poll batch is deliberately one entry
- added a dedicated `tests/backend/test_lost_scope_recovery.c` scenario comment
  for the idle poll path that processes a mature lost-scope entry on `EAGAIN`

Completed in the runtime lost-scope scenario pass:

- added a full expected-log contract to
  `tests/backend/test_lost_scope_runtime_recovery.sh`
- commented each assertion group to explain the runtime chain from
  `IN_MOVE_SELF` to `WATCH_LOST_QUEUED`, multi-root identity search,
  `WATCH_LOST_RECOVERY_END`, and the final proof `FILE_CREATED`
- updated the Italian scenario map and resync roadmap so the new test is tied
  to the backend recovery design, not treated as an isolated shell script

Completed in the first performance benchmark pass:

- added `tests/perf/bench_lost_scope_recovery.c` with comments that explain the
  manual benchmark contract, output columns, fake watch operations, and why the
  benchmark must not be used as a correctness or CI threshold
- added `tests/perf/run_lost_scope_recovery.sh` as the build/run wrapper for the
  manual lost-scope recovery benchmark
- updated Italian docs to describe `make perf-lost-scope` and the future
  performance-suite requirements for Alfred

Completed in the record text sink pass:

- added `core/include/alfred_record_sink.h` with comments for the generic
  `emit(record)` callback, borrowed-record ownership, and sink handle fields
- added `core/include/alfred_record_text_sink.h` with comments that explain the
  compatibility text sink, caller-owned buffer, write callback, and why the
  sink does not own `logger_t`
- added `core/src/alfred_record_sink.c` and
  `core/src/alfred_record_text_sink.c` with small, self-explanatory
  implementations that keep validation at the sink boundary
- added `tests/backend/test_record_text_sink.c` with an expected payload
  contract and scenario comments for successful emission, invalid sink setup,
  writer failure propagation, and truncation rejection

Completed in the core logger sink integration pass:

- updated `app/src/core_logger.c` comments to explain the application-local
  `write_event_payload()` bridge from text-sink payloads to `logger_event()`
- updated `app/include/core_logger.h` so the callback contract describes the
  current `alfred_event_t -> alfred_record_t -> text sink -> logger_event()`
  path instead of direct formatter use

Completed in the watch manager sink integration pass:

- updated `modules/inotify/src/watch_manager.c` comments so
  `WATCH_ADDED`/`WATCH_REMOVED` are described as the first backend diagnostics
  routed through `alfred_record_sink_t` and `alfred_record_text_sink_t`
- added a local bridge comment explaining why the watch manager still adapts
  text-sink payloads to `logger_event()` until the wider backend context owns a
  sink directly

Completed in the WATCH_STALE sink integration pass:

- updated `modules/inotify/src/inotify_backend.c` comments around the backend
  text-sink bridge and `backend_log_watch_stale()` so WATCH_STALE is described
  as backend reliability state routed through the shared sink boundary
- documented that the backend-local bridge is transitional until the inotify
  backend context owns a first-class record sink

Completed in the local resync sink integration pass:

- added comments for `backend_text_sink_context_t` and
  `backend_write_routed_payload()` in `modules/inotify/src/inotify_backend.c`
  so the event/error routing decision remains explicit
- updated `backend_log_resync_record()` comments to explain that
  `WATCH_RESYNC_SCAN_FAILED` keeps the historical error-log channel while other
  `WATCH_RESYNC_*` diagnostics use the event-log channel through the same text
  sink boundary

Completed in the lost-scope sink integration pass:

- updated `backend_log_lost_scope_record()` comments so `WATCH_LOST_*`
  diagnostics are described as record -> sink -> text sink output, with
  `WATCH_LOST_QUEUE_FAILED` preserving the historical error-log channel through
  the routed sink bridge

Completed in the RAW_CREATE sink integration pass:

- added `app/src/app.c` comments around the raw payload bridge and
  `log_raw_simple_record()` so the first normalized raw runtime migration is
  documented as `alfred_raw_event_t` -> record -> sink -> text sink -> raw log,
  while the original raw event still flows to `alfred_process()`
- added a backend test header documenting the expected kernel `IN_CREATE` lines
  and the normalized `RAW_CREATE path=... mask=...` lines asserted in
  `tests/backend/test_raw_create_record_sink.sh`

Completed in the RAW_DELETE sink integration pass:

- generalized the raw runtime bridge comments in `app/src/app.c` so simple
  create/delete raw facts are documented as the first path+mask records routed
  through the shared sink boundary before the original raw event reaches the
  core
- added a backend test header documenting the expected kernel `IN_DELETE` lines
  and the normalized `RAW_DELETE path=... mask=...` lines asserted in
  `tests/backend/test_raw_delete_record_sink.sh`

Completed in the RAW_ATTRIB sink integration pass:

- renamed the raw runtime bridge comments in `app/src/app.c` around the
  path+mask record helper so create/delete/attrib facts are documented as the
  currently migrated normalized raw records
- expanded `tests/backend/test_attrib_raw_log.sh` comments so chmod is
  described as metadata-only raw/backend evidence, with expected `IN_ATTRIB`
  and normalized `RAW_ATTRIB path=... mask=...` output

Completed in the RAW_MODIFY sink integration pass:

- extended the raw path+mask helper comments in `app/src/app.c` so
  create/delete/attrib/modify facts are documented as the currently migrated
  normalized raw records
- added `tests/backend/test_raw_modify_record_sink.sh` with a header explaining
  that `IN_MODIFY`/`RAW_MODIFY` is tested separately from
  `IN_CLOSE_WRITE`/`RAW_CLOSE_WRITE` because modify feeds `FILE_MODIFIED` while
  close-write feeds `FILE_READY`

Completed in the RAW_CLOSE_WRITE sink integration pass:

- extended the raw path+mask helper comments in `app/src/app.c` so
  create/delete/attrib/modify/close-write facts are documented as the currently
  migrated normalized raw records
- added `tests/backend/test_raw_close_write_record_sink.sh` with a header
  explaining that `IN_CLOSE_WRITE`/`RAW_CLOSE_WRITE` feeds `FILE_READY` and is
  tested separately from `RAW_MODIFY`
