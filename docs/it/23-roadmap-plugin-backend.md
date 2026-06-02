# Roadmap plugin backend

Questo capitolo descrive la direzione architetturale per trasformare i backend
di Alfred in plugin con una API comune.

Oggi Alfred ha un solo backend reale: `inotify`. Nel codice e nella
documentazione spesso usiamo anche la parola "modulo" per indicare la stessa
cosa. In futuro Alfred dovra' supportare piu' sorgenti:

- Linux `inotify`
- Linux `fanotify`
- Linux Audit
- eBPF
- backend Windows
- backend macOS
- eventuali backend remoti o specializzati

Questi componenti non devono diventare copie del core. Devono osservare un
sistema operativo o una tecnologia specifica e produrre fatti grezzi comuni che
il core sa interpretare.

## Obiettivo

L'obiettivo e' avere questa separazione:

```text
backend/plugin specifico
-> produce alfred_raw_event_t
-> produce diagnostica strutturata backend
-> non decide semantica FILE_* / DIR_*

core Alfred
-> riceve alfred_raw_event_t
-> correla e normalizza
-> produce alfred_event_t semantici
```

Quindi `inotify`, `fanotify`, `audit`, `ebpf`, Windows e macOS devono essere
produttori diversi dello stesso contratto raw. Il core non deve sapere se un
evento arriva da Linux, Windows o da un probe eBPF.

## Cosa deve fare un backend

Un backend deve:

- inizializzare le risorse del proprio sistema operativo
- osservare una o piu' root configurate
- produrre `alfred_raw_event_t`
- produrre diagnostica backend strutturata
- gestire il proprio stato interno, per esempio watch, handle, subscription,
  buffer, mappe kernel o file descriptor
- tradurre errori tecnici in diagnostica comprensibile
- rispettare il contratto di ownership dei path e degli eventi

Un backend non deve:

- emettere direttamente `FILE_CREATED`, `DIR_DELETED`, `FILE_RELOCATED`, ecc.
- decidere la semantica finale di move/rename/relocated
- duplicare il correlatore del core
- produrre solo stringhe testuali come API primaria
- obbligare il core a conoscere dettagli specifici del sistema operativo

## Contratto comune

Il contratto minimo tra backend e Alfred dovrebbe essere composto da due stream:

```text
raw event stream        -> alfred_raw_event_t
diagnostic event stream -> alfred_log_record_t o equivalente strutturato
```

`alfred_raw_event_t` serve al core:

```text
ALFRED_RAW_CREATE
ALFRED_RAW_DELETE
ALFRED_RAW_MODIFY
ALFRED_RAW_CLOSE_WRITE
ALFRED_RAW_MOVED_FROM
ALFRED_RAW_MOVED_TO
ALFRED_RAW_OVERFLOW
ALFRED_RAW_ISDIR
```

La diagnostica serve a manutenzione, test, osservabilita' e debug:

```text
WATCH_ADDED
WATCH_REMOVED
WATCH_STALE
WATCH_RESYNC_BEGIN
WATCH_RESYNC_FAILED
...
```

Questi nomi non devono essere solo stringhe stampate. Devono diventare record
strutturati, come descritto in [Contratto dei log](22-contratto-log.md).

## API C proposta

Una prima API comune potrebbe avere questa forma:

```c
typedef struct alfred_backend alfred_backend_t;

typedef int (*alfred_backend_raw_emit_fn)(
    const alfred_raw_event_t *raw,
    void *userdata
);

typedef int (*alfred_backend_diag_emit_fn)(
    const alfred_log_record_t *diag,
    void *userdata
);

typedef struct {
    alfred_backend_raw_emit_fn emit_raw;
    alfred_backend_diag_emit_fn emit_diag;
    void *userdata;
} alfred_backend_emit_t;

typedef struct {
    const char *name;
    uint32_t api_version;

    int (*init)(alfred_backend_t *backend,
                const alfred_backend_config_t *config,
                const alfred_backend_emit_t *emit);

    int (*start)(alfred_backend_t *backend);
    int (*poll)(alfred_backend_t *backend, int timeout_ms);
    int (*stop)(alfred_backend_t *backend);
    void (*destroy)(alfred_backend_t *backend);
} alfred_backend_ops_t;
```

Questa non e' ancora una API implementata. E' una direzione di progettazione.
Prima di scrivere codice, dovremo decidere:

- dove vive `alfred_backend_t`
- quali campi contiene `alfred_backend_config_t`
- come rappresentare root multiple
- come passare opzioni specifiche del backend
- come gestire errori strutturati
- come garantire ownership e durata dei puntatori

## Link statico prima, plugin dinamici dopo

La scelta consigliata e':

```text
fase 1 -> backend linkati staticamente
fase 2 -> plugin dinamici .so solo dopo API stabile
```

### Fase 1: link statico

In questa fase i backend vengono compilati dentro Alfred:

```text
inotify_backend.o
fanotify_backend.o
audit_backend.o
ebpf_backend.o
```

Vantaggi:

- piu' semplice per studenti e contributori
- test piu' facili
- debug piu' diretto
- niente `dlopen()`
- niente ABI stabile da mantenere subito
- niente problemi di path plugin o caricamento non sicuro

Questa fase permette di stabilizzare il contratto comune senza aggiungere
complessita' di packaging.

### Fase 2: plugin dinamici

Quando l'API sara' matura, potremo valutare:

```text
alfred-backend-inotify.so
alfred-backend-fanotify.so
alfred-backend-ebpf.so
alfred-backend-windows.dll
alfred-backend-macos.dylib
```

Per i `.so` Linux, un possibile entrypoint potrebbe essere:

```c
const alfred_backend_plugin_t *alfred_backend_plugin_init(void);
```

Con metadati:

```c
typedef struct {
    uint32_t api_version;
    const char *name;
    const char *description;
    const alfred_backend_ops_t *ops;
} alfred_backend_plugin_t;
```

Prima di supportare plugin dinamici dovremo decidere:

- versioning ABI
- compatibilita' tra Alfred e plugin
- ownership memoria attraversando il confine del plugin
- gestione errori di plugin mancante o incompatibile
- sicurezza del caricamento
- directory consentite per i plugin
- firma o trust dei plugin, se Alfred avra' uso commerciale

## Relazione con output strutturato

La roadmap plugin e la roadmap output strutturato sono collegate.

Se ogni backend emette solo stringhe, allora:

- il core o il logger dovrebbero fare parsing
- i backend avrebbero formati divergenti
- sarebbe difficile produrre output binario performante
- sarebbe difficile mantenere compatibilita' tra plugin

Se invece ogni backend emette record strutturati:

```text
backend -> alfred_raw_event_t
backend -> alfred_log_record_t
```

allora Alfred puo':

- scrivere testo per debug
- scrivere JSON Lines per strumenti
- inviare MessagePack o Protobuf su socket
- usare una socket binaria pura per prestazioni estreme
- mantenere lo stesso contratto per tutti i backend

Il testo diventa un writer, non la fonte primaria del dato.

## Come inotify deve evolvere

Il backend inotify corrente e' il prototipo da cui partire.

Oggi fa gia' molte cose che un plugin dovra' fare:

- inizializza il file descriptor inotify
- installa watch
- mantiene `wd -> path`
- produce `alfred_raw_event_t`
- produce diagnostica `WATCH_*`
- gestisce stati `VALID`, `STALE`, `RESYNCING`, `REMOVED`
- usa lo scanner per discovery e resync

Ma deve ancora essere ripulito verso una API comune:

- separare meglio lifecycle backend e dettagli inotify
- ridurre dipendenze dirette da strutture specifiche dell'app
- sostituire progressivamente `logger_event("testo")` con diagnostica
  strutturata
- rendere esplicito il contratto di configurazione backend
- isolare opzioni inotify-specifiche come `watch_mask`

## Decisioni attuali

Decisioni fissate per ora:

- Alfred avra' piu' backend/plugin
- il core deve restare backend-agnostic
- il contratto comune e' raw event + diagnostica strutturata
- la prima fase sara' link statico
- i plugin dinamici sono una fase futura, dopo stabilizzazione API
- l'output testuale resta utile ma non deve diventare l'unico contratto
- per prestazioni estreme valuteremo una socket binaria pura

## Prossimi passi

1. completare il resync inotify sul branch corrente
2. consolidare `alfred_log_record_t` o struttura equivalente nel design
3. disegnare una prima `alfred_backend_ops_t` senza implementarla subito
4. verificare quali funzioni inotify correnti corrispondono a `init`, `start`,
   `poll`, `stop`, `destroy`
5. solo dopo, iniziare un refactor verso backend statici multipli

## Rimandi

- [Contratto dei log](22-contratto-log.md)
- [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
- [Modulo inotify](05-modulo-inotify.md)
- [Core engine](06-core-engine.md)
