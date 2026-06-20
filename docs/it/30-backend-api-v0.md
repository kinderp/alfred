# Backend API v0

Questo documento definisce la proposta Backend API v0 di Alfred. L'obiettivo e'
stabilire come un backend deve parlare con Alfred senza duplicare il core e
senza trasformare stringhe di log in API.

La API non viene implementata in questa fase. Il documento serve come specifica
per il prossimo refactor architetturale, dopo [Event Model v0](29-event-model-v0.md).

La decisione principale e':

```text
Backend API v0 produce record compatibili con Event Model v0.
I backend restano linkati staticamente nella prima fase.
I plugin dinamici .so sono rimandati a una fase successiva.
```

## Perche' serve

Oggi Alfred ha un solo backend reale: Linux `inotify`.

Il confine e' gia' migliorato:

- `app.c` costruisce un `inotify_backend_context_t`;
- il backend riceve runtime, configurazione inotify e logger tramite contesto;
- `inotify_backend_poll()` consegna `alfred_raw_event_t` all'app/core boundary;
- il watch manager gestisce `WATCH_ADDED` e `WATCH_REMOVED`;
- l'adapter inotify traduce `struct inotify_event` in raw event Alfred.

Il problema e' che questo confine e' ancora specifico di inotify:

- il tipo runtime e' `inotify_backend_t`;
- la configurazione e' `inotify_config_t`;
- la diagnostica usa ancora direttamente `logger_t`;
- l'emissione primaria e' ancora `alfred_raw_event_t`, non il record comune di
  Event Model v0;
- `start` e `stop` non sono espliciti;
- non esiste un modello comune di capabilities.

Backend API v0 serve a rendere esplicito il contratto comune prima di aggiungere
fanotify, audit, eBPF, Windows, macOS o backend commerciali.

## Obiettivi

Backend API v0 deve permettere a ogni backend di:

- inizializzare e distruggere il proprio runtime;
- avviare e fermare la propria osservazione;
- aggiungere e rimuovere target osservati;
- leggere o ricevere eventi dal sistema operativo;
- emettere record compatibili con Event Model v0;
- dichiarare capabilities;
- tenere separati stato runtime, configurazione e writer;
- restare backend-specifico senza conoscere la semantica finale del core.

Un backend non deve:

- emettere direttamente eventi semantici `FILE_*` o `DIR_*`;
- duplicare il correlatore del core;
- fare parsing o generazione diretta del formato JSONL;
- usare stringhe testuali come contratto primario;
- obbligare il core a conoscere `wd`, file descriptor, handle o API kernel.

## Architettura a livelli

```text
OS / kernel / provider
-> backend specifico
-> Event Model v0 records
-> app dispatcher
-> core / writers / Lab / security pipeline
```

Per inotify oggi il percorso reale e':

```text
struct inotify_event
-> raw log backend
-> alfred_raw_event_t
-> core
-> alfred_event_t
-> events.log
```

Con Backend API v0 il percorso desiderato diventa:

```text
struct inotify_event
-> backend_observed record
-> normalized_raw record
-> core semantic processing
-> semantic record
-> text writer / JSONL writer / binary writer
```

La diagnostica segue lo stesso principio:

```text
WATCH_STALE testuale legacy
-> diagnostic + watch + WATCH_STALE record
-> writer testuale / JSONL / Lab
```

Il primo passo runtime e' gia' stato applicato a `WATCH_ADDED` e
`WATCH_REMOVED`; `WATCH_STALE` e' il primo diagnostico migrato con
`reason=...`:

```text
watch_manager_add() / watch_manager_remove()
-> alfred_record_build_watch_diagnostic(WATCH_ADDED | WATCH_REMOVED)
-> alfred_record_format_text()
-> logger_event("WATCH_ADDED wd=N path=P")
-> logger_event("WATCH_REMOVED wd=N path=P")

backend_handle_move_self/delete_self/unmount()
-> alfred_record_build_watch_diagnostic(WATCH_STALE, reason=R)
-> alfred_record_format_text()
-> logger_event("WATCH_STALE wd=N path=P reason=R")
```

Non e' ancora la Backend API completa, perche' `watch_manager_add()` non chiama
un sink generico `emit(record)`, `watch_manager_remove()` segue ancora lo
stesso confine locale e `inotify_backend.c` scrive ancora sul logger esistente.
Pero' il dato diagnostico non nasce piu' solo come stringa: nasce come
`alfred_record_t` e solo dopo viene formattato nel payload testuale
compatibile.

## Tipi concettuali

Questi tipi descrivono la direzione della API. Non sono ancora header C
implementati.

```c
typedef struct alfred_backend alfred_backend_t;
typedef struct alfred_backend_config alfred_backend_config_t;
typedef struct alfred_backend_target alfred_backend_target_t;
typedef struct alfred_record alfred_record_t;
```

`alfred_backend_t` e' runtime opaco del backend. Il chiamante non deve conoscere
campi interni come file descriptor, watcher table, mappe kernel o code di
recovery.

`alfred_backend_config_t` contiene una vista stabile della configurazione
necessaria al backend. I campi specifici del backend devono stare in una sezione
backend-specifica, non nel core.

`alfred_backend_target_t` rappresenta cosa osservare. Nel caso filesystem
contiene almeno un path e opzioni come ricorsivita' o audit opt-in. In futuro
potra' rappresentare anche target non filesystem.

`alfred_record_t` e' il record comune definito da Event Model v0. Il backend
puo' emettere record `backend_observed`, `normalized_raw` e `diagnostic`.

## Emit sink

Il backend non deve conoscere logger, JSONL writer o core direttamente. Deve
emettere record verso un sink fornito dall'applicazione.

```c
typedef int (*alfred_backend_emit_fn)(
    const alfred_record_t *record,
    void *userdata
);

typedef struct {
    alfred_backend_emit_fn emit;
    void *userdata;
} alfred_backend_emit_t;
```

Regole:

- il backend chiama `emit()` per ogni record prodotto;
- il record e' valido solo durante la chiamata, salvo diversa documentazione;
- il backend non conserva `userdata`;
- il backend non decide quale writer ricevera' il record;
- l'app dispatcher decide se inviare il record al core, al log testuale, a JSONL
  o ad altri consumer.

Questa scelta sostituisce progressivamente due confini correnti:

```text
callback raw: const alfred_raw_event_t *
logger_event("WATCH_STALE ...")
```

con un confine unico:

```text
emit(const alfred_record_t *record)
```

## Operazioni Backend API v0

```c
typedef struct {
    const char *name;
    uint32_t api_version;
    uint64_t capabilities;

    int (*init)(alfred_backend_t *backend,
                const alfred_backend_config_t *config,
                const alfred_backend_emit_t *emit);

    int (*start)(alfred_backend_t *backend);

    int (*add_target)(alfred_backend_t *backend,
                      const alfred_backend_target_t *target);

    int (*remove_target)(alfred_backend_t *backend,
                         const alfred_backend_target_t *target);

    int (*poll)(alfred_backend_t *backend,
                int timeout_ms);

    int (*stop)(alfred_backend_t *backend);

    void (*destroy)(alfred_backend_t *backend);
} alfred_backend_ops_t;
```

### `name`

Nome stabile del backend, per esempio:

```text
inotify
fanotify
audit
ebpf
windows
macos
```

Questo nome deve corrispondere al campo `source_backend` di Event Model v0.

### `api_version`

Versione della Backend API supportata dal backend. Per questa proposta vale `0`.
Servira' soprattutto quando introdurremo plugin dinamici o backend sviluppati
separatamente.

### `capabilities`

Bitmask di funzionalita' dichiarate dal backend. Le capabilities non dicono che
una feature e' attiva nella configurazione corrente: dicono che il backend e'
capace di offrirla.

### `init`

Alloca o inizializza le risorse del backend. Per inotify corrisponde oggi a:

```text
inotify_backend_init()
```

Responsabilita':

- aprire file descriptor o handle;
- inizializzare tabelle runtime;
- salvare configurazione e sink emittente;
- non iniziare necessariamente a osservare target.

### `start`

Porta il backend nello stato operativo. Per inotify v0 puo' essere quasi no-op,
ma per backend futuri potrebbe:

- avviare thread;
- registrare subscription globali;
- caricare programmi eBPF;
- connettersi a un provider remoto.

### `add_target`

Aggiunge un target osservato. Per inotify corrisponde oggi a:

```text
inotify_backend_add_startup_watch()
watch_manager_add()
watch_manager_add_recursive()
```

Il target non deve essere solo una stringa libera: deve poter rappresentare
opzioni come ricorsivita', audit opt-in e policy backend-specifica.

### `remove_target`

Rimuove un target osservato. Per inotify corrisponde oggi a una combinazione di:

```text
watch_manager_remove()
watcher_remove()
```

Il backend deve emettere diagnostica strutturata quando smette di osservare un
target, per esempio `diagnostic + watch + WATCH_REMOVED`.

### `poll`

Legge o consuma eventi disponibili. Per inotify corrisponde oggi a:

```text
inotify_backend_poll()
```

`timeout_ms` permette di usare la stessa API con backend polling e backend
event-driven. Un valore da definire potra' indicare "non bloccare", "blocca per
N millisecondi" o "blocca indefinitamente". La semantica esatta e' rimandata
alla fase di implementazione.

### `stop`

Ferma l'osservazione senza necessariamente distruggere il runtime. Per inotify
v0 puo' essere quasi no-op. Per backend futuri serve a:

- fermare thread;
- disabilitare subscription;
- fare flush ordinato;
- preparare shutdown pulito.

### `destroy`

Rilascia tutte le risorse del backend. Per inotify corrisponde oggi a:

```text
inotify_backend_shutdown()
```

Deve essere sicura dopo inizializzazione parziale.

## Target model

Backend API v0 deve distinguere il target osservato dal backend runtime.

Concetto minimo:

```c
typedef struct {
    const char *path;
    uint32_t target_type;
    uint32_t flags;
    const void *backend_options;
} alfred_backend_target_t;
```

Per il filesystem v0:

| Campo | Uso |
| --- | --- |
| `path` | root o path da osservare |
| `target_type` | per ora `filesystem_path` |
| `flags` | ricorsivo, audit opt-in, read-only policy futura |
| `backend_options` | opzioni specifiche, per esempio maschere inotify |

Decisione v0: non rendere subito generico tutto il mondo. Il target iniziale
puo' essere filesystem-oriented, ma deve lasciare spazio a target futuri non
filesystem.

## Capabilities

Capabilities iniziali consigliate:

| Capability | Significato |
| --- | --- |
| `filesystem_events` | osserva mutazioni filesystem |
| `recursive_watch` | puo' osservare alberi ricorsivi |
| `audit_events` | puo' osservare fatti audit rumorosi |
| `metadata_events` | puo' osservare cambi metadati |
| `self_events` | osserva eventi sul target stesso |
| `overflow_events` | segnala perdita eventi o overflow |
| `identity_tracking` | conserva identita' come device/inode |
| `lost_scope_recovery` | puo' cercare target persi tramite identita' |
| `permission_events` | puo' bloccare o autorizzare accessi |
| `process_context` | puo' associare processo o pid affidabile |
| `network_context` | puo' osservare rete |
| `can_block` | puo' bloccare azioni, non solo osservarle |

Capabilities del backend inotify corrente:

| Capability | Stato inotify |
| --- | --- |
| `filesystem_events` | si |
| `recursive_watch` | si |
| `audit_events` | opt-in raw log |
| `metadata_events` | raw-only tramite `IN_ATTRIB` |
| `self_events` | diagnostica backend |
| `overflow_events` | minimo: raw/core `OVERFLOW` |
| `identity_tracking` | si, con `st_dev` e `st_ino` |
| `lost_scope_recovery` | si, sincrona/batch limitato |
| `permission_events` | no |
| `process_context` | no affidabile con inotify |
| `network_context` | no |
| `can_block` | no |

## Error model

Backend API v0 deve usare errori strutturati, non solo `-1` generico.

Nella prima implementazione C potra' ancora restare compatibile con `error_t`,
ma la diagnostica emessa deve spiegare:

| Campo Event Model | Esempio |
| --- | --- |
| `layer` | `diagnostic` |
| `category` | `error`, `watch`, `recovery` |
| `type` | `WATCH_RESYNC_FAILED`, `BACKEND_INIT_FAILED` |
| `source_backend` | `inotify` |
| `severity` | `error` |
| `error` | `identity-mismatch`, `scan-failed`, `io-error` |
| `path` | path coinvolto, se noto |
| `watch_id` | watch coinvolto, se noto |

Regola: il codice puo' restituire un codice compatto, ma il record diagnostico
deve spiegare il contesto.

## Ownership e durata memoria

Regole v0:

- il backend possiede il proprio runtime;
- il backend non possiede il sink `emit`;
- il backend non possiede il core;
- il backend non possiede i writer;
- i record emessi sono validi solo durante `emit()`, salvo copia esplicita;
- path e stringhe dentro il record devono essere copiati dal consumer se devono
  sopravvivere alla callback;
- `destroy()` deve liberare tutto cio' che il backend ha allocato;
- l'app resta ownership root nella fase statica iniziale.

Queste regole sono coerenti con il comportamento corrente di
`alfred_raw_event_t`, dove `path` resta valido solo durante la chiamata a
`alfred_process()`.

## Mapping inotify corrente

| Backend API v0 | Codice corrente |
| --- | --- |
| `alfred_backend_t` | `inotify_backend_t` |
| `alfred_backend_config_t` | `inotify_config_t` dentro `config_t` |
| `init` | `inotify_backend_init()` |
| `start` | non esplicito; no-op candidato |
| `add_target` | `inotify_backend_add_startup_watch()` |
| `remove_target` | `watch_manager_remove()` e cleanup watcher |
| `poll` | `inotify_backend_poll()` |
| `stop` | non esplicito; no-op candidato |
| `destroy` | `inotify_backend_shutdown()` |
| emit normalized raw | callback `inotify_backend_event_fn` con `alfred_raw_event_t` |
| emit diagnostic | `logger_event()` con righe `WATCH_*` |
| adapter native -> raw | `inotify_adapter_build_raw()` |
| runtime watch state | `watcher_table_t` |
| recovery state | `inotify_lost_scope_queue_t` |

Il primo refactor non dovrebbe riscrivere tutto. Dovrebbe introdurre un confine
compatibile:

```text
inotify_backend_poll()
-> build alfred_raw_event_t come oggi
-> adattare a alfred_record_t quando il record comune esistera'
```

Per la diagnostica:

```text
WATCH_ADDED/WATCH_REMOVED/WATCH_STALE -> diagnostic record -> text formatter
gia' migrati
-> backend_emit_diagnostic_record(WATCH_*) domani
-> text writer produce la stessa riga leggibile
```

## Relazione con Event Model v0

Backend API v0 non definisce un secondo modello eventi. Usa Event Model v0.

Un backend puo' emettere questi layer:

| Layer | Backend puo' emetterlo? | Note |
| --- | --- | --- |
| `backend_observed` | si | fatto nativo osservato |
| `normalized_raw` | si | fatto Alfred prima del core |
| `diagnostic` | si | watch, recovery, errori |
| `trace` | futuro | tracepoint Lab/profiling |
| `security` | futuro/solo backend capaci | audit/eBPF/fanotify possono contribuire |
| `semantic` | no, salvo componenti core | la semantica finale resta del core |

Questa regola mantiene il core backend-agnostic e impedisce a un backend di
inventare `FILE_RELOCATED` o `DIR_RENAMED` senza passare dal correlatore.

## Link statico prima, plugin dinamici dopo

Backend API v0 deve essere progettata per poter diventare ABI in futuro, ma la
prima implementazione resta statica.

Fase 1:

```text
alfred binary
-> backend inotify compilato dentro
-> futuri backend statici selezionati a build/config
```

Fase 2:

```text
alfred-backend-inotify.so
alfred-backend-fanotify.so
alfred-backend-ebpf.so
```

La fase 2 richiedera' un documento separato su:

- ABI stability;
- `dlopen()`;
- entrypoint plugin;
- compatibilita' versione;
- ownership memoria attraverso confine dinamico;
- sicurezza del caricamento;
- firma/trust dei plugin.

Non va anticipata nella prima implementazione.

## Roadmap implementativa consigliata

1. Usare questa specifica come riferimento del refactor.
2. Introdurre una rappresentazione C minima di `alfred_record_t`.
   Fatto in `core/include/alfred_record.h`.
3. Scrivere adapter da `alfred_raw_event_t` ad `alfred_record_t`.
   Fatto per il raw normalizzato in `core/include/alfred_record_adapter.h` e
   `core/src/alfred_record_adapter.c`.
   Nello stesso adapter esiste anche `alfred_record_from_event()`, che converte
   `alfred_event_t` semantici in record `semantic + filesystem + FILE_*|DIR_*`.
4. Scrivere builder diagnostici per `WATCH_*`.
   Fatto per i tipi watch/recovery principali in
   `core/include/alfred_record_diagnostic.h` e
   `core/src/alfred_record_diagnostic.c`.
5. Aggiungere un text writer che produca le righe correnti da record.
   Fatto come formatter di payload in `core/include/alfred_record_text.h` e
   `core/src/alfred_record_text.c`.
6. Migrare gradualmente il backend inotify a `emit(record)`.
   Primi micro-step fatti per `WATCH_ADDED`, `WATCH_REMOVED` e `WATCH_STALE`:
   il runtime usa il builder diagnostico e il formatter testuale, poi passa il
   payload a `logger_event()`.
7. Solo dopo progettare JSONL writer.
8. Solo dopo progettare backend statici ulteriori.
9. Solo dopo valutare plugin dinamici.

`core/include/alfred_record.h` e' volutamente un header di contratto. Non
contiene logica runtime e non cambia ancora il comportamento di Alfred. Ogni
campo e' commentato nel codice in inglese per chiarire:

- significato del campo
- layer o record in cui il campo e' utile
- regola di ownership per le stringhe
- relazione con raw event, evento semantico, diagnostica watch e recovery

Questa scelta permette agli studenti di leggere il modello dati direttamente nel
codice prima di vedere gli adapter e i writer.

L'adapter `alfred_record_from_raw()` produce record
`normalized_raw + filesystem + RAW_*`. Non usa tipi semantici `FILE_*` o
`DIR_*`, perche' la semantica resta responsabilita' del core. Per esempio un
`ALFRED_RAW_MOVED_FROM` diventa `RAW_MOVED_FROM`, non `FILE_MOVED`.

Il builder `alfred_record_build_watch_diagnostic()` produce record
`diagnostic + watch` o `diagnostic + recovery` a partire dai tipi `WATCH_*`
gia' presenti in `alfred_record_type_t`. I primi usi runtime sono
`WATCH_ADDED`, `WATCH_REMOVED` e `WATCH_STALE`: `watch_manager_add()`,
`watch_manager_remove()` e gli handler `_SELF`/`IN_UNMOUNT` costruiscono il
record, lo formattano come payload testuale e lo passano al logger esistente.
Gli altri `WATCH_*` non sono ancora migrati e continuano a usare
`logger_event()` diretto.

Il formatter `alfred_record_format_text()` produce solo il payload testuale del
record, per esempio `FILE_CREATED path=...` o `WATCH_STALE wd=...`. Non scrive
timestamp, livello log, newline o file: queste responsabilita' restano del
logger corrente o di futuri output device. Questo rende possibile riusare lo
stesso formatter per `events.log`, test, debug e confronti di compatibilita'.

## Pipeline C introdotta finora

La cosa importante da capire e' che `alfred_record_t` non sostituisce ancora il
runtime. Per ora abbiamo costruito i pezzi che permetteranno la migrazione:

- un tipo dati comune: `alfred_record_t`
- un adapter dai raw event correnti: `alfred_record_from_raw()`
- un builder per diagnostica watch/recovery:
  `alfred_record_build_watch_diagnostic()`
- un formatter testuale di payload: `alfred_record_format_text()`

Non esiste una struttura separata chiamata `alfred_record_diagnostic_t`.
La diagnostica usa sempre `alfred_record_t`, con:

```text
layer    = diagnostic
category = watch oppure recovery
type     = WATCH_*
watch    = payload diagnostico con wd/state/reason/error
```

Schema dei passaggi:

```mermaid
flowchart TD
    I[Kernel inotify event] --> IA[inotify_adapter_build_raw]
    IA --> RAW[alfred_raw_event_t]
    RAW --> RA[alfred_record_from_raw]
    RA --> RR[alfred_record_t<br/>normalized_raw / filesystem / RAW_*]

    WD[WATCH_* diagnostic fact] --> DB[alfred_record_build_watch_diagnostic]
    DB --> DR[alfred_record_t<br/>diagnostic / watch|recovery / WATCH_*]

    CE[alfred_event_t<br/>semantic core event] --> SA[alfred_record_from_event]
    SA --> SR[alfred_record_t<br/>semantic / filesystem / FILE_*|DIR_*]

    RR --> TW[alfred_record_format_text]
    DR --> TW
    SR -. futuro .-> TW

    TW --> TXT[text payload<br/>senza timestamp o newline]
    TXT --> LG[logger / events.log]

    RR -. futuro .-> JSON[JSONL writer]
    DR -. futuro .-> JSON
    SR -. futuro .-> JSON

    RR -. futuro .-> BIN[binary/socket writer]
    DR -. futuro .-> BIN
    SR -. futuro .-> BIN
```

Lettura passo per passo:

1. Il backend inotify riceve un evento kernel e oggi costruisce un
   `alfred_raw_event_t`.
2. `alfred_record_from_raw()` prende quel raw event e produce un
   `alfred_record_t` con layer `normalized_raw`. Questo record conserva
   `source`, `raw_mask`, `cookie`, `pid`, timestamp e path borrowed.
3. Quando Alfred produce una diagnostica `WATCH_*`, il builder
   `alfred_record_build_watch_diagnostic()` costruisce un `alfred_record_t`
   con layer `diagnostic`. La category distingue diagnostica di watch da
   diagnostica di recovery.
4. Il core continua a emettere `alfred_event_t`, ma `core_logger_on_event()`
   ora lo converte con `alfred_record_from_event()` prima di formattare il
   payload testuale.
5. `alfred_record_format_text()` prende un record e produce solo la parte
   testuale leggibile, per esempio:

   ```text
   FILE_CREATED path=/tmp/root/a.txt
   WATCH_STALE wd=7 path=/tmp/root reason=IN_MOVE_SELF
   RAW_CREATE path=/tmp/root/dir mask=257
   ```

6. Il formatter non apre file e non scrive log. Il logger resta il proprietario
   di timestamp, livello log, newline e `FILE *`.
7. La stessa struttura potra' alimentare piu' output: testo, JSONL,
   MessagePack, protobuf o socket binaria. Il punto chiave e' non fare parsing
   del testo per ottenere dati strutturati.

Stato attuale: il lato output semantico del core usa gia' record + formatter
per produrre lo stesso payload testuale di prima. Nel backend inotify,
`WATCH_ADDED`, `WATCH_REMOVED` e `WATCH_STALE` usano gia' builder diagnostico e
formatter testuale, ma non esiste ancora un sink comune `emit(record)`. Raw
event e altri log diagnostici usano ancora il percorso runtime corrente.

## Test futuri

Quando la API verra' implementata, servira' coprire:

- lifecycle `init/start/stop/destroy`;
- `add_target` e `remove_target` su root valide e invalide;
- capabilities dichiarate dal backend inotify;
- mapping `alfred_raw_event_t -> alfred_record_t`;
- mapping `WATCH_* -> diagnostic record`;
- writer testuale che preserva le righe log correnti;
- nessun cambiamento nella semantica core;
- nessuna emissione diretta di eventi `FILE_*` / `DIR_*` dal backend.

## Decisioni rimandate

Restano da decidere nella fase codice:

- forma concreta di `alfred_backend_t`;
- se il runtime backend sara' allocato dall'app o dal backend;
- layout concreto di `alfred_backend_config_t`;
- layout concreto di `alfred_backend_target_t`;
- semantica precisa di `timeout_ms` in `poll`;
- elenco definitivo dei codici errore strutturati;
- come collegare piu' backend contemporanei;
- come gestire backpressure se `emit()` fallisce;
- quando introdurre `list_targets`;
- come versionare una futura ABI dinamica.

La decisione non rimandata e': Backend API v0 deve usare Event Model v0 come
contratto record, non stringhe testuali e non JSONL come modello primario.
