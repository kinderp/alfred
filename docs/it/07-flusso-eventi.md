# Flusso eventi

Questo capitolo spiega il percorso completo di un evento nel progetto.

## Flusso finale desiderato

Nel disegno finale vogliamo questo:

```mermaid
flowchart TD
    A[Kernel] --> B[inotify]
    B --> C[modules/inotify]
    C --> D[alfred_raw_event_t]
    D --> E[core]
    E --> F[alfred_event_t]
    F --> G[app/logger]
```

Il modulo inotify produce eventi raw. Il core produce eventi semantici. Il
livello app decide cosa farne.

## Flusso diagnostico in shadow mode

Durante l'integrazione lo shadow mode resta disponibile come modalita'
diagnostica esplicita.

Significa che lo stesso evento inotify percorre due strade:

```mermaid
flowchart TD
    A[inotify_backend_poll] --> B[struct inotify_event]
    B --> C[inotify_adapter_build_raw]
    C --> D[alfred_raw_event_t]
    D --> E[app callback]
    B --> F

    F[events.c semantic logic] --> G[logger_event legacy output]

    E --> H[alfred_process]
    H --> I[core_logger_on_event]
    I --> J[logger_event core output]
```

Il vecchio percorso resta attivo solo in questa modalita', cosi' si possono
confrontare legacy e core senza far dipendere il runtime normale dal dispatcher
storico.

In shadow mode il percorso core produce output aggiuntivo con prefisso `core`.

## Flusso di default in core mode

Il default attuale usa il core come stream ufficiale. La stessa modalita' puo'
essere forzata esplicitamente con:

```bash
ALFRED_EVENT_ENGINE=core ./alfred /path/da/osservare
```

In questa modalita' lo stesso evento percorre solo il percorso semantico del
core:

```mermaid
flowchart TD
    A[inotify_backend_poll] --> B[struct inotify_event]
    B --> C[inotify_adapter_build_raw]
    C --> D[alfred_raw_event_t]
    D --> E[app callback]
    E --> F[alfred_process]
    F --> G[core_logger_on_event]
    G --> H[logger_event plain output]
    B --> I{IN_CREATE IN_ISDIR?}
    I -->|si| J[backend recursive discovery]
    J --> K[WATCH_ADDED / synthetic raw]
    K --> E
```

Il vecchio dispatcher `legacy_events_dispatch()` non viene chiamato. Questo e'
il punto chiave: `events.c` resta nel codice per il confronto in shadow mode, ma
non produce lo stream ufficiale quando `event_engine=core`.

L'aggiornamento dei watch resta invece attivo nel backend inotify. Non e'
semantica legacy: e' manutenzione dello stato del backend. Senza questo
passaggio, in core mode Alfred vedrebbe la creazione della directory nuova ma
perderebbe gli eventi successivi dentro quella directory.

Esempio di output in core mode:

```text
[EVENT] FILE_CREATED path=/tmp/a.txt
[EVENT] FILE_MODIFIED path=/tmp/a.txt
[EVENT] FILE_READY path=/tmp/a.txt
```

Non c'e' prefisso `core` perche' il core non e' piu' un secondo stream di
confronto: e' la sorgente ufficiale.

## Perche' usare shadow mode

Shadow mode serve a confrontare due implementazioni:

```text
vecchio dispatcher inotify
nuovo core semantico
```

Vantaggi:

- riduce il rischio di regressioni
- permette di confrontare gli eventi prodotti
- permette ai test funzionali storici di restare osservabili in legacy mode
- rende visibili differenze tra vecchia e nuova logica
- mantiene disponibile il vecchio dispatcher finche' serve come riferimento

## Esempio di output

In shadow mode, un evento di creazione potrebbe produrre due righe:

```text
[EVENT] FILE_CREATED path=/tmp/a.txt
[EVENT] core seq=1 type=FILE_CREATED path=/tmp/a.txt pid=0
```

La prima riga viene dal vecchio dispatcher.

La seconda riga viene dal core.

## Limiti attuali

Non tutte le politiche di recovery sono ancora definitive nel percorso core.

Esempi:

- `IN_Q_OVERFLOW` puo' non avere un path associato
- `IN_IGNORED` resta diagnostica/backend state, non semantica core
- il vecchio `move_cache` esiste ancora nel modulo inotify per shadow mode

Per questo il vecchio dispatcher resta disponibile come confronto, ma non e' il
percorso ufficiale in `event_engine=core`.

## Prossimo obiettivo

Il prossimo obiettivo non e' rimuovere alla cieca il vecchio codice. Prima
bisogna:

1. mantenere documentata la differenza tra stream core e stream legacy
2. rendere il legacy shadow opzionale a livello build
3. decidere quali test funzionali storici migrare al core
4. confinare o rimuovere `events.c` e `move_cache.c`
5. progettare overflow/resync come passo separato
