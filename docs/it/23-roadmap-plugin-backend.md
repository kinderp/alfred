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

Il contratto minimo tra backend e Alfred ora e' descritto in
[Backend API v0](30-backend-api-v0.md). In sintesi, un backend deve produrre
record compatibili con [Event Model v0](29-event-model-v0.md), non stringhe di
log come API primaria.

La vecchia formulazione a due stream era:

```text
raw event stream        -> alfred_raw_event_t
diagnostic event stream -> alfred_log_record_t o equivalente strutturato
```

La formulazione aggiornata e':

```text
backend -> alfred_record_t layer=backend_observed
backend -> alfred_record_t layer=normalized_raw
backend -> alfred_record_t layer=diagnostic
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
strutturati, come descritto in [Backend API v0](30-backend-api-v0.md) e
[Contratto dei log](22-contratto-log.md).

## API C proposta

La roadmap non deve duplicare la definizione C di `alfred_backend_ops_t`.
La fonte autorevole e' ora:

- [Backend API v0](30-backend-api-v0.md), per il contratto spiegato;
- `core/include/alfred_backend_ops.h`, per la forma C compilata;
- `tests/backend/test_backend_ops.c`, per i casi validi e rifiutati.

Questa separazione evita che una bozza di roadmap resti indietro rispetto al
contratto reale. La specifica aggiornata usa un emit boundary basato su
`alfred_record_t` e separa lifecycle, target management, capabilities,
ownership ed error model. Prima di completare il refactor runtime restano da
decidere:

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

Se invece ogni backend emette record strutturati secondo Backend API v0:

```text
backend -> alfred_record_t
```

allora Alfred puo':

- scrivere testo per debug
- scrivere JSON Lines per strumenti
- inviare MessagePack o Protobuf su socket
- usare una socket binaria pura per prestazioni estreme
- mantenere lo stesso contratto per tutti i backend

Il testo diventa un writer, non la fonte primaria del dato.

## Relazione con AI agent guardrail

La roadmap plugin nasce anche per l'obiettivo piu' ampio di Alfred: diventare
un runtime security layer per agenti AI.

Un guardrail per agenti non puo' osservare solo il filesystem. Deve poter
correlare prompt, tool call, processi, rete, accesso a credenziali e modifiche
reali sul sistema. Questo richiede backend diversi:

- `inotify` per cambiamenti filesystem
- `fanotify` per permission event su file
- audit/eBPF per processi, exec, rete e segnali kernel
- backend Windows/macOS per ambienti non Linux

Il contratto plugin deve quindi restare abbastanza generale da supportare eventi
di sicurezza, non solo eventi filesystem. La semantica filesystem resta il
primo passo; il guardrail agentico e' la direzione di prodotto piu' ampia.

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

1. usare [Backend API v0](30-backend-api-v0.md) come riferimento del refactor
2. usare i tipi e helper gia' introdotti:
   `alfred_record_t`, `alfred_record_from_raw()`,
   `alfred_record_build_watch_diagnostic()` e
   `alfred_record_format_text()`
3. migrare gradualmente inotify verso `emit(record)`
4. solo dopo, iniziare un refactor verso backend statici multipli

## Rimandi

- [Backend API v0](30-backend-api-v0.md)
- [Event Model v0](29-event-model-v0.md)
- [Contratto dei log](22-contratto-log.md)
- [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
- [Modulo inotify](05-modulo-inotify.md)
- [Core engine](06-core-engine.md)
