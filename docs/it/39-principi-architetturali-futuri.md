# Principi architetturali futuri

Questo documento traduce la visione di Alfred come Observation Runtime in
principi pratici. Non e' una lista di feature da implementare subito. E' una
checklist concettuale da usare quando una scelta di codice o di modello dati
potrebbe chiudere il progetto dentro il solo filesystem o dentro il solo
backend inotify.

## Principio 1: ogni fatto entra come osservazione

Nel lungo periodo Alfred deve poter rappresentare piu' di un evento.

```text
evento filesystem
misura
percezione
inferenza
previsione
azione
esito
feedback
```

Oggi il codice usa `alfred_record_t` e `alfred_event_t`. Non va rinominato per
ragioni speculative. Pero' ogni nuovo campo comune deve essere valutato con
questa domanda:

```text
questo concetto vale solo per inotify o puo' valere per una osservazione
prodotta da qualunque sensore?
```

## Principio 2: provenance obbligatoria

Ogni record importante deve poter rispondere:

- chi lo ha prodotto?
- quale backend, adapter o modulo lo ha osservato?
- quando e' stato osservato?
- quando e' entrato in Alfred?
- quali dati grezzi lo supportano?
- e' raw, normalizzato, semantico, inferito o deciso?

Senza provenance Alfred diventa un log opaco. Con provenance diventa un sistema
di audit e spiegazione.

## Principio 3: osservazioni e inferenze restano separate

Un backend osserva. Il core interpreta. Un policy engine decide. Un action layer
agisce.

Non bisogna mescolare questi livelli.

Esempio corretto:

```text
inotify adapter:
  RAW_MODIFY

core filesystem:
  FILE_MODIFIED

future inference layer:
  sensitive_file_modified

future policy layer:
  WOULD_BLOCK

future action layer:
  alert_written oppure process_frozen
```

Esempio sbagliato:

```text
inotify backend:
  decide direttamente "comportamento malevolo"
```

La separazione protegge test, spiegabilita', benchmark e sostituibilita' dei
backend.

## Principio 4: il core non dipende da un dominio

Il core comune non deve conoscere concetti obbligatori validi solo per un
dominio:

- `wd`;
- `IN_*`;
- `cookie` inotify;
- path Linux obbligatorio;
- inode obbligatorio;
- filesystem come unica categoria possibile.

Questi dettagli vanno conservati, ma nel payload o nei dettagli specifici del
backend.

Il record comune deve invece poter rappresentare concetti generali:

- `source`;
- `domain`;
- `kind` o livello;
- `type`;
- `subject` o actor;
- `object`;
- `action`;
- `context`;
- `evidence`;
- `confidence`;
- relazioni/correlazioni.

## Principio 5: i backend sono sensori con capabilities

Un backend non dovrebbe essere solo codice che emette record. Deve dichiarare
cosa sa fare.

Esempio per il backend inotify:

```text
sensor/backend:
  name: inotify
  domain: filesystem

observes:
  create
  modify
  delete
  move
  attrib
  overflow

entities:
  file
  directory
  watch

capabilities:
  recursive_watch: yes, implemented by Alfred watch manager
  blocking: no
  process_context: no
  network_context: no
  identity_tracking: partial
  can_overflow: yes

limitations:
  cannot reliably identify the process that caused an event
  cannot block access before it happens
  may lose events on kernel queue overflow
```

Questa descrizione sara' importante per policy e degrade mode futuri. Una
policy puo' chiedere `deny`, ma se il backend corrente non puo' bloccare Alfred
deve produrre `would_block` o `alert`, non fingere enforcement.

## Principio 6: lo stato e' derivato dal log

Il log append-only e' la fonte primaria. Le viste di stato sono derivate.

```text
record log
    |
    v
projectors
    |
    +--> file_state
    +--> watch_state
    +--> process_state futuro
    +--> session_state futuro
    +--> risk_state futuro
```

Questo principio rende possibili:

- replay;
- debug;
- test golden;
- ricostruzione di bug;
- Lab;
- audit;
- confronto tra versioni del core;
- projection diverse sullo stesso stream.

Nel codice corrente non tutto e' ancora costruito cosi'. Il principio serve a
guidare i prossimi refactor, non a riscrivere immediatamente tutto.

## Principio 7: il percorso caldo resta corto

La visione futura non deve peggiorare il percorso caldo.

Il target resta:

```text
evento OS
-> backend/sensor
-> normalizzazione minima
-> record strutturato
-> enqueue bounded
```

Fuori dal percorso caldo:

- JSONL;
- text writer;
- protobuf;
- MessagePack;
- socket;
- Lab;
- knowledge graph;
- projection pesanti;
- inference;
- policy pesante;
- LLM;
- report.

Se una idea di Observation Runtime richiede allocazioni pesanti, query grafiche
o encoding costoso per ogni evento, deve stare a valle della coda o in un
profilo separato.

## Principio 7 bis: le astrazioni devono pagare affitto

Architettura esagonale, porte, adapter, plugin-like API, dispatcher e projection
sono strumenti, non obiettivi. Vanno introdotti solo quando risolvono un
problema reale di Alfred.

Ogni nuova astrazione deve rispondere a queste domande:

- quale dipendenza elimina?
- quale test rende piu' semplice?
- quale backend o writer futuro abilita concretamente?
- quale rischio riduce?
- quale costo aggiunge sul percorso caldo?
- quale benchmark dimostra che il costo e' accettabile?

Se la risposta e' solo "rende l'architettura piu' pulita", la modifica e'
sospetta. La pulizia deve essere visibile in codice piu' semplice, confini piu'
chiari, test migliori, performance preservate o sicurezza piu' forte.

Regola:

```text
Architecture must pay rent.
```

Per Alfred significa che ogni astrazione deve comprare chiarezza,
testabilita', affidabilita', sicurezza, estensibilita' necessaria o prestazioni
misurate. Altrimenti resta roadmap, non codice.

La discussione GitHub di riferimento per questa scelta e':
[Architecture direction: lightweight ports-and-adapters for Alfred](https://github.com/kinderp/alfred/discussions/44).

## Principio 8: versionare modello, API e output separatamente

Non basta versionare il codice.

Alfred dovrebbe trattare come contratti distinti:

- Event/Observation Model;
- Backend API;
- Writer API;
- JSONL schema;
- Pattern library futura;
- Policy schema futuro.

Questo permette di evolvere una parte senza confondere tutte le altre.

Esempio:

```text
Alfred binary: 0.x
Event Model: v0
Backend API: v0
Writer API: v0
JSONL schema: v0
```

## Principio 9: azioni, decisioni e feedback sono record

Nel futuro Alfred non dovra' registrare solo cio' che arriva dal sistema
operativo. Dovra' registrare anche cio' che decide e cio' che fa.

Esempi futuri:

```text
POLICY_DECISION would_block
ACTION_REQUEST freeze_process
ACTION_RESULT freeze_process succeeded
USER_APPROVAL allow_once
PREDICTION_MADE risk_high
OUTCOME_OBSERVED risk_reduced
LEARNING_UPDATE pattern_weight_changed
```

Questo rende ispezionabile il ciclo:

```text
osservazione -> previsione -> decisione -> azione -> outcome -> feedback
```

Per ora questi record restano roadmap. Ma il modello non deve impedirli.

## Principio 10: LLM come adapter, non come fonte di verita'

Un LLM puo' essere utilissimo, ma non deve diventare la memoria primaria o il
motore unico di decisione.

Ruoli adatti:

- spiegare record;
- riassumere sessioni;
- generare query;
- proporre policy;
- tradurre richieste utente in vincoli;
- aiutare nel Lab.

Ruoli da evitare:

- decidere enforcement critico senza policy deterministica;
- essere l'unico posto in cui vive la memoria;
- sostituire provenance, log e replay;
- introdurre output non verificabili nel percorso caldo.

Se un LLM produce una inferenza, quella inferenza deve diventare un record con
source, modello, input, confidenza ed evidence, non una verita' implicita.

## Checklist per nuove scelte architetturali

Prima di introdurre un nuovo campo comune, modulo o API chiedersi:

1. Questo concetto e' generale o specifico di inotify?
2. Il core puo' usarlo senza conoscere il backend sorgente?
3. Esiste provenance sufficiente?
4. Stiamo distinguendo raw, semantica, inferenza e decisione?
5. La modifica entra nel percorso caldo?
6. Serve benchmark prima di renderla default?
7. Il record resta serializzabile in JSONL senza perdere significato?
8. La scelta prepara replay o lo rende piu' difficile?
9. La scelta introduce una nuova superficie d'attacco?
10. La documentazione spiega cosa e' scope corrente e cosa e' visione futura?

## Applicazione pratica alla roadmap corrente

Impatto immediato:

- continuare Backend API v0;
- rendere capabilities esplicite;
- non introdurre policy nel backend;
- evitare campi obbligatori troppo filesystem-specific nel record comune;
- preparare source/evidence/confidence come direzione futura;
- mantenere writer e serializzazione fuori dal percorso caldo.

Impatto rimandato:

- rinominare `record` in `observation`;
- knowledge graph runtime;
- prediction layer;
- policy engine completo;
- action engine;
- LLM integration;
- sensori non-security come video, GPS o browser.

La scelta pragmatica e':

```text
costruire Alfred Security oggi,
senza chiudere la porta a un Alfred Observation Runtime domani.
```
