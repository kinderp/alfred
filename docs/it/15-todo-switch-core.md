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
- il runtime manda eventi inotify al core
- `event_engine=core` e' il default e usa il core come stream ufficiale plain
- `ALFRED_EVENT_ENGINE=shadow` abilita il confronto legacy/core solo se il
  binario e' stato compilato con `ENABLE_LEGACY_SHADOW=1`
- esiste un primo backend inotify esplicito in
  `modules/inotify/src/inotify_backend.c`
- il backend legge il fd inotify, logga gli eventi raw, costruisce
  `alfred_raw_event_t` e li consegna all'app tramite callback
- l'aggiornamento dei watch per `IN_CREATE | IN_ISDIR` e' stato spostato dal
  loop applicativo al backend inotify
- `events.c` contiene ancora semantica legacy, ma non viene compilato nella
  build core-only normale
- `move_cache.c` e' ancora usato dal legacy dispatcher quando
  `ENABLE_LEGACY_SHADOW=1`; la cache e' posseduta da `events.c` e viene
  inizializzata solo in `event_engine=shadow`
- `watch_manager_add_recursive_with_discovery()` puo' notificare directory
  scoperte dallo scan ricorsivo
- il backend trasforma directory scoperte in raw event sintetici per il core
- il core recupera `DIR_CREATED` mancanti in scenari tipo
  `recursive_create_nested_dir`, mentre il legacy resta incompleto
- `inotify_fd` e `watchers` non sono piu' campi diretti di `app_t`: vivono in
  `inotify_backend_t`, contenuto nel campo `app_t.inotify`
- il backend riceve ancora `app_t *` per usare configurazione, logger e callback
  verso il core

## Mappa della logica legacy rimasta

Questa sezione fotografa dove si trova ancora la logica storica e dove dovrebbe
finire dopo lo switch completo al core.

### `modules/inotify/src/events.c`

Questo file e' ancora il dispatcher semantico legacy. Viene compilato solo con
`ENABLE_LEGACY_SHADOW=1` e viene chiamato solo in `event_engine=shadow`; nel
default `event_engine=core` e nella build core-only normale non partecipa al
runtime.

Responsabilita' ancora presenti:

- `legacy_events_dispatch()`: riceve direttamente `struct inotify_event` e
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

`app.c` e' meno legato al backend inotify rispetto alla fase precedente, ma non
e' ancora nella forma finale.

Responsabilita' attuali:

- inizializza configurazione, logger, core e backend inotify
- inizializza il backend inotify
- chiama `inotify_backend_poll()` nel loop principale
- riceve raw event reali e sintetici tramite callback
- inoltra gli `alfred_raw_event_t` al core
- non include piu' `events.h`: in shadow mode il backend inizializza, chiama e
  spegne il dispatcher legacy

La catena ricorsiva non parte piu' da `app.c`. Ora e':

```text
inotify_backend.c -> backend_handle_dir_create()
                  -> watch_manager_add_recursive_with_discovery()
                  -> backend_emit_synthetic_dir_create()
                  -> callback app
                  -> core
```

Questa catena serve a non perdere le directory create rapidamente prima che il
watch del padre sia installato. E' un miglioramento perche' la manutenzione dei
watch sta nel backend, non nell'app. Resta da migliorare il fatto che il backend
usa ancora `app_t` per raggiungere configurazione, logger e callback verso il
core.

### `modules/inotify/src/inotify_backend.c`

Questo file e' il primo confine esplicito del backend inotify.

Responsabilita' attuali:

- `inotify_backend_init()`: apre il file descriptor inotify non bloccante
- `inotify_backend_add_startup_watch()`: aggiunge i watch iniziali
- `inotify_backend_poll()`: legge il buffer inotify, logga il raw event,
  costruisce `alfred_raw_event_t` e chiama la callback app
- `inotify_backend_shutdown()`: chiude il file descriptor
- `backend_handle_dir_create()`: mantiene i watch ricorsivi sulle directory
  create
- `backend_emit_synthetic_dir_create()`: emette raw create sintetici per le
  directory scoperte dallo scan

`inotify_backend_t` possiede:

- `fd`: file descriptor inotify non bloccante
- `watchers`: tabella `wd -> path`

Limite intenzionale: le funzioni del backend ricevono ancora `app_t *`. Questo
evita un grande refactor immediato, ma il target resta separare in modo piu'
netto configurazione, logger e callback dal contesto applicativo completo.

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

`app_t` contiene ancora un campo specifico inotify:

- `inotify`
- `moves`

`inotify` e' il backend runtime. `moves` resta nel contesto applicativo solo per
il dispatcher legacy in shadow mode; in `event_engine=core` non viene
inizializzato.

## Ordine consigliato per lo switch

L'ordine piu' pulito e':

1. mantenere `ALFRED_EVENT_ENGINE=shadow` finche' serve per confronto e prove
   manuali
2. spostare il codice di lettura inotify e manutenzione watch fuori da `app.c`
   verso un backend inotify esplicito: fatto
3. far emettere al backend solo `alfred_raw_event_t`, includendo eventuali raw
   sintetici per directory scoperte dallo scan
4. lasciare al core tutta la semantica: create, delete, modify, close-write,
   move, rename, relocated
5. rimuovere `move_cache` dal percorso runtime core: fatto; resta confinata al
   dispatcher legacy
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
event_engine=core
    core_logger_on_event -> logger_event ufficiale plain
    legacy events.c      -> non chiamato dal loop

ALFRED_EVENT_ENGINE=shadow
    core_logger_on_event -> logger_event con prefisso core
    legacy events.c      -> logger_event ufficiale storico
```

Quindi lo switch del default e' gia' avvenuto senza rimuovere subito
`events.c`. Questa e' una scelta prudente: il core e' la sorgente ufficiale, ma
il confronto shadow resta disponibile con override esplicito.

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

- `modules/inotify/src/move_cache.c` non deve piu' servire al runtime core
- `modules/inotify/include/move_cache.h` puo' essere rimosso dal modulo runtime
- `app_t.moves` puo' essere eliminato: fatto
- `move_cache_size` puo' essere rimosso dalla configurazione

La correlazione move deve restare nel core.

Stato attuale: `move_cache` e' gia' esclusa da `event_engine=core` ed e'
posseduta da `events.c`, non da `app_t`. Inoltre non viene compilata nella
build core-only normale: resta solo nella variante
`ENABLE_LEGACY_SHADOW=1`, dove `events.c` produce ancora il confronto legacy.

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

## Roadmap orientativa allo switch definitivo

Questa lista riassume i passi ancora necessari prima di considerare concluso lo
switch dal dispatcher legacy al core. L'effort e' indicativo: serve a capire
quali punti sono semplici pulizie e quali invece richiedono discussione,
verifica e documentazione piu' estesa.

| Passo | Effort | Perche' serve |
| --- | --- | --- |
| Rendere legacy shadow opzionale a livello build | Fatto | `events.c` e `move_cache.c` restano disponibili con `ENABLE_LEGACY_SHADOW=1`, ma non fanno parte del binario core normale. |
| Documentazione pesante del codice | Alto | Prima di altri refactor bisogna rendere leggibili responsabilita', confini, invarianti e motivazioni direttamente vicino alle funzioni C. |
| Pulizia finale delle responsabilita' | Medio/alto | Backend, app e core devono avere ruoli netti: raw nel backend, orchestrazione nell'app, semantica nel core. |
| Revisione completa della suite core-only | Medio | I test core devono fissare il comportamento ufficiale prima di archiviare o ridurre i test legacy. |
| Allineamento scenari/eventi/documentazione | Alto | Per ogni scenario importante deve essere chiaro il passaggio `filesystem -> inotify -> raw Alfred -> evento semantico`. |
| Decisione sui test funzionali legacy | Medio | Dopo lo switch bisogna scegliere se mantenerli come shadow, migrarli al core o archiviarne una parte. |
| Spegnere shadow come modalita' ordinaria | Medio | `core` e' gia' il default, ma shadow deve diventare chiaramente una modalita' di debug/confronto e non un requisito operativo. |
| Quarantena o rimozione finale del legacy | Medio/alto | A fine migrazione bisogna decidere se eliminare `events.c`/`move_cache.c` o spostarli in un'area legacy esplicita. |
| Overflow/resync | Alto | E' rimandato a dopo lo switch perche' richiede una policy di recovery quando il backend perde eventi. |

### Mappa test funzionali legacy e test core

La mappa dettagliata e' in
[Scenari di test](14-scenari-test.md#mappa-funzionali-legacy-e-core).

Risultato della revisione:

- tutti gli scenari funzionali storici realmente implementati hanno una
  copertura core equivalente o piu' precisa
- la suite core aggiunge scenari non presenti nei funzionali storici:
  `move_dir`, `modify_file`, `recursive_create_nested_dir` e
  `move_rename_file`
- `tests/functional/test_move_rename_file.sh` esiste ma non contiene uno
  scenario, quindi non deve essere considerato copertura reale
- `tests/functional/test_recursive.sh` resta diverso da
  `tests/core/test_recursive_create_nested_dir.sh`: il primo verifica una
  creazione lenta con watch aggiunti in tempo, il secondo verifica il caso
  veloce `mkdir -p` recuperato con raw event sintetici
- i test funzionali storici controllano anche diagnostica backend come
  `WATCH_ADDED` e `WATCH_REMOVED`; questi log non devono diventare eventi
  semantici core

Decisione provvisoria:

```text
make test       -> resta legacy-shadow per compatibilita' e diagnostica
make test-core  -> resta contratto semantico ufficiale del core
```

Non conviene rinominare o riscrivere subito `make test`, perche' quel nome oggi
ha ancora il valore pratico di smoke test storico. Il prossimo passo, quando
vorremo avvicinarci allo switch definitivo, sara' progettare una suite
funzionale core end-to-end separata oppure decidere esplicitamente di cambiare
il significato di `make test`.

L'overflow resta fuori dal percorso immediato per una ragione precisa: non e'
solo un evento da tradurre, ma una condizione in cui il backend ammette di non
conoscere piu' con certezza lo stato del filesystem. Gestirlo bene significa
decidere se fare resync, emettere diagnostica, ricostruire eventi sintetici o
combinare piu' strategie. Farlo prima dello switch rischierebbe di mescolare due
problemi diversi: migrazione della semantica al core e recovery da perdita di
eventi.

## Fase A: documentazione pesante del codice

Prima di altri cambiamenti strutturali conviene fare una fase dedicata ai
commenti e alla documentazione tecnica. L'obiettivo non e' aumentare il numero
di commenti, ma mettere le spiegazioni nei punti in cui servono davvero.

Passi operativi:

1. rileggere `docs/commenting-style.md`
2. rileggere `docs/commenting-progress.md`
3. mappare i file C principali ancora poco commentati
4. aggiornare `docs/commenting-progress.md` con priorita' e stato reale
5. aggiungere commenti in inglese nel codice e spiegazioni didattiche in
   italiano negli `.md`

Priorita' consigliata:

| Priorita' | File | Motivo |
| --- | --- | --- |
| Alta | `modules/inotify/src/inotify_backend.c` | E' il confine piu' importante tra inotify, raw event, watch ricorsivi, raw sintetici e shadow legacy. |
| Alta | `modules/inotify/include/inotify_backend.h` | Deve spiegare che cosa possiede il backend e quale contratto offre all'app. |
| Alta | `core/src/alfred_correlator.c` | Contiene la logica che trasforma raw event in eventi semantici, inclusa la correlazione move/rename. |
| Alta | `core/include/alfred_correlator.h` | Deve rendere chiaro il contratto pubblico del core. |
| Alta | `app/src/app.c` | Ora deve essere descritto come orchestratore, non come luogo della semantica filesystem. |
| Media | `modules/inotify/src/watch_manager.c` | Gestisce stato backend reale e discovery ricorsiva; e' delicato per il bug `mkdir -p`. |
| Media | `modules/inotify/src/inotify_adapter.c` | Va mantenuto conversion-only; i commenti devono proteggere questo confine. |
| Media | `app/src/config.c` | Contiene scelte importanti su default core, shadow mode e parsing sicuro dei numeri. |
| Bassa/legacy | `modules/inotify/src/events.c` | Va commentato come legacy shadow, evitando di investirci troppo se verra' rimosso o quarantinato. |
| Bassa/legacy | `modules/inotify/src/move_cache.c` | Serve ancora solo al dispatcher legacy; utile documentarlo per confronto storico. |

Regola pratica: i commenti nel codice devono essere in inglese e vicini alle
funzioni; le spiegazioni lunghe, didattiche e con contesto storico devono stare
in `docs/it`.

## Mappa chiamate e documentazione generabile

Sarebbe utile avere una documentazione discorsiva in italiano che racconti
"quale funzione chiama quale funzione" e che si possa aggiornare con un comando
quando il codice cambia. L'idea consigliata e' separare tre livelli:

1. commenti strutturati nel codice C, in inglese, secondo
   `docs/commenting-style.md`
2. dati generati automaticamente da un comando, per esempio una mappa
   `caller -> callee` estratta con `cflow`, `clang` o uno script basato su
   `ctags`
3. capitolo italiano scritto a mano che interpreta la mappa e collega le
   funzioni ai concetti architetturali

Un possibile target futuro del Makefile potrebbe essere:

```bash
make docs-callgraph
```

Output possibile:

```text
docs/generated/callgraph.txt
docs/generated/functions-index.md
docs/it/16-mappa-chiamate-codice.md
```

La parte generata dovrebbe essere considerata supporto tecnico, non
documentazione didattica finale. Gli studenti hanno bisogno anche della lettura
ragionata: per esempio non basta sapere che `inotify_backend_poll()` chiama
`inotify_adapter_from_event()`, bisogna spiegare che quella chiamata e' il punto
in cui il backend smette di parlare inotify e inizia a produrre fatti raw Alfred.

Il capitolo italiano dovrebbe avere sezioni come:

- avvio dell'applicazione
- configurazione e scelta `event_engine`
- inizializzazione backend inotify
- loop di polling
- conversione `struct inotify_event` -> `alfred_raw_event_t`
- ingresso nel core
- emissione degli eventi semantici
- percorso shadow legacy, se compilato

Questa mappa va progettata dopo la fase A, perche' i commenti strutturati nel
codice renderanno piu' facile generare o collegare un indice affidabile delle
funzioni.

## Stato build legacy shadow

Il legacy shadow e' ora una variante di build esplicita:

```bash
make ENABLE_LEGACY_SHADOW=1
```

La build normale:

```bash
make
```

non compila `events.c` e `move_cache.c`. Se un binario core-only riceve
`ALFRED_EVENT_ENGINE=shadow`, Alfred fallisce con un errore esplicito invece di
fare fallback silenzioso a core mode.

I target di test sono separati:

```bash
make test-core
```

ricostruisce il binario core-only ed esegue la suite `tests/core/`.

```bash
make test
```

ricostruisce il binario con `ENABLE_LEGACY_SHADOW=1` ed esegue i test
funzionali storici, che usano ancora il confronto shadow/legacy.

## Regola di avanzamento

Non fare uno switch grande e cieco.

Per ogni passo:

1. documentare la decisione
2. aggiungere o aggiornare scenario shadow
3. verificare output legacy/core
4. modificare codice in modo piccolo
5. aggiornare documentazione e progressi
6. solo dopo passare al punto successivo
