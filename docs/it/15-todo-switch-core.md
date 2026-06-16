# TODO post-switch core

Questo documento raccoglie lo stato dopo lo switch runtime dal dispatcher
legacy al core. La migrazione principale e' conclusa: Alfred usa il core come
unica sorgente ufficiale degli eventi semantici, mentre il vecchio shadow mode
resta solo storia della migrazione.

Le sezioni storiche restano per gli studenti perche' spiegano perche' certe
scelte sono state fatte. Non vanno lette come istruzioni operative correnti.

## Stato post-switch

Il flusso runtime corrente e':

```text
inotify
    -> modules/inotify
    -> alfred_raw_event_t
    -> core
    -> alfred_event_t
    -> app/logger
```

Il modulo inotify deve produrre fatti raw. Il core deve produrre semantica.

Lo switch previsto era totale, e dal punto di vista runtime e' stato completato:

- `events.c`, `events.h`, `move_cache.c` e `move_cache.h` sono stati rimossi
- `ENABLE_LEGACY_SHADOW` e `test-legacy-shadow` sono stati rimossi dal Makefile
- `tests/functional/` e `tests/shadow/` sono archivi storici, non verifiche
  correnti
- `make test` verifica il percorso core end-to-end ufficiale
- `make test-backend-diagnostics` verifica la diagnostica tecnica del backend

### `EVENT_ENGINE_SHADOW` rimosso

`EVENT_ENGINE_SHADOW` e il parsing speciale di `ALFRED_EVENT_ENGINE=shadow` sono
stati rimossi. Per noi erano solo un guard rail interno durante la migrazione,
non una feature per utenti o contributori.

Il test:

```text
tests/core/test_invalid_event_engine_shadow.sh
```

protegge il nuovo contratto: `shadow` e' un normale valore non valido e il
messaggio indica che l'unico valore ammesso e' `core`.

## Responsabilita' migrate fuori da `events.c`

`modules/inotify/src/events.c` conteneva logica semantica legacy. Queste
responsabilita' sono state spostate fuori dal modulo inotify o eliminate:

- conversione `IN_CREATE` in `FILE_CREATED` o `DIR_CREATED`
- conversione `IN_DELETE` in `FILE_DELETED` o `DIR_DELETED`
- correlazione `IN_MOVED_FROM` + `IN_MOVED_TO`
- classificazione rename, move e relocate
- gestione del `move_cache`
- emissione di eventi semantici finali con `logger_event`

Questa logica ora appartiene al core. Il backend inotify non deve ricreare un
dispatcher semantico parallelo.

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

## Stato corrente

Al momento:

- il core e' inizializzato in `app_t`
- il runtime manda eventi inotify al core
- `event_engine=core` e' il default e usa il core come stream ufficiale plain
- `ALFRED_EVENT_ENGINE=shadow` non e' piu' una modalita' riconosciuta: fallisce
  come normale valore di configurazione non valido
- esiste un primo backend inotify esplicito in
  `modules/inotify/src/inotify_backend.c`
- il backend legge il fd inotify, logga gli eventi raw, costruisce
  `alfred_raw_event_t` e li consegna all'app tramite callback
- l'aggiornamento dei watch per `IN_CREATE | IN_ISDIR` e' stato spostato dal
  loop applicativo al backend inotify
- `events.c`, `events.h`, `move_cache.c` e `move_cache.h` sono stati rimossi
  dal codice corrente
- `backend_handle_dir_create()` usa `fs_scan_tree(..., emit_root = 0)` per
  scoprire directory annidate dopo `IN_CREATE | IN_ISDIR`
- il backend trasforma le directory annidate scoperte in raw event sintetici
  per il core
- il core recupera `DIR_CREATED` mancanti in scenari tipo
  `recursive_create_nested_dir`
- `inotify_fd` e `watchers` non sono piu' campi diretti di `app_t`: vivono in
  `inotify_backend_t`, contenuto nel campo `app_t.inotify`
- le funzioni lifecycle pulite del backend ricevono `inotify_backend_context_t *`
  per usare runtime, configurazione e logger
- `inotify_backend_poll()` riceve `inotify_backend_context_t *`, callback
  raw/core e `userdata`; non contiene piu' bridge shadow e non chiama piu'
  `legacy_events_dispatch()`

### Decisione: nessuna sopravvivenza dello shadow

La direzione scelta e' rimuovere completamente lo shadow legacy. Questo cambia
il modo in cui leggiamo i prossimi refactor:

- non conviene trasformare il bridge shadow in una API observer stabile
- non conviene rendere `events.c` un secondo backend semantico supportato
- non conviene mantenere `ALFRED_EVENT_ENGINE=shadow` come opzione utente
- bisogna prima verificare la copertura core e poi cancellare il confronto
  legacy in passi piccoli

Il bridge shadow e' stato rimosso dal `inotify_backend_context_t`, init/shutdown
legacy sono stati rimossi da `app.c`, e i file storici del dispatcher legacy
sono stati cancellati. Le suite storiche sono state marcate come archivio.

## Mappa della logica legacy rimasta

Questa sezione conserva la mappa storica della logica legacy rimossa. Non
descrive piu' file presenti nel codice corrente.

### `modules/inotify/src/events.c`

Questo file era il dispatcher semantico legacy. Non e' piu' presente nel codice
corrente: la sua responsabilita' e' stata sostituita dal core.

Responsabilita' storiche che appartenevano a quel file:

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
- supporto legacy gia' eliminato dopo lo switch: `move_cache`

Nota storica importante: il legacy non conosceva `FILE_RELOCATED` o
`DIR_RELOCATED`. Quando cambiavano sia contenitore sia nome, `events.c`
emetteva due eventi (`MOVED` + `RENAMED`). Il core invece emette un solo evento
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

Responsabilita' storiche rimosse:

- inizializzare e spegnere il dispatcher legacy quando shadow mode era attivo
- includere `events.h` per adattare il bridge opaco alla vecchia funzione
  `legacy_events_dispatch()`

La catena ricorsiva non parte piu' da `app.c`. Ora e':

```text
inotify_backend.c -> backend_handle_dir_create()
                  -> fs_scan_tree(..., emit_root = 0)
                  -> watch_manager_add()
                  -> backend_emit_synthetic_dir_create()
                  -> callback app
                  -> core
```

Questa catena serve a non perdere le directory create rapidamente prima che il
watch del padre sia installato. E' un miglioramento perche' la manutenzione dei
watch sta nel backend, non nell'app. Il backend non riceve piu' l'intera
`app_t`: riceve un context con runtime/config/logger, una callback raw/core e
userdata opaco.

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

Limite intenzionale residuo: il backend contiene ancora il ponte verso il
dispatcher legacy shadow, ma il ponte non espone piu' `app_t` nella API
pubblica. La dipendenza reale da `app_t` resta localizzata tra `app.c` ed
`events.c`, cioe' nel codice storico usato solo per il confronto.

#### Mappa delle dipendenze residue da `app_t`

Il backend inotify non accede direttamente al core: non chiama
`alfred_process()` e non legge `app->core`. Il collegamento con il core passa
solo dalla callback ricevuta da `inotify_backend_poll()`. La maggior parte delle
funzioni backend non riceve piu' l'intero `app_t`; le dipendenze rimaste sono
esplicite nel context backend.

Dipendenze reali osservate oggi:

| Funzione | Campi di `app_t` usati | Motivo | Direzione futura |
| --- | --- | --- | --- |
| `inotify_backend_init()` | riceve `inotify_backend_context_t *` pubblico; delega a `backend_init(ctx)` | inizializza fd/watch table, legge configurazione e logga errori | gia' fuori da `app_t`; non inizializza piu' legacy shadow |
| `inotify_backend_add_startup_watch()` | riceve `inotify_backend_context_t *` pubblico; delega a `backend_add_startup_watch(ctx, path)` | sceglie watch singolo o ricorsivo tramite context | gia' fuori da `app_t`; resta da valutare se mantenere anche helper interno |
| `inotify_backend_shutdown()` | riceve `inotify_backend_context_t *` pubblico; delega a `backend_shutdown(ctx)` | chiude fd e distrugge watch table | gia' fuori da `app_t`; non spegne piu' legacy shadow |
| `inotify_backend_poll()` | riceve `inotify_backend_context_t *`, callback raw/core e `userdata` | legge eventi, logga raw, costruisce raw Alfred, chiama callback, aggiorna diagnostica watch e discovery ricorsiva | gia' fuori da `app_t` e fuori dal dispatch legacy |
| `backend_handle_dir_create()` | riceve `inotify_backend_context_t`; usa `ctx.config->recursive`, `ctx.runtime->watchers`, `fs_scan_tree()` e watch manager | aggiunge watch alla root creata, scansiona le directory annidate con `emit_root = 0` e prepara discovery sintetica | gia' interna al backend context |
| `backend_process_scanned_dir_create()` | usa il context backend e propaga `userdata` | adatta le directory annidate viste dallo scanner al percorso raw/core | mantenere la discovery agganciata al backend context, senza dipendere dall'app completa |
| `backend_emit_synthetic_dir_create()` | chiama `on_event(raw, userdata)` | genera `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR` sintetico | gia' allineata alla callback raw con contesto opaco |
| `watch_manager_add()` | `app->inotify.fd`, `app->config.watch_mask`, `app->inotify.watchers`, `app->logger` | installa watch kernel, salva mapping, logga `WATCH_ADDED` | ricevere fd/watchers/mask/logger espliciti o un contesto backend |
| `watch_manager_remove()` | `app->inotify.fd`, `app->inotify.watchers`, `app->logger` | rimuove watch kernel, pulisce mapping, logga `WATCH_REMOVED` | ricevere fd/watchers/logger espliciti o un contesto backend |
| `watch_manager_add_recursive*()` | dipende da `watch_manager_add()`, logger e callback discovery | attraversa directory e aggiunge watch ricorsivi | ricevere contesto backend e callback discovery senza conoscere l'app completa |

Questa mappa chiarisce una cosa importante: `app_t` oggi viene usato come
contenitore comodo, non perche' il backend abbia davvero bisogno di tutta
l'applicazione. Le dipendenze sono raggruppabili in quattro famiglie:

- stato backend: `fd` e `watcher_table_t`
- configurazione backend: `recursive`, `watch_mask`, `watcher_capacity`
- diagnostica: `logger_t`
- callback raw/core: funzione `on_event` e puntatore opaco `userdata`

Il prossimo passo tecnico non deve ancora cambiare comportamento. Deve
scegliere quale API renda esplicite queste quattro famiglie senza introdurre
ownership confusa.

#### Proposta scelta: context backend separato

La direzione scelta per il refactor e' separare lo stato posseduto dal backend
dalle dipendenze esterne prese in prestito.

Stato posseduto:

```c
typedef struct inotify_backend {
    int fd;
    watcher_table_t watchers;
} inotify_backend_t;
```

Dipendenze operative prese in prestito:

```c
typedef struct inotify_backend_context {
    inotify_backend_t *runtime;
    const config_t *config;
    logger_t *logger;
    inotify_backend_event_fn on_event;
    void *userdata;
} inotify_backend_context_t;
```

Questa e' una proposta di forma, non ancora codice definitivo. Il punto
didattico e architetturale e' il seguente:

- `inotify_backend_t` risponde alla domanda: "che cosa possiede il backend?"
- `inotify_backend_context_t` risponde alla domanda: "di cosa ha bisogno il
  backend mentre lavora?"

Regole di ownership desiderate:

| Campo | Ownership | Lifetime richiesta |
| --- | --- | --- |
| `runtime` | posseduto dall'app, mutato dal backend | valido da `inotify_backend_init()` a `inotify_backend_shutdown()` |
| `config` | posseduto dall'app, letto dal backend | valido per tutta la vita del backend |
| `logger` | posseduto dall'app, usato dal backend | inizializzato prima del backend e chiuso dopo il backend |
| `on_event` | funzione fornita dall'app | valida durante `inotify_backend_poll()` e raw sintetici |
| `userdata` | posseduto dal chiamante | valido almeno durante la chiamata che lo usa |

Vantaggi della scelta:

- evita di passare `app_t` intero al backend
- mantiene `inotify_backend_t` piccolo e chiaramente proprietario solo di fd e
  watcher table
- rende esplicite configurazione e diagnostica senza trasferirne la proprieta'
- prepara `watch_manager_*()` a ricevere un contesto backend invece dell'app
  completa
- aiuta gli studenti a distinguere ownership, accesso e lifetime

Svantaggi e attenzioni:

- introduce un tipo in piu'
- richiede disciplina sui puntatori presi in prestito
- non va applicato in modo massivo senza test intermedi
- durante la migrazione il bridge shadow richiedeva ancora attenzione; oggi
  quel ponte e' rimosso e il context non deve reintrodurlo

Migrazione consigliata:

1. introdurre il tipo context senza cambiare subito tutta l'API pubblica: fatto
2. convertire prima il watch manager, perche' usa solo fd, watchers, mask e
   logger: fatto
3. convertire poi le funzioni interne del backend che gestiscono discovery e raw
   sintetici: parziale
4. lasciare per ultimo il ponte legacy shadow, oppure rimuoverlo quando shadow
   non sara' piu' necessario
5. aggiornare documentazione e test a ogni micro-passaggio

Questa scelta non cambia la semantica degli eventi: serve solo a rendere piu'
pulito il confine tra app, backend e core.

Stato implementato del primo micro-refactor:

- `inotify_backend_context_t` e' definito in
  `modules/inotify/include/inotify_backend.h`
- `watch_manager_add()`, `watch_manager_remove()`,
  `watch_manager_add_recursive()` ricevono il context, non `app_t`
- la callback di discovery del watch manager e' stata rimossa; il percorso
  runtime corrente usa `fs_scan_tree()` nel backend
- `inotify_backend.c` costruisce un context locale partendo da `app_t` nei
  punti in cui deve chiamare il watch manager
- `inotify_backend_poll()` riceve ancora `app_t` per guidare il backend, ma la
  callback raw/core non riceve piu' `app_t`: usa `userdata`

Questa scelta mantiene il refactor piccolo: il watch manager non conosce piu'
l'app completa, ma il ponte principale `app -> backend -> core` resta invariato.

Stato implementato del secondo micro-refactor:

- la callback di discovery continua a ricevere `inotify_backend_context_t`
- `backend_emit_context_t` conserva il context backend e la callback raw/core
- `backend_emit_synthetic_dir_create()` riceve il context e chiama
  `on_event(raw, userdata)`
- la firma pubblica di `inotify_backend_event_fn` non contiene piu'
  `struct app *`

Il punto residuo e' ora piu' piccolo: `inotify_backend_poll()` riceve ancora
`app_t`, ma il suo corpo puo' gia' leggere runtime e logger tramite il context
backend. La callback raw/core invece e' gia' diventata generica: il consumer
applicativo arriva da `userdata`.

Stato storico del terzo micro-refactor:

- `inotify_backend_poll()` costruisce un `inotify_backend_context_t` locale
  all'inizio della funzione
- la lettura del file descriptor usa `ctx.runtime->fd`
- il lookup del path padre usa `ctx.runtime->watchers`
- i log raw e gli errori interni al poll usano `ctx.logger`
- restavano volutamente su `app_t` solo la scelta `event_engine_mode` e la
  chiamata legacy `legacy_events_dispatch(app, ev)`, perche' allora
  appartenevano al ponte shadow temporaneo e non al backend core finale

Questo non e' piu' lo stato corrente: `event_engine_mode` e il ponte shadow
sono stati rimossi.

Stato storico del quarto micro-refactor:

- `inotify_backend_init()` costruisce un `inotify_backend_context_t` locale
  subito dopo il controllo degli argomenti
- l'inizializzazione della watcher table usa `ctx.runtime->watchers` e
  `ctx.config->watcher_capacity`
- l'apertura e il cleanup del file descriptor usano `ctx.runtime->fd`
- i log di init, errore e shadow setup usano `ctx.logger`
- nel codice di allora il ponte legacy restava esplicito tramite
  `ctx.config->event_engine_mode` e `ctx.config->move_cache_size`

In quel passo la firma pubblica era ancora `inotify_backend_init(app_t *app)`.
Quella forma e' stata superata dal dodicesimo micro-refactor, che ha cambiato
la firma pubblica pulita in `inotify_backend_init(inotify_backend_context_t *)`.
Il valore didattico del quarto passo resta comunque valido: prima abbiamo
ristretto il corpo della funzione, poi abbiamo cambiato il confine pubblico.

Stato implementato del quinto micro-refactor:

- `inotify_backend_shutdown()` costruisce un `inotify_backend_context_t` locale
  dopo il controllo degli argomenti
- la chiusura del file descriptor usa `ctx.runtime->fd`
- la distruzione della watcher table usa `ctx.runtime->watchers`
- in quel momento `legacy_events_shutdown()` restava esplicito e condizionato
  da `ALFRED_ENABLE_LEGACY_SHADOW`, perche' faceva ancora parte del ponte
  temporaneo verso il dispatcher legacy

In quel momento init e shutdown ricevevano ancora `app_t`, ma internamente
lavoravano gia' sullo stato runtime del backend attraverso il context. Dopo il
dodicesimo micro-refactor, anche le firme pubbliche pulite ricevono direttamente
`inotify_backend_context_t *`. La semantica osservabile e l'ordine di cleanup
non sono cambiati.

Stato storico del sesto micro-refactor:

- dentro `inotify_backend_poll()`, la lettura di `event_engine_mode` passa da
  `app->config.event_engine_mode` a `ctx.config->event_engine_mode`
- il dispatch legacy resta ancora `legacy_events_dispatch(app, ev)`, perche' il
  dispatcher storico richiede tuttora l'app completa
- dopo questo passo, l'uso sostanziale residuo di `app_t` nel poll path e' il
  solo ponte legacy/shadow

Questo era un passo storico della migrazione. Non cambiava comportamento:
spostava solo una dipendenza di configurazione nel context, lasciando visibile
il confine ancora aperto. In quel momento il legacy dispatcher non era ancora
stato isolato o rimosso.

Stato implementato allora:

- il corpo principale di `inotify_backend_poll()` non chiama piu' direttamente
  `legacy_events_dispatch(app, ev)`
- la chiamata legacy e' confinata nel bridge interno
  `backend_dispatch_legacy_shadow(app, &ctx, ev)`
- nel codice di allora il bridge leggeva la modalita' da
  `ctx.config->event_engine_mode`, usava `ctx.logger` per l'errore di build
  senza legacy e passava `app_t` solo al dispatcher storico
- il comportamento osservabile non cambia: in `event_engine=core` il bridge non
  fa nulla; in `event_engine=shadow` chiama il dispatcher legacy se compilato,
  altrimenti restituisce `ERR_CONFIG`

Questo passo e' didatticamente importante perche' trasformava un uso residuo di
`app_t` in un confine nominato. Quella funzione e' stata poi rimossa insieme al
confronto shadow.

Stato implementato dell'ottavo micro-refactor:

- in quel passo `inotify_backend_add_startup_watch(app, path)` restava ancora
  la funzione pubblica chiamata da `app.c`
- il wrapper pubblico controllava gli argomenti, costruiva
  `inotify_backend_context_t` con `backend_context_from_app()` e delegava a
  `backend_add_startup_watch(&ctx, path)`
- `backend_add_startup_watch()` contiene la logica reale: legge
  `ctx->config->recursive` e chiama `watch_manager_add_recursive(ctx, path)` o
  `watch_manager_add(ctx, path)`
- il comportamento non cambia, ma la forma interna mostra gia' la futura API
  naturale: context backend piu' path, senza app completa

Questo e' un esempio utile di migrazione graduale di una API C. Prima si crea
una funzione interna con la firma desiderata, poi si lascia la vecchia funzione
pubblica come adattatore. Solo quando tutti i chiamanti saranno pronti si potra'
cambiare la firma pubblica senza mescolare refactor e comportamento.

Stato implementato del nono micro-refactor:

- in quel passo `inotify_backend_shutdown(app)` restava ancora la funzione
  pubblica chiamata da `app_shutdown()`
- il wrapper pubblico controllava `app == NULL`, costruiva
  `inotify_backend_context_t` e delegava a `backend_shutdown(&ctx)`
- `backend_shutdown()` chiude `ctx->runtime->fd`, lo riporta a `-1`, spegne il
  legacy shadow opzionale con `legacy_events_shutdown()` e distrugge
  `ctx->runtime->watchers`
- a differenza del poll path, shutdown non ha bisogno di un bridge con `app_t`:
  il cleanup legacy non riceve l'app completa e il cleanup backend lavora solo
  sul runtime

Questo rende startup watch e shutdown simmetrici: entrambi hanno una funzione
pubblica compatibile con `app.c` e una funzione interna che mostra gia' il
confine futuro basato su `inotify_backend_context_t`.

Stato storico del decimo micro-refactor:

- in quel passo `inotify_backend_init(app)` restava ancora la funzione pubblica
  chiamata da `app_init()`
- il wrapper pubblico controllava `app == NULL`, costruiva
  `inotify_backend_context_t` e delegava a `backend_init(&ctx)`
- `backend_init()` conteneva tutta la logica reale di inizializzazione:
  impostava `ctx->runtime->fd = -1`, inizializzava la watcher table, apriva il
  file descriptor inotify non bloccante e, nel codice di allora,
  inizializzava il legacy shadow solo se
  `ctx->config->event_engine_mode == EVENT_ENGINE_SHADOW`
- gli error path restano invariati: fallimento di `watcher_init()` ritorna
  `ERR_ALLOC`; fallimento di `inotify_init1()` distrugge la watcher table e
  ritorna `ERR_INOTIFY`; fallimento di `legacy_events_init()` chiude fd,
  riporta fd a `-1`, distrugge la watcher table e ritorna `ERR_ALLOC`; shadow
  richiesto senza build legacy fa lo stesso cleanup e ritorna `ERR_CONFIG`

Questo passo era piu' delicato di startup watch e shutdown perche'
l'inizializzazione costruisce risorse in sequenza. In C, quando una funzione
inizializza piu' risorse, ogni uscita di errore deve rilasciare solo le risorse
gia' acquisite e lasciare lo stato in una forma prevedibile. Il refactor ha
quindi cambiato la forma della funzione, non l'ordine di acquisizione e cleanup.

Stato implementato dell'undicesimo micro-refactor:

- in quel passo `inotify_backend_poll(app, on_event, userdata)` restava la
  funzione pubblica chiamata da `app_run()`
- il wrapper pubblico controllava gli argomenti, costruiva
  `inotify_backend_context_t` e delegava a
  `backend_poll(&ctx, app, on_event, userdata)`
- `backend_poll()` contiene il corpo reale del polling e usa `ctx->runtime`,
  `ctx->logger` e gli helper raw/core
- il parametro `app_t *` veniva rinominato semanticamente in `legacy_app`
  all'interno di `backend_poll()`, per chiarire che serviva solo a
  `backend_dispatch_legacy_shadow(legacy_app, ctx, ev)`
- il comportamento non cambia: il raw/core path lavora sul context; shadow mode
  in quel momento restava disponibile come ponte temporaneo

Questo completava la forma interna context-shaped delle funzioni principali del
backend. Nel dodicesimo micro-refactor le firme pubbliche pulite di init,
startup watch e shutdown sono state effettivamente cambiate; nel tredicesimo
anche `poll()` ha ricevuto una firma pubblica a context piu' bridge legacy.

Stato implementato del dodicesimo micro-refactor:

- le firme pubbliche pulite del backend non ricevono piu' `app_t`
- `inotify_backend_init()` riceve `inotify_backend_context_t *ctx`
- `inotify_backend_add_startup_watch()` riceve
  `inotify_backend_context_t *ctx` e `path`
- `inotify_backend_shutdown()` riceve `inotify_backend_context_t *ctx`
- `app.c` costruisce il context con `app_build_inotify_backend_context()`,
  passando al backend solo `app.inotify`, `app.config` e `app.logger`
- `inotify_backend_poll()` resta temporaneamente separato dagli altri perche'
  il suo helper interno riceve ancora un bridge legacy per shadow

Questo e' il primo passo in cui il confine pubblico cambia davvero. Non e'
solo un refactor interno: da questo punto il chiamante applicativo deve sapere
costruire il context backend. La scelta e' corretta per `init`, startup watch e
shutdown perche' queste funzioni non hanno piu' bisogno dell'app completa. In
questo passo non viene applicata ancora a `poll()` per non nascondere il
problema residuo: `events.c` richiede ancora `app_t` durante il confronto
legacy/shadow.

Stato storico del tredicesimo micro-refactor:

- introdotto `legacy_shadow_bridge_t`, un piccolo bridge che contiene il
  puntatore `struct app *` richiesto dal dispatcher storico `events.c`
- `inotify_backend_poll()` non riceve piu' `app_t`: riceve
  `inotify_backend_context_t *ctx`, `legacy_shadow_bridge_t *legacy`, callback
  raw/core e `userdata`
- `app_run()` costruisce sia `backend_ctx` sia `legacy_shadow_bridge_t`
- `backend_dispatch_legacy_shadow()` riceveva il bridge e leggeva
  `legacy->app` solo quando, nel codice di allora,
  `ctx->config->event_engine_mode == EVENT_ENGINE_SHADOW`
- il percorso raw/core usa solo `ctx`; il ponte legacy/shadow e' confinato nel
  tipo dedicato

Questo non rimuove ancora la dipendenza da `app_t`, ma la mette in quarantena:
non e' piu' parte della firma normale del backend context e non e' piu' passata
come parametro principale del poll. La presenza del bridge rende esplicito che
il problema rimasto appartiene al confronto shadow, non al backend core.

Stato storico del quattordicesimo micro-refactor:

- `legacy_shadow_bridge_t` non contiene piu' `struct app *`
- il bridge contiene `legacy_shadow_dispatch_fn dispatch` e `void *userdata`
- in quel momento `app.c` costruiva il bridge con
  `app_legacy_shadow_dispatch()` e `app` come userdata solo quando la build
  abilitava `ALFRED_ENABLE_LEGACY_SHADOW`
- `app_legacy_shadow_dispatch()` e' l'unico punto nuovo in cui il puntatore
  opaco viene ricondotto ad `app_t` e passato a `legacy_events_dispatch()`
- `inotify_backend.c` non sa piu' che cosa ci sia dentro `userdata`: controlla
  solo se la callback shadow esiste e la invoca in `EVENT_ENGINE_SHADOW`

Questo e' piu' pulito del tredicesimo passo perche' la quarantena non e' piu'
"il backend contiene un campo `app` ma lo usa solo per shadow"; diventa "il
backend conosce solo una callback opzionale". Il backend resta responsabile del
momento in cui lo shadow viene chiamato, ma non conosce il tipo applicativo
storico necessario a `events.c`.

Stato implementato del quindicesimo micro-refactor:

- `legacy_events_init()` e `legacy_events_shutdown()` sono stati spostati fuori
  dal backend inotify
- `app.c` possiede ora `app_init_legacy_shadow()` e
  `app_shutdown_legacy_shadow()`
- `inotify_backend.c` non include piu' `events.h`
- `inotify_backend_init()` inizializza solo stato backend: watcher table e file
  descriptor inotify
- `inotify_backend_shutdown()` chiude solo il file descriptor e distrugge la
  watcher table
- in quel momento la build senza `ALFRED_ENABLE_LEGACY_SHADOW` rifiutava
  `event_engine=shadow` in `app_init_legacy_shadow()`, prima che il backend
  partisse

Questa scelta separa due responsabilita' che prima erano mescolate:

- il backend inotify gestisce il collegamento tecnico con Linux e produce fatti
  raw
- l'applicazione orchestra la modalita' di confronto shadow, perche' shadow e'
  una strategia di integrazione del progetto, non una responsabilita' del
  backend

Il backend mantiene solo il punto di chiamata del confronto durante il poll,
tramite callback opaca. Non inizializza, non spegne e non include piu' il codice
semantico legacy.

Stato storico del sedicesimo micro-refactor:

- `app_run()` costruisce ancora il valore `legacy_shadow_bridge_t`, ma passa al
  backend un puntatore separato `legacy_bridge`
- `legacy_bridge` restava `NULL` quando, nel codice di allora,
  `app->config.event_engine_mode == EVENT_ENGINE_CORE`
- `legacy_bridge` puntava al bridge opaco solo quando era attivo
  `EVENT_ENGINE_SHADOW`
- `backend_dispatch_legacy_shadow()` continuava ad accettare `NULL` quando il
  mode era core, perche' ritornava prima di guardare il bridge

Quel passo chiariva il percorso normale: core mode non usava nessun bridge
legacy. La firma e' stata poi semplificata ulteriormente quando shadow mode e'
stato rimosso.

Stato storico del diciassettesimo micro-refactor:

- in quel momento `inotify_backend_context_t` conteneva
  `const legacy_shadow_bridge_t *legacy_shadow`
- `app_build_inotify_backend_context()` inizializza sempre
  `ctx->legacy_shadow = NULL`
- `app_run()` assegna `backend_ctx.legacy_shadow = &legacy` solo in
  `EVENT_ENGINE_SHADOW`
- `inotify_backend_poll()` non riceve piu' il bridge shadow come parametro:
  la firma pubblica torna a `ctx`, callback raw/core e `userdata`
- `backend_dispatch_legacy_shadow()` legge il bridge da `ctx->legacy_shadow`

Quel passo non rimuoveva ancora shadow mode, ma lo rendeva meno invasivo nella
API pubblica del backend. E' stato poi seguito dalla rimozione del bridge e
dalla cancellazione del dispatcher legacy.

Stato implementato del diciottesimo micro-refactor:

- `legacy_shadow_bridge_t` e `legacy_shadow_dispatch_fn` sono stati rimossi
  dalla API pubblica del backend
- `inotify_backend_context_t` non contiene piu' `ctx->legacy_shadow`
- `app_run()` non costruisce piu' un bridge shadow e non lo passa al backend
- `backend_dispatch_legacy_shadow()` e' stato eliminato dal poll path
- `inotify_backend_poll()` consegna raw event al core e mantiene diagnostica
  watch, ma non invoca piu' `legacy_events_dispatch()`

Da questo punto il vecchio `make test-legacy-shadow` e' storico e non va piu'
usato come verifica ordinaria. I controlli utili sui watch sono in
`make test-backend-diagnostics`; il contratto semantico ufficiale resta
`make test` / `make test-core`.

- `backend_handle_dir_create()` riceve lo stesso context gia' costruito dal
  poll path, quindi non ricostruisce piu' un context da `app_t`

Questo passaggio non cambia ancora la firma pubblica di
`inotify_backend_poll()`: e' un refactor interno. Serve a rendere visibile agli
studenti una tecnica importante: si puo' restringere prima il corpo delle
funzioni e cambiare la API solo quando il nuovo confine e' abbastanza chiaro.

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

`WATCH_ADDED` e `WATCH_REMOVED` qui sono log diagnostici del backend, non eventi
semantici core. Lo startup ricorsivo usa `fs_scan_tree()` tramite
`watch_manager_add_recursive()`. Il percorso runtime corrente fa discovery nel
backend con `fs_scan_tree(..., emit_root = 0)` per generare raw event sintetici
verso il core. La vecchia API callback del watch manager e' stata rimossa:
questa separazione rende esplicito che il watch manager gestisce osservazione,
mentre il backend decide la recovery runtime.

### `app_t`

`app_t` contiene ancora lo stato runtime del backend inotify:

- `inotify`

La vecchia cache `moves` non esiste piu' nel contesto applicativo. Era uno
stato del dispatcher legacy e non deve tornare nel percorso core.

## Ordine storico dello switch

Questa era la strategia usata durante la migrazione. Resta utile per capire
perche' il lavoro e' stato spezzato in micro-refactor, ma non descrive piu'
azioni da eseguire oggi.

1. mantenere temporaneamente `ALFRED_EVENT_ENGINE=shadow` per confronto e prove
   manuali: fatto nella fase di migrazione, poi rimosso
2. spostare il codice di lettura inotify e manutenzione watch fuori da `app.c`
   verso un backend inotify esplicito: fatto
3. far emettere al backend solo `alfred_raw_event_t`, includendo eventuali raw
   sintetici per directory scoperte dallo scan: fatto
4. lasciare al core tutta la semantica: create, delete, modify, close-write,
   move, rename, relocated: fatto per il percorso ufficiale
5. rimuovere `move_cache` dal percorso runtime core: fatto; il file legacy e'
   stato poi cancellato
6. rimuovere `events.c` dal percorso ufficiale e poi dal codice corrente: fatto
7. progettare overflow/resync come feature separata: rimandato

## Passi consigliati

### 1. Stabilizzare lo shadow mode storico

Durante la migrazione abbiamo aggiunto scenari in
`tests/shadow/compare_shadow_output.py` e documentato le differenze attese.
Oggi non bisogna aggiungere nuovi test shadow: i nuovi scenari devono entrare in
`tests/core/` se verificano il contratto semantico, oppure in `tests/backend/`
se verificano diagnostica tecnica del backend.

Scenari gia' usati durante quella fase:

- move directory semplice: aggiunto come `move_dir`, allineato tra legacy e core
- modify file: aggiunto come `modify_close_write_file`; ora il backend consegna
  `IN_MODIFY` e il core produce `FILE_MODIFIED`
- close-write / file ready: il backend consegna `IN_CLOSE_WRITE` e il core
  produce `FILE_READY`; `FILE_MODIFIED` e `FILE_READY` sono semantica target
  ufficiale del core
- overflow: non risolto in shadow mode, rimandato alla progettazione
  post-switch

Nota successiva allo switch: il bridge minimo di overflow ora esiste
(`IN_Q_OVERFLOW -> ALFRED_RAW_OVERFLOW -> OVERFLOW`), ma la recovery completa
resta rimandata. La nuova area di lavoro non e' piu' "shadow vs core", ma la
mappa degli eventi/flag inotify non ancora abilitati: `IN_ACCESS`, `IN_OPEN` e
`IN_CLOSE_NOWRITE` come possibili eventi audit; `IN_ONLYDIR` e
`IN_MASK_CREATE` come candidati di robustezza per l'installazione dei watch.
Gli eventi audit non devono essere aggiunti al core filesystem principale:
servira' uno stream separato, disabilitato di default e attivabile solo con una
configurazione esplicita.
`IN_MASK_CREATE` non deve essere aggiunto alla configurazione come semplice
token di `inotify_watch_mask`: serve prima una policy esplicita del backend,
per esempio `strict|compat`, per distinguere l'errore utile `EEXIST` dal
fallback di compatibilita' su kernel che non supportano il flag.

### 1b. Stabilizzare la suite core end-to-end

La suite parallela `tests/core/` fissa il comportamento futuro del percorso
core come stream ufficiale plain, senza sostituire subito i test funzionali
storici. E' una suite end-to-end: avvia Alfred reale, riceve eventi inotify
reali e controlla l'`events.log` prodotto dal core.

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

Nota: l'overflow non fa parte del blocco iniziale dei test core end-to-end. Gli
altri scenari fissano la semantica prodotta a partire da eventi filesystem
normali; l'overflow richiede invece una decisione di recovery/resync quando il
backend non puo' piu' garantire di aver consegnato tutti gli eventi. Lo
rimandiamo a dopo lo switch completo al core, quando sara' piu' chiaro se
rappresentarlo come diagnostica backend, evento semantico di resync richiesto,
scan sintetico dell'albero o una combinazione di queste scelte.

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

Questo passo e' concluso: l'app usa l'output del core come evento ufficiale.
La tabella seguente descrive lo stato intermedio che avevamo durante la
migrazione, non il runtime corrente.

Stato intermedio storico:

```text
event_engine=core
    core_logger_on_event -> logger_event ufficiale plain
    legacy events.c      -> non chiamato dal loop

ALFRED_EVENT_ENGINE=shadow
    core_logger_on_event -> logger_event con prefisso core
    legacy events.c      -> logger_event ufficiale storico
```

In quella fase lo switch del default era gia' avvenuto senza rimuovere subito
`events.c`. Era una scelta prudente: il core diventava la sorgente ufficiale,
ma il confronto shadow restava disponibile con override esplicito. Questo stato
e' stato poi superato dalla rimozione completa del legacy.

Disegno finale:

```text
core_logger_on_event -> logger_event ufficiale
legacy events.c      -> rimosso
```

Il prefisso `core` dello shadow mode non fa parte dello stream ufficiale. Lo
stream corrente resta plain.

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

Formato shadow temporaneo storico:

```text
core seq=17 type=FILE_CREATED path=... pid=...
```

Quel formato serviva al confronto legacy/core. Se in futuro servira' uno stream
piu' ricco per debug o lezioni, conviene aggiungere un'opzione di
configurazione esplicita, per esempio:

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
- `move_cache_size` puo' essere rimosso dalla configurazione: fatto

La correlazione move deve restare nel core.

Stato attuale: `move_cache.c`, `move_cache.h` e `move_cache_size` sono stati
rimossi. La correlazione move resta nel core.

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

## Roadmap post-switch

Questa lista riassume cosa resta dopo lo switch runtime. I punti marcati
`Fatto` non sono piu' lavoro aperto: servono a dare contesto agli studenti e a
chi rilegge la migrazione.

| Passo | Effort | Perche' serve |
| --- | --- | --- |
| Rimuovere la variante build legacy-shadow | Fatto | `ENABLE_LEGACY_SHADOW`, `test-legacy-shadow`, `events.c` e `move_cache.c` non fanno piu' parte della build Makefile. |
| Rimuovere fisicamente il legacy morto | Fatto | `events.c`, `events.h`, `move_cache.c`, `move_cache.h` e `move_cache_size` sono stati rimossi dal codice corrente. |
| Archiviare suite functional/shadow | Temporaneo | `tests/functional/` e `tests/shadow/` sono marcati come storici e non sono verifiche correnti; la direzione finale e' eliminarli quando la loro utilita' didattica sara' migrata nella documentazione. |
| Spegnere shadow come modalita' ordinaria | Fatto | `core` e' il runtime ufficiale; `shadow` non e' piu' un valore riconosciuto. |
| Documentazione pesante del codice | Fatto per lo stato corrente | Backend inotify e watch manager sono stati rafforzati dopo lo switch; eventuali nuove passate saranno collegate a refactor futuri. |
| Pulizia finale delle responsabilita' | Fatto per lo stato corrente | L'audit non ha trovato un micro-refactor tecnico urgente: backend, app e core hanno confini sufficientemente chiari per chiudere questa fase. |
| Revisione completa della suite core end-to-end | Fatto per lo stato corrente | La suite core copre il contratto utente post-switch; nuovi test vanno aggiunti solo quando nasce un nuovo scenario o una regressione. |
| Allineamento scenari/eventi/documentazione | Fatto per lo stato corrente | Le mappe principali sono state riallineate al context backend e la revisione finale scenari/test e' stata eseguita. |
| Decisione finale sui file storici di test | Decisa, non eseguita | Per ora restano come archivio didattico, ma il target finale e' eliminarli completamente dopo aver salvato le parti utili nella documentazione. |
| Rimuovere `EVENT_ENGINE_SHADOW` | Fatto | `shadow` e' ora un valore di configurazione invalido generico; resta valido solo `core`. |
| Overflow/resync | Alto | E' rimandato a dopo lo switch perche' richiede una policy di recovery quando il backend perde eventi. |
| Separare subscription mask e bit riconosciuti | Medio | Il parser di `inotify_watch_mask` oggi vive vicino ai bit che Alfred sa nominare o gestire in output. Per chiarezza futura, gli eventi che l'utente puo' chiedere al kernel vanno separati dai bit tecnici che il kernel aggiunge agli eventi. |
| Gestire `IN_MOVE_SELF` per evitare path stale | Parziale | Il backend ora richiede e logga `IN_MOVE_SELF`, marca il watch `STALE` e scrive `WATCH_STALE` senza inventare eventi core. Resta da progettare la recovery/resync che riporti il watch a `VALID` o lo invalidi definitivamente. |

### Stato di chiusura della fase post-switch

Per lo stato corrente, la fase post-switch core e' chiusa. Non risultano
micro-refactor tecnici urgenti e non risultano buchi immediati nella copertura
dei test core/backend.

Restano aperti solo lavori futuri separati:

1. eliminare completamente `tests/functional/` e `tests/shadow/` quando la loro
   utilita' didattica sara' stata migrata nella documentazione
2. progettare overflow/resync come tema autonomo
3. separare nel backend inotify la subscription mask configurabile dai bit
   riconosciuti in output
4. progettare la gestione `IN_MOVE_SELF` per evitare path stale dopo lo
   spostamento della root osservata
5. aggiungere nuove passate di documentazione o test solo quando un refactor,
   un bug reale o un nuovo scenario utente lo richiedono

Questa chiusura non significa che il progetto sia finito. Significa che lo
switch totale al core non ha piu' debiti tecnici immediati da chiudere prima di
passare a temi nuovi.

### Subscription mask e bit riconosciuti

`inotify` usa sempre una `mask`, ma non tutte le `mask` hanno lo stesso ruolo.
Quando Alfred chiama:

```c
inotify_add_watch(fd, path, mask);
```

quella `mask` e' la subscription mask: indica quali eventi Alfred chiede al
kernel di monitorare. Esempi naturali sono:

```text
IN_CREATE
IN_DELETE
IN_MODIFY
IN_CLOSE_WRITE
IN_MOVED_FROM
IN_MOVED_TO
```

Quando invece Alfred legge una `struct inotify_event`, nella `event->mask`
possono comparire anche bit tecnici aggiunti dal kernel. Esempi:

```text
IN_ISDIR
IN_IGNORED
IN_Q_OVERFLOW
```

`IN_ISDIR` non e' un evento autonomo: modifica il significato dell'evento
principale. Per esempio:

```text
IN_CREATE | IN_ISDIR
```

significa "e' stata creata una directory", non "sono successi due eventi".

`IN_IGNORED` e' ancora diverso: indica che il watch e' stato rimosso dal kernel.
Questo e' fondamentale per il backend, perche' deve aggiornare la tabella
`wd -> path`, ma non e' automaticamente un evento semantico del core.

Il refactor futuro deve quindi separare due liste:

```text
eventi configurabili nella watch mask
bit riconosciuti nel raw log/adattatore
```

Questa separazione rende piu' chiaro il parser di `inotify_watch_mask`: una
chiave di configurazione dovrebbe accettare solo cio' che ha senso chiedere al
kernel come subscription. Il raw log e l'adapter, invece, devono continuare a
riconoscere anche bit tecnici restituiti dal kernel.

La matrice completa e' in
[Matrice eventi inotify](20-matrice-eventi-inotify.md).

### Audit responsabilita' residue

Audit eseguito dopo la rimozione dello shadow e le passate sui commenti di
backend e watch manager.

Risultato sintetico:

| Candidato | Rischio | Conviene ora? | Motivo |
| --- | --- | --- | --- |
| `path_join()` in `app/src/utils.c` usata solo da `modules/inotify` | Basso/medio | Non subito | La funzione e' generica sui path, non specifica di inotify. Spostarla ora ridurrebbe un include ma rischierebbe di creare una utility duplicata o troppo locale. |
| `inotify_backend_context_t` prende ancora `config_t` e `logger_t` applicativi | Basso | No | Sono dipendenze prese in prestito e documentate. Il backend non possiede quei dati e il context rende esplicita la lifetime. |
| `app.c` passa `app_t` come `userdata` della callback raw | Basso | No | Il backend non interpreta `userdata`. Il cast resta nel livello applicativo e serve a collegare raw event, core e logger. |
| `utils.h` espone molte utility generiche | Basso | Solo se cresce ancora | Dopo lo spostamento della mask inotify non contiene piu' dettagli backend-specific. Una divisione prematura aumenterebbe file piccoli senza beneficio immediato. |

Decisione: non c'e' un micro-refactor tecnico evidente da fare subito. La
revisione finale scenari/test e' stata eseguita nel passo successivo.

### Revisione finale scenari/test

Revisione eseguita sullo stato corrente:

- `tests/core/` copre il contratto semantico utente: create, delete, rename,
  move, relocated, modify, ready, recursive fast e configurazione shadow
  invalida
- `tests/backend/` copre la diagnostica watch utile: `WATCH_ADDED`,
  `WATCH_REMOVED` e watch ricorsivi lenti
- `tests/functional/` e `tests/shadow/` restano archivio storico e non devono
  tornare nella verifica ordinaria
- non viene aggiunto per ora un test backend separato sui raw sintetici: il test
  core `recursive_create_nested_dir` copre il risultato utente, mentre il
  formato raw diagnostico non e' ancora un contratto stabile

Decisione: nessun nuovo test immediato. La copertura corrente e' sufficiente
per lo stato post-switch; i prossimi test devono nascere da un bug reale, da un
nuovo scenario utente o dalla futura progettazione overflow/resync.

### Mappa test funzionali legacy e test core

La mappa dettagliata e' in
[Scenari di test](14-scenari-test.md#mappa-funzionali-legacy-e-core).

Risultato della revisione:

- tutti gli scenari funzionali storici realmente implementati hanno una
  copertura core equivalente o piu' precisa
- la suite core aggiunge scenari non presenti nei funzionali storici:
  `move_dir`, `modify_file` e `recursive_create_nested_dir`
- `tests/functional/test_move_rename_file.sh` ora contiene lo scenario legacy
  esplicito: il legacy emette `FILE_MOVED + FILE_RENAMED`, mentre il core
  emette un solo `FILE_RELOCATED`
- `tests/functional/test_recursive.sh` resta diverso da
  `tests/core/test_recursive_create_nested_dir.sh`: il primo verifica una
  creazione lenta con watch aggiunti in tempo, il secondo verifica il caso
  veloce `mkdir -p` recuperato con raw event sintetici
- i test funzionali storici controllano anche diagnostica backend come
  `WATCH_ADDED` e `WATCH_REMOVED`; questi log non devono diventare eventi
  semantici core

Decisione attuale:

```text
make test               -> alias ufficiale del percorso core
make test-core          -> nome esplicito della stessa suite core
test-legacy-shadow      -> target storico rimosso dal Makefile
```

Il nome esplicito `test-legacy-shadow` riduceva l'ambiguita' per studenti e
contributori durante la migrazione. Ora e' stato rimosso dal Makefile: la suite
storica resta solo come codice da leggere o archiviare e non deve bloccare i
refactor. Non serve creare ora una suite
`test-functional-core`: `make test` e `make test-core` avviano gia' Alfred
reale, passano dal backend inotify reale e controllano l'`events.log` core.

Roadmap completata per il cambio di `make test`:

1. fase precedente: `make test` puntava a `test-legacy-shadow`
2. fase switch: `make test` punta a `test-core`
3. fase attuale: la diagnostica utile e' stata spostata in `tests/backend/`
   e `test-legacy-shadow` e' stato rimosso dal Makefile

Prima di archiviare fisicamente gli script funzionali storici devono essere
vere queste condizioni:

- `make test-core` copre tutti gli scenari semantici ufficiali
- gli scenari legacy che controllano solo diagnostica backend sono classificati
  come diagnostica, non come contratto utente
- gli studenti sanno che `WATCH_ADDED` e `WATCH_REMOVED` sono log backend
- la documentazione degli scenari indica quali test legacy sono storici
- gli scenari diagnostici utili sono migrati in una suite backend separata,
  senza dipendere da `events.c` o dallo shadow legacy

Decisione aggiornata: "archiviare" qui e' una fase temporanea. Il target finale
non e' mantenere per sempre `tests/functional/` e `tests/shadow/`, ma eliminarli
completamente quando:

- le parti didatticamente utili sono state riportate negli `.md`
- eventuali scenari ancora utili sono stati portati in `tests/core/` o
  `tests/backend/`
- studenti e contributori non hanno piu' bisogno di leggere direttamente gli
  script legacy per capire la migrazione

Fino a quel momento le directory restano nel repository solo come archivio
storico esplicitamente non eseguibile come verifica ordinaria.

### Audit di copertura prima della rimozione legacy

L'audit corrente dice che il core copre gia' il comportamento semantico utile
del prodotto finale:

- create/delete file e directory
- rename file e directory
- move file e directory
- move+rename file e directory come singolo evento `RELOCATED`
- modify e close-write come `FILE_MODIFIED` e `FILE_READY`
- creazione ricorsiva veloce con raw sintetici per directory scoperte
- errore se si chiede `shadow` come valore di `ALFRED_EVENT_ENGINE`

La suite legacy copre ancora due categorie che non devono bloccare lo switch:

- diagnostica backend: `WATCH_ADDED`, `WATCH_REMOVED` e aggiunta progressiva
  dei watch nelle directory ricorsive lente sono gia' coperti da
  `tests/backend/`
- comportamento storico diverso dal target: doppia emissione
  `MOVED + RENAMED` invece di un solo `RELOCATED`

Questa distinzione ha autorizzato la rimozione dello shadow dal percorso
runtime. Da questo punto, se emerge uno scenario utente non coperto, lo step
corretto non e' ripristinare lo shadow, ma aggiungere un test core mirato.

Ordine operativo consigliato per la rimozione:

1. documentare che `ENABLE_LEGACY_SHADOW` e' stato rimosso dal Makefile
2. creare una suite `tests/backend/` per la diagnostica utile dei watch: fatto
   per `WATCH_ADDED`, `WATCH_REMOVED` e watch ricorsivi lenti
3. eliminare il bridge shadow da `inotify_backend_context_t`: fatto
4. eliminare `backend_dispatch_legacy_shadow()` dal poll path: fatto
5. rimuovere init/shutdown legacy da `app.c`: fatto
6. togliere `event_engine=shadow` dalla configurazione ordinaria: fatto; oggi
   `shadow` e' un valore invalido generico
7. rimuovere fisicamente `events.c`, `events.h`, `move_cache.c`,
   `move_cache.h` e `move_cache_size`: fatto
8. archiviare nei documenti la vecchia semantica solo come storia della
   migrazione: fatto

L'overflow resta fuori dal percorso immediato per una ragione precisa: non e'
solo un evento da tradurre, ma una condizione in cui il backend ammette di non
conoscere piu' con certezza lo stato del filesystem. Gestirlo bene significa
decidere se fare resync, emettere diagnostica, ricostruire eventi sintetici o
combinare piu' strategie. Farlo prima dello switch rischierebbe di mescolare due
problemi diversi: migrazione della semantica al core e recovery da perdita di
eventi.

Stato aggiornato: la traduzione minima dell'overflow e' stata implementata, ma
la parte difficile descritta sopra resta valida. In parallelo, la matrice
eventi inotify ora classifica anche gli eventi audit non gestiti e i flag di
installazione watch. Il primo flag da valutare in codice dovrebbe essere
`IN_ONLYDIR`, gia' implementato. Il passo successivo su `IN_MASK_CREATE` deve
invece partire dalla policy: modalita' strict per evitare sostituzioni
accidentali di watch esistenti, modalita' compat per mantenere il comportamento
storico o supportare kernel senza quel flag. Anche `IN_DONT_FOLLOW` deve
partire dalla policy, non dal codice: il default compatibile puo' continuare a
seguire symlink, mentre una modalita' hardening futura dovrebbe rifiutare o
diagnosticare root symlink e symlink scoperti nello scan ricorsivo.
`IN_EXCL_UNLINK` va rimandato a benchmark e scenari audit: puo' ridurre molto
il rumore in directory temporanee, ma una modalita' suppress puo' nascondere
eventi utili su file gia' unlinkati e ancora aperti.
`IN_MASK_ADD` resta fuori finche' non esiste aggiornamento dinamico parziale
delle maschere. `IN_ONESHOT` e' escluso dal runtime continuo di Alfred perche'
rimuove il watch dopo un solo evento.
Il prossimo filone sugli eventi audit deve partire dal contratto, non dal raw:
`IN_OPEN`, `IN_ACCESS` e `IN_CLOSE_NOWRITE` sono utili solo se separiamo lo
stream audit dallo stream filesystem e se documentiamo che inotify da solo non
fornisce il contesto processo/prompt necessario a un guardrail completo.

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
| Alta | `modules/inotify/src/inotify_backend.c` | E' il confine piu' importante tra inotify, raw event, watch ricorsivi e raw sintetici. |
| Alta | `modules/inotify/include/inotify_backend.h` | Deve spiegare che cosa possiede il backend e quale contratto offre all'app. |
| Alta | `core/src/alfred_correlator.c` | Contiene la logica che trasforma raw event in eventi semantici, inclusa la correlazione move/rename. |
| Alta | `core/include/alfred_correlator.h` | Deve rendere chiaro il contratto pubblico del core. |
| Alta | `app/src/app.c` | Ora deve essere descritto come orchestratore, non come luogo della semantica filesystem. |
| Media | `modules/inotify/src/watch_manager.c` | Gestisce stato backend reale e discovery ricorsiva; e' delicato per il bug `mkdir -p`. |
| Media | `modules/inotify/src/inotify_adapter.c` | Va mantenuto conversion-only; i commenti devono proteggere questo confine. |
| Media | `app/src/config.c` | Contiene scelte importanti su default core, validazione di `event_engine=core` e parsing sicuro dei numeri. |

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
- configurazione e validazione di `event_engine=core`
- inizializzazione backend inotify
- loop di polling
- conversione `struct inotify_event` -> `alfred_raw_event_t`
- ingresso nel core
- emissione degli eventi semantici
- percorso shadow legacy storico, per capire cosa e' stato rimosso

Questa mappa va progettata dopo la fase A, perche' i commenti strutturati nel
codice renderanno piu' facile generare o collegare un indice affidabile delle
funzioni.

## Stato build core-only

Il legacy shadow non e' piu' una variante di build. La build normale:

```bash
make
```

non contiene `events.c` e `move_cache.c`. Se Alfred riceve
`ALFRED_EVENT_ENGINE=shadow`, fallisce come valore di configurazione non valido
invece di fare fallback silenzioso a core mode.

Questo contratto e' fissato da:

```text
tests/core/test_invalid_event_engine_shadow.sh
```

Il test appartiene alla suite core perche' il rifiuto di valori engine non
supportati fa parte del contratto runtime ufficiale.

I target di test sono separati:

```bash
make test-core
```

ricostruisce il binario core-only ed esegue la suite `tests/core/`. Il target:

```bash
make test
```

punta alla stessa suite core. Il vecchio target `make test-legacy-shadow` non
esiste piu' nel Makefile. I test funzionali storici restano nel repository solo
come materiale di audit finche' non vengono archiviati o rimossi.

## Regola di avanzamento

Non fare uno switch grande e cieco.

Per ogni passo:

1. documentare la decisione
2. aggiungere o aggiornare test core/backend
3. verificare output semantico core e diagnostica backend
4. modificare codice in modo piccolo
5. aggiornare documentazione e progressi
6. solo dopo passare al punto successivo
