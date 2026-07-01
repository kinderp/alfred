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
I backend non aspettano writer o I/O lento nel percorso caldo target.
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

## Responsabilita' dei moduli

La regola piu' importante della Backend API v0 non e' solo "quale funzione
chiamare", ma "dove puo' vivere una responsabilita'". Senza questo confine,
Alfred rischia di diventare un insieme di scorciatoie: backend che decidono
semantica, writer che interpretano eventi, core che conosce dettagli kernel o
JSONL usato come API interna.

| Modulo | Puo' fare | Non deve fare |
| --- | --- | --- |
| Backend | leggere eventi OS, gestire stato sorgente, preservare dettagli nativi, emettere record ammessi | decidere semantica finale, generare JSONL, applicare policy Agent Guard |
| Adapter | convertire strutture esistenti in `alfred_record_t`, mappare campi senza perdere evidenza | interpretare comportamenti complessi o modificare il significato degli eventi |
| Core | correlare raw event, produrre semantica stabile, restare backend-agnostic | scrivere output specifici o dipendere da `wd`, `mask`, file descriptor o API kernel |
| Dispatcher/sink | ricevere record e indirizzarli ai consumer configurati | cambiare il significato del record o nascondere errori di emissione |
| Writer testuale | serializzare record in payload leggibile compatibile con i log correnti | decidere policy, ricostruire dati facendo parsing di stringhe o possedere stato backend |
| Writer JSONL/binario | serializzare record strutturati per integrazioni e performance | diventare il modello dati primario o sostituire `alfred_record_t` |
| Policy/security futuro | decidere allow, warn, approval, would-block, block | vivere dentro backend, adapter o writer |
| Lab/tooling | osservare record, visualizzare pipeline, aiutare debug e didattica | diventare dipendenza necessaria del core runtime |

Questa tabella e' una checklist architetturale. Ogni volta che una modifica
sembra comoda ma attraversa una colonna "Non deve fare", bisogna fermarsi e
discutere se serve un ponte temporaneo documentato o una diversa divisione del
codice.

Per la parte output leggere anche [Writer API v0](32-writer-api-v0.md). Backend
API v0 definisce come i backend producono record; Writer API v0 definisce come
quei record vengono serializzati o trasportati fuori dal percorso caldo.

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

Il primo passo runtime e' gia' stato applicato a `WATCH_ADDED`,
`WATCH_REMOVED` e `WATCH_STALE` attraverso il sink comune:

```text
watch_manager_add() / watch_manager_remove()
-> alfred_record_build_watch_diagnostic(WATCH_ADDED | WATCH_REMOVED)
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> alfred_record_format_text()
-> logger_event("WATCH_ADDED wd=N path=P")
-> logger_event("WATCH_REMOVED wd=N path=P")

backend_handle_move_self/delete_self/unmount()
-> alfred_record_build_watch_diagnostic(WATCH_STALE, reason=R)
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> alfred_record_format_text()
-> logger_event("WATCH_STALE wd=N path=P reason=R")
```

Non e' ancora la Backend API completa, perche' `inotify_backend.c` scrive ancora
molti diagnostici sul logger esistente e il raw path runtime resta basato su
`alfred_raw_event_t`. Pero' il dato diagnostico non nasce piu' solo come
stringa: nasce come `alfred_record_t` e, per
`WATCH_ADDED`/`WATCH_REMOVED`/`WATCH_STALE`, attraversa gia' il confine
`emit(record)` prima del payload testuale compatibile.

## Tipi concettuali

Questi tipi descrivono la direzione della API. La prima forma C dello
scheletro operations esiste ora in:

```text
core/include/alfred_backend_ops.h
core/src/alfred_backend_ops.c
```

Questo non significa che il runtime inotify sia gia' migrato alla nuova API.
Significa che il contratto minimo e' compilabile e testabile: nome backend,
versione API, capabilities, emit boundary e callback lifecycle possono essere
validati prima di collegare `app.c` a un backend generico.

```c
typedef struct alfred_backend alfred_backend_t;
typedef struct alfred_backend_config alfred_backend_config_t;
typedef struct alfred_record alfred_record_t;
```

`alfred_backend_t` e' runtime opaco del backend. Il chiamante non deve conoscere
campi interni come file descriptor, watcher table, mappe kernel o code di
recovery.

`alfred_backend_config_t` contiene una vista stabile della configurazione
necessaria al backend. I campi specifici del backend devono stare in una sezione
backend-specifica, non nel core.

`alfred_backend_target_t` rappresenta cosa osservare. Nel codice v0 esiste gia'
come struct concreta minimale per target filesystem-path: contiene un path
borrowed, un tipo target, flags e un puntatore a opzioni backend-specifiche
ancora non supportate. In futuro potra' rappresentare anche target non
filesystem.

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
- il backend non conserva il puntatore `alfred_backend_emit_t *`;
- il backend puo' copiare nel proprio runtime i valori `emit->emit` e
  `emit->userdata`;
- il backend non possiede e non libera `userdata`;
- il chiamante deve mantenere `userdata` valido finche' il backend puo'
  emettere record, normalmente fino a `stop()`/`destroy()`;
- il backend non decide quale writer ricevera' il record;
- l'app dispatcher decide se inviare il record al core, al log testuale, a JSONL
  o ad altri consumer.

Questa distinzione e' fondamentale per evitare dangling pointer.
`init()` riceve un puntatore a una piccola "busta" caller-owned:

```c
alfred_backend_emit_t emit = {
    .emit = app_emit_record,
    .userdata = &app
};

backend->ops->init(backend, config, &emit);
```

Il backend non deve salvare l'indirizzo della busta:

```c
backend->emit = emit; /* sbagliato: conserva un puntatore esterno */
```

Se `emit` era una variabile locale del chiamante, dopo il ritorno della funzione
quell'indirizzo puo' non essere piu' valido. Una futura chiamata a `poll()`
potrebbe quindi dereferenziare memoria scaduta.

Il backend deve invece copiare i valori contenuti nella busta:

```c
backend->emit_fn = emit->emit;
backend->emit_userdata = emit->userdata;
```

In questo modo il backend conserva la function pointer e il valore opaque
`userdata`, ma non dipende dalla durata della variabile locale
`alfred_backend_emit_t`. `userdata` resta comunque caller-owned: il backend lo
passa alla callback, ma non lo libera e non ne decide la durata.

## Percorso caldo e I/O

Il percorso caldo target non e':

```text
backend
-> writer JSONL/text/protobuf/socket
-> I/O
```

Il percorso caldo target e':

```text
evento OS
-> backend
-> normalizzazione minima
-> alfred_record_t
-> enqueue su coda/ring buffer
```

Tutto il resto deve stare fuori dal percorso caldo:

- formatter testuale;
- JSONL;
- protobuf;
- MessagePack;
- socket TCP o Unix socket;
- file flush;
- dashboard e Alfred Lab;
- report;
- policy pesante;
- plugin writer lenti.

I bridge sincroni correnti verso `logger_raw()`, `logger_event()` e
`logger_error()` sono ponti di migrazione per conservare compatibilita' e test
mentre spostiamo il contratto su `alfred_record_t`. Non sono il disegno finale
ad alte prestazioni.

La regola architetturale da mantenere e':

```text
Il backend non aspetta il writer.
```

Questa scelta sostituisce progressivamente due confini correnti:

```text
callback raw: const alfred_raw_event_t *
logger_event("WATCH_STALE ...")
```

con un confine unico:

```text
emit(const alfred_record_t *record)
```

Il prossimo micro-step tecnico e' introdurre questo sink comune senza cambiare
ancora il comportamento osservabile. Il primo sink puo' essere un writer
testuale compatibile che chiama `alfred_record_format_text()` e poi usa il
logger esistente. In questo modo:

- i log restano uguali;
- i test correnti possono continuare a confrontare lo stesso testo;
- il backend non sceglie piu' direttamente il dispositivo di output;
- JSONL potra' essere aggiunto dopo come secondo writer, non come sostituto del
  modello interno.

Nel codice corrente questo concetto e' stato introdotto in forma minima e
indipendente dal runtime backend:

```text
core/include/alfred_record_sink.h
core/src/alfred_record_sink.c
```

`alfred_record_sink_t` espone una callback `emit(record)` generica e
`alfred_record_sink_emit()` ne fa un wrapper difensivo. Il sink non possiede il
record: riceve un `alfred_record_t` borrowed valido solo durante la chiamata.

Il primo writer compatibile e':

```text
core/include/alfred_record_text_sink.h
core/src/alfred_record_text_sink.c
```

Il text sink non dipende da `logger_t`. Riceve un buffer caller-owned, formatta
il record con `alfred_record_format_text()` e chiama una callback
`write(payload)`. Nel runtime questa callback potra' essere un ponte verso
`logger_event()`; nei test e in output futuri puo' essere un collector, un file,
un socket o altro. Questa scelta evita di portare il logger app dentro il core.

## Decisioni critiche prima di stabilizzare `emit(record)`

Prima di considerare stabile il contratto Backend API v0, queste decisioni
devono essere esplicite. Alcune sono gia' implementate in forma transitoria,
altre sono ancora contratto documentale.

### 1. Ownership memoria

Il backend puo' costruire un `alfred_record_t` leggero con stringhe borrowed
solo se il record viene consumato subito. Appena `emit(record)` inserira' il
record in una coda o lo consegnera' a un dispatcher thread, il runtime dovra'
creare una copia owned.

Regola:

```text
Il record accodato non puo' dipendere da stack frame, buffer temporanei o
memoria riusabile del backend.
```

La forma C precisa e' rimandata: potra' essere un tipo
`alfred_owned_record_t` o una coppia di funzioni
`alfred_record_clone_owned()` / `alfred_record_destroy_owned()`.

### 2. Dispatcher e coda

`emit(record)` non deve significare "scrivi log". Deve significare "consegna un
fatto strutturato al runtime Alfred". Il runtime decidera' poi se inviare quel
record al core, al writer testuale, al writer JSONL, a un writer binario, ad
Alfred Lab o a un livello policy futuro.

Schema target:

```text
backend/adapter -> alfred_record_t -> emit(record) -> queue -> dispatcher
```

I bridge sincroni attuali sono accettati solo come migrazione controllata.

### 3. Drop, backpressure e record di perdita

Quando esiste una coda, puo' riempirsi. Quando esistono writer, possono essere
lenti. Alfred non deve perdere eventi in silenzio.

Categorie diagnostiche da prevedere:

| Caso | Significato |
| --- | --- |
| queue overflow | il runtime non riesce ad accodare tutti i record |
| sink drop | un writer best-effort scarta record |
| backend overflow | la sorgente OS segnala perdita, per esempio inotify `IN_Q_OVERFLOW` |
| sink disabled | un writer viene disattivato per errore o lentezza |
| dispatcher backpressure | il dispatcher non consuma abbastanza velocemente |

La policy concreta puo' cambiare per sink critici, best-effort e debug, ma la
perdita deve diventare osservabile tramite record diagnostico o contatore
esposto.

### 4. Writer lifecycle e registry statico

I writer dovranno essere plugin-like, ma inizialmente statici. Il backend non
deve chiamare `jsonl_writer_write()`, `fprintf()`, `fflush()` o `socket_send()`
nel percorso caldo. Il dispatcher inviera' record a writer registrati.

Il lifecycle minimo di un writer e':

```text
init/open -> write o write_batch -> flush -> close -> destroy
```

La API dettagliata e' descritta in [Writer API v0](32-writer-api-v0.md).

### 5. Backend lifecycle e capabilities

Il core non deve assumere che ogni backend sappia fare le stesse cose. Inotify
puo' osservare eventi filesystem e cookie di move, ma non identifica
nativamente l'agente e non blocca accessi. Fanotify potra' supportare alcune
forme di enforcement filesystem. eBPF potra' contribuire con processo, rete e
syscall.

Per questo ogni backend deve dichiarare capabilities invece di lasciare che il
core deduca il comportamento dal nome del backend.

Nel codice C questo primo pezzo del contratto esiste come metadata statico:

```text
core/include/alfred_backend_capabilities.h
core/src/alfred_backend_capabilities.c
modules/inotify/src/inotify_backend_capabilities.c
```

`alfred_backend_capabilities_t` contiene:

| Campo | Significato |
| --- | --- |
| `backend_name` | nome stabile del backend, per esempio `inotify` |
| `api_version` | versione della Backend API supportata; per v0 vale `0` |
| `flags` | bitmask delle capability dichiarate dal backend |

La funzione `alfred_backend_capabilities_has()` verifica una sola capability
alla volta. Se riceve `NULL`, `0` o piu' bit insieme ritorna `0`. Questa scelta
rende esplicita la domanda del chiamante: "questo backend ha proprio questa
capability?", non "ha almeno una capability fra queste?".

Il backend inotify espone:

```c
const alfred_backend_capabilities_t *inotify_backend_capabilities(void);
```

Il puntatore restituito e' borrowed e punta a metadata statico con durata pari
al processo. Non va liberato. Questa funzione non appartiene al percorso caldo
degli eventi: descrive il backend durante wiring, test, diagnostica o futura
selezione backend. Non aggiunge allocazioni, I/O, lock o dispatch per evento.

### 6. Contesto futuro agente/processo/policy

Backend API v0 resta system-event-first, ma non deve impedire Agent Guard. Il
record potra' essere arricchito in futuro con `agent_session_id`, workspace,
process tree, decisione policy e sensitivity. Questi campi non sono
responsabilita' del backend inotify se il backend non ha fonti affidabili.

Regola:

```text
Il backend osserva fatti OS.
Un livello superiore collega quei fatti ad agente, processo, workspace e
policy.
```

### Normalizzazione, semantica e correlazione

Il backend deve fermarsi alla normalizzazione minima:

```text
evento OS/backend nativo
-> fatto raw Alfred o record normalized_raw
```

Per inotify questo include tradurre `IN_CREATE`, `IN_DELETE`, `IN_MOVED_FROM`,
`IN_MOVED_TO` e altri flag in tipi raw Alfred, costruire il path completo e
preservare dettagli come `mask`, `cookie`, `wd` e `IN_ISDIR`.

Il backend non deve decidere la semantica finale:

```text
RAW_CREATE + ISDIR -> DIR_CREATED
RAW_MOVED_FROM + RAW_MOVED_TO -> FILE_RENAMED / DIR_RELOCATED
```

Queste trasformazioni appartengono al core semantico. Allo stesso modo, la
correlazione fra backend diversi non deve vivere nel singolo backend. Se in
futuro inotify segnala una modifica, fanotify fornisce il processo e eBPF
fornisce l'albero di processi, il merge appartiene a un livello centrale di
correlazione/enrichment.

Regola sintetica:

```text
I backend osservano e normalizzano.
Il core semantico interpreta per dominio.
Il correlation engine collega fonti diverse.
Il policy engine decide.
I writer serializzano.
```

## Operazioni Backend API v0

```c
typedef struct {
    const char *name;
    uint32_t api_version;
    const alfred_backend_capabilities_t *capabilities;

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

Nel codice corrente lo scheletro vive in
`core/include/alfred_backend_ops.h`. Il campo `capabilities` non e' piu' una
semplice bitmask locale: punta a `alfred_backend_capabilities_t`, cioe' allo
stesso descriptor statico introdotto per dichiarare cosa un backend sa
osservare o controllare. Questa scelta evita due sorgenti di verita' diverse:
il nome, la versione API e le capability del backend devono restare coerenti.

Il helper:

```c
int alfred_backend_ops_is_minimally_valid(
    const alfred_backend_ops_t *ops
);
```

valida solo metadata statici e callback richieste. Non avvia backend, non
registra target e non viene chiamato nel percorso caldo degli eventi. Serve per
test, wiring futuro e registry statico.

Il contratto positivo e':

- `ops` non sia `NULL`;
- `name` sia presente e non vuoto;
- `api_version` sia `ALFRED_BACKEND_API_VERSION_V0`;
- `capabilities` sia presente;
- `capabilities->backend_name` sia presente e non vuoto;
- `ops->name` e `capabilities->backend_name` coincidano;
- `ops->api_version` e `capabilities->api_version` coincidano;
- le capability non siano vuote;
- tutte le callback lifecycle siano valorizzate.

Di conseguenza il validatore deve rifiutare esplicitamente questi casi:

| Caso rifiutato | Perche' non e' valido |
| --- | --- |
| `ops == NULL` | non esiste nessun descriptor da registrare |
| `ops->name == NULL` | il backend non ha identita' stabile |
| `ops->name` vuoto | il backend ha un'identita' non usabile |
| `ops->api_version != ALFRED_BACKEND_API_VERSION_V0` | il chiamante non puo' assumere il contratto v0 |
| `ops->capabilities == NULL` | mancano le capability del backend |
| `ops->capabilities->backend_name == NULL` | il descriptor capabilities non identifica il backend |
| `ops->capabilities->backend_name` vuoto | il descriptor capabilities ha un'identita' non usabile |
| `ops->capabilities->backend_name != ops->name` | ops table e capabilities descrivono backend diversi o incoerenti |
| `ops->capabilities->api_version != ops->api_version` | metadata e ops table non parlano la stessa versione API |
| `ops->capabilities->flags == 0` | il backend non dichiara nessuna capability osservabile o controllabile |
| una callback lifecycle e' `NULL` | il runtime non puo' chiamare il lifecycle in modo prevedibile |

Questa tabella non e' solo una nota di test: e' parte del contratto Backend API
v0. Se un caso viene rifiutato dal validatore, deve essere documentato qui, nei
test e, quando rilevante per utenti o tooling, nelle pagine man.

Per v0 tutte le callback sono obbligatorie anche se alcune implementazioni,
come `start` e `stop` per inotify, potranno essere no-op. Questo rende il
contratto piu' esplicito: un backend non lascia buchi nella tabella ops, ma
dichiara deliberatamente che una fase non richiede lavoro.

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

Descriptor statico di funzionalita' dichiarate dal backend. Le capabilities
non dicono che una feature e' attiva nella configurazione corrente: dicono che
il backend e' capace di offrirla. Il puntatore e' borrowed: punta a metadata
statico del backend e non deve essere liberato dal chiamante.

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
inotify_backend_remove_startup_watch()
watch_manager_remove()
watcher_remove()
```

Il backend deve emettere diagnostica strutturata quando smette di osservare un
target, per esempio `diagnostic + watch + WATCH_REMOVED`.

Nel sottoinsieme v0 implementato per inotify, `remove_target` riceve lo stesso
target filesystem-path usato da `add_target`. Il path e' borrowed solo durante
la chiamata. Il path deve corrispondere esattamente a una root configurata da
`add_target`: `remove_target` e' gestione dei target API, non rimozione
arbitraria di watch descriptor interni. Questo impedisce a un chiamante di
rimuovere direttamente un watch figlio creato dalla ricorsivita' di un target
padre e di lasciare incompleta la copertura ricorsiva del padre. Dopo questa
validazione, l'adapter inotify cerca i watch attivi associati a quel path e
delega ogni rimozione a `watch_manager_remove()`, cosi' la diagnostica
`WATCH_REMOVED` resta nello stesso punto del codice.

Poiche' `recursive` e' abilitato di default, una rimozione del target root non
puo' lasciare attivi i watch figli: quando la configurazione e' ricorsiva,
`remove_target` rimuove anche i watch sotto quel path usando una regola di
prefisso con separatore `/`. Quindi `/tmp/root-old` non e' considerato figlio di
`/tmp/root`. La root del filesystem `/` e' un caso speciale intenzionale: se e'
un target osservato ricorsivamente, tutti i watch con path assoluto appartengono
al suo sottoalbero e devono essere rimossi insieme alla root.

Il helper che raccoglie i watch descriptor per prefisso deve avere un contratto
di output semplice: i descriptor raccolti e `count` sono validi solo quando la
funzione restituisce successo. Se la raccolta fallisce, per input invalido o
capacita' insufficiente dell'array di output, `count` viene azzerato. In questo
modo `remove_target` non rischia di usare un conteggio vecchio o un risultato
parziale dopo un errore.

`configured_roots` e' il registro dei target API configurati; la watcher table
e' invece stato operativo dei watch kernel attivi. Questi due livelli possono
divergere temporaneamente: per esempio un `IN_IGNORED` puo' far sparire un
watch attivo mentre la root resta ancora un target configurato. Per questo
l'autorita' per `remove_target` e' la root configurata esatta, non l'esistenza
istantanea di un watch attivo. Se non ci sono piu' watch associati a quel
target, `remove_target` rimuove comunque la root da `configured_roots` e
restituisce `ERR_OK`.

Una volta raccolti eventuali watch descriptor da rimuovere, `remove_target`
completa la pulizia dello stato target anche se una diagnostica `WATCH_REMOVED`
fallisce. Questo dettaglio e' importante perche' `watch_manager_remove()`
rimuove il watch dal kernel e dalla watcher table prima di emettere il record
diagnostico: un fallimento del callback segnala un problema di
output/fail-closed, ma non deve lasciare `configured_roots` e watcher table
fuori sincronizzazione. Per questo il backend ricorda il primo errore, continua
a rimuovere i watch raccolti, rimuove la root configurata esatta e poi
restituisce l'errore al chiamante.

Il duplicato esatto e' idempotente in tutte le modalita' inotify v0:
richiamare `add_target(path)` con un path gia' configurato restituisce `ERR_OK`,
non reinstalla watch e non emette un secondo `WATCH_ADDED`. Questa e' una
regola di target management della Backend API, non un dettaglio della modalita'
ricorsiva. L'idempotenza e' basata sul registro `configured_roots`: se il target
e' ancora configurato ma i watch attivi sono gia' spariti, `add_target(path)`
continua a restituire `ERR_OK` senza ripristinare automaticamente la copertura
kernel. Questo `ERR_OK` significa "target API gia' registrato", non "watch
attivi reinstallati". Per forzare una reinstallazione il chiamante deve prima
eseguire `remove_target(path)` e poi chiamare di nuovo `add_target(path)`.

Backend API v0 rifiuta invece target ricorsivi sovrapposti per inotify. Per
esempio, se `/tmp/root` e' gia' configurato, aggiungere `/tmp/root/child`
fallisce; se `/tmp/root/child` e' gia' configurato, aggiungere `/tmp/root`
fallisce. Questa scelta evita uno stato ambiguo in cui un watch figlio
appartiene sia al target padre sia al target figlio: Alfred non ha ancora
refcount o ownership esplicita dei watch. Path con prefisso testuale simile ma
fuori subtree, come `/tmp/root` e `/tmp/root-old`, non sono considerati
sovrapposti.

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
#define ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH 1u
#define ALFRED_BACKEND_TARGET_FLAG_NONE 0u

typedef struct alfred_backend_target {
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
| `target_type` | per ora `ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH` |
| `flags` | per ora solo `ALFRED_BACKEND_TARGET_FLAG_NONE` |
| `backend_options` | opzioni specifiche future, per ora rifiutate se non `NULL` |

Decisione v0: non rendere subito generico tutto il mondo. Il target iniziale
puo' essere filesystem-oriented, ma deve lasciare spazio a target futuri non
filesystem.

Il path e' borrowed: il chiamante deve tenerlo valido solo durante l'operazione
backend che riceve il target, oggi `add_target()` o `remove_target()`. Se il
backend deve conservarlo dopo il ritorno della chiamata, deve copiarlo nel
proprio stato interno e non deve salvare direttamente il puntatore posseduto dal
chiamante. Il backend inotify lo fa gia' nel watch manager, che copia i path
nella tabella dei watch, e in `configured_roots`, che conserva copie proprie
delle root configurate.

Nel sottoinsieme inotify v0, il path deve avere lunghezza maggiore di zero e
minore di `PATH_MAX`. Il backend conserva configured roots e watcher path in
buffer fissi `PATH_MAX`, quindi un path con `strlen(path) >= PATH_MAX` non puo'
essere rappresentato con terminatore `NUL` dentro lo storage corrente. Questo
caso e' un errore di validazione target (`ERR_INVALID_ARG`), non un errore di
allocazione (`ERR_ALLOC`). `ERR_ALLOC` resta riservato a fallimenti reali di
memoria per target altrimenti validi.

Il target filesystem v0 usa identita' lessicale ristretta, non una
canonicalizzazione completa. Questo significa che il backend confronta i path
come stringhe dopo aver applicato poche regole di validazione. Per evitare
alias evidenti dello stesso target, i path con slash finale sono rifiutati con
`ERR_INVALID_ARG`, tranne la root del filesystem `/`. Quindi `/tmp/root` e'
valido, `/tmp/root/` e' invalido, `/` resta valido. Alfred non risolve ancora
in modo generale symlink, `..`, mount boundary, path relativi o regole
cross-platform: quel livello di normalizzazione dovra' essere progettato come
passo separato.

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
| `audit_events` | no: l'opt-in corrente e' raw-log-only |
| `metadata_events` | raw-only tramite `IN_ATTRIB` |
| `self_events` | diagnostica backend |
| `overflow_events` | minimo: raw/core `OVERFLOW` |
| `identity_tracking` | si, con `st_dev` e `st_ino` |
| `lost_scope_recovery` | si, sincrona/batch limitato |
| `permission_events` | no |
| `process_context` | no affidabile con inotify |
| `network_context` | no |
| `can_block` | no |

Questa tabella e' ora bloccata anche da
`tests/backend/test_backend_capabilities.c`. Il test verifica due cose:

- il helper comune rifiuta input ambigui come `NULL`, `0` o maschere con piu'
  bit;
- inotify dichiara solo capability osservazionali filesystem che attraversano
  il confine API/record e non dichiara capability che non possiede, come audit
  API-level, processo, rete, permission events o block.

Questo e' un primo passo deliberatamente piccolo. Non cambia
`inotify_backend_poll()` e non aggiunge fanotify/eBPF. Serve a rendere
verificabile una domanda fondamentale: "cosa puo' davvero fare questo
backend?".

Il passo successivo nello stesso filone introduce
`alfred_backend_ops_t` come scheletro C compilabile. Anche questo resta un
contratto, non un cambio runtime: nessun evento passa ancora attraverso
`alfred_backend_ops_t`.

Nota su `audit_events`: il backend inotify ha gia' una configurazione
`inotify_audit_events` per osservare `IN_OPEN`, `IN_ACCESS` e
`IN_CLOSE_NOWRITE` nel raw log inotify. Questa visibilita' e' utile per misurare
rumore e comportamento del kernel, ma non e' ancora una capability Backend API
v0 perche' non produce record audit strutturati. Quando questi fatti
attraverseranno il confine `alfred_record_t`, il backend potra' dichiarare
`audit_events`.

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
| `os_error_code` | `2`, per Linux `errno` |
| `os_error_name` | `ENOENT` |
| `os_error_message` | `No such file or directory` |
| `path` | path coinvolto, se noto |
| `watch_id` | watch coinvolto, se noto |

Regola: il codice puo' restituire un codice compatto, ma il record diagnostico
deve spiegare il contesto.

`error` e i campi OS non sono la stessa cosa:

- `error` e' il token Alfred stabile che descrive la scelta della pipeline;
- `os_error_code` conserva il codice numerico del sistema operativo;
- `os_error_name` e `os_error_message` sono opzionali e servono per debug o
  output umano.

Per esempio un resync puo' usare:

```text
error=path-unreachable
os_error_code=2
os_error_name=ENOENT
os_error_message="No such file or directory"
```

Il backend non deve comprimere queste informazioni in una sola stringa se sta
emettendo un record strutturato. Il writer testuale potra' continuare a
stampare `errno=2 (No such file or directory)` per compatibilita', ma JSONL e
output binari dovranno esporre campi separati.

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
| `remove_target` | `inotify_backend_remove_startup_watch()` -> `watch_manager_remove()` e cleanup watcher |
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

Lo scheletro `alfred_backend_ops_t` permette di preparare questa migrazione
senza cambiare ancora il comportamento esterno. Il futuro adapter inotify verso
Backend API v0 dovra' riempire la tabella ops con:

- `name = "inotify"`;
- `api_version = ALFRED_BACKEND_API_VERSION_V0`;
- `capabilities = inotify_backend_capabilities()`;
- callback verso le funzioni inotify correnti o wrapper sottili;
- no-op espliciti per lifecycle non ancora necessari.

Fino a quel momento `app.c` continua a chiamare direttamente il backend inotify
corrente.

Il primo passo concreto esiste ora in due livelli:

```text
modules/inotify/src/inotify_backend_ops.c
tests/backend/test_backend_inotify_ops.c
```

`inotify_backend_ops()` restituisce una tabella `alfred_backend_ops_t` con nome
`inotify`, versione Backend API v0 e puntatore allo stesso descriptor statico
di `inotify_backend_capabilities()`. Il test verifica che questa tabella passi
`alfred_backend_ops_is_minimally_valid()`.

Il descriptor capabilities resta privato al file che lo definisce. La tabella
ops lo ottiene passando dall'accessor pubblico `inotify_backend_capabilities()`,
non da un simbolo globale condiviso. Questo mantiene una sola porta ufficiale
per leggere le capabilities inotify ed evita di creare una API C implicita.

`init`, `destroy`, `add_target` e `remove_target` sono il primo pezzo reale
della tabella ops inotify. Per non trasformare subito `app.c`, il modulo espone
due tipi concreti:

```c
inotify_backend_ops_runtime_t
inotify_backend_ops_config_t
```

La Backend API v0 resta opaca (`alfred_backend_t` e
`alfred_backend_config_t`), quindi il test e il futuro composition root passano
questi oggetti concreti con un cast. Il runtime concreto deve essere azzerato
prima del primo `init`. `init` costruisce il vecchio `inotify_backend_context_t`
interno, copia function pointer e `userdata` dell'emit boundary, poi chiama il
percorso esistente `inotify_backend_init()`. `destroy` chiama
`inotify_backend_shutdown()` solo se `init` e' riuscito, azzera i puntatori
borrowed del contesto e torna a uno stato distrutto riutilizzabile.
`add_target` accetta solo target `ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH`
con path non vuoto, flags pari a `ALFRED_BACKEND_TARGET_FLAG_NONE` e
`backend_options == NULL`; poi delega al percorso esistente
`inotify_backend_add_startup_watch()`.
Se il path e' gia' una root configurata esatta, `add_target` e' idempotente sia
in modalita' ricorsiva sia non ricorsiva: non chiama di nuovo
`watch_manager_add()`/`watch_manager_add_recursive()`, non reinstalla watch e
non produce un nuovo `WATCH_ADDED`.
Il contratto v0 di `add_target` e' atomic-like rispetto alla gestione target:
se l'operazione restituisce errore, il target non deve restare visibile in
`configured_roots` e non devono restare watch nuovi installati per quel target.
Per questo il backend registra prima la root configurata e poi installa i
watch; se l'installazione fallisce, rimuove i watch parziali e annulla la root
registrata prima di restituire `ERR_INOTIFY`. Se invece la registrazione della
root fallisce con `ERR_ALLOC`, nessun watch e' ancora stato installato.
Questa regola e' importante per i chiamanti: un `add_target()` fallito non deve
significare "forse Alfred sta comunque osservando quel path".
`remove_target` accetta lo stesso sottoinsieme di target, rifiuta runtime
incoerenti prima di toccare la watch table e delega a
`inotify_backend_remove_startup_watch()`. In modalita' ricorsiva rimuove il
watch della root e i watch figli sotto quella root; in modalita' non ricorsiva
rimuove solo il path esatto. Il path deve essere una root configurata esatta:
un child watch creato dalla ricorsivita' del parent non puo' essere rimosso come
target autonomo se non e' stato aggiunto come target autonomo. Se la root
configurata esiste ma i watch attivi sono gia' spariti, la rimozione resta
valida: viene tolta la root esatta da `configured_roots` e il backend restituisce
`ERR_OK`. Dopo che la rimozione dei watch e' iniziata, un errore diagnostico
`WATCH_REMOVED` viene propagato al chiamante ma non interrompe la pulizia dello
stato target: i watch raccolti vengono comunque rimossi e la root esatta viene
tolta da `configured_roots`.

Questo passo non cambia il runtime normale di Alfred: `app.c` continua a
chiamare direttamente `inotify_backend_init()` e `inotify_backend_shutdown()`.
La tabella ops serve per provare il confine statico in modo incrementale.

Le callback `start`, `poll` e `stop` non sono ancora il runtime reale. Se
chiamate, falliscono con `ERR_INVALID_ARG` invece di fare finta di avviare o
leggere eventi. La migrazione di polling e `app.c` resta un passo successivo e
dovra' avere test propri.

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
6. Introdurre un sink comune `emit(record)` e un text sink compatibile.
   Fatto in `core/include/alfred_record_sink.h`,
   `core/src/alfred_record_sink.c`,
   `core/include/alfred_record_text_sink.h` e
   `core/src/alfred_record_text_sink.c`.
7. Migrare gradualmente il backend inotify a `emit(record)`.
   Primi micro-step fatti per `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`,
   per tutta la famiglia locale `WATCH_RESYNC_*` e per i diagnostici
   `WATCH_LOST_*`: il runtime usa record diagnostici, sink comune e text sink.
   `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE` e
   `WATCH_STALE_EVENT_DROPPED` usano anche il callback
   `inotify_backend_context_t.emit_record` per offrire lo stesso record
   diagnostico alla output pipeline JSONL quando `output_enabled=true`.
   Micro-step raw fatti per `RAW_CREATE`, `RAW_DELETE`, `RAW_ATTRIB`,
   `RAW_MODIFY`, `RAW_CLOSE_WRITE`, `RAW_MOVED_FROM`, `RAW_MOVED_TO` e
   `RAW_OVERFLOW`: `app.c` riceve `alfred_raw_event_t`, lo converte con
   `alfred_record_from_raw()`, lo emette al sink comune e scrive il payload
   compatibile su `raw.log`, poi consegna comunque il raw originale ad
   `alfred_process()`.
8. Progettare Writer API v0, coda/ring buffer e backpressure.
   Specifica documentale in [Writer API v0](32-writer-api-v0.md).
9. Solo dopo implementare JSONL come writer, non come API primaria.
10. Solo dopo progettare backend statici ulteriori.
11. Solo dopo valutare plugin dinamici backend o writer.

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
Il primo uso runtime copre `ALFRED_RAW_CREATE`, `ALFRED_RAW_DELETE`,
`ALFRED_RAW_ATTRIB`, `ALFRED_RAW_MODIFY`, `ALFRED_RAW_CLOSE_WRITE`,
`ALFRED_RAW_MOVED_FROM`, `ALFRED_RAW_MOVED_TO` e `ALFRED_RAW_OVERFLOW`:
`handle_backend_event()` in `app.c` costruisce record `RAW_*`, li passa al sink
comune e lascia al text sink produrre righe come:

```text
RAW_CREATE path=/tmp/root/a.txt mask=1
RAW_DELETE path=/tmp/root/a.txt mask=2
RAW_ATTRIB path=/tmp/root/a.txt mask=8
RAW_MODIFY path=/tmp/root/a.txt mask=4
RAW_CLOSE_WRITE path=/tmp/root/a.txt mask=16
RAW_MOVED_FROM path=/tmp/root/old.txt mask=32 cookie=42
RAW_MOVED_TO path=/tmp/root/new.txt mask=64 cookie=42
RAW_OVERFLOW path= mask=128
```

Dopo il log normalizzato, lo stesso `alfred_raw_event_t` continua a essere
passato ad `alfred_process()` per non cambiare la semantica del core.
I record `RAW_MOVED_FROM` e `RAW_MOVED_TO` restano raw facts separati: il cookie
viene preservato nel testo compatibile per rendere visibile la chiave di
correlazione, ma la scelta semantica finale resta del core.
`RAW_OVERFLOW` resta un raw fact globale: il path compatibile e' vuoto perche'
l'overflow descrive perdita di eventi sull'istanza inotify, non un singolo
oggetto filesystem.

Il builder `alfred_record_build_watch_diagnostic()` produce record
`diagnostic + watch` o `diagnostic + recovery` a partire dai tipi `WATCH_*`
gia' presenti in `alfred_record_type_t`. I primi usi runtime sono
`WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, la famiglia locale
`WATCH_RESYNC_*` e i diagnostici `WATCH_LOST_*`: il backend costruisce record
diagnostici, riempie gli eventuali campi recovery, passa il record al sink
comune e il text sink produce il payload testuale compatibile per il logger
esistente.

Per `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE` e
`WATCH_STALE_EVENT_DROPPED` il
`inotify_backend_context_t` contiene anche un callback opzionale `emit_record`.
Il watch manager o il backend lo chiamano dopo aver preservato `events.log`: il
record diagnostico resta borrowed, quindi la output pipeline deve clonarlo prima
di accodarlo. Questa scelta evita di far dipendere il backend da `app_t`, da
JSONL o da un writer specifico. Se il callback non e' configurato, il
comportamento resta identico al path compatibile.

`WATCH_STALE_EVENT_DROPPED` usa un builder dedicato,
`alfred_record_build_stale_event_dropped()`, perche' deve conservare campi
diversi da una normale transizione stale:

- `watch.event_mask`: mask testuale dell'evento kernel droppato;
- `watch.event_name`: nome figlio dell'evento kernel droppato.

Questi campi servono a dire "Alfred ha visto questo evento, ma non lo ha
inoltrato al core perche' il path era stale".

La scelta e' intenzionalmente limitata: `backend_log_watch_diagnostic_record()`
sa costruire anche molti `WATCH_RESYNC_*` e `WATCH_LOST_*`, ma quei record
hanno payload recovery piu' ricchi e in alcuni casi relazione con `errors.log`.
Restano quindi fuori da `output.jsonl` finche' non verranno migrati con test e
contratto dedicati.

Nel backend inotify esiste ora un helper locale,
`backend_log_watch_diagnostic_record()`, che centralizza il ponte provvisorio:

```text
campi diagnostici runtime
-> alfred_record_build_watch_diagnostic()
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> alfred_record_format_text()
-> logger_event()
```

Questo helper non e' la Backend API pubblica e non sostituisce `emit(record)`.
Serve a evitare duplicazione mentre migriamo gradualmente i diagnostici runtime.
Il percorso diretto `alfred_record_format_text() -> logger_event()` resta solo
fallback di compatibilita' se il sink bridge non riesce a inizializzarsi o a
emettere il record.
Per ora e' usato dai percorsi `WATCH_STALE`, `WATCH_RESYNC_FAILED`, dai record
di dettaglio del resync locale (`WATCH_RESYNC_SCAN_*`,
`WATCH_RESYNC_REINSTALLED`, `WATCH_RESYNC_ROLLBACK`) e dai diagnostici
lost-scope `WATCH_LOST_*`. I `WATCH_RESYNC_FAILED` che aggiungono anche
`errno=N (...)` passano dallo stesso ponte strutturato: il runtime chiama
`alfred_record_build_watch_diagnostic_with_os_error()`, il record conserva
`record.os_error.code` e `record.os_error.message`, e il formatter testuale li
rende nella forma compatibile `errno=N` o `errno=N (messaggio)`.
`record.os_error.name` resta opzionale e puo' essere `NULL`.

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
- un sink generico: `alfred_record_sink_t`
- un text sink compatibile: `alfred_record_text_sink_t`

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

    RR -. prossimo .-> SINK[alfred_record_sink_t<br/>emit(record)]
    DR -. prossimo .-> SINK
    SR -. prossimo .-> SINK
    SINK --> TS[alfred_record_text_sink_t]
    TS --> TW

    RR -. futuro .-> JSON[JSONL writer]
    DR -. futuro .-> JSON
    SR -. futuro .-> JSON

    RR -. futuro .-> BIN[binary/socket writer]
    DR -. futuro .-> BIN
    SR -. futuro .-> BIN
```

Schema del prossimo confine `emit(record)`:

```mermaid
flowchart LR
    B[Backend inotify<br/>diagnostic/raw records] --> E[emit(record)]
    C[Core semantic path<br/>semantic records] --> E
    E --> D[app dispatcher<br/>or record sink]
    D --> T[text writer<br/>compatibile]
    D -. futuro .-> J[JSONL writer]
    D -. futuro .-> P[policy/security<br/>pipeline]
    D -. futuro .-> X[binary/socket<br/>writer]

    T --> L[logger corrente<br/>events.log/raw.log]
    J -.-> JL[records.jsonl]
    X -.-> S[socket o file<br/>binario]
```

Lettura del diagramma:

1. backend e core producono entrambi `alfred_record_t`;
2. `emit(record)` e' il confine comune;
3. l'app dispatcher o record sink decide quali writer ricevono il record;
4. il text writer conserva il comportamento visibile corrente;
5. JSONL, policy e output binari restano consumer successivi dello stesso
   record, non sorgenti alternative del modello.

Lettura passo per passo:

1. Il backend inotify riceve un evento kernel e oggi costruisce un
   `alfred_raw_event_t`.
2. `alfred_record_from_raw()` prende quel raw event e produce un
   `alfred_record_t` con layer `normalized_raw`. Questo record conserva
   `source`, `raw_mask`, `cookie`, `pid`, timestamp e path borrowed.
   L'adapter non fa semantica, non deduplica e non correla eventi: valida pero'
   il contratto della raw mask. Una raw mask valida contiene una sola azione
   primaria; `ALFRED_RAW_ISDIR` e' solo un qualificatore ammesso. Se una mask
   contiene due azioni primarie, l'adapter fallisce invece di scegliere una
   priorita' implicita.
3. Quando Alfred produce una diagnostica `WATCH_*`, il builder
   `alfred_record_build_watch_diagnostic()` costruisce un `alfred_record_t`
   con layer `diagnostic`. La category distingue diagnostica di watch da
   diagnostica di recovery.
4. Il core continua a emettere `alfred_event_t`, ma `core_logger_on_event()`
   ora lo converte con `alfred_record_from_event()`, lo invia al sink generico
   con `alfred_record_sink_emit()` e lascia al text sink la formattazione del
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

Stato attuale: il lato output semantico del core usa gia' record + sink + text
sink per produrre lo stesso payload testuale di prima. Nel backend inotify,
`WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, `WATCH_RESYNC_*` e
`WATCH_LOST_*` usano gia' builder diagnostico, sink comune e text sink.
`WATCH_RESYNC_SCAN_FAILED` e `WATCH_LOST_QUEUE_FAILED` conservano il canale
error tramite un bridge di sink con routing event/error. Il raw path runtime e'
iniziato con `RAW_CREATE`, `RAW_DELETE`, `RAW_ATTRIB`, `RAW_MODIFY`,
`RAW_CLOSE_WRITE`, `RAW_MOVED_FROM`, `RAW_MOVED_TO` e `RAW_OVERFLOW`: il raw
originale resta consegnato al core, mentre il log raw normalizzato passa da
record + sink + text sink.

## Test futuri

Quando la API verra' implementata, servira' coprire:

- lifecycle `init/start/stop/destroy`;
- `add_target` e `remove_target` su root valide e invalide;
- capabilities dichiarate dal backend inotify;
- mapping `alfred_raw_event_t -> alfred_record_t`;
- mapping `WATCH_* -> diagnostic record`;
- writer testuale che preserva le righe log correnti;
- text sink che chiama un writer callback senza cambiare payload;
- nessun cambiamento nella semantica core;
- nessuna emissione diretta di eventi `FILE_*` / `DIR_*` dal backend.

Il contratto ops minimo e' gia' coperto da:

```text
tests/backend/test_backend_ops.c
tests/backend/test_backend_ops.sh
```

Il test verifica descriptor validi e casi invalidi: `ops == NULL`, nome ops
mancante o vuoto, versione API sbagliata, capabilities mancanti, nome
capabilities mancante o vuoto, nome capabilities diverso dal nome ops, versione
capabilities incoerente, capability vuote e callback lifecycle non valorizzate.
Non esercita il runtime inotify e non misura performance: blocca solo la forma
minima della tabella operations prima del refactor successivo.

## Decisioni rimandate

Restano da decidere nella fase codice:

- forma concreta di `alfred_backend_t`;
- se il runtime backend sara' allocato dall'app o dal backend;
- layout concreto di `alfred_backend_config_t`;
- layout concreto di `alfred_backend_target_t`;
- semantica precisa di `timeout_ms` in `poll`;
- elenco definitivo dei codici errore strutturati;
- migrazione del raw path runtime verso record e sink comune;
- come collegare piu' backend contemporanei;
- come gestire backpressure se `emit()` fallisce;
- quando introdurre `list_targets`;
- come versionare una futura ABI dinamica.

La decisione non rimandata e': Backend API v0 deve usare Event Model v0 come
contratto record, non stringhe testuali e non JSONL come modello primario.
