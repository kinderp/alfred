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

## Flusso attuale in shadow mode

Durante l'integrazione stiamo usando shadow mode.

Significa che lo stesso evento inotify percorre due strade:

```mermaid
flowchart TD
    A[struct inotify_event] --> B[legacy app_dispatch_raw_event]
    A --> C[dispatch_event_to_core]

    B --> D[events.c semantic logic]
    D --> E[logger_event legacy output]

    C --> F[inotify_adapter_build_raw]
    F --> G[alfred_raw_event_t]
    G --> H[alfred_process]
    H --> I[core_logger_on_event]
    I --> J[logger_event core output]
```

Il vecchio percorso resta attivo per non rompere subito il comportamento
esistente.

Il nuovo percorso core produce output aggiuntivo con prefisso `core`.

## Flusso in core mode

Durante lo switch verso il core possiamo avviare Alfred con:

```bash
ALFRED_EVENT_ENGINE=core ./alfred /path/da/osservare
```

In questa modalita' lo stesso evento percorre solo il percorso semantico del
core:

```mermaid
flowchart TD
    A[struct inotify_event] --> B[dispatch_event_to_core]
    B --> C[inotify_adapter_build_raw]
    C --> D[alfred_raw_event_t]
    D --> E[alfred_process]
    E --> F[core_logger_on_event]
    F --> G[logger_event plain output]
```

Il vecchio dispatcher `app_dispatch_raw_event()` non viene chiamato. Questo e'
il punto chiave: `events.c` resta nel codice per il confronto in shadow mode, ma
non produce lo stream ufficiale quando `event_engine=core`.

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
- evita di rompere subito i test
- rende visibili differenze tra vecchia e nuova logica
- consente di spegnere il vecchio dispatcher solo quando il core e' affidabile

## Esempio di output

Un evento di creazione potrebbe produrre temporaneamente due righe:

```text
[EVENT] FILE_CREATED path=/tmp/a.txt
[EVENT] core seq=1 type=FILE_CREATED path=/tmp/a.txt pid=0
```

La prima riga viene dal vecchio dispatcher.

La seconda riga viene dal core.

## Limiti attuali

Non tutti gli eventi sono ancora perfetti nel percorso core.

Esempi:

- `IN_Q_OVERFLOW` puo' non avere un path associato
- `IN_IGNORED` non ha ancora un raw flag dedicato nel core
- il vecchio `move_cache` esiste ancora nel modulo inotify

Per questo il vecchio dispatcher resta attivo.

## Prossimo obiettivo

Il prossimo obiettivo non e' rimuovere subito il vecchio codice. Prima bisogna:

1. confrontare output legacy e output core
2. capire quali eventi il core gestisce diversamente
3. aggiungere eventuali raw flag mancanti
4. spostare gradualmente la semantica da `events.c` al core
5. eliminare `move_cache` dal modulo inotify quando non serve piu'
