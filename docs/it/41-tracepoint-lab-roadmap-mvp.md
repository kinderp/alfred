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
| watch stale / recovery | Mostra diagnostica backend e differenza tra fatto osservato e affidabilita' del watch. E' il primo scenario Lab non puramente semantico. | watcher state, diagnostic record, log diagnostici, recovery. |

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

## Formato scenario Lab v0

Uno scenario Lab v0 dovrebbe contenere almeno:

```text
id:
  nome stabile dello scenario

goal:
  cosa deve capire il lettore

trigger:
  comando o azione che produce il fatto osservato

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
  test C o shell che proteggono il comportamento

common failures:
  errori tipici, race, limiti backend o differenze ambientali
```

Questo formato e' pensato per essere leggibile a mano e generabile in futuro.
Non richiede ancora un parser.

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
6. implementare output trace minimo solo se serve e solo opt-in;
7. aggiungere test focused per ogni nuovo contratto;
8. chiudere audit finale e debiti rimandati.

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
- almeno tre scenari MVP hanno mappa scenario -> tracepoint -> funzioni -> test;
- la documentazione spiega il percorso con abbastanza dettaglio per studenti e
  contributori;
- ogni eventuale output nuovo e' opt-in, versionato se pubblico e coperto da
  test;
- i debiti rimandati sono dichiarati nella issue madre e nel registro milestone.

La chiusura non richiede una UI Lab completa. Richiede un contratto piccolo,
chiaro e verificabile su cui una UI futura possa appoggiarsi.
