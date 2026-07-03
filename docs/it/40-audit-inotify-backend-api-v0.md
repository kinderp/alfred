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

## Mappa capabilities inotify v0

La issue [#82](https://github.com/kinderp/alfred/issues/82) rende esplicito il
contratto capabilities del backend inotify staged. Le capabilities sono
metadata statici del backend, non opzioni di configurazione e non stato per
evento. Servono a rispondere in modo conservativo alla domanda:

```text
che tipo di evidenza o controllo puo' fornire questo backend,
quando e' configurato nel modo giusto?
```

Questa distinzione e' importante per la roadmap futura di policy e backend
multipli. Una policy non deve dedurre dal nome `inotify` cosa e' possibile:
deve guardare le capabilities. Se una capability non e' dichiarata, il core o
il policy engine futuro devono trattarla come non disponibile e degradare in
modo esplicito, per esempio da `block` a `alert/would-block` quando il backend
non puo' bloccare.

### Descriptor statico

```text
inotify_backend_ops()
-> ops.capabilities = inotify_backend_capabilities()
-> static const alfred_backend_capabilities_t INOTIFY_BACKEND_CAPABILITIES
```

Il descriptor e' statico e process-lifetime. Il chiamante riceve un puntatore
borrowed: non deve modificarlo e non deve liberarlo. Il validator comune
`alfred_backend_ops_is_minimally_valid()` controlla che:

- `ops->capabilities` esista;
- `ops->name` e `capabilities->backend_name` coincidano;
- `ops->api_version` e `capabilities->api_version` coincidano;
- la bitmask capabilities non sia vuota.

Il helper `alfred_backend_capabilities_has()` accetta una sola capability alla
volta. Se riceve `NULL`, `0` o una maschera con piu' bit restituisce `0`. Questa
regola evita query ambigue come "ha almeno una di queste capability?" oppure
"ha tutte queste capability?".

### Capability dichiarate

| Capability | Perche' inotify la dichiara | Evidenza corrente | Test principali |
| --- | --- | --- | --- |
| `ALFRED_BACKEND_CAP_FILESYSTEM_EVENTS` | Il backend osserva mutazioni filesystem tramite inotify e le converte in raw event/record filesystem. | `inotify_backend_poll()`, adapter raw, core filesystem corrente. | `test_backend_capabilities.c`, `test_inotify_ops_uses_inotify_capabilities` |
| `ALFRED_BACKEND_CAP_RECURSIVE_WATCH` | Il backend puo' installare watch ricorsivi sulle directory esistenti e aggiungere watch su directory create durante il runtime. | `watch_manager_add_recursive()`, discovery ricorsiva, target management ricorsivo. | `test_backend_capabilities.c`, test target ricorsivi in `test_backend_inotify_ops.c` |
| `ALFRED_BACKEND_CAP_METADATA_EVENTS` | Il backend puo' osservare eventi metadata inotify, oggi esposti come raw/backend diagnostics dove supportato dal modello corrente. | Maschere inotify e matrice eventi; non implica process context o policy. | `test_backend_capabilities.c` |
| `ALFRED_BACKEND_CAP_SELF_EVENTS` | Il backend gestisce eventi sul watch stesso, come move/delete/ignored/unmount, per marcare stato stale, rimuovere watch o avviare recovery. | self events in watcher/recovery path, diagnostica `WATCH_STALE` e cleanup. | `test_backend_capabilities.c`, test backend diagnostics e recovery esistenti |
| `ALFRED_BACKEND_CAP_OVERFLOW_EVENTS` | Il backend riconosce overflow della coda inotify e lo rende osservabile invece di perderlo silenziosamente. | mapping overflow raw/semantico e diagnostica backend. | `test_backend_capabilities.c`, scenari overflow esistenti |
| `ALFRED_BACKEND_CAP_IDENTITY_TRACKING` | Il backend conserva identita' filesystem `device_id`/`inode_id` quando disponibile per distinguere oggetti oltre il solo path testuale. | watcher identity, record identity e lost-scope evidence. | `test_backend_capabilities.c`, test identity/JSONL e recovery collegati |
| `ALFRED_BACKEND_CAP_LOST_SCOPE_RECOVERY` | Il backend puo' tentare recovery di scope stale cercando l'identita' filesystem dentro root configurate. | lost-scope queue, scanner/resync e fallback root configurate. | `test_backend_capabilities.c`, scenari lost-scope/recovery esistenti |

### Capability non dichiarate

| Capability non dichiarata | Motivo |
| --- | --- |
| `ALFRED_BACKEND_CAP_AUDIT_EVENTS` | `inotify_audit_events` puo' aumentare il rumore del raw log con `IN_OPEN`, `IN_ACCESS` e `IN_CLOSE_NOWRITE`, ma non produce ancora record audit API-level. Finche' questi fatti non attraversano `alfred_record_t` come audit strutturato, la capability resta assente. |
| `ALFRED_BACKEND_CAP_PERMISSION_EVENTS` | Inotify osserva dopo o durante l'evento, ma non espone permission events pre-access come fanotify permission mode. |
| `ALFRED_BACKEND_CAP_PROCESS_CONTEXT` | Inotify non fornisce in modo affidabile pid, ppid, uid, exe, cmdline o process tree per ogni evento filesystem. |
| `ALFRED_BACKEND_CAP_NETWORK_CONTEXT` | Inotify non osserva rete, socket, connessioni o indirizzi remoti. |
| `ALFRED_BACKEND_CAP_CAN_BLOCK` | Inotify non puo' negare l'operazione prima che avvenga. Per v0 puo' osservare e diagnosticare; non puo' fare enforcement. |

Questa assenza e' parte del contratto, non una mancanza nascosta. Quando Alfred
avra' fanotify, audit, eBPF o backend di sistema operativi diversi, saranno
quei backend a dichiarare permission events, process context, network context o
blocking se li implementano davvero.

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
| `remove_target` | `backend_ops->remove_target()` -> `inotify_backend_ops_remove_target()` -> `inotify_backend_remove_startup_watch()` -> `watch_manager_remove()` | Rimuove una root filesystem-path gia' configurata e pulisce i watch associati. In v0 l'autorita' e' il registro `configured_roots`, non un watch descriptor interno. | Rimuove stato da `configured_roots` e watcher table. In modalita' ricorsiva rimuove anche i watch figli sotto la root con confronto di prefisso sicuro. Dopo l'inizio della rimozione, un errore diagnostico `WATCH_REMOVED` viene propagato ma non interrompe la pulizia dello stato target. | Richiede runtime inizializzato e target filesystem path valido. Il path deve corrispondere a una root configurata esatta; un child watch creato dalla ricorsivita' non puo' essere rimosso come target autonomo se non e' stato aggiunto come root. Per v0 non richiede `started == 1`. | `test_inotify_ops_rejects_invalid_remove_target_arguments`, `test_inotify_ops_remove_target_removes_recursive_subtree`, `test_inotify_ops_completes_recursive_remove_after_emit_failure` |
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

### Mappa dettagliata target management v0

La issue [#80](https://github.com/kinderp/alfred/issues/80) rende esplicito il
contratto target-management del backend inotify staged. Questa mappa serve a
evitare un secondo equivoco: `add_target()` e `remove_target()` non sono wrapper
generici intorno a `inotify_add_watch()` e `inotify_rm_watch()`. Sono l'API con
cui Alfred registra e rimuove target osservati. I watch descriptor creati dal
backend sono un dettaglio operativo interno.

Nel sottoinsieme v0, il solo target supportato e':

```text
target_type      = ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH
path             = path filesystem borrowed dal chiamante
flags            = ALFRED_BACKEND_TARGET_FLAG_NONE
backend_options  = NULL
```

Qualsiasi altro tipo, flag diverso da `NONE` o opzioni backend-specifiche
produce `ERR_INVALID_ARG`. Questa scelta e' intenzionalmente stretta: Alfred
vuole un punto di estensione per target futuri, ma non deve fingere di
supportare oggi processi, rete, mount, container o opzioni non implementate.

| Aspetto | Contratto inotify v0 | Funzioni coinvolte | Test principali |
| --- | --- | --- | --- |
| Validazione target | Il runtime ops deve essere inizializzato e avere context completo. Il target deve essere filesystem-path, path non `NULL`, non vuoto, piu' corto di `PATH_MAX`, senza slash finale salvo `/`, flags `NONE`, `backend_options == NULL`. | `inotify_backend_ops_add_target()`, `inotify_backend_ops_remove_target()`, `backend_configured_root_path_is_valid()` | `test_inotify_ops_rejects_invalid_add_target_arguments`, `test_inotify_ops_rejects_invalid_remove_target_arguments`, `test_inotify_ops_duplicate_nonrecursive_target_is_idempotent` |
| Ownership del path | Il path nel target e' borrowed solo durante la chiamata. Se il target viene accettato, il backend copia la stringa nei propri buffer `configured_roots` e nella watcher table. Il chiamante non deve mantenere vivo il target dopo il ritorno per far funzionare il backend. | `backend_configured_roots_add()`, `watch_manager_add()`, `watch_manager_add_recursive()` | Scenari target in `test_backend_inotify_ops.c` che verificano `configured_roots_count` e watcher table dopo le chiamate. |
| Add riuscito | `add_target()` registra prima la root in `configured_roots`, poi installa i watch. In modalita' ricorsiva installa la root e i figli esistenti; in modalita' non ricorsiva installa solo il path esatto. | `backend_add_startup_watch()`, `backend_configured_roots_add()`, `watch_manager_add_recursive()`, `watch_manager_add()` | `test_inotify_ops_init_destroy_lifecycle`, `test_inotify_ops_remove_target_removes_recursive_subtree` |
| Duplicato esatto | Un path gia' presente in `configured_roots` restituisce `ERR_OK`, non reinstalla watch e non emette un secondo `WATCH_ADDED`. Se i watch attivi sono spariti ma la root resta configurata, il duplicato resta idempotente: significa "target API gia' registrato", non "copertura kernel riparata". La reinstallazione forzata e' `remove_target()` seguito da `add_target()`. | `backend_configured_roots_has_exact()`, `backend_add_startup_watch()` | `test_inotify_ops_duplicate_nonrecursive_target_is_idempotent`, `test_inotify_ops_duplicate_target_does_not_repair_missing_watch`, `test_inotify_ops_rejects_overlapping_recursive_targets` |
| Overlap ricorsivo | In modalita' ricorsiva, target padre/figlio diversi sono rifiutati perche' v0 non ha ownership/refcount dei watch. Exact duplicate e' accettato. Prefissi testuali simili ma fuori subtree, per esempio `/tmp/root` e `/tmp/root-old`, sono accettati grazie alla regola con separatore `/`. | `backend_configured_roots_has_nested_overlap()`, `backend_path_matches_prefix()` | `test_inotify_ops_rejects_overlapping_recursive_targets` |
| Rollback add fallito | `add_target()` e' atomic-like rispetto allo stato target: se fallisce dopo aver riservato la root, rimuove i watch parziali e cancella la root da `configured_roots` prima di restituire errore. Se fallisce la registrazione della root, nessun watch e' ancora installato. | `backend_add_startup_watch()`, `backend_rollback_target_watches()`, `backend_configured_roots_remove()` | `test_inotify_ops_rolls_back_failed_recursive_add` |
| Autorita' della remove | `remove_target()` puo' rimuovere solo una root esatta presente in `configured_roots`. Un watch figlio creato dalla ricorsivita' del padre non e' un target autonomo, quindi viene rifiutato se non e' stato registrato come root. | `inotify_backend_remove_startup_watch()`, `backend_configured_roots_has_exact()` | `test_inotify_ops_rejects_remove_of_recursive_child_watch` |
| Remove senza watch attivi | La root configurata e' l'autorita' API. Se i watch attivi sono gia' spariti, per esempio dopo una manutenzione backend o un `IN_IGNORED`, `remove_target()` rimuove comunque la root da `configured_roots` e restituisce `ERR_OK`, salvo errori reali di rimozione. | `inotify_backend_remove_startup_watch()`, `backend_configured_roots_remove()` | `test_inotify_ops_remove_nonrecursive_target_without_active_watch`, `test_inotify_ops_remove_recursive_target_without_active_watches` |
| Cleanup ricorsivo | In modalita' ricorsiva, la rimozione della root raccoglie tutti i watch sotto la root con prefisso sicuro. La root `/` e' il caso speciale: se configurata ricorsivamente, copre tutti i path assoluti osservati. In modalita' non ricorsiva viene rimosso solo il path esatto. | `watcher_collect_wds_by_path_prefix()`, `watch_manager_remove()` | `test_inotify_ops_remove_target_removes_recursive_subtree` |
| Errore diagnostico durante remove | Dopo che i watch descriptor da rimuovere sono stati raccolti, il backend completa la pulizia anche se una diagnostica `WATCH_REMOVED` fallisce. L'errore viene propagato al chiamante, ma `configured_roots` e watcher table non restano a meta'. | `inotify_backend_remove_startup_watch()`, `watch_manager_remove()` | `test_inotify_ops_completes_recursive_remove_after_emit_failure` |

Il punto architetturale e' che `configured_roots` rappresenta i target API
configurati, mentre la watcher table rappresenta i watch kernel attivi. I due
stati sono collegati, ma non sono la stessa cosa. Questa separazione rende
possibile distinguere:

- un target configurato ma temporaneamente senza watch attivi;
- un watch figlio creato dalla ricorsivita' e non registrato come target API;
- un duplicato exact che e' un retry idempotente, non una richiesta di repair;
- un remove valido della root anche quando la copertura kernel e' gia' sparita.

Non-goal v0:

- target non filesystem;
- target flags diversi da `ALFRED_BACKEND_TARGET_FLAG_NONE`;
- `backend_options` concrete;
- ownership/refcount di watch condivisi tra root sovrapposte;
- `list_targets`;
- repair automatico della copertura kernel tramite duplicate `add_target()`;
- policy o decisioni di sicurezza dentro target management.

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
