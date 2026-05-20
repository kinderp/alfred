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

- move directory semplice: aggiunto come `move_dir`, allineato tra legacy e core
- modify file: aggiunto come `modify_close_write_file`; ora il backend consegna
  `IN_MODIFY` e il core produce `FILE_MODIFIED`
- close-write / file ready: il backend consegna `IN_CLOSE_WRITE` e il core
  produce `FILE_READY`; resta da confermare che questo diventi comportamento
  ufficiale dopo lo switch
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

Decisione documentata:

```text
lo stream ufficiale deve restare semantico e leggibile;
il numero di sequenza resta metadato di debug, non parte della semantica.
```

Formato ufficiale desiderato:

```text
FILE_CREATED path=...
DIR_MOVED from=... to=...
FILE_READY path=...
```

Formato verbose/debug configurabile:

```text
seq=17 FILE_CREATED path=...
seq=18 DIR_MOVED from=... to=...
```

Formato shadow temporaneo attuale:

```text
core seq=17 type=FILE_CREATED path=... pid=...
```

Il formato shadow attuale serve al confronto legacy/core e puo' restare finche'
lo shadow mode e' attivo. Quando il core diventera' sorgente ufficiale, conviene
aggiungere un'opzione di configurazione esplicita, per esempio:

```text
event_log_format=plain
event_log_format=verbose
```

`plain` dovrebbe essere il default. `verbose` potra' includere almeno `seq`,
perche' aiuta a ricostruire l'ordine degli eventi, diagnosticare duplicati,
confrontare raw log ed event log, e discutere sequenze complesse nei test.

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
