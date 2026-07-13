# Tracepoint e Alfred Lab MVP

Questo documento definisce la milestone `Tracepoint and Lab MVP`.

La milestone nasce dopo Event Model v0, Writer Runtime v0, JSONL v0, Backend
API v0 e chiusura del backend inotify come riferimento per il sottoinsieme
staged. Il punto non e' aggiungere una UI o un sistema di tracing pesante. Il
punto e' rendere Alfred spiegabile: uno studente, un contributore o un futuro
tool Lab devono poter seguire un fatto attraverso backend, raw event, core,
record, output e test senza dipendere da stringhe fragili o righe di codice
specifiche.

## Obiettivo

L'obiettivo della milestone e' definire il primo contratto minimo per:

- tracepoint logici stabili;
- scenari Lab descritti in modo ripetibile;
- mappa fra scenario, funzioni coinvolte, strutture dati, record e test;
- primo percorso didattico `comando -> evento backend -> raw Alfred -> evento
  semantico -> output -> spiegazione`.

Per v0, `Lab` significa soprattutto formato e contenuto verificabile. Non
significa ancora dashboard, web UI, animazioni generate, runtime tracing
continuo o integrazione con AI.

## Perche' ora

Alfred ha gia' abbastanza contratti da poter raccontare i percorsi principali:

- Event Model v0 distingue layer, category, type e record strutturati;
- Writer Runtime v0 ha introdotto queue, dispatcher, sink e output opt-in;
- JSONL v0 e' il formato macchina iniziale;
- Backend API v0 ha separato lifecycle, target, capabilities e poll staged;
- inotify e' documentato come reference backend staged.

Senza tracepoint e scenari Lab, pero', la documentazione resta manuale: ogni
volta bisogna ricostruire a mano quali funzioni partecipano allo scenario e
quali log dimostrano il comportamento. La milestone serve a stabilizzare questo
linguaggio prima di costruire strumenti automatici.

## Cosa e' un tracepoint logico

Un tracepoint logico e' un nome stabile che descrive un fatto interno di Alfred.
Non e' necessariamente una riga di log e non e' necessariamente una funzione.

Il contratto iniziale vive in
[Tracepoint Model v0](42-tracepoint-model-v0.md). Questo documento di roadmap
spiega la milestone; il modello v0 definisce naming, stabilita', metadati,
non-obiettivi, vincoli di percorso caldo e condizioni per diventare output
pubblico.

Esempi candidati:

```text
BACKEND_RAW_EVENT_READ
RAW_RECORD_BUILT
CORE_SEMANTIC_EVENT_EMITTED
WATCH_DIAGNOSTIC_EMITTED
OUTPUT_RECORD_ENQUEUED
OUTPUT_RECORD_DISPATCHED
SINK_RECORD_EMITTED
```

Il tracepoint deve restare stabile anche se il codice viene spostato da una
funzione a un'altra. Questo e' il motivo per cui il Lab non deve dipendere da
"la funzione X alla riga Y stampa questa stringa". Deve dipendere da un fatto
logico documentato.

## Cosa non e'

Un tracepoint logico non deve diventare:

- una seconda API parallela al record model;
- una scusa per fare I/O nel percorso caldo;
- un logger verbose sempre attivo;
- un nome legato a un dettaglio solo inotify se il concetto e' comune;
- una promessa che tutti i backend futuri avranno lo stesso livello di evidenza.

La regola prestazionale resta:

```text
backend -> normalizzazione minima -> record strutturato -> enqueue
```

Trace, Lab, report, JSONL, writer, socket, dashboard e spiegazioni devono stare
fuori dal percorso caldo, o essere opt-in, misurati e documentati.

## Relazione con record, log e JSONL

I tracepoint non sostituiscono `alfred_record_t`.

La relazione corretta e':

```text
alfred_record_t = contratto dati interno comune
events.log      = output umano compatibile
output.jsonl    = output macchina pubblico iniziale
tracepoint      = nome logico per spiegare uno step interno
scenario Lab    = percorso didattico che usa record, log, funzioni e test
```

Se un tracepoint diventa output esterno stabile, deve avere versione,
documentazione e test. Finche' resta solo un nome didattico o una mappa interna,
non va trattato come protocollo pubblico.

## Scenari MVP

La milestone deve partire da pochi scenari gia' maturi, non da tutti i casi
possibili. Il primo set MVP e' scelto per coprire quattro forme diverse di
spiegazione:

- un evento filesystem semplice;
- un evento che richiede interpretazione temporale;
- un evento che richiede correlazione interna;
- un evento diagnostico in cui Alfred spiega affidabilita' e recovery, non solo
  semantica filesystem.

| Scenario | Perche' e' utile | Funzioni e contratti coinvolti |
| --- | --- | --- |
| create file | Percorso base da evento backend a evento semantico. E' lo scenario piu' piccolo per spiegare `backend -> raw -> core -> output`. | inotify adapter, raw event, core, record, text/JSONL output. |
| close-write / file ready | Spiega la differenza fra modifica tecnica e file consumabile. E' importante per backup, indicizzatori e scanner. | core semantic, debounce/ready, eventi `FILE_MODIFIED` e `FILE_READY`. |
| rename/move/relocate | Mostra correlazione `MOVED_FROM` + `MOVED_TO`, cookie e classificazione. E' il primo scenario in cui Alfred fa piu' che inoltrare eventi raw. | move cache, `classify_move()`, eventi rename/move/relocate. |
| [watch stale / recovery](lab/scenarios/watch-stale-recovery.md) | Mostra diagnostica backend e differenza tra fatto osservato e affidabilita' del watch. E' il primo scenario Lab non puramente semantico. | watcher state, diagnostic record, log diagnostici, recovery. |

Altri scenari, come overflow, recursive mkdir, output pipeline completa e
future policy, possono essere aggiunti dopo. Il criterio e': prima pochi
percorsi spiegati bene, poi copertura piu' ampia.

### Tracepoint promossi per gli scenari MVP

Per questi scenari diventano `stable-doc` solo i tracepoint necessari a
spiegare il percorso. `stable-doc` non significa output pubblico e non introduce
codice runtime.

| Tracepoint | Scenario principale | Perche' serve |
| --- | --- | --- |
| `BACKEND_RAW_EVENT_READ` | tutti gli scenari filesystem | Indica che Alfred parte da evidenza backend/OS, non da un evento inventato dal Lab. |
| `RAW_EVENT_NORMALIZED` | create, close-write, rename/move | Indica il passaggio dal fatto backend alla forma raw Alfred usata dal core corrente. |
| `CORE_SEMANTIC_EVENT_EMITTED` | create, close-write, rename/move | Indica che il core ha prodotto semantica stabile, per esempio `FILE_CREATED`, `FILE_READY` o `FILE_RELOCATED`. |
| `MOVE_FROM_STORED` | rename/move/relocate | Indica che il core ha conservato la prima meta' del move in attesa del possibile match. |
| `MOVE_MATCH_FOUND` | rename/move/relocate | Indica che il core ha collegato `MOVED_FROM` e `MOVED_TO` usando cookie e contesto. |
| `WATCH_DIAGNOSTIC_EMITTED` | watch stale / recovery | Indica che Alfred sta spiegando stato e affidabilita' del watch, non un falso evento filesystem. |
| `SINK_RECORD_EMITTED` | tutti gli scenari con output osservabile | Indica che un sink ha ricevuto un record per produrre output umano o macchina. |

Tracepoint lasciati `candidate`:

| Tracepoint | Motivo del rinvio |
| --- | --- |
| `OUTPUT_RECORD_ENQUEUED` | Serve quando il primo scenario Lab include davvero la pipeline output opt-in `queue -> dispatcher -> sink`. |
| `OUTPUT_RECORD_DISPATCHED` | Come sopra: utile per Writer Runtime, ma non necessario al primo set scenario. |

### Scenari rimandati

| Scenario rimandato | Motivo |
| --- | --- |
| overflow | Importante, ma introduce perdita eventi e diagnostica di affidabilita' globale; va trattato dopo avere il linguaggio base. |
| recursive mkdir veloce | Utile e didattico, ma richiede spiegare scan sintetici e recovery discovery; meglio dopo gli scenari base. |
| output pipeline end-to-end | Serve per writer/runtime, ma rischia di spostare il primo Lab su queue/dispatcher invece che su semantica eventi. |
| policy / agent guardrail | Visione futura: richiede agent session, process context e policy, non ancora parte del Lab MVP. |
| performance trace | Deve aspettare benchmark e decisione esplicita su output trace opt-in. |

### Mappa iniziale funzioni, dati e test

La mappa dettagliata vive in
[Tracepoint Model v0](42-tracepoint-model-v0.md#mappa-tracepoint-funzioni-e-test).
Per orientarsi rapidamente, i quattro scenari MVP attraversano questi percorsi:

| Scenario | Percorso da spiegare nel Lab | Test da citare come evidenza |
| --- | --- | --- |
| create file | `inotify_backend_poll()` -> `inotify_adapter_build_raw()` -> `handle_backend_event()` -> `alfred_process()` -> `core_logger_on_event()` -> sink | `tests/core/test_create_file.sh`, `tests/backend/test_raw_create_record_sink.sh`, `tests/jsonl/test_create_file_and_dir_jsonl.sh` |
| close-write / file ready | raw `CLOSE_WRITE` normalizzato -> `alfred_process()` -> `FILE_READY` -> record/sink | `tests/core/test_modify_file.sh`, `tests/backend/test_raw_close_write_record_sink.sh`, `tests/jsonl/test_modify_file_jsonl.sh` |
| rename/move/relocate | `MOVED_FROM` salvato -> `MOVED_TO` correlato -> `classify_move()` -> evento semantico finale | `tests/core/test_rename_file.sh`, `tests/core/test_move_file.sh`, `tests/core/test_move_rename_file.sh`, test JSONL move/relocate |
| watch stale / recovery | watch state backend -> diagnostica `WATCH_*` -> record diagnostico -> sink/output | `tests/backend/test_watch_stale_output_failure.c`, `tests/backend/test_resync_output_routing.c`, test JSONL recovery |

Questa tabella non rende i tracepoint output pubblico. Serve a impedire che il
Lab venga costruito su supposizioni vaghe: ogni nome deve poter essere
ricondotto a funzioni e test reali.

## Formato scenario Lab v0

Decisione v0: uno scenario Lab e' un documento Markdown leggibile a mano, con
sezioni strutturate e nomi stabili. Non e' ancora YAML, JSON o un formato
parsato dal runtime.

Questa scelta e' intenzionale:

- uno studente deve poter leggere lo scenario senza strumenti speciali;
- un contributor deve poter collegare scenario, funzioni e test nella stessa
  pagina;
- le intestazioni stabili lasciano aperta la possibilita' di generare un
  formato macchina in futuro;
- non trasformiamo il Lab in un nuovo contratto runtime prima di avere scenari
  reali e verificati.

### Campi obbligatori

Ogni scenario Lab v0 deve contenere almeno:

```text
id:
  nome stabile dello scenario

title:
  titolo umano breve

goal:
  cosa deve capire il lettore

status:
  draft, stable-doc, public-output o deprecated

scenario kind:
  filesystem, diagnostic, output-pipeline o altro dominio futuro

trigger:
  comando o azione che produce il fatto osservato

expected evidence:
  fatti osservabili minimi, per esempio raw event, record, log o test

expected records:
  raw, semantic o diagnostic record attesi

logical tracepoints:
  nomi logici attraversati dallo scenario

function path:
  funzioni principali e helper coinvolti

state changes:
  strutture dati e campi modificati quando sono rilevanti

expected outputs:
  eventi testuali, JSONL o diagnostici osservabili

tests:
  test C o shell che proteggono il comportamento, oppure test mancanti

common failures:
  errori tipici, race, limiti backend o differenze ambientali

non-goals:
  cosa lo scenario non promette e non deve testare
```

Il campo `tests` puo' dichiarare tre stati:

```text
existing:
  test gia' presenti che proteggono il comportamento osservabile

missing:
  test necessari prima di promuovere lo scenario

future:
  test utili solo quando esistera' tracing runtime o output Lab pubblico
```

Questa distinzione evita due errori: fingere che un comportamento non testato
sia gia' stabile, oppure bloccare la documentazione perche' un test futuro non
puo' ancora esistere.

### Campi opzionali

Uno scenario puo' aggiungere:

```text
fixtures:
  directory, file o setup necessari

timing notes:
  debounce, race o attese intenzionali

environment assumptions:
  dipendenze Linux, filesystem, permessi o limiti CI

known flakiness:
  casi in cui l'ambiente puo' cambiare l'ordine o la presenza degli eventi

future parser hints:
  nomi o blocchi che un parser futuro potrebbe estrarre

related docs:
  link a modello tracepoint, contratto log, test o roadmap
```

### Esempio: create file

````markdown
# Scenario: create file

id: lab.fs.create-file.v0
title: Create a file and observe the semantic event
status: stable-doc
scenario kind: filesystem

## Goal

Show the shortest current path from an inotify create event to a semantic
`FILE_CREATED` record emitted through the sink boundary.

## Trigger

```text
touch a.txt
```

## Expected Evidence

- one backend raw create fact is read from inotify;
- the adapter builds one `alfred_raw_event_t`;
- the core emits one semantic `FILE_CREATED` event;
- the sink receives one corresponding `alfred_record_t`.

## Expected Records

- raw/normalized evidence for create;
- semantic filesystem record with type `FILE_CREATED`;
- no diagnostic record in the normal path.

## Logical Tracepoints

- `BACKEND_RAW_EVENT_READ`
- `RAW_EVENT_NORMALIZED`
- `CORE_SEMANTIC_EVENT_EMITTED`
- `SINK_RECORD_EMITTED`

## Function Path

```text
inotify_backend_poll()
-> backend_poll()
-> inotify_adapter_build_raw()
-> handle_backend_event()
-> alfred_process()
-> core_logger_on_event()
-> alfred_record_sink_emit()
```

## State Changes

- `watcher_table_t` resolves the watched parent path;
- `alfred_raw_event_t` carries mask, cookie and borrowed path fields;
- the core emits a new `alfred_event_t`;
- the logger adapter builds a semantic `alfred_record_t`.

## Expected Outputs

- text sink may emit compatibility text;
- JSONL sink may emit a structured semantic record;
- exact human log formatting is not the primary Lab contract.

## Tests

existing:
- `tests/core/test_create_file.sh`
- `tests/backend/test_raw_create_record_sink.sh`
- `tests/jsonl/test_create_file_and_dir_jsonl.sh`

missing:
- none for the documentation-only scenario v0

future:
- focused Lab output test if scenarios become machine-rendered

## Common Failures

- asserting a full text log line instead of the semantic record;
- expecting `FILE_READY`, which belongs to close-write/file-ready;
- treating the backend raw fact as final product semantics.

## Non-goals

- no runtime trace output is required;
- no queue/dispatcher behavior is asserted;
- no policy or agent guardrail decision is involved.
````

### Esempi brevi per gli altri scenari MVP

```text
close-write / file-ready:
  trigger: scrivere e chiudere un file
  focus: differenza tra modifica tecnica e file consumabile
  tracepoint: CORE_SEMANTIC_EVENT_EMITTED con tipo FILE_READY
  errore tipico: leggere il file prima della close-write

rename / move / relocate:
  trigger: mv old new, mv file dir/, move fuori scope
  focus: MOVED_FROM conservato, MOVED_TO correlato, classify_move()
  tracepoint: MOVE_FROM_STORED, MOVE_MATCH_FOUND
  errore tipico: aspettarsi due eventi semantici invece di uno

watch stale / recovery:
  trigger: spostamento o perdita affidabilita' di un watch
  focus: diagnostica backend e recovery, non falso FILE_*
  tracepoint: WATCH_DIAGNOSTIC_EMITTED
  errore tipico: trasformare un problema di monitoraggio in evento filesystem
```

Il formato scenario Lab v0 resta documentazione strutturata. Non richiede un
parser, non aggiunge I/O e non cambia il percorso caldo.

## Decisione su output trace runtime

Decisione #103: il Tracepoint/Lab MVP non implementa adesso `trace.jsonl` o
altro output trace runtime.

La scelta corrente e' deferire il runtime trace e scrivere prima scenari Lab
concreti usando il formato Markdown v0. Gli scenari devono appoggiarsi a:

- tracepoint `stable-doc`;
- funzioni e strutture dati reali;
- record Alfred gia' esistenti;
- output testuale/JSONL gia' disponibili;
- test rappresentativi gia' presenti o test mancanti dichiarati.

Questa decisione evita di introdurre un nuovo contratto pubblico prima di sapere
quali informazioni mancano davvero al Lab. Evita anche costi nel percorso caldo,
I/O aggiuntivo, confusione tra tracepoint e `alfred_record_t`, e test fragili
basati su un formato trace ancora non necessario.

Il runtime trace potra' essere riaperto solo dopo avere scritto scenari reali e
avere identificato una mancanza concreta. In quel caso dovra' nascere da una
issue dedicata con:

- campo e schema proposti;
- stato `public-output` dei tracepoint coinvolti;
- test focused su nomi, campi e ordering;
- vincoli di costo e comportamento nel percorso caldo;
- regole di perdita, best-effort o criticalita';
- documentazione per studenti, contributor e utenti.

## Scenari Lab concreti

Gli scenari concreti vivono in [Alfred Lab](lab/README.md).

Il primo MVP documentale contiene quattro scenari `stable-doc`:

| Scenario | Stato | Cosa verifica |
| --- | --- | --- |
| [Create file](lab/scenarios/create-file.md) | `stable-doc` | Percorso minimo da `IN_CREATE` a `RAW_CREATE`, `FILE_CREATED` e sink/output. |
| [Close-write / file-ready](lab/scenarios/file-ready.md) | `stable-doc` | Differenza fra `RAW_MODIFY`/`FILE_MODIFIED` e `RAW_CLOSE_WRITE`/`FILE_READY`. |
| [Rename / move / relocate](lab/scenarios/rename-move-relocate.md) | `stable-doc` | Correlazione `RAW_MOVED_FROM`/`RAW_MOVED_TO`, cookie, move table e classificazione in rename/move/relocate. |
| [Watch stale / recovery](lab/scenarios/watch-stale-recovery.md) | `stable-doc` | Diagnostica `WATCH_*`, path stale, resync e recovery lost-scope senza inventare semantica filesystem falsa. |

## Primo percorso architetturale

Il percorso didattico da rendere esplicito e':

```text
comando umano o operazione filesystem
    |
    v
kernel / inotify
    |
    v
backend / adapter
    |
    v
alfred_raw_event_t
    |
    v
core semantic
    |
    v
alfred_event_t / alfred_record_t
    |
    v
text sink / JSONL sink / diagnostic output
    |
    v
scenario Lab e spiegazione
```

Nella pipeline output opt-in il percorso include anche:

```text
alfred_record_t borrowed
    |
    v
alfred_record_clone_owned()
    |
    v
alfred_record_queue_t
    |
    v
alfred_record_runtime_drain_once()
    |
    v
alfred_record_dispatcher_dispatch_one()
    |
    v
alfred_record_sink_emit()
```

La milestone deve spiegare entrambi i percorsi senza confonderli: il percorso
core semantic corrente e il percorso output strutturato opt-in.

## Checklist operativa

La issue madre GitHub e' il piano operativo della milestone:

- [Tracepoint and Lab MVP: parent plan](https://github.com/kinderp/alfred/issues/92)

Checklist iniziale:

1. setup documentazione e registro milestone;
2. definire [Tracepoint Model v0](42-tracepoint-model-v0.md);
3. scegliere gli scenari MVP;
4. mappare tracepoint, funzioni, strutture dati e test;
5. decidere formato scenario Lab v0;
6. decidere se serve output trace runtime nel breve;
7. scrivere scenari Lab concreti prima di aggiungere un nuovo output trace;
8. aggiungere test focused solo per contratti nuovi realmente introdotti;
9. chiudere audit finale e debiti rimandati.

## Non obiettivi della milestone

Questa milestone non implementa:

- dashboard o UI web;
- animazioni generate automaticamente;
- Doxygen/Graphviz completo;
- trace runtime sempre attivo;
- tracing distribuito;
- OpenTelemetry exporter;
- Agent Guard completo;
- policy engine;
- nuovi backend come fanotify, audit, eBPF, Windows o macOS.

Questi temi restano importanti, ma non sono il primo MVP Lab.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- esiste un Tracepoint Model v0 documentato;
- i quattro scenari MVP scelti hanno mappa scenario -> tracepoint -> funzioni
  -> test;
- la documentazione spiega il percorso con abbastanza dettaglio per studenti e
  contributori;
- l'eventuale assenza di output trace runtime e' una scelta esplicita, non una
  lacuna nascosta;
- ogni output nuovo futuro sara' opt-in, versionato se pubblico e coperto da
  test;
- i debiti rimandati sono dichiarati nella issue madre e nel registro milestone.

La chiusura non richiede una UI Lab completa. Richiede un contratto piccolo,
chiaro e verificabile su cui una UI futura possa appoggiarsi.

## Stato di chiusura

La milestone `Tracepoint and Lab MVP` chiude come MVP documentale
`stable-doc`.

Risultato consolidato:

- `42-tracepoint-model-v0.md` definisce vocabolario, stabilita',
  anti-pattern, metadati minimi e relazione fra tracepoint, record e log;
- il primo set MVP resta intenzionalmente piccolo: create file,
  close-write/file-ready, rename/move/relocate e watch stale/recovery;
- i tracepoint necessari a questi scenari sono documentati come `stable-doc`;
- la mappa tracepoint -> funzioni -> dati -> test vive nel Tracepoint Model v0;
- `docs/it/lab/` contiene i quattro scenari Markdown v0 leggibili a mano;
- il runtime trace pubblico e' stato deliberatamente rimandato perche' gli
  scenari non hanno ancora dimostrato una mancanza concreta che richieda
  `trace.jsonl`.

Debiti rimandati deliberati:

- nessun parser scenario;
- nessuna UI o dashboard Lab;
- nessun `trace.jsonl` o output trace runtime;
- nessun tracepoint `public-output`;
- nessun nuovo I/O nel percorso caldo;
- nessun test focused nuovo, perche' questa milestone non introduce un nuovo
  formato macchina o comportamento runtime;
- scenari futuri come overflow, recursive mkdir, output pipeline end-to-end,
  policy/Agent Guard e performance trace.

Questi debiti non sono lacune nascoste della milestone: sono il perimetro
scelto per mantenere il Lab MVP piccolo, verificabile e utile agli studenti
prima di aggiungere strumenti automatici.
