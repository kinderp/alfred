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
| Parziale | `glossario.md` |

## Aggiornamenti recenti

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
  modalita' `event_engine=shadow|core`, con `shadow` come default e override
  temporaneo `ALFRED_EVENT_ENGINE=core` per provare il core come stream
  ufficiale.
- `04-livello-applicazione.md`, `07-flusso-eventi.md`,
  `10-debugging-test-e-strumenti.md`, `12-confronto-shadow-mode.md` e
  `15-todo-switch-core.md`: documentata la differenza tra shadow mode e core
  mode, incluso il formato plain ufficiale del core.
- `app/include/config.h` e `app/src/config.c`: aggiornate le API di
  configurazione per restituire codici `error_t` invece di `-1` generico.
- `.gitignore` e `10-debugging-test-e-strumenti.md`: esclusi i log di runtime
  dai file versionati; i log tracciati in `tests/functional/` sono stati rimossi
  dall'indice.
