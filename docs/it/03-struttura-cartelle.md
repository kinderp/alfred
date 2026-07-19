# Struttura delle cartelle

Il progetto e' organizzato come un piccolo monorepo C modulare.

```text
progetto_3a_inf/
├── app/
├── core/
├── modules/
├── tests/
├── tools/
├── docs/
├── Makefile
└── README.md
```

## app/

Contiene il livello applicazione.

```text
app/
├── include/
└── src/
```

Responsabilita':

- avviare il programma
- inizializzare configurazione e logger
- gestire il ciclo principale
- coordinare shutdown e cleanup

File principali:

```text
app/include/app.h
app/src/main.c
app/src/app.c
```

## core/

Contiene il motore semantico.

```text
core/
├── include/
├── src/
└── examples/
```

Responsabilita':

- ricevere eventi raw
- correlare eventi collegati
- produrre eventi semantici

Il core non dovrebbe dipendere da `inotify`.

File principali:

```text
core/include/alfred_correlator.h
core/src/alfred_correlator.c
core/src/alfred_tables.c
core/src/alfred_utils.c
core/examples/main_demo.c
```

`core/examples/main_demo.c` e' una dimostrazione isolata: parte gia' da eventi
raw sintetici e non rappresenta il runtime reale con backend inotify.

## modules/

Contiene i backend specifici.

Oggi:

```text
modules/inotify/
├── include/
└── src/
```

Responsabilita':

- parlare con Linux tramite `inotify`
- gestire watch descriptor
- osservare directory e file
- trasformare eventi Linux in eventi raw comuni

File principali:

```text
modules/inotify/src/inotify_backend.c
modules/inotify/src/inotify_adapter.c
modules/inotify/src/watch_manager.c
modules/inotify/src/watcher.c
```

Il vecchio dispatcher legacy `events.c` e la sua `move_cache` sono stati
rimossi da questa cartella: il modulo inotify corrente produce solo raw event e
diagnostica backend, non eventi semantici finali.

## tests/

Contiene suite contrattuali, test funzionali, test di installazione, controlli
documentali e workload esplorativi o prestazionali.

```text
tests/
├── core/
├── backend/
├── jsonl/
├── install/
├── compatibility/
├── first-user/
├── perf/
└── exploratory/
```

`tests/first-user/` verifica il passaggio privato e validato del contesto di
sessione tra shell realmente separate. Non sostituisce una sessione con un
partecipante: protegge soltanto il contratto eseguibile della guida.

## tools/

Contiene strumenti di sviluppo e helper operativi che non fanno parte del
runtime installato di Alfred.

```text
tools/
├── ci/
├── code-browsing/
├── codex/
└── first-user/
```

`tools/first-user/session-context.sh` crea la root sacrificabile della
validazione first-user e carica in ogni terminale soltanto path verificati. E'
un helper del protocollo: non modifica Event Model, backend, core o CLI e non
viene installato da `make install`.

## docs/

Contiene documentazione per sviluppatori e studenti.

```text
docs/
├── commenting-style.md
├── commenting-progress.md
└── it/
```

`docs/it/` contiene la documentazione didattica in italiano.

Il capitolo `docs/it/16-mappa-codice-e-strutture.md` e' la lettura guidata piu'
vicina al codice: collega funzioni, strutture dati e diagrammi.

## Makefile

Il Makefile principale compila l'intero progetto.

Gestisce:

- compilazione debug
- compilazione release
- sanitizer
- test
- Valgrind
- GDB
- formattazione
- analisi statica

Per iniziare:

```bash
make
```
