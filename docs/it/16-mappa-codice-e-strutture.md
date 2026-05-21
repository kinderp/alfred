# Mappa del codice e strutture dati

Questo capitolo e' una lettura guidata della codebase. Va letto come se una
persona esperta accompagnasse uno studente dentro Alfred, mostrando non solo
"quale funzione chiama quale", ma anche perche' quella chiamata esiste, quale
responsabilita' separa, quale struttura dati viene modificata e quale evento del
filesystem ha fatto partire il cambiamento.

Questo capitolo collega tre viste dello stesso sistema:

- quali funzioni chiamano quali altre funzioni
- quali strutture dati vengono lette o modificate
- quali eventi del filesystem fanno partire quei cambiamenti

L'obiettivo e' aiutare chi studia il progetto a vedere il programma come un
insieme di stati che cambiano nel tempo, non solo come una lista di funzioni.
Quando un passaggio richiede concetti teorici o tecnici gia' spiegati altrove,
questa guida deve rimandare agli altri capitoli, per esempio:

- [Guida C usato nel progetto](08-guida-c-usato-nel-progetto.md)
- [Architettura generale](02-architettura-generale.md)
- [Flusso eventi](07-flusso-eventi.md)
- [Semantica degli eventi](13-semantica-eventi.md)
- [Glossario](glossario.md)

## Vista generale runtime

Il flusso normale, con `event_engine=core`, e':

```mermaid
flowchart TD
    A["kernel inotify<br/>struct inotify_event"] --> B["inotify_backend_poll()"]
    B --> C["inotify_adapter_build_raw()"]
    C --> D["handle_backend_event()"]
    D --> E["alfred_process()"]
    E --> F["core_logger_on_event()"]
    F --> G["logger_event()"]
```

Il punto importante e' che `inotify_backend_poll()` non deve decidere la
semantica finale. Il backend produce fatti raw; `alfred_process()` produce
eventi semantici.

## Strutture dati backend

Le strutture dati principali del backend inotify sono:

```mermaid
flowchart TD
    A["app_t"] --> B["inotify_backend_t inotify"]
    B --> C["int fd"]
    B --> D["watcher_table_t watchers"]
    D --> E["watcher_entry_t items[]"]
    E --> F["wd"]
    E --> G["active"]
    E --> H["path[PATH_MAX]"]
```

`app_t` contiene ancora il backend inotify perche' il progetto e' in fase di
integrazione. La direzione finale e' avere un backend sempre piu' autonomo, ma
oggi il campo `app_t.inotify` rende esplicito dove vivono `fd` e tabella dei
watch.

## Struttura dati di configurazione

`config_t` guida molte decisioni prese durante l'inizializzazione. Non e' una
struttura dati del backend in senso stretto, ma il backend legge alcuni suoi
campi per sapere come comportarsi.

```mermaid
flowchart TD
    A["config_t"] --> B["recursive"]
    A --> C["move_cache_size"]
    A --> D["watcher_capacity"]
    A --> E["watch_mask"]
    A --> F["event_engine_mode"]
    A --> G["raw_log / event_log / error_log"]

    D --> H["watcher_init(capacity)"]
    E --> I["inotify_add_watch(mask)"]
    F --> L["core oppure shadow legacy"]
```

Campi rilevanti:

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `recursive` | abilita watch ricorsivi | `config_defaults()`, `config_load()` | `inotify_backend_add_startup_watch()`, `backend_handle_dir_create()` |
| `move_cache_size` | capacita' della cache move legacy | `config_defaults()`, `config_load()` | `legacy_events_init()` |
| `watcher_capacity` | capacita' iniziale della tabella watch | `config_defaults()`, `config_load()` | `watcher_init()` |
| `watch_mask` | maschera inotify usata per aggiungere watch | `config_defaults()` | `watch_manager_add()` |
| `event_engine_mode` | sceglie core o shadow | `config_defaults()`, `config_load()`, `config_set_event_engine()` | `app_init()`, `inotify_backend_init()`, `inotify_backend_poll()`, `core_logger_on_event()` |

`watch_mask` e' un buon esempio di confine fra configurazione e backend:
`config_defaults()` prende il valore da `watch_manager_default_mask()`, poi
`watch_manager_add()` usa quel valore quando chiama `inotify_add_watch()`.

```mermaid
sequenceDiagram
    participant Config as config_defaults()
    participant Mask as watch_manager_default_mask()
    participant App as inotify_backend_init()
    participant Watch as watch_manager_add()
    participant Kernel as inotify_add_watch()

    Config->>Mask: richiede maschera predefinita
    Mask-->>Config: IN_CREATE | IN_DELETE | IN_MODIFY | ...
    App->>Watch: aggiungi path
    Watch->>Kernel: fd, path, config.watch_mask
```

Il parsing delle capacita' usa una funzione dedicata invece di `atoi()`. Il
motivo e' che campi come `move_cache_size` e `watcher_capacity` sono `size_t`:
accettare per errore valori negativi o stringhe non numeriche potrebbe produrre
valori enormi o ambigui. La funzione di parsing mantiene il valore precedente
quando l'input non e' valido.

### `inotify_backend_t`

Definizione:

```c
typedef struct inotify_backend {
    int fd;
    watcher_table_t watchers;
} inotify_backend_t;
```

Campi:

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `fd` | file descriptor inotify non bloccante | `inotify_backend_init()`, `inotify_backend_shutdown()` | `inotify_backend_poll()`, `watch_manager_add()`, `watch_manager_remove()` |
| `watchers` | tabella `wd -> path` | `watcher_init()`, `watcher_store()`, `watcher_remove()`, `watcher_destroy()` | `watcher_get_path()`, `watcher_exists()` |

### `watcher_table_t`

Definizione:

```c
typedef struct {
    watcher_entry_t *items;
    size_t count;
    size_t capacity;
} watcher_table_t;
```

Questa tabella e' indicizzata direttamente dal watch descriptor `wd`. Se il
kernel restituisce `wd=7`, il path associato si trova in `items[7]`, dopo aver
controllato che l'indice sia valido e che lo slot sia attivo.

Campi:

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `items` | array dinamico di slot | `watcher_init()`, `watcher_expand()`, `watcher_destroy()` | tutte le funzioni `watcher_*` |
| `count` | numero di slot attivi | `watcher_init()`, `watcher_store()`, `watcher_remove()`, `watcher_destroy()` | `watcher_count()`, `watcher_dump()` |
| `capacity` | numero di slot allocati | `watcher_init()`, `watcher_expand()`, `watcher_destroy()` | `watcher_expand()`, `watcher_get_path()`, `watcher_exists()` |

### `watcher_entry_t`

Definizione:

```c
typedef struct {
    int wd;
    int active;
    char path[PATH_MAX];
} watcher_entry_t;
```

Campi:

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `wd` | watch descriptor restituito dal kernel | `watcher_store()`, `watcher_remove()` | `watcher_dump()` |
| `active` | indica se lo slot contiene una mappatura valida | `watcher_store()`, `watcher_remove()`, `watcher_expand()` | `watcher_get_path()`, `watcher_exists()`, `watcher_dump()` |
| `path` | directory osservata associata al `wd` | `watcher_store()`, `watcher_remove()` | `watcher_get_path()`, `watcher_dump()` |

## Inserimento di un watch

Scenario:

```text
Alfred deve osservare /tmp/progetto
```

Sequenza:

```mermaid
sequenceDiagram
    participant Backend as inotify_backend_add_startup_watch()
    participant Manager as watch_manager_add()
    participant Kernel as inotify_add_watch()
    participant Table as watcher_store()
    participant Logger as logger_event()

    Backend->>Manager: path=/tmp/progetto
    Manager->>Kernel: add watch con config.watch_mask
    Kernel-->>Manager: wd=3
    Manager->>Table: store wd=3 path=/tmp/progetto
    Table-->>Manager: ok
    Manager->>Logger: WATCH_ADDED wd=3 path=/tmp/progetto
```

Effetto sulla struttura dati:

```text
prima:
items[3].active = 0

dopo watcher_store():
items[3].wd     = 3
items[3].active = 1
items[3].path   = "/tmp/progetto"
count           = count + 1
```

Il log `WATCH_ADDED` e' diagnostica backend. Non e' un evento semantico del
core.

## Lettura di un evento inotify

Scenario:

```text
kernel invia IN_CREATE name=file.txt wd=3
```

Sequenza:

```mermaid
sequenceDiagram
    participant Kernel as kernel inotify
    participant Backend as inotify_backend_poll()
    participant Table as watcher_get_path()
    participant Adapter as inotify_adapter_build_raw()
    participant App as handle_backend_event()
    participant Core as alfred_process()

    Kernel-->>Backend: wd=3 name=file.txt mask=IN_CREATE
    Backend->>Table: watcher_get_path(wd=3)
    Table-->>Backend: /tmp/progetto
    Backend->>Adapter: parent=/tmp/progetto name=file.txt
    Adapter-->>Backend: ALFRED_RAW_CREATE path=/tmp/progetto/file.txt
    Backend->>App: raw event
    App->>Core: alfred_process(raw)
```

La tabella dei watch serve per ricostruire il path completo. Senza
`watcher_get_path()`, il backend conoscerebbe solo `file.txt`, non
`/tmp/progetto/file.txt`.

## Adapter inotify e raw event

`inotify_adapter.c` e' intenzionalmente stateless. Non possiede strutture dati
persistenti: riceve una `struct inotify_event`, il path parent recuperato dalla
watcher table e un buffer temporaneo per costruire il path completo.

```mermaid
flowchart LR
    A["struct inotify_event"] --> B["inotify_adapter_mask_to_alfred()"]
    C["parent path da watcher_table_t"] --> D["inotify_adapter_build_path()"]
    A --> D
    B --> E["alfred_raw_event_t.mask"]
    D --> F["alfred_raw_event_t.path"]
    E --> G["alfred_raw_event_t"]
    F --> G
```

La funzione `inotify_adapter_build_raw()` non alloca memoria per il path. Il
campo `raw.path` punta al buffer `full_path` passato dal chiamante:

```text
inotify_backend_poll()
    char full_path[PATH_MAX]
    alfred_raw_event_t raw
    inotify_adapter_build_raw(..., full_path, ..., &raw)
    handle_backend_event(..., &raw, ...)
    alfred_process(core, &raw)
```

Questo significa che il core deve consumare il raw event subito. Se un livello
volesse conservare l'evento oltre la chiamata, dovrebbe copiare il path.

## Rimozione di un watch

Scenario:

```text
un watch non serve piu' oppure inotify segnala IN_IGNORED
```

Sequenza:

```mermaid
sequenceDiagram
    participant Manager as watch_manager_remove()
    participant Table as watcher_get_path()
    participant Kernel as inotify_rm_watch()
    participant Remove as watcher_remove()
    participant Logger as logger_event()

    Manager->>Table: watcher_get_path(wd)
    Table-->>Manager: path
    Manager->>Kernel: inotify_rm_watch(fd, wd)
    Manager->>Remove: watcher_remove(wd)
    Remove-->>Manager: slot inactive
    Manager->>Logger: WATCH_REMOVED wd=... path=...
```

Effetto sulla struttura dati:

```text
prima:
items[wd].active = 1
items[wd].path   = "/tmp/progetto"

dopo watcher_remove():
items[wd].active = 0
items[wd].wd     = -1
items[wd].path   = ""
count            = count - 1
```

Anche `WATCH_REMOVED` e' diagnostica backend, non semantica core.

## Creazione ricorsiva veloce

Scenario delicato:

```bash
mkdir -p one/two/three
```

Problema: inotify puo' consegnare solo la creazione di `one`, perche' `two` e
`three` possono nascere prima che Alfred abbia installato i watch sui nuovi
genitori.

Sequenza semplificata:

```mermaid
sequenceDiagram
    participant Kernel as inotify
    participant Backend as inotify_backend_poll()
    participant Manager as watch_manager_add_recursive_with_discovery()
    participant Table as watcher_store()
    participant Synthetic as backend_emit_synthetic_dir_create()
    participant Core as alfred_process()

    Kernel-->>Backend: IN_CREATE|IN_ISDIR one
    Backend->>Manager: scan ricorsivo di one
    Manager->>Table: store watch per one
    Manager->>Table: store watch per one/two
    Manager->>Synthetic: discovered one/two
    Synthetic->>Core: ALFRED_RAW_CREATE|ALFRED_RAW_ISDIR one/two
    Manager->>Table: store watch per one/two/three
    Manager->>Synthetic: discovered one/two/three
    Synthetic->>Core: ALFRED_RAW_CREATE|ALFRED_RAW_ISDIR one/two/three
```

Qui il watch manager non dice "e' avvenuto un evento semantico". Dice solo:

```text
ho scoperto una directory che esiste gia' durante lo scan
```

Il backend trasforma questa scoperta in un raw event sintetico. Il core decide
la semantica e produce `DIR_CREATED`.

## Strutture dati del core

Arrivati a questo punto della lettura guidata, il backend ha gia' trasformato
gli eventi del kernel in `alfred_raw_event_t`. Ora entra in gioco il core, che
ha bisogno di memoria interna per trasformare eventi raw isolati in eventi
semantici coerenti.

Le due situazioni principali sono:

- `MOVED_FROM` e `MOVED_TO` arrivano come due raw event separati, ma devono
  diventare un solo evento semantico
- molti `MODIFY` ravvicinati sullo stesso file devono essere ridotti con debounce

Le strutture vivono in:

```text
core/src/alfred_tables.h
core/src/alfred_tables.c
```

Schema generale:

```mermaid
flowchart TD
    A["alfred_engine"] --> B["cfg"]
    A --> C["emit callback"]
    A --> D["userdata"]
    A --> E["seq"]
    A --> F["moves[1024]"]
    A --> G["debounce[1024]"]

    F --> H["alfred_move_entry_t chain"]
    H --> I["cookie"]
    H --> J["ts_ns"]
    H --> K["is_dir"]
    H --> L["src_path"]

    G --> M["alfred_debounce_entry_t chain"]
    M --> N["path"]
    M --> O["last_emit_ns"]
    M --> P["create_ns"]
```

### `alfred_engine`

Campi principali:

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `cfg` | configurazione interna del core | `alfred_create()` | correlatore, debounce, sweep move |
| `emit` | callback per emettere eventi semantici | `alfred_create()` | `emit()` |
| `userdata` | contesto passato alla callback | `alfred_create()` | `emit()` |
| `seq` | numero progressivo degli eventi semantici | `alfred_create()`, `emit()` | `core_logger_on_event()` tramite evento emesso |
| `moves[1024]` | tabella hash dei move pendenti | `alfred_move_insert()`, `alfred_move_take()`, `alfred_move_sweep()` | `alfred_process()` |
| `debounce[1024]` | tabella hash per debounce MODIFY | `alfred_debounce_get()`, `alfred_debounce_should_emit()` | `alfred_process()` |

### `alfred_move_entry_t`

Questa struttura salva un `MOVED_FROM` finche' arriva il `MOVED_TO` con lo
stesso cookie.

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `cookie` | cookie raw che collega `MOVED_FROM` e `MOVED_TO` | `alfred_move_insert()` | `alfred_move_take()` |
| `ts_ns` | timestamp raw del `MOVED_FROM` | `alfred_move_insert()` | `alfred_move_sweep()` |
| `pid` | pid se noto | `alfred_move_insert()` | uso futuro |
| `is_dir` | indica se l'oggetto e' directory | `alfred_move_insert()` | `classify_move()` tramite `alfred_process()` |
| `src_path` | path sorgente copiato e posseduto dal core | `alfred_move_insert()` | `alfred_process()` |
| `next` | prossimo elemento nello stesso bucket hash | `alfred_move_insert()`, `alfred_move_take()` | funzioni tabella |

Perche' `src_path` viene copiato? Perche' il raw event ricevuto dal backend
punta spesso a un buffer locale valido solo durante la chiamata. Il core deve
conservare il path sorgente fino all'arrivo del `MOVED_TO`, quindi ne prende una
copia.

Sequenza move nel core:

```mermaid
sequenceDiagram
    participant Backend as backend raw event
    participant Core as alfred_process()
    participant Moves as moves hash table
    participant Classifier as classify_move()
    participant Emit as emit()

    Backend-->>Core: ALFRED_RAW_MOVED_FROM cookie=10 src=/a/x
    Core->>Moves: alfred_move_insert(cookie=10, src=/a/x)
    Backend-->>Core: ALFRED_RAW_MOVED_TO cookie=10 dst=/b/y
    Core->>Moves: alfred_move_take(cookie=10)
    Moves-->>Core: src=/a/x is_dir=...
    Core->>Classifier: src=/a/x dst=/b/y
    Classifier-->>Core: FILE_RELOCATED or DIR_RELOCATED
    Core->>Emit: semantic event
```

Frame logici per una futura animazione:

```text
frame 1:
  moves[bucket].head = NULL

frame 2:
  raw MOVED_FROM cookie=10 path=/a/x

frame 3:
  alfred_move_insert():
    entry.cookie = 10
    entry.src_path = copy("/a/x")
    entry.next = old bucket head
    moves[bucket] = entry

frame 4:
  raw MOVED_TO cookie=10 path=/b/y

frame 5:
  alfred_move_take():
    remove entry from bucket
    return src_path=/a/x

frame 6:
  classify_move(/a/x, /b/y)
  emit FILE_RELOCATED or DIR_RELOCATED
```

### `alfred_debounce_entry_t`

Questa struttura riduce i `MODIFY` ripetuti sullo stesso path.

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `path` | path usato come chiave debounce | `alfred_debounce_get()` | `alfred_debounce_get()` |
| `last_emit_ns` | ultimo timestamp in cui e' stato emesso `FILE_MODIFIED` | `alfred_debounce_should_emit()` | `alfred_debounce_should_emit()` |
| `create_ns` | riservato per futura correlazione create-ready | non ancora usato in modo completo | uso futuro |
| `next` | prossimo elemento nello stesso bucket hash | `alfred_debounce_get()` | funzioni tabella |

Sequenza debounce:

```mermaid
sequenceDiagram
    participant Backend as backend raw event
    participant Core as alfred_process()
    participant Debounce as debounce hash table
    participant Emit as emit()

    Backend-->>Core: ALFRED_RAW_MODIFY path=/tmp/a.txt
    Core->>Debounce: alfred_debounce_get(path)
    Core->>Debounce: alfred_debounce_should_emit(now)
    Debounce-->>Core: yes
    Core->>Emit: FILE_MODIFIED
    Backend-->>Core: ALFRED_RAW_MODIFY path=/tmp/a.txt ravvicinato
    Core->>Debounce: alfred_debounce_should_emit(now)
    Debounce-->>Core: no, dentro finestra debounce
```

Qui il fatto raw non viene perso: il backend lo ha osservato. Il core decide
pero' che non tutti i `MODIFY` ravvicinati devono diventare eventi semantici,
perche' molti programmi salvano file generando raffiche di modifiche tecniche.

## Vista dinamica futura

Questa pagina e' pensata per essere trasformata in una vista dinamica in una
fase successiva.

Mermaid e' ottimo per diagrammi statici e sequence diagram, ma non produce GIF
animate vere. Per animare gli stati delle strutture dati conviene separare i
dati dalla resa grafica:

1. descrivere ogni scenario come una sequenza di frame
2. generare SVG o PNG per ogni frame
3. assemblare i frame in GIF o video con strumenti esterni

Esempio di frame per `watch_manager_add()`:

```text
frame 1:
  watcher_table.count = 0
  items[3].active = 0

frame 2:
  inotify_add_watch() restituisce wd=3

frame 3:
  watcher_store() scrive:
    items[3].wd = 3
    items[3].active = 1
    items[3].path = "/tmp/progetto"
    count = 1

frame 4:
  logger_event("WATCH_ADDED wd=3 path=/tmp/progetto")
```

Possibili tecniche future:

| Tecnica | Vantaggio | Svantaggio |
| --- | --- | --- |
| Mermaid | integrato nei Markdown, facile da leggere | statico, non adatto a GIF |
| Graphviz | ottimo per grafi e strutture dati | statico se usato da solo |
| SVG generato da script | controllabile e animabile | richiede codice dedicato |
| HTML/CSS/JavaScript | ideale per animazioni interattive | non sempre comodo da leggere offline |
| Manim | animazioni didattiche molto potenti | dipendenza pesante e piu' complessa |
| PNG + ffmpeg/ImageMagick | semplice per GIF/video | serve generare i frame |

La direzione consigliata e' partire da SVG generati da uno script, per esempio:

```bash
make docs-animations
```

Questo target non esiste ancora nel Makefile. Qui e' indicato come nome
probabile per una futura automazione.

con output in:

```text
docs/generated/animations/
```

Gli stessi dati potrebbero generare sia una GIF sia una pagina HTML
interattiva. Prima pero' conviene stabilizzare le tabelle e gli schemi statici,
perche' saranno la base dei frame animati.

### Formato consigliato per i frame

Per rendere generabile la documentazione dinamica, ogni scenario dovrebbe
seguire sempre questa struttura:

````text
### Scenario animabile: nome scenario

Trigger:

```text
comando o evento che fa partire lo scenario
```

Frame:

```text
frame N - titolo breve:
  evento:
    cosa e' appena successo
  funzioni:
    funzione_a()
    funzione_b()
  strutture:
    struttura.campo = valore
  output:
    log o evento semantico prodotto
```
````

Non tutti i frame devono avere tutte le sottosezioni. Pero' quando un frame
modifica una struttura dati, la modifica deve essere esplicita. Questo e' il
punto che rende lo scenario animabile: uno script futuro puo' evidenziare il
campo modificato, mentre uno studente puo' seguire manualmente l'evoluzione
dello stato.

### Convenzioni per scenari futuri

- Usare nomi di funzioni reali, non pseudonimi generici.
- Usare nomi di campi reali quando il codice li espone.
- Separare eventi raw, eventi diagnostici backend ed eventi semantici core.
- Specificare quando un path e' copiato e quando invece e' solo preso in
  prestito da un buffer temporaneo.
- Evidenziare sempre se un comportamento appartiene al runtime core o al
  percorso legacy shadow.
- Se una scelta dipende da una regola teorica, aggiungere un rimando ai capitoli
  pertinenti: guida C, flusso eventi, semantica eventi o glossario.

### Possibile pipeline futura

Una pipeline leggera potrebbe essere:

```text
docs/it/16-mappa-codice-e-strutture.md
    -> parser scenari
    -> frame JSON intermedi
    -> renderer SVG
    -> GIF/video/HTML
```

Output ipotetico:

```text
docs/generated/animations/watch-add/
├── frames.json
├── frame-001.svg
├── frame-002.svg
├── watch-add.gif
└── index.html
```

Il file Markdown deve restare la sorgente didattica principale. I file generati
devono essere considerati derivati: utili per le lezioni, ma non il posto dove
scrivere le spiegazioni.

## Scenari animabili

Questa sezione raccoglie scenari gia' scritti come sequenze di frame. Ogni frame
descrive:

- l'evento o la chiamata che fa avanzare il processo
- quali funzioni entrano in gioco
- quali campi delle strutture dati cambiano
- quale output diagnostico o semantico viene prodotto

Questi frame sono volutamente testuali. In una fase successiva potranno essere
letti da uno script o trasformati manualmente in SVG, GIF, video o pagine HTML
interattive.

### Scenario animabile: aggiunta watch iniziale

Trigger:

```text
./alfred /tmp/progetto
```

Frame:

```text
frame 1 - configurazione pronta:
  config.recursive = 1
  config.watch_mask = IN_CREATE | IN_DELETE | IN_MODIFY | ...
  config.watcher_capacity = 128

frame 2 - backend inizializzato:
  inotify_backend_init()
  app.inotify.fd = <fd inotify>
  watcher_init(&app.inotify.watchers, 128)
  watchers.count = 0

frame 3 - richiesta watch:
  inotify_backend_add_startup_watch(app, "/tmp/progetto")
  watch_manager_add_recursive() oppure watch_manager_add()

frame 4 - chiamata kernel:
  watch_manager_add()
  inotify_add_watch(fd, "/tmp/progetto", config.watch_mask)
  kernel restituisce wd=3

frame 5 - aggiornamento watcher table:
  watcher_store(wd=3, path="/tmp/progetto")
  watchers.items[3].wd = 3
  watchers.items[3].active = 1
  watchers.items[3].path = "/tmp/progetto"
  watchers.count = 1

frame 6 - output diagnostico:
  logger_event("WATCH_ADDED wd=3 path=/tmp/progetto")
```

Messaggio didattico: il watch e' stato aggiunto al backend, non al core. Il core
non conosce `wd=3`; ricevera' solo raw event con path gia' ricostruito.

### Scenario animabile: create file

Trigger:

```bash
touch /tmp/progetto/a.txt
```

Frame:

```text
frame 1 - evento kernel:
  struct inotify_event:
    wd = 3
    mask = IN_CREATE
    name = "a.txt"

frame 2 - lookup path parent:
  inotify_backend_poll()
  watcher_get_path(&watchers, 3) -> "/tmp/progetto"

frame 3 - conversione raw:
  inotify_adapter_build_path("/tmp/progetto", "a.txt")
  full_path = "/tmp/progetto/a.txt"
  inotify_adapter_mask_to_alfred(IN_CREATE)
  raw.mask = ALFRED_RAW_CREATE
  raw.path = full_path

frame 4 - ingresso nel core:
  handle_backend_event(app, &raw, NULL)
  alfred_process(app->core, &raw)

frame 5 - evento semantico:
  alfred_process()
  raw.mask contiene ALFRED_RAW_CREATE
  emit(ALFRED_EV_FILE_CREATED, "/tmp/progetto/a.txt", NULL)

frame 6 - log ufficiale:
  core_logger_on_event()
  logger_event("FILE_CREATED path=/tmp/progetto/a.txt")
```

Messaggio didattico: il backend ricostruisce il fatto tecnico, il core decide il
nome semantico `FILE_CREATED`.

### Scenario animabile: close-write / file ready

Trigger:

```bash
printf "hello" > /tmp/progetto/a.txt
```

Frame:

```text
frame 1 - evento tecnico:
  kernel invia IN_CLOSE_WRITE per a.txt

frame 2 - raw event:
  inotify_adapter_mask_to_alfred(IN_CLOSE_WRITE)
  raw.mask = ALFRED_RAW_CLOSE_WRITE
  raw.path = "/tmp/progetto/a.txt"

frame 3 - core:
  alfred_process()
  raw.mask contiene ALFRED_RAW_CLOSE_WRITE

frame 4 - semantica:
  emit(ALFRED_EV_FILE_READY, "/tmp/progetto/a.txt", NULL)

frame 5 - log:
  FILE_READY path=/tmp/progetto/a.txt
```

Messaggio didattico: `FILE_READY` non e' un duplicato di `FILE_CREATED`; indica
che una scrittura e' stata chiusa e il file e' pronto per lettura o indicizzazione.

### Scenario animabile: modify debounced

Trigger:

```bash
printf "x" >> /tmp/progetto/a.txt
printf "y" >> /tmp/progetto/a.txt
```

Frame:

```text
frame 1 - primo MODIFY:
  raw.mask = ALFRED_RAW_MODIFY
  raw.path = "/tmp/progetto/a.txt"

frame 2 - creazione stato debounce:
  alfred_debounce_get(path)
  bucket = path_index("/tmp/progetto/a.txt")
  nuova alfred_debounce_entry_t:
    path = copy("/tmp/progetto/a.txt")
    last_emit_ns = 0

frame 3 - prima emissione:
  alfred_debounce_should_emit(now)
  now - last_emit_ns >= modify_debounce_ms
  last_emit_ns = now
  emit(FILE_MODIFIED)

frame 4 - secondo MODIFY ravvicinato:
  alfred_debounce_get(path) trova la stessa entry
  alfred_debounce_should_emit(now2)

frame 5 - soppressione:
  now2 - last_emit_ns < modify_debounce_ms
  nessun evento semantico emesso
```

Messaggio didattico: il raw event arriva comunque al core. La soppressione
avviene solo nello stream semantico, per evitare rumore applicativo.

### Scenario animabile: move + rename nel core

Trigger:

```bash
mv /tmp/progetto/a.txt /tmp/altro/b.txt
```

Frame:

```text
frame 1 - prima meta':
  raw MOVED_FROM:
    cookie = 10
    path = "/tmp/progetto/a.txt"

frame 2 - salvataggio nel core:
  alfred_move_insert()
  bucket = move_index(10)
  entry.cookie = 10
  entry.src_path = copy("/tmp/progetto/a.txt")
  entry.next = moves[bucket]
  moves[bucket] = entry

frame 3 - seconda meta':
  raw MOVED_TO:
    cookie = 10
    path = "/tmp/altro/b.txt"

frame 4 - recupero:
  alfred_move_take(cookie=10)
  rimuove entry da moves[bucket]
  restituisce src_path="/tmp/progetto/a.txt"

frame 5 - classificazione:
  parent sorgente = "/tmp/progetto"
  parent destinazione = "/tmp/altro"
  basename sorgente = "a.txt"
  basename destinazione = "b.txt"
  parent diverso e nome diverso -> FILE_RELOCATED

frame 6 - output:
  emit(FILE_RELOCATED, src="/tmp/progetto/a.txt", dst="/tmp/altro/b.txt")
```

Messaggio didattico: il core produce un solo evento `RELOCATED`, mentre il
legacy storico puo' produrre due eventi (`MOVED` e `RENAMED`).

### Scenario animabile: recursive mkdir veloce

Trigger:

```bash
mkdir -p /tmp/progetto/one/two/three
```

Frame:

```text
frame 1 - evento osservabile:
  il kernel invia IN_CREATE|IN_ISDIR per "one"
  non invia create affidabili per "two" e "three" perche' i watch non esistono ancora

frame 2 - backend gestisce one:
  inotify_backend_poll()
  raw reale:
    ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR
    path="/tmp/progetto/one"
  core emette DIR_CREATED one

frame 3 - discovery ricorsiva:
  backend_handle_dir_create()
  watch_manager_add_recursive_with_discovery("/tmp/progetto/one")

frame 4 - watch su one:
  watch_manager_add("/tmp/progetto/one")
  watcher_store(wd=4, path="/tmp/progetto/one")
  WATCH_ADDED wd=4 path=/tmp/progetto/one

frame 5 - scoperta two:
  recursive_walk() vede "/tmp/progetto/one/two"
  watch_manager_add()
  watcher_store(wd=5, path="/tmp/progetto/one/two")
  backend_process_discovered_dir(path="/tmp/progetto/one/two")

frame 6 - raw sintetico two:
  backend_emit_synthetic_dir_create()
  raw.mask = ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR
  raw.path = "/tmp/progetto/one/two"
  core emette DIR_CREATED one/two

frame 7 - scoperta three:
  stesso schema:
    watcher_store(wd=6, path="/tmp/progetto/one/two/three")
    raw sintetico CREATE|ISDIR
    core emette DIR_CREATED one/two/three
```

Messaggio didattico: il raw sintetico non inventa una directory; rappresenta una
directory reale scoperta troppo tardi per ricevere l'evento kernel originale.

### Scenario animabile: rimozione watch

Trigger:

```text
il backend riceve IN_IGNORED oppure decide di rimuovere un watch
```

Frame:

```text
frame 1 - stato iniziale:
  watchers.items[3].active = 1
  watchers.items[3].path = "/tmp/progetto"

frame 2 - lookup per log:
  watch_manager_remove(app, wd=3)
  watcher_get_path(wd=3) -> "/tmp/progetto"

frame 3 - rimozione kernel:
  inotify_rm_watch(app.inotify.fd, 3)

frame 4 - rimozione tabella:
  watcher_remove(wd=3)
  watchers.items[3].active = 0
  watchers.items[3].wd = -1
  watchers.items[3].path = ""
  watchers.count = watchers.count - 1

frame 5 - output diagnostico:
  WATCH_REMOVED wd=3 path=/tmp/progetto
```

Messaggio didattico: `WATCH_REMOVED` descrive stato del backend. Non e' un
evento semantico del filesystem come `DIR_DELETED`.

## Strutture legacy shadow

Il percorso legacy e' ancora presente per confronto, ma non e' l'architettura
target. I file principali sono:

```text
modules/inotify/src/events.c
modules/inotify/src/move_cache.c
```

`events.c` riceve direttamente `struct inotify_event` e produce eventi
semantici legacy. Questo era il vecchio centro della semantica, ma oggi deve
essere letto come codice storico/shadow-only.

```mermaid
flowchart TD
    A["struct inotify_event"] --> B["legacy_events_dispatch()"]
    B --> C["handle_create() / handle_delete()"]
    B --> D["handle_move_from()"]
    B --> E["handle_move_to()"]
    D --> F["move_cache_store()"]
    E --> G["move_cache_find()"]
    E --> H["emit_event()"]
    C --> H
    H --> I["logger_event() legacy stream"]
```

### `move_cache_t`

Definizione:

```c
typedef struct {
    move_slot_t *slots;
    size_t size;
} move_cache_t;
```

Campi:

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `slots` | array dei `MOVED_FROM` legacy pendenti | `move_cache_init()`, `move_cache_destroy()` | `move_cache_store()`, `move_cache_find()`, `move_cache_clear()` |
| `size` | numero massimo di slot | `move_cache_init()`, `move_cache_destroy()` | helper interni di ricerca |

### `move_slot_t`

Definizione:

```c
typedef struct {
    uint32_t cookie;
    int src_wd;
    char src_name[NAME_MAX];
    int used;
} move_slot_t;
```

Campi:

| Campo | Significato | Scritto da | Letto da |
| --- | --- | --- | --- |
| `cookie` | cookie inotify della coppia move | `move_cache_store()`, `move_cache_clear()` | `move_cache_find()`, `handle_move_to()` |
| `src_wd` | watch descriptor sorgente | `move_cache_store()` | `handle_move_to()` |
| `src_name` | basename sorgente | `move_cache_store()` | `handle_move_to()` |
| `used` | slot occupato/libero | `move_cache_store()`, `move_cache_clear()` | helper interni e conteggi |

Sequenza legacy move:

```mermaid
sequenceDiagram
    participant Kernel as inotify
    participant Legacy as legacy_events_dispatch()
    participant Cache as move_cache_t
    participant Logger as logger_event()

    Kernel-->>Legacy: IN_MOVED_FROM cookie=10 src=a.txt
    Legacy->>Cache: move_cache_store(cookie=10, src_wd, a.txt)
    Kernel-->>Legacy: IN_MOVED_TO cookie=10 dst=b.txt
    Legacy->>Cache: move_cache_find(cookie=10)
    Cache-->>Legacy: src_wd + src_name
    Legacy->>Logger: FILE_RENAMED or FILE_MOVED
    Legacy->>Cache: move_cache_clear(cookie=10)
```

Differenza importante rispetto al core: se cambiano sia directory sia nome, il
legacy puo' emettere due eventi (`MOVED` e poi `RENAMED`). Il core invece emette
un solo evento `RELOCATED`. Per questo `move_cache_t` serve solo come confronto
storico e non deve guidare la semantica futura.
