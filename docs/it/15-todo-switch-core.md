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

Lo switch previsto e' totale: lo shadow legacy non e' una feature da mantenere
nel prodotto finale. Deve restare solo il tempo necessario a verificare la
migrazione e poi deve sparire dal percorso runtime, dal Makefile ordinario e
dalla configurazione utente.

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
- le funzioni lifecycle pulite del backend ricevono `inotify_backend_context_t *`
  per usare runtime, configurazione e logger
- `inotify_backend_poll()` riceve `inotify_backend_context_t *`, callback
  raw/core e `userdata`; l'eventuale bridge legacy shadow vive nel context come
  campo opzionale e resta `NULL` in `event_engine=core`

### Decisione: nessuna sopravvivenza dello shadow

La direzione scelta e' rimuovere completamente lo shadow legacy. Questo cambia
il modo in cui leggiamo i prossimi refactor:

- non conviene trasformare il bridge shadow in una API observer stabile
- non conviene rendere `events.c` un secondo backend semantico supportato
- non conviene mantenere `ALFRED_EVENT_ENGINE=shadow` come opzione utente
- bisogna prima verificare la copertura core e poi cancellare il confronto
  legacy in passi piccoli

Il bridge rimasto in `inotify_backend_context_t` e' quindi debito temporaneo,
non un punto di estensione futuro. Va eliminato quando la suite core e la
documentazione degli scenari sono sufficienti a sostituire il vecchio
riferimento legacy.

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
- inizializza e spegne il dispatcher legacy solo quando shadow mode e' attivo
- inizializza il backend inotify
- chiama `inotify_backend_poll()` nel loop principale
- riceve raw event reali e sintetici tramite callback
- inoltra gli `alfred_raw_event_t` al core
- nella build shadow include `events.h` solo per adattare il bridge opaco alla
  vecchia funzione `legacy_events_dispatch()` e per gestire init/shutdown del
  dispatcher legacy

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
watch sta nel backend, non nell'app. Il backend non riceve piu' l'intera
`app_t`: riceve un context con runtime/config/logger, una callback raw/core e,
solo per shadow mode, un bridge legacy opaco.

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
esplicite nel context backend e nel bridge shadow opaco.

Dipendenze reali osservate oggi:

| Funzione | Campi di `app_t` usati | Motivo | Direzione futura |
| --- | --- | --- | --- |
| `inotify_backend_init()` | riceve `inotify_backend_context_t *` pubblico; delega a `backend_init(ctx)` | inizializza fd/watch table, legge configurazione e logga errori | gia' fuori da `app_t`; non inizializza piu' legacy shadow |
| `inotify_backend_add_startup_watch()` | riceve `inotify_backend_context_t *` pubblico; delega a `backend_add_startup_watch(ctx, path)` | sceglie watch singolo o ricorsivo tramite context | gia' fuori da `app_t`; resta da valutare se mantenere anche helper interno |
| `inotify_backend_shutdown()` | riceve `inotify_backend_context_t *` pubblico; delega a `backend_shutdown(ctx)` | chiude fd e distrugge watch table | gia' fuori da `app_t`; non spegne piu' legacy shadow |
| `inotify_backend_poll()` | riceve `inotify_backend_context_t *`, callback raw/core e `userdata`; il bridge shadow opzionale e' in `ctx->legacy_shadow` | legge eventi, logga raw, costruisce raw Alfred, chiama callback, invoca la callback shadow solo se attiva | rimuovere `ctx->legacy_shadow` quando shadow legacy non servira' piu' |
| `backend_handle_dir_create()` | riceve `inotify_backend_context_t`; usa `ctx.config->recursive`, `ctx.runtime->watchers` e watch manager | aggiorna watch ricorsivi e prepara discovery sintetica | gia' interna al backend context; resta da rimuovere il ponte shadow dal poll |
| `backend_process_discovered_dir()` | usa il context backend e propaga `userdata` | adatta la callback di discovery del watch manager al percorso raw/core | mantenere la discovery agganciata al backend context, senza dipendere dall'app completa |
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
- compatibilita' temporanea: legacy shadow tramite callback opaca; il cast a
  `app_t` resta fuori dal backend

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
- finche' legacy shadow esiste, alcune funzioni potranno ancora avere bisogno
  di un ponte temporaneo verso `app_t` o verso una struttura legacy dedicata

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
  `watch_manager_add_recursive()` e
  `watch_manager_add_recursive_with_discovery()` ricevono ora il context, non
  `app_t`
- la callback di discovery del watch manager riceve il context
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

Stato implementato del terzo micro-refactor:

- `inotify_backend_poll()` costruisce un `inotify_backend_context_t` locale
  all'inizio della funzione
- la lettura del file descriptor usa `ctx.runtime->fd`
- il lookup del path padre usa `ctx.runtime->watchers`
- i log raw e gli errori interni al poll usano `ctx.logger`
- restavano volutamente su `app_t` solo la scelta `event_engine_mode` e la
  chiamata legacy `legacy_events_dispatch(app, ev)`, perche' appartenevano al
  ponte shadow temporaneo e non al backend core finale

Stato implementato del quarto micro-refactor:

- `inotify_backend_init()` costruisce un `inotify_backend_context_t` locale
  subito dopo il controllo degli argomenti
- l'inizializzazione della watcher table usa `ctx.runtime->watchers` e
  `ctx.config->watcher_capacity`
- l'apertura e il cleanup del file descriptor usano `ctx.runtime->fd`
- i log di init, errore e shadow setup usano `ctx.logger`
- il ponte legacy resta esplicito tramite `ctx.config->event_engine_mode` e
  `ctx.config->move_cache_size`

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
- `legacy_events_shutdown()` resta esplicito e condizionato da
  `ALFRED_ENABLE_LEGACY_SHADOW`, perche' fa ancora parte del ponte temporaneo
  verso il dispatcher legacy

In quel momento init e shutdown ricevevano ancora `app_t`, ma internamente
lavoravano gia' sullo stato runtime del backend attraverso il context. Dopo il
dodicesimo micro-refactor, anche le firme pubbliche pulite ricevono direttamente
`inotify_backend_context_t *`. La semantica osservabile e l'ordine di cleanup
non sono cambiati.

Stato implementato del sesto micro-refactor:

- dentro `inotify_backend_poll()`, la lettura di `event_engine_mode` passa da
  `app->config.event_engine_mode` a `ctx.config->event_engine_mode`
- il dispatch legacy resta ancora `legacy_events_dispatch(app, ev)`, perche' il
  dispatcher storico richiede tuttora l'app completa
- dopo questo passo, l'uso sostanziale residuo di `app_t` nel poll path e' il
  solo ponte legacy/shadow

Il passo non cambia comportamento. Sposta solo una dipendenza di configurazione
nel context, lasciando visibile il confine ancora aperto: il legacy dispatcher
non e' stato ancora isolato o rimosso.

Stato implementato del settimo micro-refactor:

- il corpo principale di `inotify_backend_poll()` non chiama piu' direttamente
  `legacy_events_dispatch(app, ev)`
- la chiamata legacy e' confinata nel bridge interno
  `backend_dispatch_legacy_shadow(app, &ctx, ev)`
- il bridge legge la modalita' da `ctx.config->event_engine_mode`, usa
  `ctx.logger` per l'errore di build senza legacy e passa `app_t` solo al
  dispatcher storico
- il comportamento osservabile non cambia: in `event_engine=core` il bridge non
  fa nulla; in `event_engine=shadow` chiama il dispatcher legacy se compilato,
  altrimenti restituisce `ERR_CONFIG`

Questo passo e' didatticamente importante perche' trasforma un uso residuo di
`app_t` in un confine nominato. Il backend poll path resta ancora compatibile
con shadow mode, ma ora il debito legacy e' localizzato in una funzione che
potra' essere rimossa o spostata quando il confronto shadow non servira' piu'.

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

Stato implementato del decimo micro-refactor:

- in quel passo `inotify_backend_init(app)` restava ancora la funzione pubblica
  chiamata da `app_init()`
- il wrapper pubblico controllava `app == NULL`, costruiva
  `inotify_backend_context_t` e delegava a `backend_init(&ctx)`
- `backend_init()` contiene tutta la logica reale di inizializzazione:
  imposta `ctx->runtime->fd = -1`, inizializza la watcher table, apre il file
  descriptor inotify non bloccante e inizializza il legacy shadow solo se
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
  resta disponibile come ponte temporaneo

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

Stato implementato del tredicesimo micro-refactor:

- introdotto `legacy_shadow_bridge_t`, un piccolo bridge che contiene il
  puntatore `struct app *` richiesto dal dispatcher storico `events.c`
- `inotify_backend_poll()` non riceve piu' `app_t`: riceve
  `inotify_backend_context_t *ctx`, `legacy_shadow_bridge_t *legacy`, callback
  raw/core e `userdata`
- `app_run()` costruisce sia `backend_ctx` sia `legacy_shadow_bridge_t`
- `backend_dispatch_legacy_shadow()` riceve il bridge e legge `legacy->app`
  solo quando `ctx->config->event_engine_mode == EVENT_ENGINE_SHADOW`
- il percorso raw/core usa solo `ctx`; il ponte legacy/shadow e' confinato nel
  tipo dedicato

Questo non rimuove ancora la dipendenza da `app_t`, ma la mette in quarantena:
non e' piu' parte della firma normale del backend context e non e' piu' passata
come parametro principale del poll. La presenza del bridge rende esplicito che
il problema rimasto appartiene al confronto shadow, non al backend core.

Stato implementato del quattordicesimo micro-refactor:

- `legacy_shadow_bridge_t` non contiene piu' `struct app *`
- il bridge contiene `legacy_shadow_dispatch_fn dispatch` e `void *userdata`
- `app.c` costruisce il bridge con `app_legacy_shadow_dispatch()` e `app` come
  userdata solo quando la build abilita `ALFRED_ENABLE_LEGACY_SHADOW`
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
- la build senza `ALFRED_ENABLE_LEGACY_SHADOW` rifiuta `event_engine=shadow` in
  `app_init_legacy_shadow()`, prima che il backend parta

Questa scelta separa due responsabilita' che prima erano mescolate:

- il backend inotify gestisce il collegamento tecnico con Linux e produce fatti
  raw
- l'applicazione orchestra la modalita' di confronto shadow, perche' shadow e'
  una strategia di integrazione del progetto, non una responsabilita' del
  backend

Il backend mantiene solo il punto di chiamata del confronto durante il poll,
tramite callback opaca. Non inizializza, non spegne e non include piu' il codice
semantico legacy.

Stato implementato del sedicesimo micro-refactor:

- `app_run()` costruisce ancora il valore `legacy_shadow_bridge_t`, ma passa al
  backend un puntatore separato `legacy_bridge`
- `legacy_bridge` resta `NULL` quando
  `app->config.event_engine_mode == EVENT_ENGINE_CORE`
- `legacy_bridge` punta al bridge opaco solo quando e' attivo
  `EVENT_ENGINE_SHADOW`
- `backend_dispatch_legacy_shadow()` continua ad accettare `NULL` quando il
  mode e' core, perche' ritorna prima di guardare il bridge

Questo chiarisce il percorso normale: core mode non usa nessun bridge legacy.
Il parametro rimane nella firma di `inotify_backend_poll()` solo perche' shadow
mode e' ancora disponibile come strumento temporaneo di confronto.

Stato implementato del diciassettesimo micro-refactor:

- `inotify_backend_context_t` contiene ora
  `const legacy_shadow_bridge_t *legacy_shadow`
- `app_build_inotify_backend_context()` inizializza sempre
  `ctx->legacy_shadow = NULL`
- `app_run()` assegna `backend_ctx.legacy_shadow = &legacy` solo in
  `EVENT_ENGINE_SHADOW`
- `inotify_backend_poll()` non riceve piu' il bridge shadow come parametro:
  la firma pubblica torna a `ctx`, callback raw/core e `userdata`
- `backend_dispatch_legacy_shadow()` legge il bridge da `ctx->legacy_shadow`

Questo passo non rimuove shadow mode, ma lo rende meno invasivo nella API
pubblica del backend. Il percorso core ufficiale vede una firma normale:
context piu' callback. Il ponte legacy resta confinato in un campo opzionale
del context, da eliminare quando il confronto shadow non servira' piu'.

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
| Revisione completa della suite core end-to-end | Medio | I test core devono fissare il comportamento ufficiale prima di archiviare o ridurre i test legacy. |
| Allineamento scenari/eventi/documentazione | Alto | Per ogni scenario importante deve essere chiaro il passaggio `filesystem -> inotify -> raw Alfred -> evento semantico`. |
| Decisione sui test funzionali legacy | Medio | Dopo lo switch bisogna scegliere se mantenerli come shadow, migrarli al core o archiviarne una parte. |
| Spegnere shadow come modalita' ordinaria | Medio | `core` e' gia' il default; shadow non deve sopravvivere come modalita' utente. |
| Rimozione finale del legacy | Medio/alto | La decisione e' eliminare il percorso legacy/shadow dopo aver verificato copertura core e documentazione. |
| Overflow/resync | Alto | E' rimandato a dopo lo switch perche' richiede una policy di recovery quando il backend perde eventi. |

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
make test-legacy-shadow -> funzionali storici legacy/shadow
```

Il nome esplicito `test-legacy-shadow` riduce l'ambiguita' per studenti e
contributori: la suite storica resta disponibile, ma non e' piu' presentata come
il contratto futuro del prodotto. Non serve creare ora una suite
`test-functional-core`: `make test` e `make test-core` avviano gia' Alfred
reale, passano dal backend inotify reale e controllano l'`events.log` core.

Roadmap completata per il cambio di `make test`:

1. fase precedente: `make test` puntava a `test-legacy-shadow`
2. fase switch: `make test` punta a `test-core`
3. fase successiva: `test-legacy-shadow` resta come target diagnostico
   esplicito oppure viene archiviato insieme al vecchio dispatcher

Prima di archiviare `test-legacy-shadow` devono essere vere queste condizioni:

- `make test-core` copre tutti gli scenari semantici ufficiali
- gli scenari legacy che controllano solo diagnostica backend sono classificati
  come diagnostica, non come contratto utente
- gli studenti sanno che `WATCH_ADDED` e `WATCH_REMOVED` sono log backend
- la documentazione degli scenari indica quali test legacy sono storici
- gli scenari diagnostici utili sono migrati in una suite backend separata,
  senza dipendere da `events.c` o dallo shadow legacy

### Audit di copertura prima della rimozione legacy

L'audit corrente dice che il core copre gia' il comportamento semantico utile
del prodotto finale:

- create/delete file e directory
- rename file e directory
- move file e directory
- move+rename file e directory come singolo evento `RELOCATED`
- modify e close-write come `FILE_MODIFIED` e `FILE_READY`
- creazione ricorsiva veloce con raw sintetici per directory scoperte
- errore esplicito se si chiede shadow in una build core-only

La suite legacy copre ancora due categorie che non devono bloccare lo switch:

- diagnostica backend: `WATCH_ADDED`, `WATCH_REMOVED` e aggiunta progressiva
  dei watch nelle directory ricorsive lente sono gia' coperti da
  `tests/backend/`
- comportamento storico diverso dal target: doppia emissione
  `MOVED + RENAMED` invece di un solo `RELOCATED`

Questa distinzione autorizza la prossima fase: rimuovere progressivamente lo
shadow dal percorso runtime, mantenendo il core come unico contratto
semantico. Se durante la rimozione emerge uno scenario utente non coperto, lo
step corretto non e' ripristinare lo shadow, ma aggiungere un test core mirato.

Ordine operativo consigliato per la rimozione:

1. documentare che `ENABLE_LEGACY_SHADOW` e' temporaneo
2. creare una suite `tests/backend/` per la diagnostica utile dei watch: fatto
   per `WATCH_ADDED`, `WATCH_REMOVED` e watch ricorsivi lenti
3. eliminare il bridge shadow da `inotify_backend_context_t`
4. eliminare `backend_dispatch_legacy_shadow()` dal poll path
5. rimuovere init/shutdown legacy da `app.c`
6. togliere `event_engine=shadow` dalla configurazione ordinaria
7. rimuovere `events.c`, `move_cache.c` e target `test-legacy-shadow`
8. archiviare nei documenti la vecchia semantica solo come storia della
   migrazione

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

Questo contratto e' fissato da:

```text
tests/core/test_shadow_requires_legacy_build.sh
```

Il test appartiene alla suite core perche' `make test-core` ricostruisce Alfred
con `ENABLE_LEGACY_SHADOW=0`. In questo modo controlliamo proprio il caso che ci
interessa: shadow mode richiesto a runtime, ma dispatcher legacy assente dalla
build.

I target di test sono separati:

```bash
make test-core
```

ricostruisce il binario core-only ed esegue la suite `tests/core/`. Il target:

```bash
make test
```

punta alla stessa suite core. Il target:

```bash
make test-legacy-shadow
```

`make test-legacy-shadow` ricostruisce il binario con
`ENABLE_LEGACY_SHADOW=1` ed esegue i test funzionali storici, che usano ancora
il confronto shadow/legacy.

## Regola di avanzamento

Non fare uno switch grande e cieco.

Per ogni passo:

1. documentare la decisione
2. aggiungere o aggiornare scenario shadow
3. verificare output legacy/core
4. modificare codice in modo piccolo
5. aggiornare documentazione e progressi
6. solo dopo passare al punto successivo
