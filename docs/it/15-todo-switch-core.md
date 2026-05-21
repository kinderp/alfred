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
- `event_engine=shadow` e' il default e mantiene il legacy dispatcher come
  stream ufficiale storico
- `ALFRED_EVENT_ENGINE=core` abilita una modalita' di prova in cui il core
  scrive lo stream ufficiale plain e il legacy dispatcher non viene chiamato
- l'aggiornamento dei watch per `IN_CREATE | IN_ISDIR` e' stato spostato nel
  loop applicativo, quindi funziona anche quando `events.c` viene saltato
- questa collocazione in `app.c` e' temporanea: quando il legacy non servira'
  piu', la manutenzione dei watch dovra' stare nel backend inotify o in una
  futura interfaccia backend, non nel core semantico
- `events.c` contiene ancora semantica legacy
- `move_cache.c` e' ancora usato dal legacy dispatcher
- `watch_manager_add_recursive_with_discovery()` puo' notificare directory
  scoperte dallo scan ricorsivo
- `app_process_synthetic_dir_create()` trasforma directory scoperte in raw event
  sintetici per il core
- il core recupera `DIR_CREATED` mancanti in scenari tipo
  `recursive_create_nested_dir`, mentre il legacy resta incompleto

## Mappa della logica legacy rimasta

Questa sezione fotografa dove si trova ancora la logica storica e dove dovrebbe
finire dopo lo switch completo al core.

### `modules/inotify/src/events.c`

Questo file e' ancora il dispatcher semantico legacy. Viene chiamato solo in
`event_engine=shadow`, mentre in `ALFRED_EVENT_ENGINE=core` viene saltato dal
loop applicativo.

Responsabilita' ancora presenti:

- `app_dispatch_raw_event()`: riceve direttamente `struct inotify_event` e
  decide quale handler legacy chiamare
- `handle_create()`: trasforma `IN_CREATE` in `FILE_CREATED` o `DIR_CREATED`
- `handle_delete()`: trasforma `IN_DELETE` in `FILE_DELETED` o `DIR_DELETED`
- `handle_move_from()`: salva la prima meta' del move nel `move_cache`
- `handle_move_to()`: recupera il cookie, ricostruisce sorgente/destinazione e
  classifica rename/move
- `handle_ignored()`: trasforma `IN_IGNORED` in `WATCH_REMOVED` e rimuove il
  watch dalla tabella
- `handle_overflow()`: trasforma `IN_Q_OVERFLOW` in `QUEUE_OVERFLOW`
- `emit_event()`: scrive direttamente lo stream semantico legacy con
  `logger_event()`

Classificazione:

- semantica da rimuovere: create, delete, move, rename, emissione degli eventi
  finali
- diagnostica/backend state da preservare altrove: `IN_IGNORED`, rimozione del
  watch, overflow
- supporto legacy da eliminare quando il core sara' ufficiale: `move_cache`

Nota importante: oggi il legacy non conosce `FILE_RELOCATED` o
`DIR_RELOCATED`. Quando cambiano sia contenitore sia nome, `events.c` emette due
eventi (`MOVED` + `RENAMED`). Il core invece deve emettere un solo evento
`RELOCATED`.

### `app/src/app.c`

`app.c` e' ancora troppo legato al backend inotify. Questo e' accettabile nella
fase di integrazione, ma non e' la forma finale.

Responsabilita' attuali:

- apre direttamente il file descriptor con `inotify_init1()`
- legge direttamente `struct inotify_event` dal file descriptor
- scrive il raw log usando `raw_event_name_from_mask()`
- chiama `dispatch_event_to_core()` per convertire l'evento in
  `alfred_raw_event_t` e passarlo al core
- in shadow mode chiama ancora `app_dispatch_raw_event()` del legacy
- chiama `handle_backend_dir_create()` per mantenere i watch ricorsivi
- contiene `app_process_synthetic_dir_create()`, che crea raw event sintetici
  `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR` per le directory scoperte dallo scan

Il punto piu' brutto, ma intenzionale e temporaneo, e':

```text
app.c -> handle_backend_dir_create()
      -> watch_manager_add_recursive_with_discovery()
      -> app_process_synthetic_dir_create()
      -> core
```

Questa catena serve a non perdere le directory create rapidamente prima che il
watch del padre sia installato. Dal punto di vista architetturale, pero',
`app.c` non dovrebbe conoscere questo dettaglio: e' manutenzione dello stato del
backend inotify e dovrebbe stare nel backend o in una futura interfaccia
backend.

### `modules/inotify/src/inotify_adapter.c`

Questo file e' gia' vicino alla forma desiderata. Converte `struct
inotify_event` in `alfred_raw_event_t` senza fare semantica finale.

Responsabilita' corrette:

- conversione maschere `IN_*` in `ALFRED_RAW_*`
- ricostruzione del path a partire da `parent + name`
- inizializzazione dei campi raw: timestamp, source, mask, cookie, pid, path

Da preservare: deve restare conversion-only. Non deve assorbire la semantica che
stiamo togliendo da `events.c`.

### `modules/inotify/src/watch_manager.c`

Questo file contiene stato backend reale:

- `watch_manager_default_mask()`
- `watch_manager_add()`
- `watch_manager_remove()`
- `watch_manager_add_recursive()`
- `watch_manager_add_recursive_with_discovery()`

`WATCH_ADDED` e `WATCH_REMOVED` qui sono log diagnostici del backend, non eventi
semantici core. Lo scan ricorsivo e' backend state; la parte delicata e' la
notifica delle directory scoperte, perche' oggi chiama indietro `app.c` per
generare raw event sintetici verso il core.

### `app_t`

`app_t` contiene ancora stato specifico inotify:

- `inotify_fd`
- `watchers`
- `moves`

Nel design finale `moves` scompare dal runtime legacy perche' la correlazione
vive nel core. `inotify_fd` e `watchers` dovrebbero essere incapsulati in un
backend inotify, non esposti direttamente come campi principali
dell'applicazione.

## Ordine consigliato per lo switch

L'ordine piu' pulito e':

1. mantenere `event_engine=shadow` e `ALFRED_EVENT_ENGINE=core` finche' servono
   per confronto e prove manuali
2. spostare il codice di lettura inotify e manutenzione watch fuori da `app.c`
   verso un backend inotify esplicito
3. far emettere al backend solo `alfred_raw_event_t`, includendo eventuali raw
   sintetici per directory scoperte dallo scan
4. lasciare al core tutta la semantica: create, delete, modify, close-write,
   move, rename, relocated
5. rimuovere `move_cache` da `app_t` e dal percorso runtime
6. rimuovere `events.c` dal percorso ufficiale, conservandolo solo se serve
   ancora come strumento temporaneo di confronto
7. solo dopo progettare overflow/resync come feature separata

## Passi consigliati

### 1. Stabilizzare lo shadow mode

Continuare ad aggiungere scenari in `tests/shadow/compare_shadow_output.py` e
documentare differenze attese.

Scenari ancora utili:

- move directory semplice: aggiunto come `move_dir`, allineato tra legacy e core
- modify file: aggiunto come `modify_close_write_file`; ora il backend consegna
  `IN_MODIFY` e il core produce `FILE_MODIFIED`
- close-write / file ready: il backend consegna `IN_CLOSE_WRITE` e il core
  produce `FILE_READY`; `FILE_MODIFIED` e `FILE_READY` sono semantica target
  ufficiale del core
- overflow, se riproducibile

### 1b. Stabilizzare la suite core-only

La suite parallela `tests/core/` fissa il comportamento futuro del core come
stream ufficiale plain, senza sostituire subito i test funzionali storici.

Copertura iniziale:

- create directory
- create file
- delete directory
- delete file
- modify / close-write file
- move directory
- move file
- move and rename directory
- move and rename file
- recursive create nested directory
- rename directory
- rename file

Scenari ancora da aggiungere:

- overflow, se si decide di renderlo riproducibile in modo stabile

Nota: l'overflow non fa parte del blocco iniziale dei test core-only. Gli altri
scenari fissano la semantica prodotta a partire da eventi filesystem normali;
l'overflow richiede invece una decisione di recovery/resync quando il backend
non puo' piu' garantire di aver consegnato tutti gli eventi. Lo rimandiamo a
dopo lo switch completo al core, quando sara' piu' chiaro se rappresentarlo come
diagnostica backend, evento semantico di resync richiesto, scan sintetico
dell'albero o una combinazione di queste scelte.

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
- `FILE_CREATED`, `FILE_MODIFIED` e `FILE_READY` sono eventi distinti: non sono
  duplicati, ma fasi diverse della vita di un file

### 3. Spostare il logging semantico ufficiale sul core

Quando gli scenari principali sono documentati, l'app dovrebbe usare l'output
del core come evento ufficiale.

Stato attuale:

```text
event_engine=shadow
    core_logger_on_event -> logger_event con prefisso core
    legacy events.c      -> logger_event ufficiale storico

ALFRED_EVENT_ENGINE=core
    core_logger_on_event -> logger_event ufficiale plain
    legacy events.c      -> non chiamato dal loop
```

Quindi lo switch e' gia' provabile senza rimuovere subito `events.c`. Questa e'
una scelta prudente: conserva il confronto shadow e permette di testare il core
come sorgente ufficiale.

Disegno finale:

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
