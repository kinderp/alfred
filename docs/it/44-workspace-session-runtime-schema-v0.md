# Workspace/session runtime schema v0

Questo documento definisce la milestone `Workspace/session runtime schema v0`.

La milestone precedente,
[Agent workspace observe ledger v0](43-agent-workspace-observe-ledger-v0.md),
ha chiuso un contratto observe-mode: Alfred puo' leggere come ledger i record
strutturati gia' prodotti e runtime-routed, ma non ha ancora campi runtime per
descrivere workspace e sessione osservazionale.

Questa milestone prende solo quel debito minimo:

```text
workspace_root
workspace_id
ledger_session_id
```

Non implementa Agent Guard completo, policy, enforcement, process tree,
network, fanotify, eBPF, audit o attribuzione reale agente/processo.

## Obiettivo

L'obiettivo e' decidere come questi tre campi devono entrare nel contratto di
Alfred prima di scrivere codice runtime.

Le domande da chiudere sono:

1. qual e' il significato preciso di ogni campo?
2. qual e' la fonte di verita'?
3. dove vive il valore nel runtime?
4. chi possiede la memoria delle stringhe?
5. quando il valore viene copiato dentro un record?
6. come appare in JSONL?
7. quali test golden proteggono il contratto pubblico?
8. quale benchmark serve se aumentiamo dimensione record, copie stringa o
   volume JSONL?

La regola pratica e':

```text
prima source of truth, ownership, test e benchmark gate;
poi eventualmente schema C e JSONL.
```

## Non obiettivi

Questa milestone non introduce:

- `agent_session_id`;
- `agent_name`;
- `task_id`;
- `declared_intent`;
- process tree;
- rete;
- file read affidabili;
- classificazione `inside_workspace` / `outside_workspace`;
- decisioni `would_block`, `blocked`, `allowed` o `requires_approval`;
- policy engine;
- enforcement;
- integrazione LLM.

Questi campi e comportamenti restano futuri perche' richiedono una fonte di
verita' diversa dal solo inotify e, in molti casi, backend o motori non ancora
implementati.

## Campi candidati

| Campo | Significato | Fonte di verita' candidata | Stato iniziale |
| --- | --- | --- | --- |
| `workspace_root` | Root filesystem dichiarata come perimetro osservazionale | CLI/config futura, non inferenza automatica | Da progettare |
| `workspace_id` | Identificatore stabile e opaco del workspace | Alfred/config o derivazione esplicita documentata | Da progettare |
| `ledger_session_id` | Identificatore della sessione osservazionale Alfred | Runtime Alfred locale | Da progettare |

### `workspace_root`

`workspace_root` e' un path dichiarato. Non deve essere dedotto dal primo evento
osservato o dal prefisso comune dei path.

Rischio principale:

```text
se Alfred deduce il workspace dai path osservati, trasforma una supposizione in
contratto di sicurezza.
```

Per questo la fonte corretta deve essere esplicita: opzione CLI, file di config
o contesto runtime creato dall'utente o da un orchestratore fidato.

### `workspace_id`

`workspace_id` serve a riferirsi al workspace senza esporre sempre il path
testuale. Deve essere stabile abbastanza da collegare record della stessa
sessione, ma non deve promettere identita' globale se non viene progettata.

Domande aperte:

- e' configurato dall'utente?
- e' generato da Alfred a inizio sessione?
- e' derivato da `workspace_root` con hash?
- se e' un hash, quali dati include e quali rischi privacy introduce?

### `ledger_session_id`

`ledger_session_id` identifica una sessione osservazionale Alfred. Non significa
ancora sessione agente.

Esempio corretto:

```text
questa run di Alfred ha prodotto questi record osservazionali.
```

Esempio non corretto:

```text
l'agente Codex ha causato questi record.
```

Il secondo richiede `agent_session_id`, process context e un adapter agente o
orchestratore. Non appartiene a questa milestone.

## Possibili posizionamenti runtime

Prima di modificare C bisogna scegliere dove vivono questi valori.

| Opzione | Vantaggi | Rischi |
| --- | --- | --- |
| Campi dentro `alfred_record_t` | Ogni record e' autosufficiente, JSONL semplice | Piu' memoria, piu' copie stringa, piu' costo al confine queue |
| Contesto runtime separato | Evita copie per record, piu' leggero | Writer e sink devono accedere al contesto in modo chiaro |
| Enrichment nel writer JSONL | Non tocca hot path se il contesto e' stabile | JSONL contiene campi non presenti nel record interno |
| Record diagnostico/sessione separato | Pochi byte per record, sessione descritta una volta | Consumatori devono correlare record e sessione |

La scelta migliore non va fatta a sensazione. Deve dipendere da:

- costo sul percorso caldo;
- costo di clone owned;
- leggibilita' del JSONL;
- facilita' dei golden test;
- compatibilita' con futuri writer binari;
- privacy e minimizzazione dei dati.

## Ownership e lifetime

Se i campi entrano in `alfred_record_t`, devono seguire le regole gia' usate per
la memoria owned/borrowed dei record.

Domande da chiudere prima del codice:

- il valore e' borrowed dal contesto runtime?
- viene clonato quando entra in queue?
- chi distrugge la copia owned?
- cosa succede se la configurazione viene ricaricata?
- il valore puo' cambiare durante una run?

Regola iniziale:

```text
nessun puntatore borrowed deve sopravvivere al contesto che lo possiede.
```

Per questo la milestone deve decidere se il confine queue continua a fare copia
profonda dei campi stringa oppure se il writer puo' arricchire l'output da un
contesto stabile non per-record.

## JSONL e test

I campi diventano contratto pubblico solo quando entrano in JSONL.

Quando succede, servono:

- test formatter/sink per forma, escaping e omissione dei campi assenti;
- golden JSONL per almeno uno scenario filesystem reale;
- test per campo assente, per non fingere contesto non disponibile;
- aggiornamento del contratto log e di questo documento.

Regola:

```text
campo assente e campo vuoto non devono significare automaticamente la stessa
cosa.
```

Un campo assente puo' voler dire "Alfred non ha quel contesto". Un campo vuoto
potrebbe essere configurazione invalida o valore esplicitamente vuoto. Prima di
implementare bisogna scegliere e documentare.

## Benchmark gate

La milestone eredita il gate definito in
[Report benchmark prestazioni](34-report-benchmark-prestazioni.md) e nel ledger
observe-mode.

Serve benchmark refresh se:

- i campi entrano in `alfred_record_t`;
- aumenta la dimensione del record;
- aumenta il numero di stringhe clonate per record;
- aumenta il volume JSONL per ogni evento;
- cambia queue, clone owned, dispatcher, sink o writer;
- il writer arricchisce ogni record con lavoro per-event.

Non serve benchmark refresh per:

- setup documentale;
- decisione di naming;
- issue/roadmap;
- test che assertano comportamento gia' esistente senza cambiare runtime.

## Endpoint della milestone

La milestone puo' chiudersi quando esiste una decisione documentata su:

- significato dei tre campi;
- fonte di verita';
- posizionamento runtime;
- ownership e lifetime;
- forma JSONL;
- test/golden necessari;
- benchmark richiesti o esplicitamente non richiesti;
- debiti rimandati.

Solo dopo questo endpoint ha senso aprire una milestone o PR di implementazione
C/JSONL.
