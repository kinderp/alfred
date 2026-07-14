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

| Campo | Significato | Fonte di verita' v0 | Stato dopo questo step |
| --- | --- | --- | --- |
| `workspace_root` | Root filesystem dichiarata come perimetro osservazionale | Valore esplicito da CLI/config/orchestratore fidato | Semantica e fonte definite; runtime placement futuro |
| `workspace_id` | Identificatore opaco del workspace dichiarato o generato da Alfred | Valore esplicito o generazione Alfred documentata, mai inferenza silenziosa | Semantica e fonte definite; algoritmo futuro |
| `ledger_session_id` | Identificatore della run/sessione osservazionale Alfred | Runtime Alfred locale all'avvio della run | Semantica e fonte definite; algoritmo futuro |

## Regole generali di source of truth

Questi campi descrivono contesto operativo. Non sono fatti osservati dal
backend inotify e non sono prove di causalita' agente/processo.

La fonte di verita' v0 deve rispettare queste regole:

1. un valore deve essere esplicito oppure generato da Alfred con una regola
   documentata;
2. Alfred non deve dedurre automaticamente il workspace da path osservati, PID,
   timestamp, nome processo o contenuto dei file;
3. un campo assente significa "contesto non disponibile", non "fuori
   workspace" e non "policy violata";
4. un valore presente descrive il contesto dichiarato della run, non dimostra
   che un agente abbia causato un record;
5. ogni algoritmo futuro di generazione deve essere deterministicamente
   documentato o dichiarato esplicitamente come opaco/non stabile.

Questa distinzione e' importante per la sicurezza: confondere un contesto
dichiarato con una prova osservata trasformerebbe Alfred da ledger affidabile a
sistema che attribuisce responsabilita' senza evidenza.

## Fonti vietate in v0

Per v0 questi campi non possono essere derivati da:

| Fonte vietata | Perche' e' vietata |
| --- | --- |
| Primo path osservato | Il primo evento puo' essere fuori scope o arrivare dopo altri fatti persi |
| Prefisso comune dei path | I path osservati sono effetti, non dichiarazione di perimetro |
| Directory corrente di Alfred | Alfred puo' essere lanciato da una directory diversa dal workspace |
| PID, PPID o nome processo | Inotify non fornisce attribuzione processo affidabile nel modello corrente |
| Timestamp di evento filesystem | Identifica un fatto osservato, non una sessione o un workspace |
| Nome agente o prompt | Non esiste ancora adapter agente fidato in questa milestone |
| Contenuto dei file | Sarebbe invasivo, costoso e fuori dallo scope observe-ledger v0 |

Se in futuro una fonte vietata diventa supportabile, deve passare da una
milestone esplicita con backend, test, privacy review e benchmark adeguati.

## Relazione fra `workspace_root` e `workspace_id`

`workspace_root` e `workspace_id` sono collegati, ma non sono equivalenti.

Regole v0:

- `workspace_root` e' il solo campo che puo' descrivere un perimetro
  filesystem dichiarato;
- `workspace_id` e' solo una label opaca di correlazione e non prova che un
  path sia dentro o fuori dal workspace;
- `workspace_root` puo' essere presente senza `workspace_id`: in quel caso
  Alfred conosce il perimetro dichiarato ma non ha ancora una label opaca;
- `workspace_id` puo' essere presente senza `workspace_root`: in quel caso
  Alfred puo' raggruppare record per workspace dichiarato, ma non puo'
  interpretare quel valore come boundary filesystem;
- quando entrambi sono presenti, devono provenire dalla stessa configurazione o
  dallo stesso contesto runtime dichiarato;
- se una PR futura genera `workspace_id` da `workspace_root`, deve dichiarare
  algoritmo, stabilita', normalizzazione, rischi privacy e casi di collisione.

Queste regole evitano un errore importante: un consumatore JSONL non deve
trattare un record con solo `workspace_id` come prova che il path osservato sia
contenuto in un workspace specifico.

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

Contratto v0:

- rappresenta una root locale dichiarata per interpretare una run;
- non implica che Alfred possa bloccare accessi fuori root;
- non implica che tutti i record siano dentro quella root;
- non deve essere normalizzato usando eventi osservati;
- se configurato come path relativo, l'eventuale risoluzione deve essere
  definita in una PR di implementazione, non lasciata al writer;
- se assente, Alfred deve omettere il contesto invece di inventarlo.

Esempio corretto:

```text
utente/orchestratore avvia Alfred con workspace_root=/home/user/progetto
```

Esempio non corretto:

```text
Alfred osserva /home/user/progetto/src/main.c e decide che /home/user/progetto
e' il workspace.
```

### `workspace_id`

`workspace_id` serve a riferirsi al workspace senza esporre sempre il path
testuale. Deve essere stabile abbastanza da collegare record della stessa
sessione, ma non deve promettere identita' globale se non viene progettata.

Contratto v0:

- identifica il workspace dichiarato, non il filesystem object osservato;
- deve essere opaco per i consumatori: non devono parsarlo come path o hash
  semantico;
- puo' essere fornito esplicitamente oppure generato da Alfred;
- se generato da Alfred, la PR di implementazione deve dichiarare se e' stabile
  solo per la run, per quella macchina o per quel path normalizzato;
- non deve essere derivato silenziosamente da `workspace_root` senza dichiarare
  algoritmo, stabilita' e rischi privacy;
- se assente, i record restano validi ma non correlabili per workspace id.

Scelte ancora future:

- formato (`ws_...`, UUID, hash, contatore locale);
- stabilita' richiesta;
- algoritmo di generazione;
- eventuale configurazione utente;
- relazione con privacy e minimizzazione del path.

Esempio corretto:

```text
workspace_id=ws_7f3a... e' un identificatore opaco assegnato alla run o alla
configurazione.
```

Esempio non corretto:

```text
un consumatore assume che workspace_id sia sempre SHA-256(workspace_root).
```

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

Contratto v0:

- identifica la run o finestra osservazionale di Alfred;
- e' prodotto dal runtime Alfred, non dal backend;
- non e' un `agent_session_id`;
- non attribuisce causalita' a processi o agenti;
- puo' essere usato per raggruppare record emessi dalla stessa run;
- deve essere opaco per i consumatori;
- se Alfred non lo emette ancora, il ledger observe-mode resta valido ma non ha
  identificatore di sessione.

Scelte ancora future:

- algoritmo di generazione;
- quando viene creato;
- se cambia dopo reload di configurazione;
- se viene scritto una volta come record sessione o ripetuto su ogni record;
- come viene preservato in replay.

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
