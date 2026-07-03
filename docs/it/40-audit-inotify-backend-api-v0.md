# Audit inotify vs Backend API v0

Questo documento e' l'audit iniziale della issue GitHub
[#72](https://github.com/kinderp/alfred/issues/72). Serve a capire quanto il
backend Linux `inotify` e' gia' conforme a Backend API v0 e quali micro-step
restano prima di poterlo trattare come backend di riferimento.

L'audit non cambia il runtime. Il suo scopo e' evitare refactor guidati
dall'intuizione: prima si mappano confini, test, gap e debiti; poi si aprono
issue/PR piccole.

## Risultato sintetico

`inotify` e' gia' molto vicino al ruolo di reference backend per il sottoinsieme
staged di Backend API v0:

- esiste una tabella statica `alfred_backend_ops_t`;
- esiste un descriptor capabilities conservativo;
- `app.c` usa le ops per init, target startup, start, stop e destroy;
- `add_target()` e `remove_target()` sono mappati su target filesystem path;
- `poll()` e' implementata nella tabella ops e produce record normalizzati;
- il main loop mantiene intenzionalmente il raw bridge legacy per il core;
- i test focused coprono descriptor, capabilities, lifecycle, target
  management e poll staged.

Il punto non chiuso e' il runtime end-to-end:

```text
app_run()
-> app_poll_legacy_raw_backend_once()
-> inotify_backend_poll()
-> handle_backend_event(alfred_raw_event_t)
-> alfred_process(core, raw)
```

Questo percorso resta corretto per ora, perche' il core semantico consuma ancora
`alfred_raw_event_t`. La poll Backend API v0 invece emette `alfred_record_t`:

```text
backend_ops->poll()
-> inotify_backend_poll()
-> inotify_backend_ops_emit_raw_record()
-> alfred_record_from_raw()
-> emit(alfred_record_t)
```

Quindi il prossimo lavoro non deve essere "sostituire una chiamata con
un'altra". Deve decidere se il core dovra' consumare record, oppure se serve un
ponte misurato record -> raw/core.

## Tabella di conformita'

| Area | Stato | Evidenza corrente | Gap / prossimo passo |
| --- | --- | --- | --- |
| Ops descriptor | Conforme per v0 staged | `inotify_backend_ops()` ritorna una tabella statica con nome, versione, capabilities e callback richieste. | Nessun gap immediato. Futuro: registry backend se arrivano piu' backend. |
| Validator ops | Conforme | `alfred_backend_ops_is_minimally_valid()` rifiuta nome/versione/capabilities/callback mancanti. | Nessun gap immediato. |
| Capabilities | Conforme e conservativo | `inotify_backend_capabilities()` dichiara filesystem, recursive watch, metadata, self events, overflow, identity tracking e lost-scope recovery. Non dichiara audit API, process, network, permission o blocking. | Documentare sempre nuovi flag prima di usarli in policy. |
| Init/destroy | Conforme staged | `app_init()` chiama `backend_ops->init`; `destroy` chiude fd/watchers e azzera context. | La config resta concreta `inotify_backend_ops_config_t`, non esiste ancora una config comune multi-backend. |
| Start/stop | Conforme come marker v0 | `start()` e `stop()` sono idempotenti e gestiscono `runtime->started`. Non possiedono ancora risorse kernel. | Se un backend futuro avvia thread o subscription reali, il contratto start/stop dovra' essere piu' forte. |
| Target management | Conforme per filesystem path | `add_target()` e `remove_target()` validano target type, path, flags e backend_options; delegano a `inotify_backend_add_startup_watch()` e `inotify_backend_remove_startup_watch()`. | Solo target filesystem path. Niente target non-filesystem e niente opzioni backend-specifiche. |
| Target ownership | Conforme | Il target path e' borrowed per la chiamata; il backend copia nei propri buffer `PATH_MAX` quando serve. | Continuare a testare casi limite path/trailing slash/rollback. |
| Poll staged | Parzialmente conforme | `backend_ops->poll(timeout_ms=0)` delega a `inotify_backend_poll()` e converte raw in record. | Non supporta timeout bloccanti. Non e' usata dal main loop. Richiede emit callback. |
| Main loop | Ponte intenzionale | `app_poll_legacy_raw_backend_once()` isola la chiamata diretta a `inotify_backend_poll()`. | Decidere in milestone futura il core input model o un ponte misurato. |
| Emit ownership | Conforme | `init()` copia `emit->emit` e `emit->userdata`; non conserva il puntatore alla envelope `alfred_backend_emit_t`. | `userdata` resta caller-owned e valido fino a stop/destroy. Va mantenuto chiaro nei test e nelle doc. |
| Diagnostica | Conforme per record gia' migrati | WATCH_ADDED/REMOVED/STALE e molti resync/lost-scope passano da builder record e sink. | Verificare che ogni nuova diagnostica backend rimanga sink-capable e runtime-routed quando richiesto. |
| Error handling | Parziale | Molti errori ritornano `ERR_INVALID_ARG`, `ERR_IO`, `ERR_CONFIG`, `ERR_ALLOC`. | Manca ancora una tassonomia backend piu' ricca recoverable/unrecoverable. |
| Hot path | Accettabile per v0 | Il percorso caldo runtime continua a usare raw bridge; output strutturato enqueua record e draina fuori dal callback producer. | Ogni futura migrazione di poll/core deve avere benchmark. |
| Test focused | Buono per subset staged | `test_backend_ops.c`, `test_backend_capabilities.c`, `test_backend_inotify_ops.c`. | Aggiungere test nuovi solo quando un micro-step cambia un contratto o chiude un gap specifico. |

## Call path correnti

### Lifecycle applicativo

```text
app_init()
-> app->backend_ops = inotify_backend_ops()
-> alfred_backend_ops_is_minimally_valid()
-> backend_ops->init()
   -> inotify_backend_ops_init()
   -> inotify_backend_init()
-> backend_ops->add_target()
   -> inotify_backend_ops_add_target()
   -> inotify_backend_add_startup_watch()
-> backend_ops->start()
   -> inotify_backend_ops_start()
```

Shutdown:

```text
app_shutdown()
-> backend_ops->stop()
   -> inotify_backend_ops_stop()
-> backend_ops->destroy()
   -> inotify_backend_ops_destroy()
   -> inotify_backend_shutdown()
```

Questa parte e' gia' collegata alla tabella ops. `app.c` resta composition root:
sceglie il backend statico `inotify_backend_ops()` e costruisce la configurazione
concreta.

### Mappa dettagliata del lifecycle v0

La issue [#78](https://github.com/kinderp/alfred/issues/78) rende esplicito il
contratto lifecycle del backend inotify staged. Questa mappa serve a evitare un
equivoco importante: avere le callback `init`, `start`, `stop` e `destroy` nella
tabella comune non significa ancora che il runtime principale sia
backend-agnostic end-to-end. Significa che il backend inotify puo' gia' essere
inizializzato, avviato logicamente, fermato logicamente e distrutto attraverso
la forma comune `alfred_backend_ops_t`.

| Callback | Funzioni coinvolte | Responsabilita' corrente | Risorse possedute o rilasciate | Precondizioni e comportamento di errore | Test principali |
| --- | --- | --- | --- | --- | --- |
| `init` | `app_init()` -> `backend_ops->init()` -> `inotify_backend_ops_init()` -> `inotify_backend_init()` | Costruisce il vecchio `inotify_backend_context_t`, copia i puntatori borrowed a config/logger, copia function pointer e `userdata` dell'emit boundary, inizializza il runtime inotify esistente. | Apre e inizializza le risorse backend tramite `inotify_backend_init()`: fd inotify, watcher table, code e registri backend. Non osserva ancora target startup finche' `add_target()` non viene chiamata. | Richiede runtime azzerato/non inizializzato, config concreta valida, logger valido. Rifiuta un secondo `init()` su runtime gia' inizializzato con `ERR_INVALID_ARG`. Se l'inizializzazione fallisce, ripulisce i puntatori del context. | `test_inotify_ops_rejects_invalid_init_arguments`, `test_inotify_ops_init_destroy_lifecycle` |
| `add_target` | `app_init()` -> `backend_ops->add_target()` -> `inotify_backend_ops_add_target()` -> `inotify_backend_add_startup_watch()` | Registra una root filesystem-path e installa i watch secondo la configurazione inotify. In v0 e' target management, non policy. | Aggiunge stato in `configured_roots` e nella watcher table. In caso di errore durante l'installazione deve fare rollback del target parziale. | Richiede runtime inizializzato e target filesystem path non vuoto, flags `NONE`, `backend_options == NULL`. Per v0 non richiede `started == 1`: `app_init()` aggiunge target prima di `start()`. | `test_inotify_ops_rejects_invalid_add_target_arguments` e gli scenari target in `test_backend_inotify_ops.c` |
| `start` | `app_init()` -> `backend_ops->start()` -> `inotify_backend_ops_start()` | Porta il runtime ops nello stato operativo logico impostando `started = 1`. | Non apre fd, non installa watch, non avvia thread, non legge eventi. Le risorse kernel sono gia' state create da `init()` e verranno rilasciate da `destroy()`. | Richiede runtime inizializzato e context completo. Prima di `init()` o con context incoerente restituisce `ERR_INVALID_ARG`. Chiamarlo piu' volte dopo `init()` e' idempotente. | `test_inotify_ops_rejects_invalid_start_stop_arguments`, `test_inotify_ops_init_destroy_lifecycle` |
| `poll` | `backend_ops->poll()` -> `inotify_backend_ops_poll()` -> `inotify_backend_poll()` -> `inotify_backend_ops_emit_raw_record()` -> `alfred_record_from_raw()` -> `emit(alfred_record_t)` | Espone una poll comune staged che legge eventi disponibili e li converte in record `normalized_raw`. | Non possiede risorse nuove. Usa l'fd non bloccante gia' inizializzato e l'emit callback copiato da `init()`. | Richiede runtime inizializzato, `started == 1`, context completo, emit callback valida e `timeout_ms == 0`. Rifiuta timeout diversi per non promettere blocking semantics non implementate. Non e' ancora il main loop di `app_run()`. | `test_inotify_ops_rejects_invalid_poll_arguments`, `test_inotify_ops_init_destroy_lifecycle` |
| `stop` | `app_shutdown()` -> `backend_ops->stop()` -> `inotify_backend_ops_stop()` | Ferma logicamente il backend portando `started = 0`. | Non chiude fd, non rimuove watch, non fa flush di risorse kernel. Il cleanup resta responsabilita' di `destroy()`. | Richiede runtime inizializzato e context completo. Dopo `init()` restituisce `ERR_OK` anche se `start()` non era stato chiamato. Chiamarlo piu' volte e' idempotente. | `test_inotify_ops_rejects_invalid_start_stop_arguments`, `test_inotify_ops_init_destroy_lifecycle` |
| `destroy` | `app_shutdown()` -> `backend_ops->destroy()` -> `inotify_backend_ops_destroy()` -> `inotify_backend_shutdown()` | E' il confine di cleanup reale. Rilascia il runtime inotify esistente e riporta l'ops runtime in stato riutilizzabile dopo un nuovo `init()`. | Chiude fd, rilascia watcher table, configured roots e lost-scope state tramite `inotify_backend_shutdown()`. Azzera context, `initialized` e `started`. | E' safe su `NULL` e su runtime non inizializzato. Deve funzionare anche se il chiamante distrugge un backend ancora `started`, per gestire shutdown o failure path. | `test_inotify_ops_init_destroy_lifecycle` |

La scelta piu' importante e' che `start()` e `stop()` sono marker di lifecycle,
non proprietari di risorse. Questo e' accettabile nello staged subset perche'
inotify non ha ancora thread, subscription remote o programmi kernel caricati da
`start()`. Un backend futuro potrebbe invece dover rendere `start()` e `stop()`
molto piu' forti: per esempio avviare un worker, agganciare tracepoint,
sottoscrivere un provider o fare flush ordinato.

Il vincolo per i prossimi micro-step e' quindi:

```text
non assumere che start/stop siano no-op universali;
documentare e testare sempre quali risorse appartengono a init/destroy
e quali appartengono a start/stop per ogni backend.
```

### Poll runtime corrente

```text
app_run()
-> app_poll_legacy_raw_backend_once()
-> inotify_backend_poll(ctx, handle_backend_event, app)
-> handle_backend_event(alfred_raw_event_t)
-> alfred_process(core, raw)
```

Questo e' il percorso runtime principale. E' ancora raw-oriented perche' il core
semantico deve ricevere `alfred_raw_event_t`.

### Poll staged Backend API v0

```text
backend_ops->poll(runtime, 0)
-> inotify_backend_ops_poll()
-> inotify_backend_poll(ctx, inotify_backend_ops_emit_raw_record, runtime)
-> inotify_backend_ops_emit_raw_record(raw)
-> alfred_record_from_raw(raw, &record)
-> runtime->context.emit_record(&record, userdata)
```

Questo percorso e' reale e testato, ma non e' ancora il main loop. Serve a
dimostrare che il backend puo' emettere record tramite la forma comune.

## Test gia' collegati

| Test | Cosa copre |
| --- | --- |
| `tests/backend/test_backend_ops.c` | Forma minima di `alfred_backend_ops_t`, validator e ownership dell'emit envelope. |
| `tests/backend/test_backend_capabilities.c` | Flag capabilities inotify e assenza di capability non supportate. |
| `tests/backend/test_backend_inotify_ops.c` | Descriptor inotify, init/destroy, start/stop, target validation, duplicate target, rollback, recursive overlaps, remove target, poll staged e record raw emesso. |
| `tests/backend/test_output_counter_runtime.sh` | Runtime applicativo con output counter e compatibilita' del percorso app/output. |
| `tests/backend/test_output_pipeline_runtime.sh` | Runtime applicativo con output pipeline e JSONL/counter boundary. |

Questi test sono sufficienti per il subset staged attuale. Non dimostrano ancora
che il main loop sia backend-agnostic end-to-end, perche' quella migrazione e'
esplicitamente rimandata.

## Micro-step consigliati

1. **Documentare questo audit**.
   Deve chiudere #72 come mappa iniziale, senza cambiare runtime.

2. **Aggiornare la documentazione Backend API v0 con un link all'audit**.
   Il lettore deve arrivare facilmente dal contratto generale alla conformita'
   specifica inotify.

3. **Aprire micro-step solo per gap concreti**.
   Esempi: chiarire error taxonomy, migliorare test diagnostici, o isolare
   ulteriormente app composition root se utile.

4. **Non migrare il main loop senza decisione separata**.
   La migrazione richiede prima una scelta sul core input model e benchmark sul
   percorso caldo.

## Decisione adottata

La issue [#76](https://github.com/kinderp/alfred/issues/76) fissa
l'endpoint di questa milestone. Per questa milestone, "inotify conforms to
Backend API v0" significa:

```text
inotify e' il backend reference per il subset staged:
ops, capabilities, lifecycle, target management, emit boundary e poll staged
sono implementati, testati e documentati.

Il main loop raw bridge resta un ponte intenzionale e isolato,
non una non-conformita' nascosta.
```

Questa definizione e' pragmatica: chiude il contratto usabile oggi senza
promettere un runtime multi-backend che non esiste ancora.

Di conseguenza, ogni PR successiva della milestone deve essere valutata rispetto
a questo endpoint. Se una modifica prova a migrare il main loop o a cambiare il
tipo di input del core, non e' piu' un micro-step di conformita' inotify: e' una
decisione architetturale separata, con test e benchmark propri.
