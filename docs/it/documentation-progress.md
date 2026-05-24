# Stato documentazione didattica

Questo file traccia lo stato dei capitoli in `docs/it`.

Stati usati:

- `Completo`: capitolo leggibile e gia' utile agli studenti.
- `Parziale`: capitolo presente ma da espandere.
- `Da fare`: capitolo ancora vuoto o solo pianificato.

| Stato | Capitolo |
| --- | --- |
| Completo | `README.md` |
| Completo | `01-panoramica-progetto.md` |
| Completo | `02-architettura-generale.md` |
| Parziale | `03-struttura-cartelle.md` |
| Completo | `04-livello-applicazione.md` |
| Completo | `05-modulo-inotify.md` |
| Completo | `06-core-engine.md` |
| Completo | `07-flusso-eventi.md` |
| Completo | `08-guida-c-usato-nel-progetto.md` |
| Completo | `09-makefile-e-build-system.md` |
| Completo | `10-debugging-test-e-strumenti.md` |
| Parziale | `11-come-contribuire.md` |
| Completo | `12-confronto-shadow-mode.md` |
| Completo | `13-semantica-eventi.md` |
| Completo | `14-scenari-test.md` |
| Parziale | `15-todo-switch-core.md` |
| Parziale | `16-mappa-codice-e-strutture.md` |
| Parziale | `17-roadmap-documentazione-avanzata.md` |
| Completo | `../code-browser/README.md` |
| Completo | `../sourcebot-browser/README.md` |
| Parziale | `../kythe-browser/README.md` |
| Parziale | `glossario.md` |

## Aggiornamenti recenti

- `modules/inotify/src/inotify_backend.c`: aggiunto
  `backend_dispatch_legacy_shadow()` per confinare in un bridge interno la
  chiamata residua a `legacy_events_dispatch(app, ev)`. Il poll path principale
  resta invariato come comportamento, ma il debito legacy/shadow e' ora
  localizzato e nominato.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  settimo micro-refactor del backend context, chiarendo che `app_t` resta
  necessario solo per il bridge legacy e non per il percorso raw/core.
- `tools/code-browsing/`: aggiunto uno strato operativo comune per i browser
  del codice, con controllo Docker, setup/start/stop/restart/status aggregati e
  un `docker-compose.yml` di riferimento per Sourcebot, Elixir e Kythe. Graphify
  resta escluso finche' non viene completato uno spike tecnico dedicato.
- `docs/sourcebot-browser/`, `docs/code-browser/`, `docs/kythe-browser/`:
  aggiunti gli script mancanti di setup/status/restart per rendere uniforme il
  ciclo operativo dei container.
- `10-debugging-test-e-strumenti.md` e `00-regole-operative.md`: documentato il
  provisioning Docker per contributori e studenti, chiarendo che Docker e' un
  prerequisito host e che gli script del progetto preparano i container ma non
  installano il daemon.
- `README.md`: aggiunto il rimando agli script aggregati per la gestione dei
  browser del codice.
- `modules/inotify/src/inotify_backend.c`: nel poll path la scelta
  `event_engine_mode` viene ora letta da `ctx.config` invece che da
  `app->config`. Resta intenzionalmente legato ad `app_t` solo il dispatch
  legacy `legacy_events_dispatch(app, ev)`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  sesto micro-refactor del backend context, chiarendo che il residuo sostanziale
  nel poll path e' il ponte legacy/shadow.
- `10-debugging-test-e-strumenti.md` e `00-regole-operative.md`: aggiunto un
  esempio dettagliato e riproducibile di ricognizione prima di un micro-refactor
  usando Kythe, `rg`, `sed` e lettura della documentazione collegata. La sezione
  spiega opzioni CLI, sintassi delle regex e perche' ogni pattern viene cercato.
- `00-regole-operative.md` e `10-debugging-test-e-strumenti.md`: collegata la
  procedura pre-commit alla fase precedente di consultazione del codice per
  contributori umani. Aggiunti rimandi operativi a Sourcebot, Elixir e Kythe,
  con indicazione degli URL locali e del ruolo degli strumenti prima della
  modifica.
- `17-roadmap-documentazione-avanzata.md` e `README.md`: aggiunta una roadmap
  per documentazione navigabile, grafi del codice e animazioni. La roadmap
  confronta Markdown/Mermaid, Graphviz, Doxygen, Sphinx+Breathe, Kythe,
  Graphify, Kroki, D2, VHS e asciinema, indicando esempi online da studiare e
  una sequenza di spike futuri a partire da Doxygen + Graphviz.
- `00-regole-operative.md` e `10-debugging-test-e-strumenti.md`: aggiunta la
  procedura manuale pre-commit riproducibile senza strumenti di AI. La
  documentazione spiega l'ordine `git diff --check`, `make`, `make test-core`,
  `make test`, `make`, il motivo di ogni comando, cosa significa percorso core
  end-to-end ufficiale e perche' il `make` finale riporta il workspace alla
  build core-only dopo la suite legacy/shadow.
- `modules/inotify/src/inotify_backend.c`: `inotify_backend_shutdown()` ora
  costruisce un `inotify_backend_context_t` locale e usa `ctx.runtime` per
  chiudere il file descriptor e distruggere la watcher table. Il cleanup legacy
  shadow resta esplicito e condizionato dalla build.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  quinto micro-refactor del backend context, chiarendo la simmetria tra init e
  shutdown e il fatto che il cleanup legacy non appartiene al backend core
  finale.
- `modules/inotify/src/inotify_backend.c`: `inotify_backend_init()` ora
  costruisce un `inotify_backend_context_t` locale e usa `ctx.runtime`,
  `ctx.config` e `ctx.logger` per watcher table, file descriptor, configurazione
  e diagnostica. La firma pubblica resta `inotify_backend_init(app_t *app)` e
  il ponte legacy/shadow resta esplicito tramite la configurazione.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  quarto micro-refactor del backend context, chiarendo che anche l'init e'
  ristretto internamente senza cambiare API pubblica o semantica osservabile.
- `00-regole-operative.md`: aggiunte regole operative per usare Kythe e un
  futuro Graphify nel workflow: gli indici semantici servono a restringere il
  campo e ridurre letture inutili, ma ogni risultato va verificato sui file
  sorgente reali prima di modificare codice o documentazione. Aggiunti anche
  principi di ragionamento ispirati alle guideline Karpathy-style:
  assunzioni esplicite, semplicita', modifiche chirurgiche e obiettivi
  verificabili.
- `docs/sourcebot-browser/`: aggiunto setup locale per Sourcebot con script di
  avvio, stop e stato, configurazione per indicizzare Alfred come repository
  Git locale read-only, telemetria disabilitata, accesso anonimo locale e uso
  di un Docker named volume per evitare problemi di ownership del database
  embedded. Il README spiega avvio, log, reset completo, query utili, limiti
  della code navigation Enterprise e confronto con Elixir/Kythe.
- `docs/kythe-browser/README.md` e `10-debugging-test-e-strumenti.md`:
  chiarito a cosa puo' servire Kythe in pratica anche senza GUI completa:
  interrogazione di simboli e cross-reference, generazione futura di mappe,
  controlli automatici sulla documentazione e riduzione del lavoro esplorativo
  per agenti che devono leggere il codice senza aprire molti file irrilevanti.
- `10-debugging-test-e-strumenti.md` e `README.md`: aggiunta una sezione sui
  browser del codice, chiarendo quando usare Elixir, Sourcebot e Kythe e quali
  comandi servono per avviarli, fermarli e verificarli.
- `docs/code-browser/`: aggiunta una configurazione locale per Bootlin Elixir,
  con script di setup, reindex, avvio e stop. La guida spiega come usare il code
  browser per navigare Alfred dal browser e collegare la lettura del codice alla
  documentazione italiana.
- `modules/inotify/src/inotify_backend.c`: `inotify_backend_poll()` ora
  costruisce un `inotify_backend_context_t` locale e usa `ctx.runtime` e
  `ctx.logger` per file descriptor, watcher table e diagnostica. Restano su
  `app_t` solo la selezione `event_engine_mode` e la dispatch legacy/shadow.
  Anche `backend_handle_dir_create()` riceve ora il context gia' costruito dal
  poll path invece di ricostruirlo dall'applicazione.
- `modules/inotify/src/inotify_backend.c`: lo startup watch path legge ora
  `recursive` da `ctx.config`, quindi anche la scelta tra watch singolo e
  ricorsivo passa dal context backend interno.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  terzo micro-refactor del backend context, spiegando perche' il poll path puo'
  essere ristretto internamente prima di cambiare la firma pubblica.
- `modules/inotify/include/inotify_backend.h`,
  `modules/inotify/src/inotify_backend.c` e `app/src/app.c`: cambiata la
  callback raw/core `inotify_backend_event_fn` da `(app, raw, userdata)` a
  `(raw, userdata)`. L'applicazione passa `app_t` come `userdata`, cosi' il
  backend non deve piu' conoscere il tipo del consumer degli eventi raw.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: aggiornata la
  documentazione del context backend per spiegare che la callback raw/core e'
  ora generica e che il residuo `app_t` resta in `inotify_backend_poll()`.
- `modules/inotify/src/inotify_backend.c`: esteso l'uso del context backend alla
  discovery ricorsiva e all'emissione dei raw event sintetici. Dopo il cambio
  della callback pubblica, anche questi raw sintetici vengono consegnati con
  `on_event(raw, userdata)`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  secondo micro-refactor del context, chiarendo che il prossimo confine da
  discutere e' la firma della callback raw/core.
- `modules/inotify/include/inotify_backend.h`,
  `modules/inotify/include/watch_manager.h`,
  `modules/inotify/src/watch_manager.c` e
  `modules/inotify/src/inotify_backend.c`: introdotto
  `inotify_backend_context_t` minimo e convertito il watch manager a ricevere
  il context invece dell'intero `app_t`. Il backend costruisce ancora il context
  da `app_t`, quindi il cambiamento resta confinato e non modifica la semantica
  degli eventi.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: aggiornato lo
  stato della proposta context per indicare che il primo micro-refactor sul
  watch manager e' stato implementato.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentata la
  proposta scelta per il prossimo refactor delle responsabilita': introdurre un
  `inotify_backend_context_t` separato dallo stato posseduto
  `inotify_backend_t`, cosi' config, logger e callback restano dipendenze prese
  in prestito e non campi posseduti dal backend.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: aggiunta la
  mappa delle dipendenze residue del backend inotify da `app_t`. La mappa
  distingue stato backend (`fd`, watcher table), configurazione backend
  (`recursive`, `watch_mask`, `watcher_capacity`), diagnostica (`logger`) e
  compatibilita' temporanea legacy/shadow.
- `14-scenari-test.md`, `15-todo-switch-core.md`,
  `10-debugging-test-e-strumenti.md`, `09-makefile-e-build-system.md` e
  `12-confronto-shadow-mode.md`: chiarito che `make test-core` e' gia' un test
  end-to-end del percorso core reale, non un test unitario del solo core in
  memoria. Rimossa l'ambiguita' sulla possibile suite `test-functional-core`:
  per ora non serve una terza suite.
- `tests/functional/test_move_rename_file.sh`: implementato lo scenario
  funzionale legacy per il file spostato e rinominato, fissando la doppia
  emissione storica `FILE_MOVED + FILE_RENAMED`.
- `14-scenari-test.md`: aggiornata la mappa tra test funzionali legacy e test
  core-only: `move_rename_file` ora e' coperto da entrambe le suite, con
  differenza semantica intenzionale tra doppio evento legacy e singolo
  `FILE_RELOCATED` del core.
- `15-todo-switch-core.md`: documentata la decisione provvisoria sui test:
  `make test` resta per ora legacy-shadow, mentre `make test-core` resta il
  percorso end-to-end ufficiale del core; piu' avanti si decidera' se `make
  test` dovra' diventare alias del percorso core o restare legacy.
- `16-mappa-codice-e-strutture.md`: aggiunto un call graph guidato
  `main -> app -> backend -> core -> logger`, piu' due sezioni discorsive sul
  ciclo backend inotify e sul ciclo core. Le nuove sezioni spiegano quali
  funzioni cambiano livello di astrazione, quali strutture dati vengono mutate e
  perche' watch state, raw event e semantica restano separati.
- `app/include/app.h`, `app/src/app.c`, `core/include/alfred_correlator.h`,
  `core/src/alfred_tables.h` e `core/src/alfred_tables.c`: rafforzati i
  commenti di confine su app come orchestratore, backend come produttore di raw
  fact, core come proprietario di sequenza, debounce e correlazione move.
- `core/src/alfred_tables.c`: aggiunti controlli di allocazione mancanti in
  `alfred_move_insert()` e `alfred_debounce_get()`, mantenendo invariato il
  contratto pubblico ma evitando dereferenziazioni in caso di memoria esaurita.
- `Makefile` e `modules/inotify/src/inotify_backend.c`: reso il legacy shadow
  opzionale a livello build con `ENABLE_LEGACY_SHADOW=1`; la build normale non
  compila piu' `events.c` e `move_cache.c`, mentre shadow mode fallisce con
  errore esplicito se il binario non contiene il legacy.
- `09-makefile-e-build-system.md`, `10-debugging-test-e-strumenti.md`,
  `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  flag `ENABLE_LEGACY_SHADOW`, la separazione fra build core-only e build
  legacy-shadow, il comportamento di `make test` e il vincolo di
  `ALFRED_EVENT_ENGINE=shadow`; chiarito anche che `make test-core` ricostruisce
  esplicitamente la variante core-only.
- `04-livello-applicazione.md`: corrette ulteriori sezioni obsolete su
  `app_t`, shadow mode ed `event_engine`, chiarendo che il core e' il percorso
  ufficiale di default e che il legacy e' solo confronto esplicito.
- `01-panoramica-progetto.md`: aggiornato lo stato corrente per indicare che il
  core e' lo stream semantico ufficiale di default, mentre `events.c` e
  `move_cache.c` restano supporto legacy per shadow mode; aggiunto rimando alla
  lettura guidata della codebase.
- `16-mappa-codice-e-strutture.md`: chiarito anche nel capitolo della lettura
  guidata che `make docs-animations` e' un nome ipotetico futuro e non un target
  gia' presente nel Makefile.
- `14-scenari-test.md`: aggiunta una mappa rapida tra scenari di test e
  scenari animabili della lettura guidata, distinguendo contratto osservabile
  dei test e percorso interno nel codice.
- `glossario.md`: aggiornato `Shadow mode` per riflettere il default core e
  aggiunte voci per frame animabile, documentazione dinamica, artefatto
  generato, Mermaid e SVG.
- `09-makefile-e-build-system.md` e `10-debugging-test-e-strumenti.md`:
  documentato che `make docs-animations` e `docs/generated/animations/` sono
  una direzione futura, non target gia' disponibili, e che il Markdown resta la
  sorgente didattica primaria.
- `16-mappa-codice-e-strutture.md`: aggiunte convenzioni per scrivere frame
  animabili futuri e una pipeline ipotetica
  `Markdown -> JSON -> SVG -> GIF/video/HTML`, mantenendo il Markdown come
  sorgente didattica principale.
- `16-mappa-codice-e-strutture.md`: aggiunta una sezione di scenari animabili
  con frame testuali per aggiunta watch, create file, close-write/file-ready,
  modify debounced, move+rename nel core, `mkdir -p` ricorsivo veloce e
  rimozione watch. Questi frame preparano una futura generazione di GIF, video o
  viste HTML interattive.
- `README.md`, `03-struttura-cartelle.md` e `11-come-contribuire.md`: aggiunti
  rimandi alla nuova lettura guidata della codebase, chiarendo che il capitolo
  sulle strutture dati va aggiornato quando cambiano funzioni centrali, campi o
  flussi eventi.
- `04-livello-applicazione.md`, `07-flusso-eventi.md` e
  `12-confronto-shadow-mode.md`: corrette frasi obsolete che descrivevano il
  core come solo shadow o il legacy come comportamento ufficiale; ora la
  documentazione distingue default `event_engine=core` e shadow mode
  diagnostico esplicito.
- `core/examples/main_demo.c`: aggiunti commenti strutturati per chiarire che il
  file e' una dimostrazione isolata del core, non il percorso runtime
  produttivo, e che parte gia' da `alfred_raw_event_t`.
- `docs/commenting-progress.md`: completata anche la revisione del demo core,
  rimuovendo l'ultimo file `Pending` dalla passata corrente.
- `core/src/alfred_tables.h`, `core/src/alfred_tables.c`,
  `core/src/alfred_utils.h` e `core/src/alfred_utils.c`: aggiunti commenti
  strutturati sulle tabelle interne del core, sugli helper di tempo/hash/path e
  sulla proprieta' dei path copiati per la correlazione move.
- `16-mappa-codice-e-strutture.md`: aggiunta la sezione sulle strutture dati
  del core, con schema `alfred_engine`, tabelle per `alfred_move_entry_t` e
  `alfred_debounce_entry_t`, sequence diagram per move/debounce e frame logici
  preparatori per future animazioni; riordinato il capitolo in modo che prima
  completi il ciclo backend e poi passi al core.
- `docs/commenting-progress.md`: segnati come completati i file interni del
  core `alfred_tables.*` e `alfred_utils.*`.
- `00-regole-operative.md` e `16-mappa-codice-e-strutture.md`: impostata la
  mappa del codice come lettura guidata della codebase, pensata per accompagnare
  studenti poco esperti attraverso funzioni, responsabilita', strutture dati,
  eventi trigger e rimandi ai capitoli teorici o tecnici gia' presenti.
- `modules/inotify/include/events.h`, `modules/inotify/src/events.c`,
  `modules/inotify/include/move_cache.h` e `modules/inotify/src/move_cache.c`:
  aggiunti commenti che marcano esplicitamente il dispatcher e la cache move
  come percorso legacy/shadow-only, distinguendoli dalla semantica target del
  core.
- `16-mappa-codice-e-strutture.md`: aggiunta la sezione sulle strutture legacy
  shadow, con schema chiamate di `legacy_events_dispatch()`, tabelle per
  `move_cache_t` e `move_slot_t`, e differenza fra doppio evento legacy e
  singolo `RELOCATED` del core.
- `docs/commenting-progress.md`: completata la passata pesante corrente sui
  principali file C/H coinvolti nello switch core.
- `modules/inotify/src/inotify_adapter.c`, `app/include/config.h` e
  `app/src/config.c`: rafforzati i commenti sul ruolo conversion-only
  dell'adapter, sulla lifetime del buffer `raw.path`, sul default `core`, sullo
  shadow mode temporaneo e sui campi di configurazione che guidano watcher
  table e mask inotify.
- `16-mappa-codice-e-strutture.md`: estesa la mappa con `config_t`,
  `watch_mask`, `event_engine_mode`, parsing sicuro delle capacita' e ruolo
  stateless dell'adapter inotify.
- `docs/commenting-progress.md`: aggiornata la lista dei prossimi file da
  commentare; restano in priorita' i file legacy `events.c` e `move_cache.c`.
- `00-regole-operative.md`: aggiunta la regola di metodo per documentare flussi
  complessi e strutture dati con diagrammi, tabelle campo/funzione, sequence
  diagram e frame logici preparatori per future animazioni o GIF didattiche.
- `modules/inotify/include/watch_manager.h`,
  `modules/inotify/src/watch_manager.c`, `modules/inotify/include/watcher.h` e
  `modules/inotify/src/watcher.c`: aggiunti commenti strutturati su gestione
  dei watch, tabella `wd -> path`, discovery ricorsiva e separazione tra
  diagnostica backend e semantica core.
- `16-mappa-codice-e-strutture.md`: aggiunto nuovo capitolo didattico con
  call graph runtime, strutture dati backend, tabelle campo/funzione, sequenze
  di inserimento/rimozione watch, scenario `mkdir -p` e proposta per future GIF
  o animazioni generate.
- `README.md`: aggiunto il nuovo capitolo sulla mappa del codice e strutture
  dati al percorso consigliato.
- `docs/commenting-progress.md`: aggiornata la passata pesante indicando come
  completati `watch_manager` e `watcher`.
- `modules/inotify/include/inotify_backend.h`,
  `modules/inotify/src/inotify_backend.c`, `core/include/alfred_correlator.h`,
  `core/src/alfred_correlator.c` e `app/src/app.c`: avviata la passata pesante
  sui commenti del codice, chiarendo contratti, proprieta' dello stato,
  confini backend/core/app, raw event sintetici e ruolo temporaneo dello shadow
  legacy.
- `05-modulo-inotify.md` e `06-core-engine.md`: aggiornata la spiegazione
  didattica delle funzioni principali commentate nel codice e corretto lo stato
  corrente: il core e' lo stream ufficiale di default, mentre lo shadow mode e'
  un confronto esplicito.
- `docs/commenting-progress.md`: segnato il completamento della prima passata
  pesante su backend inotify e correlatore core; aggiornata la lista dei file
  successivi da commentare.
- `15-todo-switch-core.md`: aggiunta roadmap orientativa allo switch definitivo
  con effort stimato, motivazione del rinvio overflow, piano della fase A per
  la documentazione pesante del codice e proposta per una futura mappa
  generabile delle chiamate tra funzioni.
- `docs/commenting-progress.md`: aggiunta la priorita' della prossima passata
  pesante sui commenti del codice, distinguendo file core/backend/app e file
  legacy shadow.

- `10-debugging-test-e-strumenti.md`: aggiunta spiegazione dei test shadow e
  dell'uso prudente di `--strict`.
- `12-confronto-shadow-mode.md`: aggiunto lo scenario `move_rename_dir` e la
  differenza attesa tra legacy e core.
- `13-semantica-eventi.md`: chiarito che `RELOCATED` e' un evento unico sia per
  file sia per directory.
- `10-debugging-test-e-strumenti.md`: aggiunto riferimento allo scenario
  diagnostico `recursive_create_nested_dir`.
- `12-confronto-shadow-mode.md`: aggiunto lo scenario
  `recursive_create_nested_dir` per osservare il bug delle directory create
  prima dell'aggiunta dei watch ricorsivi.
- `12-confronto-shadow-mode.md`: documentato l'uso di `--keep-logs` per
  distinguere eventi semantici, raw log e diagnostica backend come
  `WATCH_ADDED`.
- `13-semantica-eventi.md`: documentata la semantica desiderata per creazioni
  ricorsive veloci come `mkdir -p`.
- `tests/shadow/compare_shadow_output.py`: ampliato l'help del comando con note
  su modalita' diagnostica, `--strict` e `--keep-logs`.
- `14-scenari-test.md`: aggiunto capitolo dedicato agli scenari funzionali e
  shadow, con operazioni filesystem, raw log atteso, event log atteso e note
  sulle differenze semantiche.
- `02-architettura-generale.md`: aggiunto schema a tre livelli
  `inotify -> alfred_raw_event_t -> alfred_event_t`.
- `12-confronto-shadow-mode.md`: documentata la decisione iniziale per il bug
  `mkdir -p`: scan mirato sincrono, raw event sintetici verso il core e dedup
  solo se emergeranno duplicati.
- `13-semantica-eventi.md`: aggiunte sezioni su raw event sintetici, dedup e
  strategia scelta per il bug delle creazioni ricorsive veloci.
- `glossario.md`: aggiunte voci per evento raw sintetico e dedup.
- `15-todo-switch-core.md`: aggiunta roadmap didattica per rimuovere la
  semantica legacy da `events.c` e portare lo stream ufficiale sul core.
- `12-confronto-shadow-mode.md` e `14-scenari-test.md`: aggiornati i risultati
  post-fix per i raw event sintetici nelle creazioni ricorsive veloci.
- `15-todo-switch-core.md`: annotato che il core recupera i `DIR_CREATED`
  mancanti mentre il legacy resta incompleto durante shadow mode.
- `04-livello-applicazione.md`: documentato il dispatch core-before-legacy e la
  funzione temporanea `app_process_synthetic_dir_create`.
- `05-modulo-inotify.md`: documentata la callback di discovery del watch manager
  e il recupero tramite raw event sintetici.
- `tests/shadow/compare_shadow_output.py`: aggiunto lo scenario
  `recursive_create_slow_nested_dir` per osservare eventuali duplicati.
- `12-confronto-shadow-mode.md` e `14-scenari-test.md`: documentato lo scenario
  lento di controllo dedup.
- `12-confronto-shadow-mode.md` e `14-scenari-test.md`: registrato che
  `recursive_create_slow_nested_dir` non produce duplicati, quindi la dedup resta
  una protezione futura.
- `tests/shadow/compare_shadow_output.py`: aggiunti gli scenari `move_dir` e
  `modify_close_write_file`.
- `12-confronto-shadow-mode.md` e `14-scenari-test.md`: documentato che
  `move_dir` e' allineato tra legacy e core e produce `DIR_MOVED`.
- `05-modulo-inotify.md` e `13-semantica-eventi.md`: chiarito che il core
  supporta `FILE_MODIFIED` e `FILE_READY`, ma la maschera inotify attuale non
  abilita ancora `IN_MODIFY` e `IN_CLOSE_WRITE`.
- `15-todo-switch-core.md`: aggiornato lo stato dei punti su move directory,
  modify e close-write/file-ready.
- `15-todo-switch-core.md`: documentata la decisione sul formato dello stream
  eventi post-switch: formato ufficiale `plain`, formato `verbose`
  configurabile con `seq`, e formato shadow temporaneo con prefisso `core`.
- `modules/inotify/src/watch_manager.c`: abilitati `IN_MODIFY` e
  `IN_CLOSE_WRITE` nella maschera predefinita usata da `config_t.watch_mask`.
- `app/src/utils.c`: aggiornata la stampa dei raw log per mostrare `IN_MODIFY`
  e `IN_CLOSE_WRITE`.
- `05-modulo-inotify.md`, `12-confronto-shadow-mode.md`,
  `13-semantica-eventi.md`, `14-scenari-test.md` e `15-todo-switch-core.md`:
  aggiornato il punto 6 con i risultati osservati per `FILE_MODIFIED` e
  `FILE_READY`.
- `13-semantica-eventi.md`: documentata la decisione semantica per
  `FILE_CREATED`, `FILE_MODIFIED` e `FILE_READY`, inclusa la motivazione per
  considerarli fasi distinte e non duplicati.
- `14-scenari-test.md`: aggiunta una mappa a tre livelli per gli scenari shadow:
  eventi inotify, raw event Alfred e eventi semantici core target.
- `02-architettura-generale.md`: esteso lo schema `inotify ->
  alfred_raw_event_t -> alfred_event_t` con `IN_MODIFY`, `IN_CLOSE_WRITE`,
  `FILE_MODIFIED` e `FILE_READY`.
- `02-architettura-generale.md`: corrette le label lunghe del diagramma a tre
  livelli spezzando flag e path su piu' righe per evitare testo tagliato nei
  rettangoli Mermaid.
- `app/include/config.h`, `app/src/config.c`, `app/include/core_logger.h`,
  `app/src/core_logger.c`, `app/include/app.h` e `app/src/app.c`: aggiunta la
  modalita' `event_engine=shadow|core`; inizialmente `shadow` era il default,
  poi il default e' stato spostato a `core` in una fase successiva.
- `04-livello-applicazione.md`, `07-flusso-eventi.md`,
  `10-debugging-test-e-strumenti.md`, `12-confronto-shadow-mode.md` e
  `15-todo-switch-core.md`: documentata la differenza tra shadow mode e core
  mode, incluso il formato plain ufficiale del core.
- `app/include/config.h` e `app/src/config.c`: aggiornate le API di
  configurazione per restituire codici `error_t` invece di `-1` generico.
- `.gitignore` e `10-debugging-test-e-strumenti.md`: esclusi i log di runtime
  dai file versionati; i log tracciati in `tests/functional/` sono stati rimossi
  dall'indice.
- `tests/shadow/compare_shadow_output.py`: aggiunta l'opzione
  `--event-engine core` per eseguire gli stessi scenari con il core come stream
  ufficiale plain.
- `app/src/app.c` e `modules/inotify/src/events.c`: spostato l'aggiornamento
  ricorsivo dei watch per directory create fuori dal dispatcher semantico
  legacy, cosi' core mode continua a monitorare nuove directory.
- `15-todo-switch-core.md`: chiarito che la manutenzione dei watch in `app.c`
  e' una collocazione temporanea dello switch; la destinazione finale e' il
  backend inotify o una futura interfaccia backend.
- `04-livello-applicazione.md`, `07-flusso-eventi.md`,
  `10-debugging-test-e-strumenti.md`, `12-confronto-shadow-mode.md`,
  `14-scenari-test.md` e `15-todo-switch-core.md`: documentato il runner
  core-mode e il motivo per cui la manutenzione dei watch e' backend state, non
  semantica legacy.
- `tests/core/`: aggiunta suite parallela core con runner, libreria shell e
  primi tre scenari: create file, move+rename file e recursive create nested
  directory.
- `Makefile`: aggiunto target `test-core` per eseguire la suite core senza
  modificare `make test`.
- `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: documentata la suite core e chiarito che i test
  end-to-end del core fissano lo stream semantico, mentre raw inotify e
  `alfred_raw_event_t` sono piu' adatti a diagnostica o test unitari mirati.
- `tests/core/`: estesa la suite core con delete file, rename file, create
  directory e delete directory.
- `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: aggiunti gli scenari core appena coperti, con
  operazioni filesystem ed eventi semantici attesi.
- `tests/core/test_rename_dir.sh`: aggiunto lo scenario core-only per fissare
  `DIR_RENAMED` quando una directory cambia nome nello stesso contenitore.
- `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: documentato il test core `rename directory` e
  rimosso lo scenario dalla lista dei test ancora da aggiungere.
- `tests/core/test_move_file.sh`: aggiunto lo scenario core-only per fissare
  `FILE_MOVED` quando un file cambia directory mantenendo lo stesso nome.
- `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: documentato il test core `move file` e chiarita la
  distinzione fra `FILE_MOVED`, `FILE_RENAMED` e `FILE_RELOCATED`.
- `tests/core/test_move_dir.sh`: aggiunto lo scenario core-only per fissare
  `DIR_MOVED` quando una directory cambia contenitore mantenendo lo stesso nome.
- `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: documentato il test core `move directory` e chiarita
  la distinzione fra `DIR_MOVED`, `DIR_RENAMED` e `DIR_RELOCATED`.
- `tests/core/test_move_rename_dir.sh`: aggiunto lo scenario core-only per
  fissare `DIR_RELOCATED` quando una directory cambia sia contenitore sia nome.
- `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: documentato il test core `move and rename directory`
  e la differenza intenzionale rispetto al legacy, che produceva due eventi.
- `tests/core/test_lib.sh`: aggiunto `assert_count` per gli scenari in cui il
  numero di eventi e' parte del contratto semantico.
- `tests/core/test_modify_file.sh`: aggiunto lo scenario core-only per fissare
  la sequenza `FILE_MODIFIED` / `FILE_READY` su una seconda scrittura senza
  emettere un secondo `FILE_CREATED`.
- `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: documentato il test core `modify / close-write` e
  aggiornato il TODO degli scenari core rimanenti.
- `10-debugging-test-e-strumenti.md` e `15-todo-switch-core.md`: documentato
  che l'overflow viene rimandato a dopo lo switch completo al core perche' e'
  una policy di recovery/resync difficile da rendere stabile come test
  end-to-end iniziale.
- `15-todo-switch-core.md`: aggiunta la mappa operativa della logica legacy
  rimasta, distinguendo semantica ancora in `events.c`, stato backend ancora in
  `app.c`, adapter inotify gia' corretto, watch manager e campi temporanei in
  `app_t`.
- `modules/inotify/include/inotify_backend.h` e
  `modules/inotify/src/inotify_backend.c`: introdotto il primo confine backend
  inotify, che legge il fd, costruisce raw event, mantiene watch ricorsivi e
  genera raw sintetici per directory scoperte.
- `app/src/app.c` e `app/include/app.h`: spostata fuori dall'app la lettura
  diretta di `struct inotify_event` e rimossa la funzione temporanea
  `app_process_synthetic_dir_create()`.
- `04-livello-applicazione.md`, `05-modulo-inotify.md`,
  `07-flusso-eventi.md` e `15-todo-switch-core.md`: documentato il nuovo
  confine backend e il fatto che `inotify_fd` e `watchers` restano ancora in
  `app_t` come stato di transizione.
- `modules/inotify/include/inotify_backend.h`, `app/include/app.h`,
  `modules/inotify/src/inotify_backend.c`, `modules/inotify/src/watch_manager.c`
  e `modules/inotify/src/events.c`: incapsulati `fd` e watcher table in
  `inotify_backend_t`, mantenendo `moves` come stato temporaneo del legacy
  shadow.
- `04-livello-applicazione.md`, `05-modulo-inotify.md` e
  `15-todo-switch-core.md`: aggiornata la documentazione per chiarire che
  `inotify_fd` e `watchers` non sono piu' campi diretti di `app_t`.
- `app/src/app.c`: `move_cache` viene inizializzata solo in
  `event_engine=shadow`; in core mode la correlazione move resta solo nel core.
- `04-livello-applicazione.md` e `15-todo-switch-core.md`: documentato che
  `move_cache` e' ormai stato legacy escluso dal runtime core.
- `modules/inotify/src/events.c`, `modules/inotify/include/events.h`,
  `app/include/app.h` e `app/src/app.c`: spostata la proprieta' di `move_cache`
  dentro il dispatcher legacy, eliminando `app_t.moves`.
- `04-livello-applicazione.md` e `15-todo-switch-core.md`: documentato che la
  cache move e' confinata a `events.c` e non appartiene piu' al contesto
  applicativo.
- `modules/inotify/include/events.h`, `modules/inotify/src/events.c`,
  `app/src/app.c`, `02-architettura-generale.md`, `04-livello-applicazione.md`,
  `07-flusso-eventi.md` e `15-todo-switch-core.md`: rinominato l'entry point
  legacy da `app_dispatch_raw_event()` a `legacy_events_dispatch()` per chiarire
  che non e' piu' una responsabilita' dell'app.
- `modules/inotify/src/inotify_backend.c`, `modules/inotify/include/inotify_backend.h`,
  `app/src/app.c`, `04-livello-applicazione.md`, `07-flusso-eventi.md` e
  `15-todo-switch-core.md`: spostata la chiamata al dispatcher legacy dentro il
  backend inotify; la callback app riceve solo `alfred_raw_event_t` da inoltrare
  al core.
- `modules/inotify/src/inotify_backend.c`, `app/src/app.c` e
  `15-todo-switch-core.md`: spostato nel backend anche il ciclo di vita del
  dispatcher legacy, cosi' `app.c` non dipende piu' da `events.h`.
- `app/src/config.c`, `tests/lib/test_lib.sh`, `04-livello-applicazione.md`,
  `10-debugging-test-e-strumenti.md` e `15-todo-switch-core.md`: cambiato il
  default runtime a `event_engine=core`, lasciando i test funzionali legacy in
  shadow esplicito con `ALFRED_EVENT_ENGINE=shadow`.
- `app/src/config.c` e `04-livello-applicazione.md`: sostituita la conversione
  `atoi()` dei campi numerici di capacita' con parsing unsigned esplicito,
  mantenendo il valore di default se la stringa non e' valida.
- `app/src/config.c` e `04-livello-applicazione.md`: aggiunta spiegazione del
  rischio `atoi()` verso `size_t` e rifiuto esplicito dei valori negativi come
  `-1`.
- `00-regole-operative.md`: aggiunta memoria operativa della sessione con
  regole per commit, documentazione, verifiche, stato semantico corrente e
  commit chiave recenti.
