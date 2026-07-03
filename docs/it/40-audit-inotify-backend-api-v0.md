# Audit inotify vs Backend API v0

Questo documento nasce come audit iniziale della issue GitHub
[#72](https://github.com/kinderp/alfred/issues/72) ed e' stato poi aggiornato
durante i micro-step della milestone
[`Inotify backend conforms to Backend API v0`](https://github.com/kinderp/alfred/issues/71).
Serve a capire perche' il backend Linux `inotify` puo' ora essere trattato come
backend di riferimento per il sottoinsieme staged di Backend API v0 e quali
debiti restano deliberatamente fuori da questo endpoint.

L'audit non cambia il runtime. Il suo scopo e' evitare refactor guidati
dall'intuizione: prima si mappano confini, test, gap e debiti; poi si aprono
issue/PR piccole.

## Risultato sintetico

`inotify` e' ora il reference backend per il sottoinsieme staged di Backend API
v0:

- esiste una tabella statica `alfred_backend_ops_t`;
- esiste un descriptor capabilities conservativo;
- `app.c` usa le ops per init, target startup, start, stop e destroy;
- `add_target()` e `remove_target()` sono mappati su target filesystem path;
- `poll()` e' implementata nella tabella ops e produce record normalizzati;
- il main loop mantiene intenzionalmente il raw bridge legacy per il core;
- i test focused coprono descriptor, capabilities, lifecycle, target
  management, diagnostica, overflow, lost-scope recovery e poll staged.

Il punto deliberatamente non chiuso e' il runtime end-to-end:

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
ponte misurato record -> raw/core. Questa decisione esce dalla conformita'
inotify staged e richiede benchmark sul percorso caldo.

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

La funzione `app_poll_legacy_raw_backend_once()` esiste proprio per rendere
visibile questo ponte temporaneo. Non e' un backend generico: costruisce e usa
un `inotify_backend_context_t` concreto, poi consegna gli eventi raw alla
callback applicativa `handle_backend_event()`.

`handle_backend_event()` fa due cose distinte:

1. quando il raw event e' sink-capable, lo adatta con
   `alfred_record_from_raw()` per alimentare il raw log compatibile e l'output
   strutturato;
2. passa comunque il raw event originale al core con `alfred_process()`, perche'
   il core semantico corrente lavora ancora su `alfred_raw_event_t`.

Questa duplicita' non deve essere confusa con il contratto finale. E' il ponte
scelto per mantenere stabile il core mentre Backend API v0 viene resa reale a
pezzi piccoli. Il punto architetturale e' che la trasformazione raw -> record
nel main loop oggi serve all'output, non ha ancora sostituito l'input del core.

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

La issue [#84](https://github.com/kinderp/alfred/issues/84) rende esplicito il
contratto di questo confine. La staged poll comune entra da
`alfred_backend_ops_t.poll`, quindi il chiamante non vede piu' callback raw
specifiche di inotify. Il backend ops adapter usa ancora la poll inotify
esistente, ma la incapsula dietro il confine comune:

| Step | Funzioni coinvolte | Dato passato | Responsabilita' |
| --- | --- | --- | --- |
| Entrata API | `backend_ops->poll()` -> `inotify_backend_ops_poll()` | `alfred_backend_t *`, `timeout_ms` | Validare runtime, stato `started`, emit callback e semantica `timeout_ms == 0`. |
| Lettura backend | `inotify_backend_poll()` -> `backend_poll()` | `inotify_backend_context_t`, callback raw interna | Leggere fd inotify non bloccante, costruire raw event quando l'evento e' dispatchabile e gestire diagnostica/watch repair. |
| Adattamento raw | `inotify_backend_ops_emit_raw_record()` -> `alfred_record_from_raw()` | `alfred_raw_event_t` borrowed -> `alfred_record_t` stack-local | Tradurre il fatto raw in un record `NORMALIZED_RAW` del modello comune. |
| Emit comune | `runtime->context.emit_record(&record, userdata)` | `alfred_record_t` borrowed | Consegnare il record al chiamante Backend API v0 senza sapere se a valle ci sara' queue, sink, test capture o writer. |

Precondizioni v0:

- il runtime ops deve essere stato inizializzato da `init()`;
- `start()` deve essere gia' stato chiamato e `started` deve essere `1`;
- il context interno deve avere runtime/config/logger validi;
- `init()` deve aver ricevuto una callback `alfred_backend_emit_t.emit` valida;
- `timeout_ms` deve essere `0`.

`timeout_ms == 0` significa solo: prova a consumare quello che e' gia'
disponibile sull'fd inotify non bloccante. Valori diversi sono rifiutati con
`ERR_INVALID_ARG` perche' Alfred non ha ancora definito una semantica comune per
poll bloccanti, deadline, sleep o integrazione con un event loop esterno.

Ownership:

- `alfred_backend_emit_t` e' caller-owned; `init()` copia solo function pointer
  e `userdata`, non conserva il puntatore alla envelope;
- `userdata` resta caller-owned e deve restare valido finche' il backend puo'
  emettere record, normalmente fino a `stop()`/`destroy()`;
- il raw event passato da `inotify_backend_poll()` alla callback e' borrowed;
- il record creato da `inotify_backend_ops_emit_raw_record()` e' stack-local e
  borrowed durante `emit()`;
- se il consumer vuole conservare il record oltre la callback, deve clonarlo o
  accodarlo con ownership propria, come fa la futura coda bounded.

Comportamento su eventi non dispatchabili:

`backend_poll()` puo' chiamare la callback raw con `NULL` quando l'evento letto
serve solo a manutenzione backend, diagnostica o watch repair e non deve
raggiungere il core come raw Alfred event. La staged adapter
`inotify_backend_ops_emit_raw_record()` tratta `raw == NULL` come no-op
riuscito e non emette nessun record. Questo evita di trasformare segnali
backend-interni in record raw falsi.

Comportamento di errore:

- runtime non inizializzato, non started, context incompleto, emit mancante o
  timeout diverso da zero producono `ERR_INVALID_ARG`;
- se `alfred_record_from_raw()` rifiuta la maschera raw, l'adapter restituisce
  `ERR_INVALID_ARG`;
- se la callback `emit_record()` fallisce, l'errore viene propagato alla poll;
- se la poll inotify incontra errori di I/O, diagnostica o recovery, propaga il
  relativo errore gia' usato dal backend esistente.

Perche' il main loop non usa ancora questa poll:

```text
runtime attuale:
alfred_raw_event_t -> alfred_process(core, raw) -> alfred_event_t

staged Backend API v0:
alfred_raw_event_t -> alfred_record_from_raw() -> alfred_record_t -> emit()
```

Il primo percorso alimenta il core semantico. Il secondo alimenta il confine
comune di output/record. Sostituire ora `app_poll_legacy_raw_backend_once()` con
`backend_ops->poll()` cambierebbe il contratto di input del core o
richiederebbe un ponte record -> raw/core. Questa migrazione deve essere una
decisione separata, misurata con benchmark e documentata come modifica del
percorso caldo.

### Mappa errori e diagnostica backend v0

La issue [#86](https://github.com/kinderp/alfred/issues/86) chiarisce il
rapporto tra errori tecnici, diagnostica osservabile e eventi semantici. Il
principio e':

```text
un errore tecnico decide se una chiamata deve fermarsi;
un record diagnostico spiega cosa Alfred ha osservato o deciso internamente;
un evento semantico descrive un fatto filesystem stabile per l'applicazione.
```

Queste tre cose non vanno fuse. Per esempio `WATCH_STALE` puo' far fallire la
poll se il record strutturato non viene emesso in modalita' fail-closed, ma il
suo significato resta "il mapping wd -> path non e' piu' affidabile", non
`DIR_MOVED` o `DIR_DELETED`.

| Famiglia | Funzioni principali | Osservabilita' corrente | Errore restituito | Significato del contratto |
| --- | --- | --- | --- | --- |
| Watch lifecycle base | `watch_manager_log_simple_watch_diagnostic()`, `backend_log_watch_stale()`, `backend_log_stale_event_dropped()` | `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, `WATCH_STALE_EVENT_DROPPED` passano da record diagnostici e text sink; quando `emit_record` e' configurato vengono offerti anche all'output strutturato. | Il fallimento di `emit_record` viene propagato come errore backend, dopo aver scritto o tentato il log compatibile. | Diagnostica dello stato osservativo di Alfred; non sono eventi filesystem semantici. |
| Resync locale | `backend_log_resync_failure()`, `backend_log_resync_record()`, `backend_resync_watch()` | `WATCH_RESYNC_*` descrive inizio, scan, classificazione, reinstall, rollback, fallimento e fine del tentativo locale. `WATCH_RESYNC_SCAN_FAILED` conserva il canale `errors.log`; gli altri restano `events.log`. | Se la diagnostica strutturata non puo' essere emessa quando richiesta, il resync ritorna errore e la poll puo' fermarsi. | La recovery resta conservativa: senza evidenza completa Alfred non deve tornare a fidarsi del path. |
| Lost-scope recovery | `backend_enqueue_lost_scope()`, `backend_lost_scope_recover_*()`, `backend_log_lost_scope_record()` | `WATCH_LOST_*` documenta queue, scan, found/not-found, prefix update, coverage, reinstall, retry, gave-up e recovery end. | `BACKEND_LOST_SCOPE_RECOVERY_OUTPUT_FAILED` separa il fallimento di output dal risultato filesystem della recovery. | Il backend distingue "non ho trovato l'identita'", "la recovery tecnica e' fallita" e "non riesco a registrare il diagnostico strutturato". |
| Overflow | `backend_build_overflow_raw()` -> `alfred_record_from_raw()` -> core | `IN_Q_OVERFLOW` diventa `ALFRED_RAW_OVERFLOW` con path vuoto; il core puo' produrre `OVERFLOW`. | La costruzione raw e' trattata come fatto dispatchabile; errori successivi seguono il normale percorso callback/poll. | Overflow e' stream-integrity evidence: segnala che lo stream puo' essere incompleto, non una mutazione di un singolo path. |
| Poll/runtime I/O | `backend_poll()` | `EAGAIN`/`EWOULDBLOCK` sono idle poll riuscite; `EINTR` e' ignorato come poll riuscita; EOF/read error vanno in `errors.log`. | Errori di read, EOF, recovery o callback vengono propagati come `ERR_IO` o errore callback. | Errore tecnico di runtime: non e' ancora un record `diagnostic/backend` uniforme. |
| Output callback | `ctx->emit_record(...)`, `app_emit_output_record()` | Le diagnostiche gia' sink-capable raggiungono output strutturato quando `output_enabled=true`. | Il backend propaga il fallimento della callback per non perdere silenziosamente il ledger strutturato. | Fail-closed sul ledger: se Alfred promette output strutturato, non deve continuare come se il diagnostico fosse stato registrato. |

La regola operativa per questa milestone e':

```text
diagnostica osservativa = record diagnostico quando il modello lo supporta;
errore tecnico = codice di ritorno che decide se continuare;
semantica filesystem = solo core semantic event.
```

Questo evita due bug futuri:

- trasformare diagnostica backend in falsi eventi utente;
- nascondere errori di output strutturato dietro log testuali riusciti.

Non tutto e' ancora un record uniforme. Errori generici di configurazione,
inizializzazione, allocazione o I/O restano soprattutto `errors.log` e codici
`ERR_*`. Prima di serializzarli in JSONL o in un backend-agnostic diagnostic
stream serve un contratto dedicato, per esempio `diagnostic/backend` o
`diagnostic/lifecycle`, con campi stabili per fase, codice, errno, recoverable e
azione consigliata. Per v0 questo resta debito dichiarato, non una
non-conformita' nascosta.

## Test gia' collegati

| Test | Cosa copre |
| --- | --- |
| `tests/backend/test_backend_ops.c` | Forma minima di `alfred_backend_ops_t`, validator e ownership dell'emit envelope. |
| `tests/backend/test_backend_capabilities.c` | Flag capabilities inotify e assenza di capability non supportate. |
| `tests/backend/test_backend_inotify_ops.c` | Descriptor inotify, init/destroy, start/stop, target validation, duplicate target, rollback, recursive overlaps, remove target, poll staged e record raw emesso. |
| `tests/backend/test_record_diagnostic_builder.c` | Builder diagnostici `WATCH_*`, payload watch/recovery e distinzione da eventi semantici. |
| `tests/backend/test_lost_scope_*` | Famiglie `WATCH_LOST_*`, routing output, retry/completion e recovery conservativa. |
| `tests/backend/test_overflow_raw_bridge.c` | Bridging `IN_Q_OVERFLOW` -> `ALFRED_RAW_OVERFLOW` -> record raw overflow. |
| `tests/backend/test_output_counter_runtime.sh` | Runtime applicativo con output counter e compatibilita' del percorso app/output. |
| `tests/backend/test_output_pipeline_runtime.sh` | Runtime applicativo con output pipeline e JSONL/counter boundary. |

Questi test sono sufficienti per il subset staged attuale. Non dimostrano ancora
che il main loop sia backend-agnostic end-to-end, perche' quella migrazione e'
esplicitamente rimandata.

### Mappa focused tests Backend API v0

La issue [#88](https://github.com/kinderp/alfred/issues/88) chiarisce che il
micro-step "Add/update focused tests" non richiede automaticamente nuovi test.
La regola e':

```text
se il micro-step cambia comportamento o chiude un gap di contratto,
aggiungere un test mirato;
se il micro-step e' un audit di copertura,
documentare quali test proteggono gia' il contratto e dichiarare i debiti.
```

Per il subset staged corrente la seconda opzione e' quella corretta. Le PR
precedenti hanno gia' introdotto test focused mentre chiarivano i singoli
contratti. Aggiungere un secondo test che ripete lo stesso assert renderebbe la
suite piu' costosa senza aumentare la fiducia. Il valore di questo step e'
rendere leggibile la copertura: chi modifica il backend deve sapere quale test
romperebbe se violasse una regola.

| Area del contratto | Test focused principali | Funzioni o scenari protetti | Cosa dimostrano |
| --- | --- | --- | --- |
| Descriptor Backend API v0 | `tests/backend/test_backend_ops.c` | `test_ops_accepts_complete_v0_descriptor`, `test_ops_rejects_missing_or_empty_name`, `test_ops_rejects_invalid_version`, `test_ops_rejects_missing_capabilities`, `test_ops_rejects_missing_callbacks` | Una `alfred_backend_ops_t` valida deve avere nome, versione, capabilities e callback obbligatorie. Il validator rifiuta descriptor incompleti prima che entrino nel runtime. |
| Coerenza ops/capabilities | `tests/backend/test_backend_ops.c`, `tests/backend/test_backend_inotify_ops.c` | `test_ops_rejects_capability_name_mismatch`, `test_ops_rejects_capability_version_mismatch`, `test_ops_rejects_zero_capabilities`, `test_inotify_ops_descriptor_is_valid`, `test_inotify_ops_uses_inotify_capabilities` | Il nome/versione della tabella ops e del descriptor capabilities devono coincidere. Inotify espone una tabella statica valida e agganciata alle sue capabilities reali. |
| Ownership emit envelope | `tests/backend/test_backend_ops.c`, `tests/backend/test_backend_inotify_ops.c` | test helper sul fake backend, `test_inotify_ops_init_destroy_lifecycle` | `init()` copia function pointer e `userdata` dall'envelope caller-owned, ma non possiede la envelope. `destroy()` deve cancellare il context e lasciare il runtime riutilizzabile dopo nuova init. |
| Capability dichiarate | `tests/backend/test_backend_capabilities.c` | `test_capability_helper_rejects_ambiguous_input`, `test_inotify_declares_observational_filesystem_capabilities`, `test_inotify_does_not_claim_enforcement_or_context` | Il helper rifiuta query ambigue e inotify dichiara solo capability osservazionali filesystem/recovery. Non promette audit API-level, process context, network context, permission events o blocking. |
| Lifecycle init/start/stop/destroy | `tests/backend/test_backend_inotify_ops.c` | `test_inotify_ops_rejects_invalid_init_arguments`, `test_inotify_ops_rejects_invalid_start_stop_arguments`, `test_inotify_ops_init_destroy_lifecycle` | Il runtime ops rifiuta argomenti invalidi, `start/stop` sono marker idempotenti nello staged subset e `destroy` rilascia le risorse possedute da `init`. |
| Target validation | `tests/backend/test_backend_inotify_ops.c` | `test_inotify_ops_rejects_invalid_add_target_arguments`, `test_inotify_ops_rejects_invalid_remove_target_arguments` | Il target v0 accetta solo filesystem path validi, flags `NONE` e `backend_options == NULL`. Target non filesystem o parametri non supportati restano fuori contratto. |
| Target ownership e idempotenza | `tests/backend/test_backend_inotify_ops.c` | `test_inotify_ops_duplicate_nonrecursive_target_is_idempotent`, `test_inotify_ops_duplicate_target_does_not_repair_missing_watch` | Il path e' borrowed durante la chiamata e copiato dal backend se accettato. Un duplicato exact significa "target gia' registrato", non "ripara la copertura kernel". |
| Overlap e rollback ricorsivo | `tests/backend/test_backend_inotify_ops.c` | `test_inotify_ops_rejects_overlapping_recursive_targets`, `test_inotify_ops_rolls_back_failed_recursive_add` | V0 rifiuta root ricorsive sovrapposte per evitare ownership/refcount non definiti. Se un add ricorsivo fallisce, root e watch parziali vengono rimossi. |
| Remove target | `tests/backend/test_backend_inotify_ops.c` | `test_inotify_ops_remove_target_removes_recursive_subtree`, `test_inotify_ops_rejects_remove_of_recursive_child_watch`, `test_inotify_ops_remove_nonrecursive_target_without_active_watch`, `test_inotify_ops_remove_recursive_target_without_active_watches`, `test_inotify_ops_completes_recursive_remove_after_emit_failure` | `remove_target()` lavora su root API configurate, non su watch descriptor arbitrari. Deve pulire subtree ricorsivi e non lasciare stato mezzo rimosso anche se una diagnostica di output fallisce. |
| Poll staged | `tests/backend/test_backend_inotify_ops.c` | `test_inotify_ops_rejects_invalid_poll_arguments`, `test_inotify_ops_init_destroy_lifecycle` | `backend_ops->poll(timeout_ms = 0)` richiede runtime inizializzato, `started == 1`, emit callback valida e timeout non bloccante. La poll staged puo' emettere un record raw normalizzato attraverso `alfred_record_from_raw()`. |
| Diagnostic builder | `tests/backend/test_record_diagnostic_builder.c` | `test_watch_stale_builds_watch_diagnostic`, `test_stale_event_dropped_builds_watch_diagnostic`, `test_resync_failed_builds_recovery_diagnostic`, `test_resync_failed_can_carry_os_error`, `test_invalid_inputs_are_rejected` | I record `WATCH_*` sono diagnostica strutturata con payload watch/recovery, non eventi filesystem semantici. I builder rifiutano input incompleti. |
| Diagnostic output fail-closed | `tests/backend/test_watch_diagnostic_output_failure.c`, `tests/backend/test_watch_stale_output_failure.c` | scenari diretti su diagnostica watch e stale | Se Alfred promette output strutturato, un fallimento dell'emit diagnostico deve propagarsi invece di essere nascosto da un log testuale riuscito. |
| Lost-scope recovery diagnostics | `tests/backend/test_lost_scope_queue_output_routing.c`, `tests/backend/test_lost_scope_scan_output_routing.c`, `tests/backend/test_lost_scope_completion_output_routing.c`, `tests/backend/test_lost_scope_recovery.c`, `tests/backend/test_lost_scope_runtime_recovery.sh` | queue, scan, found/not-found, retry/completion e recovery runtime | Le famiglie `WATCH_LOST_*` restano diagnostica di recovery e documentano la ricerca conservativa dell'identita' filesystem. |
| Overflow | `tests/backend/test_overflow_raw_bridge.c`, `tests/jsonl/test_overflow_raw_jsonl.sh`, `tests/jsonl/test_overflow_core_jsonl.sh` | `test_overflow_event_builds_global_raw`, `test_non_overflow_event_is_ignored`, scenari JSONL raw/core | `IN_Q_OVERFLOW` diventa evidenza di integrita' dello stream (`ALFRED_RAW_OVERFLOW`) e non una mutazione path-specific. |
| Runtime output compatibility | `tests/backend/test_output_counter_runtime.sh`, `tests/backend/test_output_pipeline_runtime.sh` | output counter, output JSONL/counter, shutdown/flush/error boundary | Il runtime applicativo conserva compatibilita' mentre l'output strutturato e' opt-in. Questi test non trasformano il main loop in backend-agnostic runtime. |

La mappa lascia intenzionalmente fuori tre debiti, perche' non sono mancanze di
test sul subset staged ma decisioni future:

- migrazione del main loop da `inotify_backend_poll()` a
  `backend_ops->poll()`;
- semantica comune per `timeout_ms != 0`, deadline o poll bloccanti;
- diagnostica backend-agnostic uniforme per init/config/I/O/lifecycle generici.

Quando uno di questi debiti diventera' codice, il relativo micro-step dovra'
aggiungere test nuovi. Fino ad allora la suite focused deve proteggere il
contratto realmente implementato, non simulare un runtime futuro.

## Micro-step completati

La milestone ha chiuso questi micro-step:

| Micro-step | Issue / PR | Risultato |
| --- | --- | --- |
| Audit implementazione corrente | [#72](https://github.com/kinderp/alfred/issues/72), [PR #75](https://github.com/kinderp/alfred/pull/75) | Prima mappa dei gap tra inotify e Backend API v0 staged subset. |
| Endpoint di conformita' | [#76](https://github.com/kinderp/alfred/issues/76), [PR #77](https://github.com/kinderp/alfred/pull/77) | Definito cosa significa "conforme" senza promettere il main loop backend-agnostic. |
| Lifecycle | [#78](https://github.com/kinderp/alfred/issues/78), [PR #79](https://github.com/kinderp/alfred/pull/79) | Mappati `init`, `start`, `add_target`, `remove_target`, `poll`, `stop`, `destroy`. |
| Target management | [#80](https://github.com/kinderp/alfred/issues/80), [PR #81](https://github.com/kinderp/alfred/pull/81) | Documentato il target filesystem-path v0, inclusi ownership, duplicati, overlap e rollback. |
| Capabilities | [#82](https://github.com/kinderp/alfred/issues/82), [PR #83](https://github.com/kinderp/alfred/pull/83) | Dichiarate capability conservative e assenza di process/network/permission/blocking. |
| Poll/emit boundary | [#84](https://github.com/kinderp/alfred/issues/84), [PR #85](https://github.com/kinderp/alfred/pull/85) | Separato runtime raw bridge corrente dalla staged poll Backend API v0. |
| Errori e diagnostica | [#86](https://github.com/kinderp/alfred/issues/86), [PR #87](https://github.com/kinderp/alfred/pull/87) | Separati errore tecnico, record diagnostico ed evento semantico. |
| Focused tests | [#88](https://github.com/kinderp/alfred/issues/88), [PR #89](https://github.com/kinderp/alfred/pull/89) | Collegata ogni parte del contratto ai test focused esistenti; nessun gap test uncovered trovato. |

## Debiti rimandati

Questi debiti restano fuori dal subset staged e devono avere issue/PR proprie
quando diventeranno lavoro operativo:

1. **Migrazione main loop**.
   `app_run()` usa ancora il raw bridge perche' il core consuma
   `alfred_raw_event_t`. Migrare a `backend_ops->poll()` richiede una decisione
   sul core input model e benchmark sul percorso caldo.
2. **Poll bloccante o con deadline**.
   `timeout_ms != 0` e' rifiutato: Alfred non ha ancora un contratto comune per
   sleep, deadline, integrazione event loop o poll bloccanti.
3. **Diagnostica backend generica**.
   Errori init/config/I/O/lifecycle non sono ancora tutti record
   backend-agnostic; molti restano `errors.log` e codici `ERR_*`.
4. **Registry multi-backend**.
   Inotify e' il reference backend statico, non esiste ancora selezione runtime
   tra piu' backend.
5. **Backend futuri**.
   Fanotify, audit, eBPF, Windows e macOS non rientrano in questa milestone.

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
