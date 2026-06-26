# Nightly exploratory scenarios

This directory contains reproducible exploratory scenarios used during nightly
audits. They are not part of the official `make test` contract yet.

Use them when you want to repeat what was tested during a previous audit,
inspect the generated logs, or decide whether a scenario should become an
official backend/core/JSONL golden test.

## Run model

Each script:

1. creates a temporary directory under `/tmp`;
2. writes an `alfred.conf` tailored to the scenario;
3. starts the local `alfred` binary;
4. performs filesystem operations;
5. stops Alfred;
6. checks the relevant logs;
7. prints the artifact directory.

Example:

```sh
tests/exploratory/nightly/base-jsonl-lifecycle.sh
```

To run the passing exploratory scenarios together:

```sh
tests/exploratory/nightly/run_all.sh
```

The known failing reproducer for issue `#30` is skipped by default. To include
it explicitly:

```sh
INCLUDE_KNOWN_FAILURES=1 tests/exploratory/nightly/run_all.sh
```

Artifacts are kept under `/tmp` so they can be inspected after the run.

## Scripts

| Script | Purpose |
| --- | --- |
| `run_all.sh` | Runs the passing exploratory scenarios and skips known failures by default. |
| `base-jsonl-lifecycle.sh` | File and directory create/modify/ready/delete with JSONL enabled. |
| `moves-jsonl.sh` | File and directory rename, move and relocate with JSONL enabled. |
| `lost-scope-jsonl.sh` | Lost-scope recovery diagnostics and recovered child create. |
| `audit-raw-events.sh` | Opt-in raw audit events: open, access and close-nowrite. |
| `recursive-fast-mkdir-p.sh` | Fast recursive `mkdir -p` synthetic create and watch discovery. |
| `root-file-should-fail.sh` | Known issue reproducer for regular-file root startup. |
| `upload_audit_artifacts.sh` | Zips `/tmp` audit artifacts, uploads them with `rclone`, and can update the parent issue. |

`root-file-should-fail.sh` is expected to fail until
<https://github.com/kinderp/alfred/issues/30> is fixed.

## What to inspect

| File | Meaning |
| --- | --- |
| `raw.log` | Normalized raw backend stream. |
| `events.log` | Semantic events and backend diagnostics. |
| `output.jsonl` | Structured public output when enabled. |
| `errors.log` | Alfred error log. |
| `alfred.stdout` | Process stdout. |
| `alfred.stderr` | Process stderr and sanitizer output, if any. |
| `assertions.log` | Scenario-level assertion failures. |

When a script fails, start from `assertions.log`, then compare `raw.log`,
`events.log` and `output.jsonl`.

## Upload audit artifacts

After an exploratory audit, upload the local `/tmp` artifact directories to
Google Drive and link them from the parent issue:

```sh
tests/exploratory/nightly/upload_audit_artifacts.sh \
  --date 2026-06-25 \
  --issue 29
```

The script expects `rclone` to have a configured Google Drive remote named
`gdrive:`. It creates `/tmp/alfred-nightly-audit-YYYY-MM-DD.zip`, uploads it to
`gdrive:alfred/audits/`, prints the Drive link, and comments on the issue when
`--issue` is provided.

## Promotion rule

Move a scenario from this directory into an official suite only when its
expected behavior is stable enough to be a contract:

- use a C test for internal ownership, queue, dispatcher or adapter contracts;
- use JSONL golden tests for public structured behavior;
- use text-log tests for compatibility diagnostics or human-facing log
  behavior.
