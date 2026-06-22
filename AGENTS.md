# Alfred instructions for AI coding agents

Alfred is evolving from a Linux filesystem event engine into an AI agent
runtime control layer. The current implementation focus is still narrower:
finish the Linux inotify reference backend, Backend API v0, Event Model v0,
structured records, adapters, writers and tests.

## Always read first

Before changing code or documentation, read:

1. `docs/it/00-regole-operative.md`
2. `docs/it/27-guida-lettura-documentazione.md`
3. `docs/it/documentation-progress.md`
4. `docs/it/31-milestone-inotify-reference-backend.md`

Do not read every Markdown file blindly. Use the reading guide to choose the
documents relevant to the task.

## Current implementation priority

The current priority is:

- finish the Linux inotify reference backend;
- stabilize Backend API v0;
- stabilize Event Model v0 and the common record model;
- introduce or complete structured output through writers/sinks;
- keep backend, adapter, core, dispatcher and writers separated;
- add or update tests and documentation with every behavioral change.

Do not start fanotify, eBPF, Windows, macOS, dashboard work or a full Agent
Guard implementation unless explicitly requested.

## Architectural rules

- Backends must not generate JSONL directly.
- Backends must not implement policy or final security decisions.
- JSONL is an output format, not the primary internal API.
- The primary internal contract is the Alfred structured record model.
- The inotify backend is the first reference backend, not the final product
  boundary.
- Do not make the common record model too Linux/inotify-specific.
- Keep changes small, documented and testable.

## Before modifying code

Answer briefly:

1. Which module am I touching?
2. Which contract could be affected?
3. Which docs must be read for this task?
4. Which tests must be run or added?
5. Is this current milestone scope or future roadmap?
