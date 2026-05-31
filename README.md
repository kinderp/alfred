# Alfred

Alfred is a Linux filesystem event engine written in C.

It watches one or more directory trees with `inotify`, records backend raw
facts, and emits a semantic event stream through a dedicated core engine.

The project is currently documented in Italian because it is also used as a
teaching codebase. English documentation will be added later.

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

## Quick Start

Clone and build:

```bash
git clone https://github.com/kinderp/alfred.git
cd alfred
make
```

Run Alfred on one or more paths:

```bash
./alfred /path/to/watch
```

Run Alfred with a configuration file:

```bash
ALFRED_CONFIG=./alfred.conf ./alfred /path/to/watch
```

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

Run backend diagnostics:

```bash
make test-backend-diagnostics
```

The CI workflow runs the same commands on pull requests targeting `main` and on
pushes to `main`.

To preserve runtime logs while debugging tests locally:

```bash
ALFRED_KEEP_TEST_LOGS=1 make test
ALFRED_KEEP_TEST_LOGS=1 make test-backend-diagnostics
```

## Documentation

Detailed documentation is currently maintained in Italian:

- [Italian documentation index](docs/it/README.md)
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
- [Historical shadow-mode comparison](docs/it/12-confronto-shadow-mode.md)
- [Event semantics](docs/it/13-semantica-eventi.md)
- [Test scenarios](docs/it/14-scenari-test.md)
- [Core switch TODO and historical notes](docs/it/15-todo-switch-core.md)
- [Code and data structure map](docs/it/16-mappa-codice-e-strutture.md)
- [Advanced documentation roadmap](docs/it/17-roadmap-documentazione-avanzata.md)
- [Licensing model notes](docs/it/18-modello-licenze.md)
- [CLI and man page roadmap](docs/it/19-roadmap-cli-e-man-page.md)
- [Inotify event matrix](docs/it/20-matrice-eventi-inotify.md)
- [Glossary](docs/it/glossario.md)
- [Documentation progress](docs/it/documentation-progress.md)

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
