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
| Completo | `11-come-contribuire.md` |
| Completo | `12-confronto-shadow-mode.md` |
| Completo | `13-semantica-eventi.md` |
| Completo | `14-scenari-test.md` |
| Parziale | `15-todo-switch-core.md` |
| Parziale | `16-mappa-codice-e-strutture.md` |
| Parziale | `17-roadmap-documentazione-avanzata.md` |
| Parziale | `18-modello-licenze.md` |
| Parziale | `19-roadmap-cli-e-man-page.md` |
| Completo | `20-matrice-eventi-inotify.md` |
| Parziale | `21-roadmap-scanner-resync.md` |
| Completo | `../code-browser/README.md` |
| Completo | `../sourcebot-browser/README.md` |
| Parziale | `../kythe-browser/README.md` |
| Parziale | `glossario.md` |

## Aggiornamenti recenti

- `modules/inotify/src/watch_manager.c`,
  `modules/inotify/include/watch_manager.h`, `05-modulo-inotify.md`,
  `15-todo-switch-core.md`, `21-roadmap-scanner-resync.md` e
  `docs/commenting-progress.md`: completato il cleanup della Fase 5 dello
  scanner. Rimossi `recursive_walk()`,
  `watch_manager_add_recursive_with_discovery()` e il typedef callback di
  discovery. Lo startup e la discovery runtime usano ora `fs_scan_tree()` come
  unico attraversatore, mentre il watch manager resta responsabile solo dello
  stato dei watch.
- `modules/inotify/src/inotify_backend.c`,
  `modules/inotify/src/watch_manager.c`,
  `modules/inotify/include/watch_manager.h`, `04-livello-applicazione.md`,
  `05-modulo-inotify.md`, `15-todo-switch-core.md`,
  `16-mappa-codice-e-strutture.md`, `21-roadmap-scanner-resync.md` e
  `docs/commenting-progress.md`: migrato allo scanner anche il percorso runtime
  `IN_CREATE | IN_ISDIR`. Il backend aggiunge il watch sulla root reale, esegue
  `fs_scan_tree()` con `emit_root = 0`, aggiunge watch alle directory annidate
  e mantiene nel backend l'emissione dei raw create sintetici.
- `modules/inotify/src/watch_manager.c`, `05-modulo-inotify.md`,
  `16-mappa-codice-e-strutture.md`, `21-roadmap-scanner-resync.md` e
  `docs/commenting-progress.md`: implementato il primo refactor di integrazione
  scanner/watch manager. Il percorso startup `watch_manager_add_recursive()`
  ora usa `fs_scan_tree()` in modalita' directory-only e chiama
  `watch_manager_add()` per ogni directory trovata. Il percorso runtime
  storico e' documentato solo come passaggio della migrazione.
- `21-roadmap-scanner-resync.md`: completato l'audit iniziale della Fase 5,
  integrazione scanner/watch manager. La roadmap documenta il flusso corrente
  storico, i percorsi startup e runtime `IN_CREATE | IN_ISDIR`, i
  limiti della ricorsione attuale, la divisione target tra scanner, watch
  manager, backend e core, la regola `emit_root = 0` per evitare doppi raw
  create sintetici e una strategia di migrazione a passi piccoli.
- `tests/scanner/test_fs_scanner_dirs.c`,
  `tests/scanner/test_fs_scanner_dirs.sh`,
  `21-roadmap-scanner-resync.md`, `10-debugging-test-e-strumenti.md` e
  `14-scenari-test.md`: completato il contratto iniziale dei tipi pubblici
  dello scanner con `include_other = 1`. Il test usa una FIFO creata con
  `mkfifo`, verificata come `FS_SCAN_OTHER`, per evitare dipendenze da privilegi
  o setup piu' fragile richiesti da device file e socket.
- `tests/scanner/test_fs_scanner_dirs.c`,
  `21-roadmap-scanner-resync.md`, `10-debugging-test-e-strumenti.md` e
  `14-scenari-test.md`: avviata la fase symlink dello scanner. Il test ora
  copre `include_symlinks = 1`, che emette il link simbolico come
  `FS_SCAN_SYMLINK` senza seguirne il target. La documentazione rimanda
  `follow_symlinks = 1` e registra le policy anti-cicli comuni: non seguire
  symlink, visited set su `(st_dev, st_ino)`, `max_depth`, restare sotto la
  root, restare sullo stesso device e `max_entries`.
- `app/src/fs_scanner.c`, `tests/scanner/test_fs_scanner_dirs.c`,
  `tests/scanner/test_fs_scanner_dirs.sh`, `21-roadmap-scanner-resync.md`,
  `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `docs/commenting-progress.md`: implementato il primo passo della policy sugli
  errori parziali dello scanner. La root resta un errore duro, mentre una
  directory figlia rimossa, trasformata o non accessibile durante la discesa
  viene saltata e lo scan continua. Il test simula una race rimuovendo una
  directory `volatile` dalla callback prima della ricorsione.
- `21-roadmap-scanner-resync.md`: aggiunta una roadmap completa delle fasi
  scanner. Il documento ora separa contratto base, errori parziali, symlink,
  file/tipi speciali, integrazione con watch manager, resync sugli eventi
  critici, futura CLI di indicizzazione e performance. Aggiunta anche una
  spiegazione didattica di `readdir()`, `fstatat()`, `openat()`,
  `fdopendir()`, `path_join()` ed `ERR_IO` per preparare la discussione sulla
  policy degli errori parziali.
- `tests/scanner/test_fs_scanner_dirs.c`,
  `tests/scanner/test_fs_scanner_dirs.sh`,
  `21-roadmap-scanner-resync.md`, `10-debugging-test-e-strumenti.md` e
  `14-scenari-test.md`: estesa la copertura scanner alle opzioni pubbliche
  `emit_root`, `max_depth`, `max_entries` e `include_files`. La roadmap ora
  include anche una guida pratica all'uso dell'API `fs_scan_tree()` dal codice.
- `21-roadmap-scanner-resync.md`: ampliata la spiegazione tecnica dello scanner
  filesystem. La nuova sezione chiarisce perche' sono state scelte primitive
  come `openat()`, `fdopendir()`, `readdir()` e `fstatat()`, quali vantaggi
  danno su controllo, prestazioni, symlink e race, e perche' l'eventuale uso di
  `d_type` resta una possibile ottimizzazione futura invece che il contratto
  corrente.
- `app/include/fs_scanner.h`, `app/src/fs_scanner.c` e
  `tests/scanner/test_fs_scanner_dirs.c`: rafforzata la documentazione interna
  del primo scanner filesystem. I commenti spiegano confini di responsabilita',
  durata dei puntatori passati alla callback, opzioni di attraversamento,
  stop anticipato, limiti di profondita'/numero entry e uso di
  `openat()`/`fstatat()`. I test restano commentati in modo leggero, coerente
  con il livello della suite esistente.
- `21-roadmap-scanner-resync.md` e `README.md`: aperta la progettazione dello
  scanner filesystem e della futura policy di resync. Il documento registra lo
  stato corrente di `IN_DELETE_SELF` e `IN_MOVE_SELF`, chiarisce che non esiste
  ancora mapping semantico runtime per questi eventi, valuta `nftw()` e propone
  uno scanner custom riusabile anche per indicizzazione futura.
- `app/include/fs_scanner.h`, `app/src/fs_scanner.c`, `tests/scanner/`,
  `Makefile`, `README.md`, `09-makefile-e-build-system.md`,
  `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `21-roadmap-scanner-resync.md`: implementato il primo step dello scanner
  filesystem. Il target `make test-scanner` verifica lo scan directory-only,
  callback con `userdata` e symlink non seguiti.
- `20-matrice-eventi-inotify.md`, `README.md`, `05-modulo-inotify.md` e
  `13-semantica-eventi.md`: aggiunta una matrice completa degli eventi e flag
  `inotify(7)`. La tabella distingue eventi richiedibili, bit restituiti dal
  kernel, macro di comodita', flag di configurazione del watch, stato del raw
  log backend, mapping `ALFRED_RAW_*`, semantica core e decisioni rimandate.
  La documentazione evidenzia anche il prossimo refactor: separare meglio nel
  codice la subscription mask dai bit riconosciuti in output.
- `20-matrice-eventi-inotify.md` e `15-todo-switch-core.md`: aggiunta una
  spiegazione didattica della differenza tra subscription mask e bit
  riconosciuti in output. Gli esempi `IN_CREATE | IN_ISDIR` e `IN_IGNORED`
  chiariscono perche' alcuni bit servono al backend o al raw log ma non devono
  diventare automaticamente eventi semantici del core.
- `README.md`, `02-architettura-generale.md`, `04-livello-applicazione.md`,
  `07-flusso-eventi.md` e `20-matrice-eventi-inotify.md`: riallineati i link
  principali della sezione Documentation e aggiornate le parti Mermaid/testuali
  che descrivono il runtime corrente core-only. La matrice inotify ora include
  anche un diagramma dedicato alla separazione tra subscription mask e bit
  riconosciuti in output.
- `04-livello-applicazione.md`, `08-guida-c-usato-nel-progetto.md`,
  `10-debugging-test-e-strumenti.md`, `13-semantica-eventi.md`,
  `15-todo-switch-core.md`, `docs/sourcebot-browser/README.md`,
  `docs/commenting-style.md` e `docs/commenting-progress.md`: audit di
  riallineamento al codice corrente. Rimossi o chiariti esempi che trattavano
  `shadow`, `event_engine_mode`, `app->inotify_fd`, `events.c` e `move_cache`
  come elementi attivi invece che storici.
- `20-matrice-eventi-inotify.md`, `14-scenari-test.md` e
  `tests/backend/test_self_events_root_watch.sh`: aggiunto il primo test
  osservativo sugli eventi `IN_DELETE_SELF` / `IN_MOVE_SELF` del path osservato
  direttamente. La documentazione chiarisce che questi eventi riguardano il
  watch stesso. Il caso delete puo' comunque produrre delete reali dei figli se
  il kernel li emette; il caso move evidenzia invece il rischio di path stale
  nella tabella `wd -> path`.
- `19-roadmap-cli-e-man-page.md` e `README.md`: aggiunta una roadmap per il
  futuro parser CLI professionale e per la futura pagina man. Il documento
  registra `-c` / `--config`, `--print-config`, `--check-config`, `--help`,
  `--version`, la precedenza tra default, ambiente e CLI, e le fonti di
  riferimento GNU/POSIX/man-pages.
- `README.md`: aggiunto nel quick start un esempio di avvio con
  `ALFRED_CONFIG=./alfred.conf ./alfred /path/to/watch` e rimandi alla
  documentazione italiana su livello applicazione e strumenti di debug.
- `10-debugging-test-e-strumenti.md`: aggiunta una sezione pratica su
  `ALFRED_CONFIG`, con esempio di file temporaneo, comando di avvio, ordine di
  precedenza rispetto ai default e a `ALFRED_EVENT_ENGINE`, e richiamo a
  `ALFRED_KEEP_TEST_LOGS=1` per ispezionare i log.
- `modules/inotify/include/inotify_config.h`,
  `modules/inotify/src/inotify_config.c`, `app/src/config.c`, `app/src/app.c`,
  `tests/backend/test_watch_mask_disable_attrib.sh`,
  `tests/backend/test_watch_mask_invalid_token.sh`,
  `04-livello-applicazione.md`, `05-modulo-inotify.md`,
  `14-scenari-test.md`, `16-mappa-codice-e-strutture.md`: aggiunto il parser
  testuale di `inotify_watch_mask`. La configurazione supporta `default`, liste
  esplicite di flag `IN_*` e modificatori `+FLAG` / `-FLAG`; i token
  sconosciuti o non ancora supportati da Alfred fanno fallire `config_load()`
  con `ERR_CONFIG`. L'avvio puo' caricare un file tramite `ALFRED_CONFIG`.
- `app/include/config.h`, `app/src/config.c`,
  `modules/inotify/include/inotify_config.h`,
  `modules/inotify/src/inotify_config.c`,
  `modules/inotify/include/inotify_backend.h`, `app/src/app.c`, `Makefile`,
  `04-livello-applicazione.md`, `05-modulo-inotify.md` e
  `16-mappa-codice-e-strutture.md`: introdotta `inotify_config_t` come
  sottostruttura specifica del backend inotify. `config_t` resta il contenitore
  applicativo generale, mentre il backend riceve ora solo la configurazione
  inotify tramite `inotify_backend_context_t`. Le chiavi storiche
  `recursive` e `watcher_capacity` restano accettate; i nuovi alias
  `inotify_recursive` e `inotify_watcher_capacity` preparano il futuro parser
  di `inotify_watch_mask`.
- `05-modulo-inotify.md`, `13-semantica-eventi.md` e `14-scenari-test.md`:
  dettagliati i casi che possono generare `IN_ATTRIB` secondo `inotify(7)`:
  permessi, timestamp, attributi estesi, numero di hard link, proprietario e
  gruppo. La documentazione chiarisce anche perche' il test automatico usa solo
  `chmod` come caso rappresentativo e rimanda la copertura completa alla futura
  scelta semantica sugli eventi metadata/attrib.
- `modules/inotify/src/watch_manager.c`,
  `modules/inotify/src/inotify_backend.c`,
  `tests/backend/test_attrib_raw_log.sh`, `05-modulo-inotify.md`,
  `13-semantica-eventi.md` e `14-scenari-test.md`: aggiunto `IN_ATTRIB` alla
  maschera inotify predefinita e al raw log formatter. Il backend ora osserva i
  cambiamenti attributo come fatti raw (`ALFRED_RAW_ATTRIB`), ma il core non
  emette ancora un evento semantico metadata/attrib. Il test backend controlla
  anche che `events.log` non riceva nuove righe dopo `chmod`.
- `.github/pull_request_template.md` e `docs/it/11-come-contribuire.md`:
  aggiunto un template GitHub per le pull request. La guida contributori spiega
  come compilare summary, scope, verification, documentazione, compatibilita',
  semantica e note per reviewer.
- `docs/it/18-modello-licenze.md`, `docs/it/README.md` e `README.md`:
  documentato il modello licenze provvisorio. Il core e il backend base sono
  pensati come open source, mentre moduli avanzati futuri come `fanotify` o
  `ebpf` potranno avere licenze commerciali o source-available separate. La
  licenza definitiva non e' ancora scelta.
- `README.md`: sostituito il testo iniziale da traccia didattica con un README
  pubblico in inglese. Il nuovo README presenta Alfred come progetto C/Linux,
  descrive stato, quick start, log, test, CI, contributi, roadmap e rimanda alla
  documentazione italiana finche' non esistera' una documentazione inglese
  completa.
- `.github/workflows/ci.yml` e `docs/it/11-come-contribuire.md`: aggiunta la
  prima GitHub Action del progetto. La CI esegue `make`, `make test` e
  `make test-backend-diagnostics` su pull request verso `main` e push su
  `main`. La guida contributori ora spiega il flusso fork/upstream/branch/PR,
  la sincronizzazione del fork, i test locali, il ruolo della review, come
  leggere output/errori delle run GitHub Actions e come scaricare l'artifact
  `alfred-test-logs` prodotto quando la CI fallisce.
- `docs/it/10-debugging-test-e-strumenti.md`: documentata la modalita'
  `ALFRED_KEEP_TEST_LOGS=1` per conservare `raw.log`, `events.log` ed
  `errors.log` durante il debugging locale o nelle run CI che caricano artifact
  su fallimento.
- `docs/it/15-todo-switch-core.md`: chiusa formalmente la fase post-switch core
  per lo stato corrente. Restano separati solo l'eliminazione futura degli
  archivi storici `tests/functional/` e `tests/shadow/`, la progettazione
  overflow/resync e nuove passate legate a bug o refactor futuri.
- `tests/functional/README.md`, `tests/shadow/README.md` e
  `docs/it/15-todo-switch-core.md`: chiarita la decisione sui file storici di
  test. Le directory restano temporaneamente come archivio didattico, ma la
  direzione finale e' eliminarle completamente dopo aver migrato negli `.md` le
  parti ancora utili.
- `docs/it/14-scenari-test.md` e `docs/it/15-todo-switch-core.md`: completata
  la revisione finale scenari/test per lo stato post-switch. La suite core
  copre il contratto utente corrente, la suite backend copre la diagnostica
  watch utile e non viene aggiunto per ora un test raw separato per i raw
  sintetici.
- `docs/it/15-todo-switch-core.md`: registrato l'audit delle responsabilita'
  residue. Non emerge un micro-refactor tecnico urgente: `path_join()` resta
  utility generica, il backend context esplicita correttamente config/logger
  presi in prestito e `userdata` resta opaco per il backend.
- `docs/it/15-todo-switch-core.md`: aggiornata la roadmap post-switch con lo
  stato reale dopo le ultime passate documentali. Documentazione pesante e
  allineamento mappe sono ora "parziale avanzato"; il prossimo passo tecnico e'
  un audit delle responsabilita' residue prima di altri refactor.
- `docs/it/16-mappa-codice-e-strutture.md`,
  `docs/it/10-debugging-test-e-strumenti.md` e
  `docs/it/15-todo-switch-core.md`: aggiornate mappe ed esempi didattici per
  distinguere lo stato corrente basato su `inotify_backend_context_t` dagli
  esempi storici con `app_t`, `event_engine_mode` e bridge shadow.
- `modules/inotify/src/watch_manager.c`,
  `modules/inotify/include/watch_manager.h` e `docs/commenting-progress.md`:
  rafforzati i commenti sul ruolo del watch manager. `WATCH_ADDED` e
  `WATCH_REMOVED` restano diagnostica backend; la discovery ricorsiva segnala
  fatti al backend, ma non emette direttamente raw sintetici o eventi
  semantici.
- `modules/inotify/src/inotify_backend.c` e `docs/commenting-progress.md`:
  rafforzati i commenti in inglese sul confine backend/core. Il backend
  produce fatti raw, ripara lo stato dei watch ricorsivi e puo' generare raw
  sintetici; la semantica finale e l'eventuale dedup restano responsabilita'
  del core.
- `app/src/utils.c`, `app/include/utils.h`,
  `modules/inotify/src/inotify_backend.c` e documentazione didattica: spostata
  la formattazione testuale delle mask inotify fuori dalle utility generiche e
  dentro il backend come helper locale. `utils.c` resta per funzioni generiche,
  mentre il backend conserva i dettagli Linux `IN_*`.
- `modules/inotify/src/inotify_backend.c` e
  `docs/it/15-todo-switch-core.md`: puliti riferimenti ambigui allo shadow
  legacy dopo la rimozione completa del percorso. I vecchi micro-refactor
  restano documentati come storia della migrazione, ma non sono piu' descritti
  come stato operativo corrente.
- `app/include/config.h`, `app/src/config.c`, `app/src/app.c` e
  documentazione didattica: rimosso `event_engine_mode_t` e il campo
  `config_t.event_engine_mode`. Il runtime non conserva piu' una scelta engine
  perche' il core e' l'unico percorso supportato; `config_set_event_engine()`
  resta solo come validazione per accettare `core` e rifiutare valori storici
  come `shadow`.
- `app/include/config.h`, `app/src/config.c`, `app/src/app.c`,
  `tests/core/test_invalid_event_engine_shadow.sh` e documentazione didattica:
  rimosso `EVENT_ENGINE_SHADOW`. `ALFRED_EVENT_ENGINE=shadow` e' ora un valore
  invalido generico; l'unico valore accettato e' `core`.
- `tests/functional/README.md`, `tests/shadow/README.md`,
  `tests/lib/test_lib.sh`, `tests/shadow/compare_shadow_output.py` e
  documentazione didattica: marcate le vecchie suite functional/shadow come
  archivio storico. L'uso accidentale del confronto shadow ora fallisce con un
  messaggio esplicito; le verifiche correnti restano `make test` e
  `make test-backend-diagnostics`.
- `app/include/config.h`, `app/src/config.c`, `app/include/core_logger.h`,
  `app/src/core_logger.c`, `app/src/app.c`, `modules/inotify/` e
  documentazione didattica: rimossi fisicamente il dispatcher legacy
  `events.c`, gli header collegati, `move_cache.c`, `move_cache.h` e il campo
  `move_cache_size`. Il core logger ora produce solo lo stream plain ufficiale.
- `Makefile`, `00-regole-operative.md`, `01-panoramica-progetto.md`,
  `04-livello-applicazione.md`, `05-modulo-inotify.md`,
  `09-makefile-e-build-system.md`, `10-debugging-test-e-strumenti.md`,
  `14-scenari-test.md`, `15-todo-switch-core.md` e
  `16-mappa-codice-e-strutture.md`: rimossa la variante build
  legacy-shadow dal Makefile. `ENABLE_LEGACY_SHADOW` e
  `test-legacy-shadow` non sono piu' comandi supportati; `.PHONY` e' ora
  spiegato nel capitolo sul Makefile.
- `app/src/app.c`, `app/include/app.h`, `app/include/config.h`,
  `app/include/core_logger.h`, `app/src/config.c`,
  `tests/core/test_shadow_mode_removed.sh`, `14-scenari-test.md`,
  `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: rimosso il
  lifecycle legacy da app.c e trasformato `ALFRED_EVENT_ENGINE=shadow` in un
  errore runtime esplicito.
- `modules/inotify/include/inotify_backend.h`,
  `modules/inotify/src/inotify_backend.c`, `app/src/app.c`,
  `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: rimosso il
  bridge shadow dal context backend e spento il dispatch live
  `legacy_events_dispatch()` dal poll path. `test-legacy-shadow` diventa
  storico, mentre core e backend diagnostics restano le verifiche ordinarie.
- `tests/backend/test_recursive_slow_watch_tree.sh`,
  `10-debugging-test-e-strumenti.md`, `14-scenari-test.md` e
  `15-todo-switch-core.md`: migrata anche la diagnostica dei watch ricorsivi
  lenti, salvando lo scenario utile prima della futura rimozione della suite
  legacy/shadow.
- `Makefile`, `tests/backend/`, `modules/inotify/src/inotify_backend.c`,
  `09-makefile-e-build-system.md`, `10-debugging-test-e-strumenti.md`,
  `14-scenari-test.md` e `15-todo-switch-core.md`: aggiunta la prima suite
  backend diagnostics per `WATCH_ADDED` e `WATCH_REMOVED`; il backend core ora
  pulisce i watch su `IN_IGNORED`.
- `14-scenari-test.md` e `15-todo-switch-core.md`: pianificata la nuova
  categoria `tests/backend/` per preservare diagnostica utile come
  `WATCH_ADDED`, `WATCH_REMOVED` e watch ricorsivi lenti prima di rimuovere
  `test-legacy-shadow`.
- `14-scenari-test.md` e `15-todo-switch-core.md`: aggiunto l'audit
  pre-rimozione dello shadow legacy. La documentazione chiarisce che lo switch
  deve essere totale, che lo shadow non e' una feature futura, e che la suite
  core copre gia' il contratto semantico da preservare.
- `modules/inotify/include/inotify_backend.h`, `modules/inotify/src/inotify_backend.c`
  e `app/src/app.c`: spostato il bridge shadow opzionale dentro
  `inotify_backend_context_t`. `inotify_backend_poll()` non riceve piu' il
  bridge come parametro pubblico: usa solo context, callback raw/core e
  `userdata`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  diciassettesimo micro-refactor, chiarendo che shadow resta nel context come
  dipendenza diagnostica opzionale.
- `Makefile`: cambiato `make test` in alias del percorso core ufficiale.
  `make test-core` resta il nome esplicito della stessa suite. In quel momento
  `make test-legacy-shadow` restava disponibile per diagnostica legacy/shadow;
  oggi e' stato rimosso dal Makefile.
- `09-makefile-e-build-system.md`, `10-debugging-test-e-strumenti.md`,
  `14-scenari-test.md` e `15-todo-switch-core.md`: aggiornata la
  documentazione per indicare che `make test` ora verifica il core e non piu'
  i funzionali storici legacy/shadow.
- `Makefile`, `14-scenari-test.md` e `15-todo-switch-core.md`: aggiunta la
  roadmap per il futuro cambio di `make test` da alias legacy/shadow ad alias
  del percorso core. Questa roadmap e' stata poi completata: oggi `make test`
  punta al core.
- `14-scenari-test.md`: aggiunta tabella di decisione sugli scenari legacy,
  distinguendo contratto core, diagnostica backend e memoria storica shadow.
- `Makefile`: aggiunto il target esplicito `test-legacy-shadow`.
  In quel passo `make test` restava temporaneamente sui funzionali storici; oggi
  invece punta al core.
- `09-makefile-e-build-system.md`, `10-debugging-test-e-strumenti.md`,
  `14-scenari-test.md` e `15-todo-switch-core.md`: aggiornata la
  documentazione dei target di test per distinguere core ufficiale e
  compatibilita' legacy/shadow.
- `tests/core/test_shadow_requires_legacy_build.sh`: aggiunto test core-only
  che verifica il fallimento esplicito di `ALFRED_EVENT_ENGINE=shadow` quando
  Alfred e' compilato senza `ENABLE_LEGACY_SHADOW=1`.
- `14-scenari-test.md` e `15-todo-switch-core.md`: documentato il nuovo
  scenario runtime/configurazione, spiegando perche' vive nella suite core e
  quale contratto protegge.
- `app/src/app.c` e `modules/inotify/src/inotify_backend.c`: reso opzionale il
  bridge shadow nel poll path. In `event_engine=core`, `app_run()` passa
  `NULL`; in `event_engine=shadow`, passa il bridge opaco verso il dispatcher
  legacy.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  sedicesimo micro-refactor, chiarendo che il percorso core normale non usa
  alcun bridge legacy.
- `00-regole-operative.md`: rimossa la regola rigida sulle righe entro 75
  caratteri. Le regole commit ora seguono il gist `Git Commit Best Practices`:
  commit monoscopo, subject imperativo, body esplicativo e wrapping circa 72
  caratteri quando pratico. Restano obbligatori inglese e lista file senza
  righe vuote tra gli item.
- `app/src/app.c` e `modules/inotify/src/inotify_backend.c`: spostato il
  lifecycle del legacy shadow fuori dal backend. `app.c` inizializza e spegne
  `events.c` tramite `app_init_legacy_shadow()` e
  `app_shutdown_legacy_shadow()`, mentre il backend resta responsabile solo di
  fd inotify, watcher table, raw event e callback shadow opaca.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  quindicesimo micro-refactor, chiarendo che shadow mode e' una strategia di
  integrazione orchestrata dall'applicazione e non stato del backend.
- `modules/inotify/include/inotify_backend.h`, `modules/inotify/src/inotify_backend.c`
  e `app/src/app.c`: reso opaco `legacy_shadow_bridge_t`. Il bridge non
  contiene piu' `struct app *`; contiene una callback shadow e un `userdata`.
  Il cast a `app_t` resta in `app_legacy_shadow_dispatch()`, dentro `app.c`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  quattordicesimo micro-refactor, chiarendo la differenza tra quarantena con
  campo `app` e bridge realmente opaco.
- `modules/inotify/include/inotify_backend.h`, `modules/inotify/src/inotify_backend.c`
  e `app/src/app.c`: introdotto `legacy_shadow_bridge_t` e cambiata la firma di
  `inotify_backend_poll()`. Il poll riceve ora `inotify_backend_context_t *` e
  un bridge legacy separato. Questo passo e' stato poi completato rendendo il
  bridge opaco, senza campo `app`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  tredicesimo micro-refactor, cioe' la quarantena esplicita della dipendenza
  shadow da `app_t`.
- `modules/inotify/include/inotify_backend.h`, `modules/inotify/src/inotify_backend.c`
  e `app/src/app.c`: cambiate le firme pubbliche pulite del backend. Init,
  startup watch e shutdown ricevono ora `inotify_backend_context_t *`; `app.c`
  costruisce il context con `app_build_inotify_backend_context()`. Questo passo
  e' stato poi completato dalla quarantena di poll tramite
  `legacy_shadow_bridge_t`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  dodicesimo micro-refactor, primo cambio reale del confine pubblico tra app e
  backend.
- `modules/inotify/src/inotify_backend.c`: estratto `backend_poll()` come forma
  interna context-shaped del polling. Questo passo e' stato poi completato
  prima dalla firma pubblica `inotify_backend_poll(ctx, legacy, on_event,
  userdata)` e poi dalla variante piu' pulita in cui il bridge diagnostico vive
  in `ctx->legacy_shadow`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato
  l'undicesimo micro-refactor, chiarendo che il parametro `app_t` nel poll path
  ha ormai significato solo legacy/shadow.
- `modules/inotify/src/inotify_backend.c`: estratto `backend_init()` come forma
  interna context-shaped dell'inizializzazione backend. Questo passo e' stato
  poi completato dal cambio di firma pubblica verso
  `inotify_backend_context_t *`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  decimo micro-refactor, con attenzione agli error path di init e al fatto che
  il refactor non cambia ordine di acquisizione e cleanup delle risorse.
- `modules/inotify/src/inotify_backend.c`: estratto `backend_shutdown()` come
  forma interna context-shaped dello shutdown backend. Questo passo e' stato
  poi completato dal cambio di firma pubblica verso
  `inotify_backend_context_t *`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  nono micro-refactor, spiegando che shutdown non richiede un bridge `app_t`
  per il legacy perche' `legacy_events_shutdown()` non riceve l'app completa.
- `modules/inotify/src/inotify_backend.c`: estratto
  `backend_add_startup_watch()` come forma interna context-shaped della API di
  startup watch. Questo passo e' stato poi completato dal cambio di firma
  pubblica verso `inotify_backend_context_t *`.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato
  l'ottavo micro-refactor, spiegando la tecnica di migrazione C in cui la firma
  futura viene introdotta prima come helper interno e solo dopo potra'
  sostituire la firma pubblica.
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
- `modules/inotify/src/inotify_backend.c`: `inotify_backend_shutdown()` e' stato
  prima ristretto internamente a `inotify_backend_context_t`; dopo il
  dodicesimo micro-refactor la firma pubblica pulita riceve direttamente il
  context. Il cleanup legacy shadow resta esplicito e condizionato dalla build.
- `15-todo-switch-core.md` e `16-mappa-codice-e-strutture.md`: documentato il
  quinto micro-refactor del backend context, chiarendo la simmetria tra init e
  shutdown e il fatto che il cleanup legacy non appartiene al backend core
  finale.
- `modules/inotify/src/inotify_backend.c`: `inotify_backend_init()` e' stato
  prima ristretto internamente a `inotify_backend_context_t` per watcher table,
  file descriptor, configurazione e diagnostica; dopo il dodicesimo
  micro-refactor la firma pubblica pulita riceve direttamente il context. Il
  ponte legacy/shadow resta esplicito tramite la configurazione.
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
  in quel momento `make test` restava legacy-shadow e `make test-core` era il
  percorso ufficiale. Decisione superata: oggi `make test` punta al core e
  `make test-legacy-shadow` e' stato rimosso dal Makefile.
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
  errore esplicito se il binario non contiene il legacy. Decisione superata:
  oggi la variante legacy-shadow e' stata rimossa dal Makefile.
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
  core e' lo stream semantico ufficiale di default. Decisione superata: oggi
  `events.c` e `move_cache.c` sono stati rimossi dal codice corrente.
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
  `IN_CLOSE_WRITE` nella maschera predefinita. Nello stato corrente quella
  maschera passa da `config_t.inotify.watch_mask`.
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
  dispatcher legacy. Questo stato e' stato poi superato: il lifecycle shadow e'
  tornato in `app.c` tramite helper dedicati, mentre il backend conserva solo il
  bridge opaco di poll.
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
