# Alfred

Alfred is a Linux filesystem event engine written in C.

It watches one or more directory trees with `inotify`, records backend raw
facts, and emits a semantic event stream through a dedicated core engine.

The project is currently documented mostly in Italian because it is also used
as a teaching codebase. An Italian translation of this public README is
available in [README.it.md](README.it.md).

## Status

Alfred is in active development.

The current runtime path is:

```text
Linux inotify -> Alfred raw event -> Alfred core semantic event
```

The historical legacy/shadow comparison path has been removed from the active
runtime. The core engine is now the official event engine.

## Features

- recursive directory watching with Linux `inotify`
- backend raw event logging
- semantic file and directory events
- file create, delete, modify, and close-write ready events
- directory create and delete events
- move, rename, and relocated correlation
- backend diagnostics for watch addition and removal
- core end-to-end test suite
- backend diagnostic test suite
- GitHub Actions CI for pull requests and `main`

## Requirements

- Linux
- `make`
- a C compiler toolchain, such as `gcc`
- standard shell tools used by the test scripts

On Debian or Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y build-essential
```

Running the staged-install test also requires `man`:

```bash
sudo apt-get install -y man-db
```

## Quick Start

Clone and build:

```bash
git clone https://github.com/kinderp/alfred.git
cd alfred
make
```

Build the release profile and install the binary plus the six English/Italian
manual pages:

```bash
make release
sudo make install
```

`make install` is deliberately copy-only: it never compiles as root and fails
unless an already built readable executable is available. The default prefix
is `/usr/local`. Package builders and local checks can stage without root:

```bash
stage_dir=$(mktemp -d)
make release
make DESTDIR="$stage_dir" PREFIX=/usr install
make DESTDIR="$stage_dir" PREFIX=/usr uninstall
```

`uninstall` removes only the Alfred binary and six manual pages. It does not
remove shared directories or user configuration and runtime logs.

Run Alfred on one or more paths:

```bash
./alfred /path/to/watch
```

Show short usage and version information without starting the runtime:

```bash
./alfred --help
./alfred -h
./alfred --version
./alfred -V
```

Validate the current configuration without starting the runtime:

```bash
./alfred --check-config
ALFRED_CONFIG=./alfred.conf ./alfred --check-config
./alfred -c ./alfred.conf --check-config
./alfred --config ./alfred.conf --check-config
./alfred --config=./alfred.conf --check-config
```

Run Alfred with a configuration file:

```bash
ALFRED_CONFIG=./alfred.conf ./alfred /path/to/watch
./alfred -c ./alfred.conf /path/to/watch
./alfred --config ./alfred.conf /path/to/watch
./alfred --config=./alfred.conf /path/to/watch
./alfred -- /path/to/watch
```

Minimal optional JSONL output configuration:

```text
output_enabled=false
output_format=jsonl
output_buffer_size=65536
output_log=output.jsonl
```

The default keeps the current logs unchanged. Setting `output_enabled=true`
adds JSONL output for the currently wired record path while preserving the
compatibility logs.

Configuration details are documented in Italian for now:
[application layer](docs/it/04-livello-applicazione.md) and
[debugging, tests, and tools](docs/it/10-debugging-test-e-strumenti.md).

Stop it with `Ctrl+C`.

## Logs

Alfred writes three runtime logs in the current working directory:

```text
raw.log
events.log
errors.log
```

`raw.log` contains backend raw information from the inotify layer.

`events.log` contains semantic events emitted by the core and backend diagnostic
events such as `WATCH_ADDED` and `WATCH_REMOVED`.

`errors.log` contains error diagnostics.

Runtime logs are ignored by Git and should not be committed.

## Testing

Build the project:

```bash
make
```

Run the official core end-to-end suite:

```bash
make test
```

Run CLI tests:

```bash
make test-cli
```

Run the short MVP smoke test:

```bash
make smoke-mvp
```

This builds Alfred, checks `--help`, `--version` and `--check-config`, runs the
runtime on a temporary directory with JSONL output enabled, creates a file,
renames it, creates a directory, and validates `raw.log`, `events.log` and
`output.jsonl`. It is a user-facing sanity check, not a replacement for the
full core, backend or JSONL suites.

Run backend diagnostics:

```bash
make test-backend-diagnostics
```

Run scanner component tests:

```bash
make test-scanner
```

Run the non-root staged-install contract suite:

```bash
make test-install
```

This test rebuilds the release profile, checks default and overridden install
layouts, runs the staged informational CLI, renders all six manual pages, and
verifies preflight and uninstall ownership. It is intentionally the final CI
suite because it leaves the checkout in the release build profile.

The CI workflow runs build, core, CLI, MVP smoke, backend diagnostic, JSONL,
and staged-install tests on pull requests targeting `main` and on pushes to
`main`.

To preserve runtime logs while debugging tests locally:

```bash
ALFRED_KEEP_TEST_LOGS=1 make test
ALFRED_KEEP_TEST_LOGS=1 make test-backend-diagnostics
```

## Documentation

Detailed documentation is currently maintained in Italian:

- [Italian documentation index](docs/it/README.md)
- [AI coding agent instructions](AGENTS.md)
- [Guided documentation reading order](docs/it/27-guida-lettura-documentazione.md)
- [Operational rules](docs/it/00-regole-operative.md)
- [Project overview](docs/it/01-panoramica-progetto.md)
- [Architecture](docs/it/02-architettura-generale.md)
- [Repository layout](docs/it/03-struttura-cartelle.md)
- [Application layer](docs/it/04-livello-applicazione.md)
- [Inotify backend](docs/it/05-modulo-inotify.md)
- [Core engine](docs/it/06-core-engine.md)
- [Event flow](docs/it/07-flusso-eventi.md)
- [C guide for this project](docs/it/08-guida-c-usato-nel-progetto.md)
- [Makefile and build system](docs/it/09-makefile-e-build-system.md)
- [Debugging, tests, and tools](docs/it/10-debugging-test-e-strumenti.md)
- [Contributing guide](docs/it/11-come-contribuire.md)
- [Event semantics](docs/it/13-semantica-eventi.md)
- [Test scenarios](docs/it/14-scenari-test.md)
- [Code and data structure map](docs/it/16-mappa-codice-e-strutture.md)
- [Advanced documentation roadmap](docs/it/17-roadmap-documentazione-avanzata.md)
- [Licensing model notes](docs/it/18-modello-licenze.md)
- [CLI and man page roadmap](docs/it/19-roadmap-cli-e-man-page.md)
- [Inotify event matrix](docs/it/20-matrice-eventi-inotify.md)
- [Scanner and resync roadmap](docs/it/21-roadmap-scanner-resync.md)
- [Log contract](docs/it/22-contratto-log.md)
- [Backend plugin roadmap](docs/it/23-roadmap-plugin-backend.md)
- [AI agent guardrail roadmap](docs/it/24-roadmap-ai-agent-guardrail.md)
- [Unified dossier roadmap](docs/it/25-roadmap-unificata-dossier.md)
- [Supported feature matrix](docs/it/26-stato-funzionalita.md)
- [Documentation audit and declared debt](docs/it/28-audit-documentazione-e-debiti.md)
- [Event Model v0](docs/it/29-event-model-v0.md)
- [Backend API v0](docs/it/30-backend-api-v0.md)
- [Inotify reference backend milestone](docs/it/31-milestone-inotify-reference-backend.md)
- [MVP operational usability v0](docs/it/49-mvp-operational-usability-v0.md)
- [CLI parser v0](docs/it/50-cli-parser-v0.md)
- [Post-MVP documentation and man pages v0](docs/it/51-post-mvp-documentation-man-pages-v0.md)
- [Glossary](docs/it/glossario.md)
- [Documentation progress](docs/it/documentation-progress.md)

Draft manual pages are available locally:

- [alfred(1)](docs/man/man1/alfred.1)
- [alfred.conf(5)](docs/man/man5/alfred.conf.5)
- [alfred-events(7)](docs/man/man7/alfred-events.7)

Italian manual-page translations are available locally:

- [alfred(1) in Italian](docs/man/it/man1/alfred.1)
- [alfred.conf(5) in Italian](docs/man/it/man5/alfred.conf.5)
- [alfred-events(7) in Italian](docs/man/it/man7/alfred-events.7)

They can be read with `man -l docs/man/man1/alfred.1`, `man -l
docs/man/man5/alfred.conf.5`, `man -l docs/man/man7/alfred-events.7`, or the
corresponding paths under `docs/man/it/`.

Code browsing tools are documented separately:

- [Code browsing tools](docs/code-browser/README.md)
- [Sourcebot browser](docs/sourcebot-browser/README.md)
- [Kythe browser](docs/kythe-browser/README.md)
- [Aggregated code browsing scripts](tools/code-browsing/README.md)

Code comments are written in English. Teaching documentation is currently in
Italian.

## Contributing

New contributors should work from a fork and open a pull request against
`main`.

Start here:

- [Contributing guide](docs/it/11-come-contribuire.md)
- [Commenting style](docs/commenting-style.md)

Before opening a pull request, run:

```bash
git diff --check
make
make test
make test-cli
make smoke-mvp
make test-jsonl
make test-backend-diagnostics
```

## Roadmap

Near-term work includes:

- English documentation
- overflow and resynchronization design
- cleanup of historical legacy/shadow test archives
- more code maps, diagrams, and guided reading material
- continued separation between backend diagnostics and semantic core behavior

## License

No license has been selected yet.

Until a license is added, this repository should not be treated as an
open-source package with clearly granted reuse rights. The license choice will
be discussed before the project is presented as reusable by third parties.

The intended direction is an open-source core with the possibility of separate
commercial or source-available licensing for future advanced modules. See the
Italian licensing notes in [docs/it/18-modello-licenze.md](docs/it/18-modello-licenze.md).
