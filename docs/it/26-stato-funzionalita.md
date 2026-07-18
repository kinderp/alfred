# Stato funzionalita' supportate

Questo capitolo e' una fotografia dello stato corrente di Alfred. Serve a
rispondere velocemente a queste domande:

- cosa supporta oggi il backend `inotify`?
- cosa arriva al raw log backend?
- cosa diventa `ALFRED_RAW_*`?
- cosa diventa evento semantico del core?
- quali test coprono il comportamento?
- cosa e' solo diagnostica o roadmap futura?

Questo documento non sostituisce i capitoli di dettaglio. Li riassume. Per i
ragionamenti completi leggere:

- [Matrice eventi inotify](20-matrice-eventi-inotify.md)
- [Semantica degli eventi](13-semantica-eventi.md)
- [Scenari di test](14-scenari-test.md)
- [Contratto dei log](22-contratto-log.md)
- [Roadmap scanner e resync](21-roadmap-scanner-resync.md)
- [Use case, posizionamento e integrazioni](36-use-cases-posizionamento-integrazioni.md)

La differenza tra questo capitolo e il documento sui use case e' intenzionale:
qui si parte dalla funzionalita' implementata o pianificata; nel documento sui
use case si parte invece dal problema dell'utente. Quando una funzionalita'
importante viene aggiunta, va controllato anche se genera un nuovo caso d'uso o
rafforza un caso d'uso gia' documentato.

## Legenda

| Stato | Significato |
| --- | --- |
| Supportato | Funzionalita' implementata e coperta da test ordinari o mirati |
| Diagnostico | Visibile nei log o nello stato backend, ma non evento semantico core |
| Opt-in | Disponibile solo se abilitata esplicitamente in configurazione |
| Parziale | Implementata una parte utile, ma la policy finale e' ancora incompleta |
| Rimandato | Decisione o implementazione futura |

## Vista rapida per livelli

| Livello | Supportato oggi | Rimandato |
| --- | --- | --- |
| CLI operativa | Path posizionali `PATH...`; `-- PATH...` per path che iniziano con `-`; configurazione tramite `-c FILE`, `--config FILE`, `--config=FILE` o `ALFRED_CONFIG`; comandi informativi `-h`/`--help` e `-V`/`--version` che terminano prima di configurazione/logger/backend/core/output e non creano log runtime; `--check-config` valida default, config esplicita o `ALFRED_CONFIG`, e `ALFRED_EVENT_ENGINE` senza avviare logger/backend/core/output/watch; parser error side-effect-free per forme ambigue | `--print-config`; subcommand framework; validazione anticipata dei path fuori da `--check-config` |
| Backend inotify raw log | `IN_CREATE`, `IN_DELETE`, `IN_MODIFY`, `IN_ATTRIB`, `IN_CLOSE_WRITE`, `IN_MOVED_FROM`, `IN_MOVED_TO`, `IN_ISDIR`, `IN_DELETE_SELF`, `IN_MOVE_SELF`, `IN_IGNORED`, `IN_UNMOUNT`, `IN_Q_OVERFLOW`; audit opt-in: `IN_OPEN`, `IN_ACCESS`, `IN_CLOSE_NOWRITE`; resta log backend/debug e non `output.jsonl` v0 | audit con contesto processo, policy guardrail multi-backend, recovery completa post-unmount, eventuale `backend_observed` strutturato |
| Alfred raw | `ALFRED_RAW_CREATE`, `ALFRED_RAW_DELETE`, `ALFRED_RAW_MODIFY`, `ALFRED_RAW_ATTRIB`, `ALFRED_RAW_CLOSE_WRITE`, `ALFRED_RAW_MOVED_FROM`, `ALFRED_RAW_MOVED_TO`, `ALFRED_RAW_OVERFLOW`, `ALFRED_RAW_ISDIR`; adapter verso record `RAW_*`; formatter testuale da record; raw principali runtime via record + sink + text sink; `output.jsonl` copre create/delete/attrib/modify/close-write/move-pair runtime e `RAW_OVERFLOW` sintetico | raw audit dedicati, raw self-event dedicati, eventuali raw futuri |
| Core semantico | `FILE_CREATED`, `DIR_CREATED`, `FILE_DELETED`, `DIR_DELETED`, `FILE_MODIFIED`, `FILE_READY`, move/rename/relocated file e directory, `OVERFLOW`; adapter `alfred_event_t -> alfred_record_t`; output `core_logger` via record formatter | metadata changed, file read/open audit, close-nowrite, delete/move self semantici, risk/session/policy |
| Backend state | watch ricorsivi, `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, resync locale, lost-scope queue, recovery delayed sincrona, retry/backoff, benchmark manuale lost-scope; builder iniziale verso record diagnostici `WATCH_*`; `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, `WATCH_RESYNC_*` e `WATCH_LOST_*` runtime via record + sink + text sink | worker thread, configurazione pubblica recovery, performance suite stabile, backpressure reale |

## Mappa funzionalita' e superficie CLI

Questa tabella collega le funzionalita' gia' implementate o pianificate alla
superficie con cui un utente puo' attivarle o verificarle. Serve soprattutto
alla milestone [CLI parser v0](50-cli-parser-v0.md): prima di aggiungere
opzioni bisogna capire quali funzionalita' meritano una vera CLI, quali restano
configurazione e quali devono restare solo test o roadmap.

| Funzionalita' | Superficie attuale | Superficie CLI/config desiderata | Stato decisione |
| --- | --- | --- | --- |
| Osservare una o piu' directory | Argomenti posizionali `./alfred PATH...`; `./alfred -- PATH...` per terminare le opzioni | Mantenere `PATH...` e `-- PATH...`; rifiutare path che iniziano con `-` senza `--` | Supportato in CLI parser v0 |
| Caricare configurazione da file | `ALFRED_CONFIG=FILE`; `-c FILE`; `--config FILE`; `--config=FILE` | CLI esplicita vince su `ALFRED_CONFIG`: se `-c`/`--config` e' presente, il file ambiente non viene caricato | Supportato in CLI parser v0 |
| Mostrare usage | `./alfred --help`; `./alfred -h` | Restare comando informativo side-effect-free | Supportato |
| Mostrare versione | `./alfred --version`; `./alfred -V` | Restare comando informativo side-effect-free | Supportato |
| Validare configurazione senza runtime | `./alfred --check-config`, `./alfred -c FILE --check-config`, `./alfred --config FILE --check-config`, `./alfred --config=FILE --check-config`, con eventuale fallback `ALFRED_CONFIG=FILE` | Non richiedere `PATH...`; rifiutare path o comandi combinati; non creare log runtime | Supportato in CLI parser v0 |
| Stampare configurazione effettiva | Nessuna superficie | Candidato `--print-config`; deve terminare senza runtime se implementato | Da decidere in CLI parser v0 |
| Abilitare JSONL output | Config file: `output_enabled=true`, `output_format=jsonl`, `output_log=...` | Restare config per v0; niente flag CLI dedicata finche' il parser non e' stabile | Rimandato oltre CLI parser v0 |
| Usare counter output no-op | Config file: `output_format=counter` con output enabled | Restare config/test/benchmark; non e' una feature utente primaria | Non esporre come flag v0 |
| Configurare watch mask inotify | Config file: `inotify_watch_mask=...` | Restare config; CLI non deve conoscere token inotify nel parser v0 | Non esporre come flag v0 |
| Abilitare audit inotify opt-in | Config file: `inotify_audit_events=...` | Restare config perche' rumoroso e diagnostico | Non esporre come flag v0 |
| Scegliere event engine | Config/env: `event_engine`, `ALFRED_EVENT_ENGINE` | Nessuna nuova CLI v0; `shadow` resta rifiutato | Non esporre come flag v0 |
| Smoke test MVP | `make smoke-mvp` | Restare target Make/test, non opzione runtime | Fuori dalla CLI runtime |
| Test e benchmark performance | `make perf`, `make perf-*` | Restare target Make/manuali; non opzioni runtime | Fuori dalla CLI runtime |
| Pagine man | `man -l docs/man/...` | Packaging/installazione futuro; nessuna opzione Alfred | Rimandato |
| Agent Guard/policy/enforcement | Nessuna superficie runtime | Non introdurre flag finche' modello e backend non sono pronti | Fuori scope |

Regola pratica per questa milestone:

```text
se una funzionalita' e' gia' un comportamento utente fondamentale,
puo' ricevere una CLI;
se e' tuning, diagnostica o output avanzato, deve restare config;
se e' test, benchmark o roadmap futura, non deve diventare flag runtime.
```

## Funzionalita' end-to-end core

Queste righe descrivono il percorso completo:

```text
inotify -> alfred_raw_event_t -> core -> events.log
```

| Funzionalita' | Backend/input | Alfred raw | Core/evento finale | Stato | Test principali |
| --- | --- | --- | --- | --- | --- |
| Creazione file | `IN_CREATE` su file figlio | `ALFRED_RAW_CREATE` | `FILE_CREATED` | Supportato | `tests/core/test_create_file.sh` |
| Creazione directory | `IN_CREATE | IN_ISDIR` | `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR` | `DIR_CREATED` | Supportato | `tests/core/test_create_dir.sh`, `tests/backend/test_watch_added_create_dir.sh` |
| Cancellazione file | `IN_DELETE` su file figlio | `ALFRED_RAW_DELETE` | `FILE_DELETED` | Supportato | `tests/core/test_delete_file.sh` |
| Cancellazione directory figlia | `IN_DELETE | IN_ISDIR` | `ALFRED_RAW_DELETE | ALFRED_RAW_ISDIR` | `DIR_DELETED` | Supportato | `tests/core/test_delete_dir.sh`, `tests/backend/test_watch_removed_delete_dir.sh` |
| Modifica file | `IN_MODIFY` | `ALFRED_RAW_MODIFY` | `FILE_MODIFIED` con debounce core | Supportato | `tests/core/test_modify_file.sh` |
| File pronto dopo scrittura | `IN_CLOSE_WRITE` | `ALFRED_RAW_CLOSE_WRITE` | `FILE_READY` | Supportato | `tests/core/test_modify_file.sh`, `tests/core/test_create_file.sh` |
| Rename file nella stessa directory | `IN_MOVED_FROM` + `IN_MOVED_TO` con cookie uguale | `ALFRED_RAW_MOVED_FROM`, `ALFRED_RAW_MOVED_TO` | `FILE_RENAMED` | Supportato | `tests/core/test_rename_file.sh` |
| Rename directory nella stessa directory | `IN_MOVED_FROM | IN_ISDIR` + `IN_MOVED_TO | IN_ISDIR` | moved pair con `ALFRED_RAW_ISDIR` | `DIR_RENAMED` | Supportato | `tests/core/test_rename_dir.sh` |
| Move file tra directory osservate | `IN_MOVED_FROM` + `IN_MOVED_TO` con basename uguale e directory diversa | moved pair | `FILE_MOVED` | Supportato | `tests/core/test_move_file.sh` |
| Move directory tra directory osservate | moved pair con `IN_ISDIR` | moved pair con `ALFRED_RAW_ISDIR` | `DIR_MOVED` | Supportato | `tests/core/test_move_dir.sh` |
| Move + rename file | moved pair con directory e basename diversi | moved pair | `FILE_RELOCATED` | Supportato | `tests/core/test_move_rename_file.sh` |
| Move + rename directory | moved pair con `IN_ISDIR`, directory e basename diversi | moved pair con `ALFRED_RAW_ISDIR` | `DIR_RELOCATED` | Supportato | `tests/core/test_move_rename_dir.sh` |
| `MOVED_TO` senza `MOVED_FROM` | oggetto entrato nell'albero osservato senza sorgente nota | `ALFRED_RAW_MOVED_TO` | create fallback file o directory | Supportato come fallback | Coperto indirettamente da scenari move/recursive; da rafforzare con test dedicato |
| Overflow inotify | `IN_Q_OVERFLOW`, `wd=-1` | `ALFRED_RAW_OVERFLOW` | `OVERFLOW` | Supportato minimo | `tests/backend/test_overflow_raw_bridge.sh` |

## Watch ricorsivi e discovery directory

Queste funzionalita' sono state tenute nel backend. Non sono semantica finale
del core, anche se possono generare raw sintetici per il core.

| Funzionalita' | Comportamento | Evento/log prodotto | Stato | Test principali |
| --- | --- | --- | --- | --- |
| Aggiunta watch su directory creata | Quando nasce una directory figlia osservabile, il backend aggiunge un watch | `WATCH_ADDED`; il core vede anche `DIR_CREATED` | Supportato | `tests/backend/test_watch_added_create_dir.sh`, `tests/core/test_create_dir.sh` |
| Rimozione watch su directory cancellata | Quando il kernel rimuove un watch, il backend aggiorna la watcher table | `WATCH_REMOVED` | Supportato diagnostico | `tests/backend/test_watch_removed_delete_dir.sh` |
| Creazione ricorsiva lenta | Directory create con pause sufficienti per installare watch intermedi | `DIR_CREATED` per ogni directory, `WATCH_ADDED` per le directory osservate | Supportato | `tests/backend/test_recursive_slow_watch_tree.sh` |
| Creazione ricorsiva veloce | Directory annidate create prima che Alfred possa watchare ogni livello | raw sintetici `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR` per directory scoperte | Supportato | `tests/core/test_recursive_create_nested_dir.sh` |
| Root non directory | Il backend inotify accetta solo root directory: `add_target()` rifiuta file e symlink prima di registrare il target; `IN_ONLYDIR` resta hardening del watch manager | startup fallisce, niente `application startup complete`, niente `event loop started`, niente `WATCH_ADDED` falso | Supportato hardening | `tests/backend/test_onlydir_rejects_file_root.sh`, `tests/backend/test_root_file_rejects_startup.sh` |

## Configurazione inotify

| Funzionalita' | Chiave/config | Effetto | Stato | Test principali |
| --- | --- | --- | --- | --- |
| Maschera default | default backend | Include gli eventi filesystem principali, self-event diagnostici, `IN_IGNORED`, `IN_UNMOUNT`, `IN_Q_OVERFLOW` dove previsto dal backend | Supportato | test core e backend complessivi |
| Personalizzazione watch mask | `inotify_watch_mask=...` | Permette di scegliere token inotify supportati | Supportato | `tests/backend/test_watch_mask_invalid_token.sh` |
| Disabilitare `IN_ATTRIB` | `inotify_watch_mask=default,-IN_ATTRIB` | Rimuove `IN_ATTRIB` dalla subscription mask | Supportato | `tests/backend/test_watch_mask_disable_attrib.sh` |
| Token `IN_UNMOUNT` | `inotify_watch_mask=IN_UNMOUNT,IN_IGNORED` | Accetta il token come parte del contratto diagnostico backend | Supportato configurazione | `tests/backend/test_watch_mask_unmount_token.sh` |
| Token non valido | token sconosciuto o non ammesso | Startup fallisce con errore di configurazione | Supportato | `tests/backend/test_watch_mask_invalid_token.sh` |
| Config da file | `ALFRED_CONFIG=/path/file.conf` | Carica configurazione da file esplicito | Supportato | test config backend, doc applicazione |
| `event_engine=shadow` | config o env storica | Valore invalido; non fa fallback silenzioso a core | Supportato come rifiuto | `tests/core/test_invalid_event_engine_shadow.sh` |

## Audit inotify opt-in

Gli eventi audit `IN_OPEN`, `IN_ACCESS` e `IN_CLOSE_NOWRITE` sono reali ma molto
rumorosi. Alfred li supporta solo nel raw log inotify, non come raw Alfred e non
come semantica core.

| Funzionalita' | Config | Raw log backend | Alfred raw/core | Stato | Test principali |
| --- | --- | --- | --- | --- | --- |
| Open audit | `inotify_audit_events=open` | `IN_OPEN` | Nessuno | Opt-in diagnostico | `tests/backend/test_audit_config_raw_log.sh`, `tests/backend/test_audit_kernel_events.sh` |
| Access audit | `inotify_audit_events=access` | `IN_ACCESS` | Nessuno | Opt-in diagnostico | `tests/backend/test_audit_config_raw_log.sh`, `tests/backend/test_audit_kernel_events.sh` |
| Close-nowrite audit | `inotify_audit_events=close-nowrite` | `IN_CLOSE_NOWRITE` | Nessuno; non e' `FILE_READY` | Opt-in diagnostico | `tests/backend/test_audit_config_raw_log.sh`, `tests/backend/test_audit_kernel_events.sh` |
| Token audit invalido | `inotify_audit_events=IN_OPEN` o token sconosciuto | Startup fallisce | Nessuno | Supportato come rifiuto | `tests/backend/test_audit_config_invalid_token.sh` |
| Directory audit con `IN_ISDIR` | audit event su directory | Resta nel raw log | Non arriva al core se contiene solo `ALFRED_RAW_ISDIR` come modificatore | Supportato | coperto dal filtro backend; utile aggiungere test dedicato |

## Attributi e metadati

| Funzionalita' | Backend/input | Alfred raw | Core/evento finale | Stato | Test principali |
| --- | --- | --- | --- | --- | --- |
| Cambio attributi | `IN_ATTRIB` | `ALFRED_RAW_ATTRIB` | Nessun evento semantico | Supportato raw/backend | `tests/backend/test_attrib_raw_log.sh` |
| Disabilitazione attributi | `inotify_watch_mask=default,-IN_ATTRIB` | Nessun `IN_ATTRIB` richiesto | Nessun evento semantico | Supportato config | `tests/backend/test_watch_mask_disable_attrib.sh` |
| `FILE_METADATA_CHANGED` / `DIR_METADATA_CHANGED` | policy futura | raw gia' disponibile | Semantica rimandata | Rimandato | da progettare |

## Self events, stale watch e resync

Gli eventi `_SELF` riguardano il path osservato direttamente, non un figlio. Il
kernel non fornisce sempre abbastanza informazione per creare subito una
semantica utente corretta. Per questo Alfred li tratta prima come stato backend.

| Funzionalita' | Backend/input | Comportamento Alfred | Core/evento finale | Stato | Test principali |
| --- | --- | --- | --- | --- | --- |
| Directory osservata spostata | `IN_MOVE_SELF` | marca watch `STALE`, logga `WATCH_STALE`, prova resync locale o lost-scope recovery se applicabile | Nessun move/rename diretto da `IN_MOVE_SELF` | Parziale/supportato backend | `tests/backend/test_self_move_identity_match.sh`, `tests/backend/test_self_move_identity_mismatch.sh`, `tests/backend/test_lost_scope_runtime_recovery.sh` |
| Vecchio path torna valido con stessa identita' | `IN_MOVE_SELF` + identita' ancora coerente | resync locale, scan strict, reinstall missing watch, ritorno a `VALID` | Nuovi eventi futuri tornano affidabili | Supportato | `tests/backend/test_self_move_identity_match.sh`, `tests/backend/test_resync_reinstall_policy.sh` |
| Vecchio path torna con identita' diversa | `IN_MOVE_SELF` + identity mismatch | resta `STALE`, accoda lost-scope recovery | Nessun evento semantico falso | Supportato | `tests/backend/test_self_move_identity_mismatch.sh` |
| Directory osservata cancellata | `IN_DELETE_SELF` | marca `STALE`, aspetta `IN_IGNORED` per cleanup | Nessun `DIR_DELETED` diretto da self-event | Diagnostico/parziale | `tests/backend/test_self_events_root_watch.sh`, `tests/backend/test_delete_self_nested_watch.sh` |
| Watch ignorato/rimosso dal kernel | `IN_IGNORED` | rimuove entry dalla watcher table, logga cleanup | Nessun evento semantico | Supportato diagnostico | `tests/backend/test_watch_removed_delete_dir.sh`, self-event tests |
| Filesystem unmounted | `IN_UNMOUNT` | marca `STALE`, lascia cleanup a `IN_IGNORED` | Nessun delete semantico | Diagnostico | `tests/backend/test_watch_mask_unmount_token.sh` copre configurazione; scenario runtime reale rimandato |

## Lost-scope recovery

La lost-scope recovery serve quando Alfred non puo' piu' fidarsi del vecchio
path ma conserva un'identita' filesystem utile. L'obiettivo e' non perdere
definitivamente una directory osservata rinominata o spostata.

| Funzionalita' | Comportamento | Stato | Test principali |
| --- | --- | --- | --- |
| Coda lost-scope | Salva `wd`, vecchio path, identita', reason, `scan_root`, tentativi e tempo minimo di retry | Supportato | `tests/backend/test_lost_scope_queue.sh` |
| Enqueue da identity mismatch | Se il resync locale non puo' fidarsi del vecchio path, accoda una recovery delayed | Supportato | `tests/backend/test_self_move_identity_mismatch.sh` |
| Ricerca identita' nella root salvata | Scansiona `scan_root` cercando stesso `(dev, ino)` | Supportato | `tests/backend/test_lost_scope_recovery.sh` |
| Fallback su altre root configurate | Se `scan_root` non trova l'oggetto, prova le altre root configurate | Supportato | `tests/backend/test_lost_scope_runtime_recovery.sh` |
| Aggiornamento prefissi | Quando ritrova la directory, aggiorna i path dei watch nella subtree | Supportato | `tests/backend/test_lost_scope_recovery.sh` |
| Reinstallazione watch mancanti | Scansiona la subtree recuperata e reinstalla watch assenti | Supportato | `tests/backend/test_lost_scope_recovery.sh`, `tests/backend/test_resync_reinstall_policy.sh` |
| Rollback reinstallazione | Se una reinstallazione fallisce, rimuove i watch aggiunti nello stesso tentativo | Supportato | `tests/backend/test_lost_scope_recovery.sh` |
| Retry/backoff | Se non trova l'oggetto, rischedula fino al budget interno | Supportato | `tests/backend/test_lost_scope_recovery.sh` |
| Processamento runtime | Il poll processa batch sincrono minimo, oggi `batch_size=1` | Supportato | `tests/backend/test_lost_scope_runtime_recovery.sh`, test C recovery |
| Worker thread | Recovery asincrona con thread dedicato | Rimandato | da progettare dopo benchmark stabili |

## Performance e benchmark

| Funzionalita' | Comando | Stato | Note |
| --- | --- | --- | --- |
| Indice benchmark manuali | `make perf` | Supportato | Elenca i target disponibili e cosa misurano; non esegue benchmark |
| Benchmark manuale lost-scope | `make perf-lost-scope` | Supportato come strumento manuale | Misura recovery sintetica con fake watch operations; non e' gate CI |
| Benchmark manuale record sink | `make perf-record-sinks` | Supportato come strumento manuale | Misura record sintetici verso counter, text, JSONL, queue-counter, dispatcher e queue-dispatcher; non e' gate CI |
| Benchmark manuale runtime output | `make perf-runtime-output` | Supportato come strumento manuale | Avvia Alfred reale, crea file reali sotto inotify e confronta `compat-only`, `counter-output` e `jsonl-output`; non e' gate CI |
| Suite performance stabile | futura | Rimandato | Servono baseline, warmup, percentili, profili e benchmark end-to-end |
| No-op/counter sink benchmark | `make perf-record-sinks` | Baseline supportata | Misura il costo del confine `record -> sink` senza I/O writer |
| Queue-counter benchmark | `make perf-record-sinks` | Prima baseline queue supportata | Misura clone owned, push, pop, counter emit e destroy senza formattazione o I/O |
| Dispatcher sink benchmark | `make perf-record-sinks` | Prima baseline dispatcher supportata | Misura routing dispatcher verso counter, text, JSONL e fan-out sincrono combinato |
| Queue-dispatcher benchmark | `make perf-record-sinks` | Prima baseline runtime single-threaded supportata | Misura push queue, drain queue, dispatcher, sink emit e destroy owned senza worker thread |
| Output pipeline benchmark | `make perf-record-sinks` | Prima baseline pipeline supportata | Misura output pipeline JSONL composta con enqueue, drain, dispatcher, writer buffered e flush finale in memoria |
| Runtime output benchmark | `make perf-runtime-output` | Prima baseline runtime reale supportata | Misura backend inotify reale, callback app, log compatibili, pipeline counter no-op e output JSONL opt-in su file temporanei; il CSV include anche contatori locali della coda come enqueue, pressure drains, drain calls, drained records e max pending |
| Runtime drain single-threaded | `alfred_record_runtime_drain_once()` | Supportato come helper preparatorio | Nomina un batch drain sopra queue/dispatcher e restituisce max, dispatched, remaining e status |
| JSONL buffered writer | `alfred_record_jsonl_writer_t` | Supportato come helper preparatorio | Accumula righe JSONL in buffer caller-owned e scrive solo su flush o auto-flush |
| Output config minima | `config_t.output` + `output_log` | Supportata e collegata in opt-in JSONL/counter | Default spento; quando `output_enabled=true` e `output_format=jsonl` scrive JSONL aggiuntivo su `output_log`; quando `output_format=counter` attraversa queue e dispatcher senza produrre output |
| Output pipeline single-writer | `alfred_record_output_pipeline_t` | Collegata in modo sincrono dietro configurazione | Compone queue bounded, dispatcher, runtime drain e sink configurato per raw record normalizzati, eventi semantici core, diagnostica watch, diagnostica resync e handoff lost-scope; JSONL e' il writer reale v0, counter e' la baseline no-op per benchmark |
| Runtime counter no-op | `tests/backend/test_output_counter_runtime.sh` | Coperto da test backend end-to-end | Avvia Alfred reale con `output_format=counter`, verifica eventi compatibili, statistiche runtime output coerenti e assenza di `output_log`; protegge il contratto senza dipendere da `make perf-runtime-output` |
| Golden test JSONL | `make test-jsonl` | Chiuso per JSONL golden v0 corrente | Include scenari runtime end-to-end con Alfred reale e `output_enabled=true`, piu' golden sintetici di contratto quando il trigger reale non e' stabile, come `RAW_OVERFLOW` e `OVERFLOW` core. Verifica `output.jsonl` con parsing JSON reale per create file, create directory, modify/close-write file, attrib raw, overflow raw sintetico, overflow semantico core sintetico, delete file, delete directory, `WATCH_ADDED`, `WATCH_REMOVED`, rename file, move file, relocated file, rename directory, move directory, directory relocated, raw move cookie, `RAW_ATTRIB`, `RAW_OVERFLOW`, `OVERFLOW`, `FILE_MODIFIED`, `FILE_READY`, `FILE_DELETED`, `DIR_DELETED`, `FILE_RENAMED`, `FILE_MOVED`, `FILE_RELOCATED`, `DIR_RENAMED`, `DIR_MOVED`, `DIR_RELOCATED`, diagnostica recovery base `WATCH_STALE`/`WATCH_RESYNC_FAILED`/`WATCH_LOST_QUEUED` e recovery lost-scope completa fino a `WATCH_LOST_FOUND`/`WATCH_LOST_RECOVERY_END`; la matrice di chiusura in `22-contratto-log.md` distingue famiglie coperte, non-goal v0, rimandate e ancora da progettare |
| Smoke test MVP | `make smoke-mvp` | Supportato come prova operativa breve | Compila Alfred, verifica `--help`, `--version` e `--check-config`, avvia il runtime su una directory temporanea con JSONL opt-in, genera create/rename/dir create e valida `raw.log`, `events.log` e `output.jsonl`; non sostituisce suite contrattuali o benchmark |
| Backpressure/drop policy | futura | Rimandato | Da progettare insieme a Event Model, Backend API, Writer API e output strutturato |
| Code per sink | futura | Rimandato | Necessarie per isolare writer lenti come text, JSONL, Lab o socket |

## Funzionalita' non ancora supportate

| Area | Stato | Motivo del rinvio |
| --- | --- | --- |
| JSONL runtime stabile | Parziale/rimandato | Il runtime JSONL opt-in v0 e' collegato e testato per raw normalizzati, semantica core e diagnostica watch/resync/lost-scope rappresentativa; lifecycle/app resta non-goal JSONL v0 e rimane nei log umani; i failure writer/output sono fail-closed e restano in `errors.log`/exit status, non in record JSONL generici; mancano worker asincrono, code per sink, backpressure pubblica e un eventuale modello pubblico futuro per lifecycle/error |
| Tracepoint logici | Rimandato | Servono per Lab e debug strutturato, ma vanno progettati dopo il modello eventi |
| Backend API v0 | Documentata e chiusa come sottoinsieme staged: ops table, capabilities, target filesystem-path v0, emit boundary, lifecycle inotify, target management e poll non bloccante sono compilati e testati; core logger via record + sink; `WATCH_ADDED`/`WATCH_REMOVED`/`WATCH_STALE`/`WATCH_RESYNC_*`/`WATCH_LOST_*` backend via record + sink; raw filesystem principali `RAW_CREATE`/`RAW_DELETE`/`RAW_ATTRIB`/`RAW_MODIFY`/`RAW_CLOSE_WRITE`/`RAW_MOVED_FROM`/`RAW_MOVED_TO`/`RAW_OVERFLOW` runtime via record + sink; policy OS error documentata; campi OS error presenti in `alfred_record_t`; runtime `WATCH_RESYNC_FAILED` con `errno=N (...)` via record disponibile | Specifica in `30-backend-api-v0.md`; restano rimandati main loop su `backend_ops->poll()`, core input model su record o ponte misurato, raw audit dedicati, raw self-event dedicati, multi-backend runtime e backpressure pubblica |
| Writer API v0 | Documentata e parzialmente implementata | Specifica in `32-writer-api-v0.md`; esistono sink, text sink, JSONL sink, JSONL buffered writer, counter sink, queue, dispatcher, output pipeline e configurazione output minima; manca il collegamento runtime asincrono |
| Writer Runtime v0 | Collegato come runtime single-threaded opt-in | Specifica in `33-writer-runtime-roadmap-v0.md`; esistono benchmark queue/dispatcher, runtime drain single-threaded, JSONL buffered writer, config output minima, output pipeline single-writer, counter runtime, statistiche runtime e valvola di pressione v0; mancano worker thread, profili output, code per sink, backpressure pubblica e separazione completa dal loop backend |
| Plugin dinamici `.so` | Rimandato | Prima stabilizzare API statica e ownership memoria |
| fanotify | Rimandato | Non e' "inotify 2": serve Backend API e modello permission/enforcement |
| audit/eBPF | Rimandato | Richiedono process context, syscall/network model, capabilities e privilegi |
| Agent Guard policy engine | Rimandato | Richiede prompt/tool context, session/risk model e backend con enforcement |
| `IN_MASK_CREATE` | Rimandato | Hardening watch-manager utile, ma non urgente prima di Backend API/refactor watch policy |

## Prossimo uso pratico

Questa matrice e' stata usata per progettare
[Event Model v0](29-event-model-v0.md) e resta il controllo da consultare prima
di cambiare il modello:

1. leggere cosa Alfred supporta oggi
2. distinguere raw backend, Alfred raw, core semantico e diagnostica
3. evitare di perdere funzionalita' gia' coperte dai test
4. scegliere quali campi del futuro modello eventi servono davvero

Il modello corrente parte da questa domanda:

```text
quale struttura dati comune rappresenta tutte le righe "Supportato" di questa
matrice senza legarle per sempre a inotify?
```

La seconda domanda da fare e':

```text
quale caso d'uso concreto diventa possibile o piu' credibile grazie a questa
funzionalita'?
```

La risposta va mantenuta in
[Use case, posizionamento e integrazioni](36-use-cases-posizionamento-integrazioni.md).
