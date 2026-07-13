# Agent workspace observe ledger v0

Questo documento definisce il primo contratto operativo per la milestone
`Agent workspace observe ledger`.

La roadmap strategica resta
[AI agent guardrail](24-roadmap-ai-agent-guardrail.md). Qui pero' il perimetro
e' piu' stretto: descrivere cosa puo' significare oggi un ledger observe-mode
basato sugli effetti filesystem osservati da Alfred, senza promettere ancora
Agent Guard completo, policy engine o enforcement.

## Obiettivo del contratto v0

Un `observe ledger` e' una sequenza ordinata di record che descrivono effetti
osservati sul sistema durante un lavoro su un workspace.

Per v0 il ledger non decide ancora se un'azione e' consentita. Il suo scopo e':

- conservare effetti filesystem e diagnostica in forma strutturata;
- distinguere cio' che Alfred ha osservato direttamente da cio' che resta
  contesto futuro;
- preparare il terreno per sessioni agente, workspace boundary e would-block;
- evitare che JSONL, policy o backend inotify diventino il contratto interno.

La frase breve e':

```text
v0 registra effetti osservati; non attribuisce ancora intenzioni e non blocca.
```

## Riga di ledger

Una riga di ledger v0 e' un record Alfred che puo' contribuire alla storia di
una sessione di lavoro. In termini del modello corrente significa:

```text
alfred_record_t
-> queue / dispatcher / sink quando output e' abilitato
-> JSONL o altri writer
-> interpretazione come ledger esterno
```

La riga non e' un nuovo tipo C separato. Per v0 il ledger e' una vista sugli
`alfred_record_t` gia' prodotti, soprattutto quelli serializzati in JSONL.
Questa scelta e' intenzionale:

- mantiene `alfred_record_t` come contratto interno primario;
- mantiene JSONL come output pubblico, non come API interna;
- non aggiunge campi prima di sapere quali sono davvero necessari;
- permette test e analisi sul percorso gia' misurato da Writer Runtime v0.

## Cosa puo' essere credibile oggi

Con il backend inotify corrente Alfred puo' osservare soprattutto effetti
filesystem e diagnostica di affidabilita' del monitoraggio.

| Famiglia | Esempi | Credibilita' v0 |
| --- | --- | --- |
| Effetti filesystem semantici | `FILE_CREATED`, `FILE_MODIFIED`, `FILE_READY`, `FILE_DELETED`, rename/move/relocate, directory create/delete/move | Utilizzabili come effetti workspace osservati |
| Raw normalizzati | `RAW_CREATE`, `RAW_MODIFY`, `RAW_CLOSE_WRITE`, `RAW_MOVED_FROM`, `RAW_MOVED_TO`, `RAW_OVERFLOW` | Utili come evidenza tecnica o per debug del ledger |
| Diagnostica watch | `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, `WATCH_RESYNC_*`, `WATCH_LOST_*` | Utili per sapere se l'osservazione e' affidabile |
| Audit inotify opt-in | `IN_OPEN`, `IN_ACCESS`, `IN_CLOSE_NOWRITE` nel raw log backend | Non sono ledger strutturato v0; restano diagnostica raw backend |
| Processi, rete, tool call | comando, PID affidabile, process tree, network connect | Rimandati a backend o campi futuri |
| Decisioni policy | allow, warn, would-block, block, approval | Rimandate: v0 puo' parlarne come direzione, non come output implementato |

Questa distinzione e' importante: una modifica a `README.md` puo' essere un
effetto osservato credibile; dire che "l'agente Codex ha modificato README.md
per rispettare il task" richiede contesto agente, processo e intento che oggi
non sono ancora parte del contratto runtime.

## Campi concettuali

La tabella seguente separa i campi concettuali di un futuro Agent Action Ledger
da cio' che v0 puo' sostenere oggi.

| Campo concettuale | Significato | Stato v0 |
| --- | --- | --- |
| `seq` | Ordine progressivo locale del record quando assegnato | Disponibile dove il producer/writer lo assegna; puo' essere 0 |
| `ts_monotonic_ns` | Tempo monotono dell'effetto osservato quando disponibile | Disponibile come `ts_ns`; non e' timestamp wall-clock e puo' essere 0 |
| `layer` | Livello del fatto: raw, semantic, diagnostic | Disponibile |
| `category` | Dominio del record: filesystem, watch, backend, policy futura | Disponibile secondo Event Model v0 |
| `type` | Tipo controllato, per esempio `FILE_READY` | Disponibile |
| `path`, `old_path`, `new_path` | Oggetto filesystem coinvolto | Disponibile quando il record lo contiene |
| `identity` | Coppia device/inode quando disponibile | Disponibile solo se completa e prodotta dal record |
| `backend` / `source` | Sorgente che ha osservato il fatto | Disponibile in forma corrente |
| `workspace_id` | Workspace logico della sessione | Rimandato |
| `agent_session_id` | Sessione agente | Rimandato |
| `agent_name` | Runtime agente, per esempio Codex | Rimandato |
| `human_user` | Utente umano responsabile della sessione | Rimandato |
| `task` / `intent` | Task dichiarato dall'utente o agente | Rimandato |
| `subject.pid` / `process_tree` | Processo o albero processi che ha causato l'effetto | Rimandato |
| `decision` | `observed`, `allowed`, `warned`, `would_block`, `blocked` | Solo `observed` e' concettualmente onesto in v0 |
| `policy_rule_id` | Regola che ha prodotto una decisione | Rimandato |
| `confidence` | Confidenza dell'attribuzione o inferenza | Rimandato; gli effetti filesystem osservati non devono fingere inferenze |

Regola pratica:

```text
se Alfred non ha una fonte affidabile per un campo, il campo resta assente o
rimandato; non si riempie con supposizioni.
```

## Decisione v0: `observed`, non `allowed`

Il ledger v0 deve usare mentalmente una decisione implicita:

```text
decision = observed
```

Questo non significa che il codice debba aggiungere subito un campo
`decision`. Significa che, quando interpretiamo il record come parte del ledger,
il suo significato e':

```text
Alfred ha osservato questo effetto.
```

Non significa:

```text
Alfred ha permesso questo effetto.
Alfred lo avrebbe bloccato.
Alfred sa quale agente lo ha causato.
Alfred sa che era coerente con il task.
```

Queste frasi richiedono policy, sessione agente, processo e possibilmente
backend con capability diverse da inotify.

## Workspace boundary in v0

Il workspace boundary resta una direzione di prodotto, ma v0 deve trattarlo
come classificazione futura o analisi esterna.

Esempio sicuro oggi:

```text
Record: FILE_MODIFIED path=/repo/README.md
Interpretazione ledger v0: effetto filesystem osservato dentro un path monitorato
```

Esempio da non promettere ancora:

```text
Decisione: ALLOW perche' il file e' dentro il workspace dell'agente
```

Per arrivare al secondo esempio servono almeno:

- un workspace esplicito e normalizzato;
- una sessione agente o task dichiarato;
- una policy che definisce cosa e' dentro o fuori scope;
- un punto del runtime che produce decisioni;
- test che dimostrano il comportamento;
- benchmark se il percorso caldo o i writer cambiano.

## Relazione con JSONL

JSONL e' il primo formato pubblico utile per leggere il ledger, ma non e' il
contratto interno. Il flusso corretto resta:

```text
backend / core / diagnostica
-> alfred_record_t
-> queue / dispatcher / sink
-> JSONL writer
-> file letto come observe ledger
```

Un consumatore esterno puo' trattare `output.jsonl` come ledger osservazionale,
ma il codice Alfred non deve fare parsing del proprio JSONL per decidere
semantica, policy o routing.

## Esempi

### Modifica coerente ma non attribuita

Scenario umano:

```text
Task: aggiorna README.md
Effetto osservato: FILE_READY /repo/README.md
```

Ledger v0 puo' dire:

```text
E' stato osservato un file pronto dopo scrittura su /repo/README.md.
```

Ledger v0 non puo' ancora dire:

```text
Codex ha completato correttamente il task.
```

### Effetto fuori workspace come analisi futura

Scenario:

```text
Task: aggiorna README.md
Effetto osservato: FILE_DELETED /home/user/.ssh/config
```

Se Alfred non sta monitorando quel path, oggi non vedra' l'effetto. Se lo vede
perche' il path e' monitorato, v0 puo' registrare l'effetto. La classificazione
`outside_workspace` o `would_block` resta futura finche' non esiste un
workspace boundary runtime.

### Diagnostica che abbassa la fiducia

Scenario:

```text
WATCH_STALE /repo
WATCH_RESYNC_FAILED /repo
```

Questi record non sono azioni dell'agente. Sono pero' importanti per il ledger:
dicono che, in quel periodo, Alfred potrebbe non avere una visione affidabile
del workspace.

## Claims vietati in v0

Finche' non esistono campi, backend e test dedicati, la documentazione e il
codice non devono dichiarare che Alfred v0:

- blocca azioni agente;
- approva o nega accessi filesystem;
- sa quale processo ha causato ogni modifica;
- ricostruisce un process tree affidabile;
- osserva rete o esfiltrazione;
- sa leggere tutti i file letti da un agente;
- collega in modo affidabile prompt, tool call, comando ed effetto OS;
- protegge segreti se quei path non sono monitorati o se il backend non puo'
  bloccare l'accesso;
- usa LLM come fonte di verita' per la sicurezza.

La formulazione corretta e':

```text
Alfred puo' produrre un ledger osservazionale degli effetti filesystem che vede
oggi, e prepara i campi per attribuzione, policy e guardrail futuri.
```

## Impatto su codice e benchmark

Questo contratto non richiede modifiche immediate al codice C. Prima di
aggiungere campi runtime bisognera' aprire micro-step separati.

Un micro-step futuro deve chiedersi:

1. il campo e' osservato, derivato o dichiarato?
2. la fonte e' il sistema operativo, la config, l'utente o l'agente?
3. il campo e' generale o legato a inotify?
4. cambia il percorso caldo?
5. cambia queue, dispatcher, writer o JSONL?
6. serve un benchmark refresh secondo
   [Report benchmark prestazioni](34-report-benchmark-prestazioni.md)?
7. serve un golden test JSONL o un test core/backend separato?

## Endpoint del micro-step

Questo documento chiude solo il contratto concettuale iniziale:

```text
observe ledger v0 = vista documentata su record osservazionali correnti,
con campi futuri dichiarati e claim vietati espliciti.
```

Il passo successivo naturale della milestone e' mappare quali record e JSONL
correnti contribuiscono gia' a questa vista e quali famiglie restano solo
future o diagnostiche.
