# Metadata/session record JSONL v0

Questo documento definisce la milestone `Metadata/session record JSONL v0`.

La milestone precedente,
[Workspace/session runtime context v0](45-workspace-session-runtime-context-v0.md),
ha introdotto un contesto runtime app-owned per:

```text
workspace_root
workspace_id
ledger_session_id
```

Questi valori esistono nel runtime, ma non sono ancora pubblicati nei record o
in JSONL. Questa milestone decide e implementa il primo modo pubblico per
esporli senza pagare un costo per ogni evento filesystem.

## Obiettivo

L'obiettivo e' emettere un record metadata/sessione separato, serializzato in
JSONL quando l'output strutturato e' abilitato.

La regola principale e':

```text
il contesto della run si pubblica una volta come metadata/session record,
non si copia automaticamente su ogni evento filesystem.
```

Questo mantiene corto il percorso caldo e separa due concetti diversi:

- gli eventi filesystem descrivono fatti osservati;
- il record metadata/sessione descrive il contesto dichiarato della run.

## Perche' un record separato

Ripetere `workspace_root`, `workspace_id` e `ledger_session_id` su ogni record
sembra comodo, ma ha costi e rischi:

- aumenta la dimensione di ogni record JSONL;
- aumenta le copie stringa se i campi entrano in `alfred_record_t`;
- aumenta il lavoro del clone owned quando un record attraversa la queue;
- rende piu' facile confondere contesto dichiarato con evidenza osservata;
- espone piu' spesso path potenzialmente sensibili.

Un record separato invece dice:

```text
questa run ha questo contesto dichiarato;
gli eventi successivi appartengono allo stesso stream/output.
```

Il consumatore esterno puo' associare il contesto agli eventi usando
`ledger_session_id`, ordine stream o metadati del file, senza costringere il
backend a copiare stringhe per ogni evento.

## Non obiettivi

Questa milestone non introduce:

- arricchimento per-evento con workspace/sessione;
- `agent_session_id`;
- `agent_name`;
- task, prompt o declared intent;
- attribuzione processo/agente;
- process tree;
- rete;
- file read affidabili;
- policy `allowed`, `would_block`, `blocked` o `requires_approval`;
- Agent Guard;
- enforcement;
- generazione automatica di `workspace_id` o `ledger_session_id`;
- normalizzazione/canonicalizzazione di `workspace_root`;
- backend fanotify/eBPF/audit.

Se uno di questi punti diventa necessario, deve avere una issue dedicata e un
contratto separato.

## Record candidato

Il record candidato e':

```text
layer    = diagnostic
category = lifecycle
type     = SESSION_CONTEXT
```

Il primo micro-step della milestone ammette `lifecycle` e `SESSION_CONTEXT` come
valori controllati del modello record e della mappatura JSONL. Questo significa
che il record puo' essere nominato in modo stabile:

- `alfred_record_category_t`;
- `alfred_record_type_t`;
- la mappatura dei nomi nel formatter JSONL.

Questa espansione e' parte del contratto pubblico perche'
`alfred_record_format_jsonl()` rifiuta layer, category o type sconosciuti. Il
record non deve quindi nascere come stringa libera in `app.c`: prima il nome va
ammesso dal modello record e coperto da un test focused.

La tupla ammessa in v0 e' solo:

```text
diagnostic + lifecycle + SESSION_CONTEXT
```

Le combinazioni simili ma sbagliate devono essere rifiutate dal formatter. Per
esempio:

- `semantic + filesystem + SESSION_CONTEXT` e' invalida, perche' il contesto
  sessione non e' un evento filesystem semantico;
- `diagnostic + lifecycle + FILE_CREATED` e' invalida, perche' `FILE_CREATED`
  appartiene alla semantica filesystem;
- `diagnostic + watch + SESSION_CONTEXT` e' invalida, perche' il contesto
  sessione non descrive lo stato di un watch.

Questo step stabilizza anche il payload JSONL v0 del record. Resta invece
rimandata l'emissione runtime: Alfred sa formattare e validare il record, ma
non lo produce ancora automaticamente all'avvio della run.

Motivo:

- non e' un evento filesystem semantico;
- non e' un raw event backend;
- non e' una decisione di policy;
- descrive stato/configurazione osservabile della run;
- deve essere consumabile da test, JSONL, Lab e futuri ledger.

Il nome `SESSION_CONTEXT` e' intenzionalmente tecnico. Non usa `AGENT_SESSION`,
perche' questa milestone non sa ancora nulla di un agente AI reale.

Alternative da non scegliere in v0:

| Alternativa | Perche' no in v0 |
| --- | --- |
| `semantic/filesystem` | Farebbe sembrare il contesto un fatto filesystem osservato |
| `security/policy` | Implicherebbe policy o decisioni non implementate |
| `backend/lifecycle` | Lo farebbe sembrare proprieta' del backend inotify |
| campi su ogni evento | Aumenta costo e ambiguita' senza bisogno immediato |

## Payload JSONL v0

Il record JSONL contiene solo campi presenti e non vuoti. Il mapping dal record
C al JSONL e' questo:

| Campo C | Campo JSONL | Significato |
| --- | --- | --- |
| `record.session.workspace_root` | `workspace.root` | root workspace dichiarata esplicitamente |
| `record.session.workspace_id` | `workspace.id` | identificatore opaco del workspace |
| `record.session.ledger_session_id` | `ledger.session_id` | identificatore opaco della run/ledger Alfred |

Questi campi vivono in `alfred_record_session_t`, dentro `alfred_record_t`.
Non sono campi del backend inotify, non sono dedotti dagli eventi filesystem e
non sono attribuzione di processo o agente.

Esempio completo:

```json
{
  "schema_version": 0,
  "layer": "diagnostic",
  "category": "lifecycle",
  "type": "SESSION_CONTEXT",
  "workspace": {
    "root": "/home/antonio/alfred",
    "id": "ws-alfred-local"
  },
  "ledger": {
    "session_id": "ls-2026-07-14-local"
  }
}
```

Esempio con solo `ledger_session_id`:

```json
{
  "schema_version": 0,
  "layer": "diagnostic",
  "category": "lifecycle",
  "type": "SESSION_CONTEXT",
  "ledger": {
    "session_id": "ls-2026-07-14-local"
  }
}
```

Regole v0:

- se tutti e tre i valori sono assenti, il futuro builder runtime non deve
  accodare un `SESSION_CONTEXT` solo decorativo; il formatter puo' comunque
  serializzare un record senza payload nei test focused che fissano il nome
  controllato `diagnostic + lifecycle + SESSION_CONTEXT`;
- se `workspace_root` e' assente, omettere `workspace.root`;
- se `workspace_id` e' assente, omettere `workspace.id`;
- se entrambi i campi workspace sono assenti, omettere l'oggetto `workspace`;
- se `ledger_session_id` e' assente, omettere l'oggetto `ledger`;
- trattare le stringhe vuote come assenti;
- usare escaping JSONL corrente;
- non aggiungere campi agente, processo o policy;
- rifiutare il payload `session` su qualsiasi record diverso da
  `diagnostic + lifecycle + SESSION_CONTEXT`.

L'ultima regola e' importante: se un produttore mette per errore
`record.session.workspace_id` su un record come `FILE_CREATED`, il formatter
deve fallire invece di ignorare silenziosamente il campo. In questo modo Alfred
non introduce per-record enrichment accidentale e non fa sembrare il contesto
della run una prova osservata su ogni evento filesystem.

## Quando emettere il record

Il punto candidato e' dopo l'inizializzazione del contesto runtime e prima che
gli eventi filesystem ordinari inizino ad attraversare l'output strutturato.

Percorso concettuale:

```text
config_load()
-> app_init_workspace_session_context()
-> output pipeline init
-> build SESSION_CONTEXT record
-> alfred_record_output_pipeline_enqueue()
-> queue
-> dispatcher
-> JSONL sink
```

Il record deve usare la pipeline strutturata esistente. Non deve scrivere JSONL
direttamente da `app.c` e non deve bypassare queue, dispatcher o sink.

## Ownership

Il contesto runtime e' owned da `app_t` e usa buffer inline. Il record candidato
puo' essere costruito come borrowed view verso `app_t.workspace_session` solo
se viene immediatamente accodato tramite l'output pipeline corrente, che clona
owned al confine della queue.

Regola:

```text
nessun puntatore borrowed verso app_t puo' sopravvivere oltre l'enqueue.
```

Se il record non viene accodato, puo' essere consumato solo sincronicamente. Se
viene accodato, deve attraversare il clone owned della queue come gli altri
record.

## Test richiesti

Prima di considerare pubblico il contratto JSONL servono golden test focused.

Casi minimi:

1. `SESSION_CONTEXT` e `lifecycle` sono valori ammessi dal modello record;
2. il formatter JSONL produce i nomi pubblici attesi;
3. il formatter rifiuta `SESSION_CONTEXT` con layer o category sbagliati;
4. il formatter rifiuta altri type sotto `diagnostic + lifecycle`.

Per il payload JSONL v0 servono test formatter focused che coprano:

1. tutti i campi configurati: record completo;
2. solo `workspace_root`: oggetto `workspace` con `root`;
3. solo `workspace_id`: oggetto `workspace` con `id`;
4. solo `ledger_session_id`: oggetto `ledger`;
5. stringhe vuote trattate come assenti;
6. caratteri da escapare in path/id: JSONL valido;
7. nessun campo agente/policy/processo presente;
8. payload sessione rifiutato su record non `SESSION_CONTEXT`.

Il futuro collegamento runtime dovra' aggiungere test end-to-end separati:

1. nessun campo configurato: nessun `SESSION_CONTEXT` runtime;
2. record emesso una sola volta per run;
3. ordine del record rispetto ai record osservativi;
4. eventi filesystem successivi non duplicano i campi workspace/sessione.

Questi test devono proteggere il contratto pubblico. I test di configurazione
gia' esistenti restano necessari, ma non bastano per questa milestone.

## Benchmark gate

La fase documentale non richiede benchmark.

Un refresh diventa obbligatorio quando il codice:

- aggiunge il record metadata/sessione al percorso runtime;
- cambia `alfred_record_t`;
- aggiunge nuove stringhe al clone owned;
- cambia JSONL formatter;
- cambia queue, dispatcher, sink o output pipeline;
- emette il record in un punto che puo' influenzare startup/shutdown misurati.

Benchmark minimo candidato:

```bash
make perf-runtime-output
```

Se il record resta one-shot e non modifica i record evento, ci aspettiamo un
impatto trascurabile sul throughput per-evento. Questo va comunque verificato
quando la milestone passa da documento a codice.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- il contratto del record metadata/sessione e' documentato;
- il record viene emesso solo quando esiste contesto configurato;
- l'output passa dalla pipeline strutturata;
- i golden JSONL coprono presenza, assenza, escaping e non-obiettivi;
- la documentazione e le pagine man, se toccate, sono aggiornate;
- il benchmark gate e' rispettato o motivatamente dichiarato non necessario;
- la issue madre e il registro milestone sono aggiornati.
