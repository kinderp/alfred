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
| Backend inotify raw log | `IN_CREATE`, `IN_DELETE`, `IN_MODIFY`, `IN_ATTRIB`, `IN_CLOSE_WRITE`, `IN_MOVED_FROM`, `IN_MOVED_TO`, `IN_ISDIR`, `IN_DELETE_SELF`, `IN_MOVE_SELF`, `IN_IGNORED`, `IN_UNMOUNT`, `IN_Q_OVERFLOW`; audit opt-in: `IN_OPEN`, `IN_ACCESS`, `IN_CLOSE_NOWRITE` | audit con contesto processo, policy guardrail multi-backend, recovery completa post-unmount |
| Alfred raw | `ALFRED_RAW_CREATE`, `ALFRED_RAW_DELETE`, `ALFRED_RAW_MODIFY`, `ALFRED_RAW_ATTRIB`, `ALFRED_RAW_CLOSE_WRITE`, `ALFRED_RAW_MOVED_FROM`, `ALFRED_RAW_MOVED_TO`, `ALFRED_RAW_OVERFLOW`, `ALFRED_RAW_ISDIR`; adapter iniziale verso record `RAW_*`; formatter testuale da record; `RAW_CREATE`/`RAW_DELETE`/`RAW_ATTRIB`/`RAW_MODIFY`/`RAW_CLOSE_WRITE` runtime via record + sink + text sink | raw audit dedicati, raw self-event dedicati, migrazione runtime degli altri record raw |
| Core semantico | `FILE_CREATED`, `DIR_CREATED`, `FILE_DELETED`, `DIR_DELETED`, `FILE_MODIFIED`, `FILE_READY`, move/rename/relocated file e directory, `OVERFLOW`; adapter `alfred_event_t -> alfred_record_t`; output `core_logger` via record formatter | metadata changed, file read/open audit, close-nowrite, delete/move self semantici, risk/session/policy |
| Backend state | watch ricorsivi, `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, resync locale, lost-scope queue, recovery delayed sincrona, retry/backoff, benchmark manuale lost-scope; builder iniziale verso record diagnostici `WATCH_*`; `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, `WATCH_RESYNC_*` e `WATCH_LOST_*` runtime via record + sink + text sink | worker thread, configurazione pubblica recovery, performance suite stabile, backpressure reale |

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
| Root non directory | `IN_ONLYDIR` fa fallire atomicamente il watch su un file root | errore controllato, niente `WATCH_ADDED` falso | Supportato hardening | `tests/backend/test_onlydir_rejects_file_root.sh` |

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
| Suite performance stabile | futura | Rimandato | Servono baseline, warmup, percentili, profili e benchmark end-to-end |
| No-op/counter sink benchmark | `make perf-record-sinks` | Baseline supportata | Misura il costo del confine `record -> sink` senza I/O writer |
| Queue-counter benchmark | `make perf-record-sinks` | Prima baseline queue supportata | Misura clone owned, push, pop, counter emit e destroy senza formattazione o I/O |
| Dispatcher sink benchmark | `make perf-record-sinks` | Prima baseline dispatcher supportata | Misura routing dispatcher verso counter, text, JSONL e fan-out sincrono combinato |
| Queue-dispatcher benchmark | `make perf-record-sinks` | Prima baseline runtime single-threaded supportata | Misura push queue, drain queue, dispatcher, sink emit e destroy owned senza worker thread |
| Output pipeline benchmark | `make perf-record-sinks` | Prima baseline pipeline supportata | Misura output pipeline JSONL composta con enqueue, drain, dispatcher, writer buffered e flush finale in memoria |
| Runtime drain single-threaded | `alfred_record_runtime_drain_once()` | Supportato come helper preparatorio | Nomina un batch drain sopra queue/dispatcher e restituisce max, dispatched, remaining e status |
| JSONL buffered writer | `alfred_record_jsonl_writer_t` | Supportato come helper preparatorio | Accumula righe JSONL in buffer caller-owned e scrive solo su flush o auto-flush |
| Output config minima | `config_t.output` + `output_log` | Supportata e collegata in opt-in JSONL | Default spento; quando `output_enabled=true` e `output_format=jsonl` scrive JSONL aggiuntivo su `output_log` |
| Output pipeline single-writer | `alfred_record_output_pipeline_t` | Collegata in modo sincrono dietro configurazione | Compone queue, dispatcher, runtime drain e JSONL writer per raw record normalizzati, eventi semantici core e diagnostica watch base `WATCH_ADDED`/`WATCH_REMOVED`/`WATCH_STALE`/`WATCH_STALE_EVENT_DROPPED` |
| Golden test JSONL end-to-end | `make test-jsonl` | Primi scenari supportati | Verifica `output.jsonl` con parsing JSON reale per create file, create directory, modify/close-write file, attrib raw, overflow raw sintetico, delete file, delete directory, `WATCH_ADDED`, `WATCH_REMOVED`, rename file, move file, relocated file, rename directory, move directory, directory relocated, raw move cookie, `RAW_ATTRIB`, `RAW_OVERFLOW`, `FILE_MODIFIED`, `FILE_READY`, `FILE_DELETED`, `DIR_DELETED`, `FILE_RENAMED`, `FILE_MOVED`, `FILE_RELOCATED`, `DIR_RENAMED`, `DIR_MOVED`, `DIR_RELOCATED` e diagnostica recovery base `WATCH_STALE`/`WATCH_RESYNC_FAILED`/`WATCH_LOST_QUEUED`; da estendere a recovery completa ed errori |
| Backpressure/drop policy | futura | Rimandato | Da progettare insieme a Event Model, Backend API, Writer API e output strutturato |
| Code per sink | futura | Rimandato | Necessarie per isolare writer lenti come text, JSONL, Lab o socket |

## Funzionalita' non ancora supportate

| Area | Stato | Motivo del rinvio |
| --- | --- | --- |
| JSONL runtime stabile | Parziale/rimandato | Formatter, sink, writer buffered, configurazione output e primo golden end-to-end esistono; mancano worker asincrono, backpressure e copertura golden ampia |
| Tracepoint logici | Rimandato | Servono per Lab e debug strutturato, ma vanno progettati dopo il modello eventi |
| Backend API v0 | Documentata, primo tipo record, adapter raw, adapter semantico, builder diagnostico, formatter testuale, sink comune e text sink; core logger via record + sink; `WATCH_ADDED`/`WATCH_REMOVED`/`WATCH_STALE`/`WATCH_RESYNC_*`/`WATCH_LOST_*` backend via record + sink; `RAW_CREATE`/`RAW_DELETE`/`RAW_ATTRIB`/`RAW_MODIFY`/`RAW_CLOSE_WRITE` runtime via record + sink; policy OS error documentata; campi OS error presenti in `alfred_record_t`; runtime `WATCH_RESYNC_FAILED` con `errno=N (...)` via record disponibile | Specifica in `30-backend-api-v0.md`; manca refactor completo inotify a `emit(record)` e migrazione degli altri raw runtime |
| Writer API v0 | Documentata e parzialmente implementata | Specifica in `32-writer-api-v0.md`; esistono sink, text sink, JSONL sink, JSONL buffered writer, counter sink, queue, dispatcher, output pipeline e configurazione output minima; manca il collegamento runtime asincrono |
| Writer Runtime v0 | Roadmap documentata e helper preparatori presenti | Specifica in `33-writer-runtime-roadmap-v0.md`; esistono benchmark queue/dispatcher, runtime drain single-threaded, JSONL buffered writer, config output minima e output pipeline single-writer; mancano worker thread, profili output, backpressure e collegamento dei writer fuori hot path |
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
