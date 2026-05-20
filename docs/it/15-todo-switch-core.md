# TODO switch verso il core

Questo documento raccoglie i passi ancora aperti per passare dallo shadow mode
al core come sorgente ufficiale degli eventi semantici.

## Obiettivo finale

Il flusso finale deve essere:

```text
inotify
    -> modules/inotify
    -> alfred_raw_event_t
    -> core
    -> alfred_event_t
    -> app/logger
```

Il modulo inotify deve produrre fatti raw. Il core deve produrre semantica.

## Cosa deve uscire da `events.c`

`modules/inotify/src/events.c` oggi contiene ancora logica semantica legacy.
Queste responsabilita' devono uscire dal modulo:

- conversione `IN_CREATE` in `FILE_CREATED` o `DIR_CREATED`
- conversione `IN_DELETE` in `FILE_DELETED` o `DIR_DELETED`
- correlazione `IN_MOVED_FROM` + `IN_MOVED_TO`
- classificazione rename, move e relocate
- gestione del `move_cache`
- emissione di eventi semantici finali con `logger_event`

Questa logica appartiene al core oppure a un adattatore app/core temporaneo.

## Cosa deve restare nel backend inotify

Nel backend devono restare:

- apertura e chiusura del file descriptor inotify
- `inotify_add_watch`
- `inotify_rm_watch`
- tabella `wd -> path`
- lettura di `struct inotify_event`
- conversione in `alfred_raw_event_t`
- gestione diagnostica di `WATCH_ADDED` e `WATCH_REMOVED`
- scan ricorsivo mirato per aggiungere watch alle directory scoperte
- generazione di raw event sintetici quando lo scan scopre directory create
  prima dell'aggiunta del watch

Il backend puo' scoprire fatti. Non deve decidere la semantica finale.

## Stato attuale

Al momento:

- il core e' inizializzato in `app_t`
- il runtime manda eventi inotify al core in shadow mode
- il legacy dispatcher continua a scrivere il comportamento ufficiale
- `events.c` contiene ancora semantica legacy
- `move_cache.c` e' ancora usato dal legacy dispatcher
- `watch_manager_add_recursive_with_discovery()` puo' notificare directory
  scoperte dallo scan ricorsivo
- `app_process_synthetic_dir_create()` trasforma directory scoperte in raw event
  sintetici per il core
- il core recupera `DIR_CREATED` mancanti in scenari tipo
  `recursive_create_nested_dir`, mentre il legacy resta incompleto

## Passi consigliati

### 1. Stabilizzare lo shadow mode

Continuare ad aggiungere scenari in `tests/shadow/compare_shadow_output.py` e
documentare differenze attese.

Scenari ancora utili:

- move directory semplice
- modify file
- close-write / file ready
- overflow, se riproducibile

### 2. Rendere esplicite le differenze attese

Ogni differenza deve essere classificata:

- bug del core
- bug o limite del legacy
- decisione semantica intenzionale
- diagnostica backend fuori dal core

Esempi gia' decisi:

- `WATCH_ADDED` e `WATCH_REMOVED` restano diagnostica backend
- move+rename diventa `FILE_RELOCATED` o `DIR_RELOCATED`
- directory create con `mkdir -p` devono diventare `DIR_CREATED` anche se
  recuperate tramite raw event sintetico

### 3. Spostare il logging semantico ufficiale sul core

Quando gli scenari principali sono documentati, l'app dovrebbe usare l'output
del core come evento ufficiale.

In pratica:

```text
core_logger_on_event -> logger_event ufficiale
legacy events.c      -> solo confronto o rimozione
```

Durante questa fase bisogna decidere se mantenere temporaneamente un prefisso
`core` oppure rimuoverlo quando il core diventa sorgente ufficiale.

### 4. Rimuovere `move_cache` dal modulo inotify

Quando il core gestisce ufficialmente move e rename:

- `modules/inotify/src/move_cache.c` non deve piu' servire al runtime
- `modules/inotify/include/move_cache.h` puo' essere rimosso dal modulo runtime
- `app_t.moves` puo' essere eliminato
- `move_cache_size` puo' essere rimosso dalla configurazione

La correlazione move deve restare nel core.

### 5. Ridurre o rimuovere `events.c`

Quando il core e' ufficiale, `events.c` non deve piu' essere il cervello
semantico.

Possibili strade:

- rimuoverlo completamente dal Makefile
- trasformarlo in adapter raw temporaneo
- sostituirlo con un backend inotify piu' pulito

La direzione finale consigliata e':

```text
modules/inotify/src/inotify_backend.c
modules/inotify/src/inotify_adapter.c
modules/inotify/src/watch_manager.c
modules/inotify/src/watcher.c
```

senza semantica finale nel modulo.

### 6. Ripulire `app_t`

`app_t` oggi contiene ancora stato specifico di inotify. Nel design finale
dovrebbe possedere un backend generico o una struttura backend-specifica
incapsulata.

Da rivedere:

- `inotify_fd`
- `watchers`
- eventuali campi temporanei legacy

## Regola di avanzamento

Non fare uno switch grande e cieco.

Per ogni passo:

1. documentare la decisione
2. aggiungere o aggiornare scenario shadow
3. verificare output legacy/core
4. modificare codice in modo piccolo
5. aggiornare documentazione e progressi
6. solo dopo passare al punto successivo
