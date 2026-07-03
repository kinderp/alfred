# Tracepoint Model v0

Questo documento definisce il contratto documentale iniziale dei tracepoint
logici di Alfred.

Per v0 e' una specifica di design: non introduce ancora un formato runtime,
non aggiunge output `trace.jsonl`, non cambia il percorso caldo e non modifica
il codice C. Serve a fissare il vocabolario prima di implementare qualunque
tracing stabile.

## Problema che risolve

Alfred ha gia' diversi livelli:

```text
evento kernel/backend
-> alfred_raw_event_t
-> core semantico
-> alfred_event_t / alfred_record_t
-> text sink / JSONL sink
```

Per gli studenti, i test e un futuro Alfred Lab non basta sapere che alla fine
compare `FILE_READY` o `FILE_RENAMED`. Serve anche spiegare quali passaggi
logici sono stati attraversati e quali funzioni o strutture dati li hanno
realizzati.

Il problema e' evitare due estremi:

- dipendere da righe testuali fragili come se fossero API;
- trasformare subito ogni dettaglio interno in un protocollo pubblico.

I tracepoint logici stanno in mezzo: danno nomi stabili agli step importanti,
ma non sostituiscono il record model.

## Definizione breve

Un tracepoint logico e' un nome stabile per uno stage osservabile del percorso
interno di Alfred.

Esempio:

```text
CORE_SEMANTIC_EVENT_EMITTED
```

significa:

```text
il core ha prodotto un evento semantico stabile
```

Non significa:

```text
questa riga di codice ha chiamato printf()
questa funzione deve esistere sempre con lo stesso nome
questo tracepoint e' gia' un campo JSONL pubblico
```

## Tracepoint, record e log

Il rapporto corretto e':

| Oggetto | Ruolo |
| --- | --- |
| `alfred_record_t` | Contratto dati strutturato interno comune. |
| `events.log` | Output umano compatibile e leggibile. |
| `output.jsonl` | Output macchina pubblico iniziale. |
| Tracepoint logico | Nome stabile di uno step interno. |
| Scenario Lab | Spiegazione che collega comando, tracepoint, record, funzioni, output e test. |

Il tracepoint non deve diventare il modello dati primario. Se un futuro output
trace pubblico verra' introdotto, dovra' serializzare informazioni derivate dal
record model o da metadati espliciti, non creare una seconda verita' parallela.

## Campi concettuali minimi

Per v0 un tracepoint documentato dovrebbe avere questi campi concettuali.

| Campo | Significato | Obbligatorio in doc v0? |
| --- | --- | --- |
| `name` | Nome stabile in maiuscolo, per esempio `RAW_RECORD_BUILT`. | Si' |
| `domain` | Area logica: `backend`, `core`, `record`, `output`, `diagnostic`, `lab`. | Si' |
| `stage` | Punto del percorso: read, normalize, correlate, emit, enqueue, dispatch, write. | Si' |
| `layer` | Layer del record collegato quando esiste: backend observed, normalized raw, semantic, diagnostic. | Quando applicabile |
| `record_link` | Quale record o tipo di record dimostra il fatto. | Quando applicabile |
| `function_path` | Funzioni principali che realizzano lo step oggi. | Si' per gli scenari Lab |
| `evidence` | Log, test o record che permettono di verificare lo step. | Si' |
| `hot_path` | Se lo step sta nel percorso caldo o fuori dal percorso caldo. | Si' |
| `stability` | `candidate`, `stable-doc`, `public-output` o `deprecated`. | Si' |

Questi campi non sono ancora una struct C. Sono il vocabolario minimo che deve
comparire nella documentazione di uno scenario o di una tabella tracepoint.

## Stati di stabilita'

Un tracepoint puo' avere quattro stati.

| Stato | Significato | Cosa promette |
| --- | --- | --- |
| `candidate` | Nome utile in roadmap o discussione, ma non ancora contratto. | Nulla: puo' cambiare. |
| `stable-doc` | Nome usato nella documentazione e negli scenari Lab. | Il nome non cambia senza aggiornare doc e scenari. |
| `public-output` | Nome esposto in un output macchina, per esempio futuro `trace.jsonl`. | Serve schema versionato, test e compatibilita'. |
| `deprecated` | Nome ancora riconosciuto o documentato per transizione. | Deve indicare sostituto o motivo della rimozione. |

Regola pratica:

```text
candidate -> stable-doc -> public-output
```

Non si salta direttamente da idea a output pubblico.

## Regole di naming

Per v0 usare nomi:

- in maiuscolo;
- con parole separate da `_`;
- orientati al fatto logico, non al nome esatto della funzione;
- abbastanza generali da sopravvivere a un refactor;
- non piu' generali di cio' che Alfred puo' davvero dimostrare.

Esempi buoni:

```text
BACKEND_RAW_EVENT_READ
RAW_RECORD_BUILT
CORE_SEMANTIC_EVENT_EMITTED
WATCH_DIAGNOSTIC_EMITTED
OUTPUT_RECORD_ENQUEUED
OUTPUT_RECORD_DISPATCHED
SINK_RECORD_EMITTED
```

Esempi deboli:

```text
LINE_123_PRINTED
INOTIFY_PRINTF_CALLED
EVENT_HANDLED
THING_DONE
JSON_WRITTEN_FROM_BACKEND
```

Perche' sono deboli:

- `LINE_123_PRINTED` dipende dalla posizione del codice;
- `INOTIFY_PRINTF_CALLED` confonde tracing e output testuale;
- `EVENT_HANDLED` e' troppo generico;
- `THING_DONE` non dice nulla;
- `JSON_WRITTEN_FROM_BACKEND` viola la regola architetturale: il backend non
  deve generare JSONL direttamente.

## Relazione con il percorso caldo

Un tracepoint puo' descrivere uno step del percorso caldo, ma non deve rendere
lo step piu' costoso.

Percorso caldo target:

```text
evento OS
-> backend / collector
-> normalizzazione minima
-> record strutturato
-> enqueue su coda / ring buffer
```

Nel percorso caldo non devono entrare:

- serializzazione JSONL;
- writer testuale;
- protobuf o MessagePack;
- socket send;
- flush file;
- policy pesante;
- Lab;
- allocazioni non necessarie;
- lock globali evitabili;
- plugin lenti.

Quindi un tracepoint nel percorso caldo deve essere, per ora, documentale o
compilabile fuori. Se in futuro verra' implementato runtime tracing, dovra'
essere opt-in, misurato e progettato per non far aspettare il backend.

## Regole per diventare output pubblico

Un tracepoint diventa output pubblico solo se:

1. ha nome stabile;
2. ha schema versionato;
3. e' documentato;
4. ha test focused;
5. dichiara se e' best-effort o critical;
6. dichiara se puo' essere perso in caso di pressione;
7. non dipende da stringhe umane instabili;
8. non duplica informazioni in modo incoerente rispetto ad `alfred_record_t`;
9. ha benchmark o ragionamento esplicito sul costo se tocca il percorso caldo.

Fino ad allora resta `candidate` o `stable-doc`.

## Template documentale per un tracepoint

Usare questa forma quando uno scenario Lab cita un tracepoint.

```text
Tracepoint: RAW_RECORD_BUILT
Status: stable-doc
Domain: record
Stage: normalize
Hot path: yes
Record link: alfred_record_t layer=normalized_raw
Function path:
  alfred_record_from_raw()
Evidence:
  output JSONL raw record when output.enabled=true
  focused record adapter tests
Meaning:
  Alfred has converted a backend raw event into the common record model.
Non-goal:
  It does not mean the event has already become semantic.
```

La sezione `Non-goal` e' importante: impedisce che un nome venga interpretato
piu' di quanto Alfred possa garantire.

## Tracepoint iniziali

Questa lista separa tracepoint gia' promossi a `stable-doc` per gli scenari MVP
da tracepoint ancora candidati.

`stable-doc` significa che il nome puo' essere usato in documentazione e scenari
Lab. Non significa output pubblico e non introduce codice runtime.

| Tracepoint | Stato | Dominio | Significato |
| --- | --- | --- | --- |
| `BACKEND_RAW_EVENT_READ` | stable-doc | backend | Il backend ha letto o ricevuto un fatto raw dalla sorgente OS. |
| `RAW_RECORD_BUILT` | stable-doc | record | Un raw event e' stato convertito nel record model comune o nella forma raw normalizzata usata dal percorso spiegato. |
| `CORE_SEMANTIC_EVENT_EMITTED` | stable-doc | core | Il core ha prodotto un evento semantico. |
| `MOVE_FROM_STORED` | stable-doc | core | Il core ha conservato la prima meta' di un move/rename in attesa del match. |
| `MOVE_MATCH_FOUND` | stable-doc | core | Il core ha collegato `MOVED_FROM` e `MOVED_TO` in una coppia coerente. |
| `WATCH_DIAGNOSTIC_EMITTED` | stable-doc | diagnostic | Alfred ha prodotto diagnostica osservabile sullo stato di un watch. |
| `SINK_RECORD_EMITTED` | stable-doc | output | Un sink concreto ha ricevuto un record da emettere. |
| `OUTPUT_RECORD_ENQUEUED` | candidate | output | Un record borrowed e' stato clonato/accodato nel percorso output opt-in. |
| `OUTPUT_RECORD_DISPATCHED` | candidate | output | Un record e' stato estratto dalla coda e passato al dispatcher. |

I tracepoint output-pipeline restano `candidate` perche' il primo set MVP
spiega semantica e diagnostica filesystem. Diventeranno `stable-doc` quando uno
scenario Lab scegliera' esplicitamente la pipeline `queue -> dispatcher -> sink`
come oggetto principale.

## Scenari MVP selezionati

Il primo set MVP contiene quattro scenari.

| Scenario | Tracepoint principali | Motivo della selezione |
| --- | --- | --- |
| create file | `BACKEND_RAW_EVENT_READ`, `RAW_RECORD_BUILT`, `CORE_SEMANTIC_EVENT_EMITTED`, `SINK_RECORD_EMITTED` | Percorso minimo da fatto backend a evento semantico e output. |
| close-write / file ready | `BACKEND_RAW_EVENT_READ`, `RAW_RECORD_BUILT`, `CORE_SEMANTIC_EVENT_EMITTED`, `SINK_RECORD_EMITTED` | Spiega perche' `FILE_READY` e' diverso da una modifica tecnica. |
| rename / move / relocate | `BACKEND_RAW_EVENT_READ`, `RAW_RECORD_BUILT`, `MOVE_FROM_STORED`, `MOVE_MATCH_FOUND`, `CORE_SEMANTIC_EVENT_EMITTED`, `SINK_RECORD_EMITTED` | Mostra correlazione, cache move e classificazione semantica. |
| watch stale / recovery | `BACKEND_RAW_EVENT_READ`, `WATCH_DIAGNOSTIC_EMITTED`, `SINK_RECORD_EMITTED` | Mostra diagnostica e affidabilita' del monitoraggio, non solo eventi filesystem. |

Questa selezione e' volutamente stretta. Se il primo Lab prova a coprire subito
overflow, recursive mkdir, output pipeline completa e policy, diventa troppo
grande per essere verificabile e didattico.

## Anti-pattern

### Usare tracepoint come logger verbose

Sbagliato:

```text
ogni funzione produce sempre una riga trace
```

Perche': aumenta rumore, I/O e fragilita' dei test.

### Usare tracepoint come API interna primaria

Sbagliato:

```text
il core consuma tracepoint invece di record strutturati
```

Perche': il contratto interno primario resta il record model.

### Far decidere semantica al backend

Sbagliato:

```text
INOTIFY_RANSOMWARE_DETECTED
```

Perche': il backend osserva e normalizza; pattern, policy e rischio appartengono
a livelli superiori.

### Accoppiare il Lab alle stringhe testuali

Sbagliato:

```text
Lab scenario passes if events.log contains exactly this full line
```

Perche': `events.log` e' utile per umani e compatibilita', ma il Lab deve
appoggiarsi a record, tracepoint documentati e test robusti.

## Esempio: create file

Scenario:

```text
touch a.txt
```

Tracepoint candidati attraversati:

```text
BACKEND_RAW_EVENT_READ
RAW_RECORD_BUILT
CORE_SEMANTIC_EVENT_EMITTED
SINK_RECORD_EMITTED
```

Interpretazione:

- il backend vede un evento filesystem;
- Alfred costruisce un record raw normalizzato;
- il core produce un evento semantico come `FILE_CREATED`;
- un sink emette output umano o macchina.

Nessuno di questi tracepoint, da solo, dice che il file e' pronto per essere
consumato. Per quello serve lo scenario close-write / file-ready.

## Esempio: output pipeline opt-in

Percorso:

```text
alfred_record_t borrowed
-> alfred_record_clone_owned()
-> alfred_record_queue_t
-> alfred_record_runtime_drain_once()
-> alfred_record_dispatcher_dispatch_one()
-> alfred_record_sink_emit()
```

Tracepoint candidati:

```text
OUTPUT_RECORD_ENQUEUED
OUTPUT_RECORD_DISPATCHED
SINK_RECORD_EMITTED
```

Qui il punto didattico e' la ownership: il record borrowed non puo' vivere in
coda senza copia profonda dei campi owned. Il tracepoint deve aiutare a
spiegare il confine, non nascondere il costo della copia o la responsabilita' di
destroy.

## Criteri per il prossimo step

Prima di implementare tracing runtime bisogna completare almeno:

1. scelta degli scenari MVP;
2. promozione di pochi tracepoint da `candidate` a `stable-doc`;
3. mappa tracepoint -> funzioni -> record -> test;
4. decisione sul formato scenario Lab v0;
5. decisione esplicita se serve davvero un output `trace.jsonl` nel breve.

Questa sequenza evita di aggiungere codice prima di sapere che cosa il codice
deve dimostrare.
