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

## Mappa record/JSONL corrente

Questa mappa traduce la copertura gia' documentata nel
[Contratto dei log](22-contratto-log.md#copertura-record-sink-e-output-jsonl)
nel linguaggio del ledger v0.

Le colonne hanno questo significato:

- `Ruolo ledger v0`: come la famiglia puo' essere letta da un consumatore
  observe-mode.
- `Runtime JSONL oggi`: se il record entra davvero in `output.jsonl` quando
  `output_enabled=true`.
- `Uso corretto`: cosa puo' dire Alfred senza oltrepassare il contratto.
- `Limite`: cosa resta fuori dal contratto v0.

| Famiglia corrente | Esempi | Ruolo ledger v0 | Runtime JSONL oggi | Uso corretto | Limite |
| --- | --- | --- | --- | --- | --- |
| Semantica filesystem core | `FILE_CREATED`, `DIR_CREATED`, `FILE_MODIFIED`, `FILE_READY`, delete, rename, move, relocate, `OVERFLOW` | Effetti workspace osservati | si', quando l'output strutturato e' abilitato | Timeline degli effetti filesystem che Alfred ha normalizzato semanticamente | Non identifica ancora agente, processo, comando o intento |
| Raw Alfred normalizzati | `RAW_CREATE`, `RAW_DELETE`, `RAW_MODIFY`, `RAW_ATTRIB`, `RAW_CLOSE_WRITE`, `RAW_MOVED_FROM`, `RAW_MOVED_TO`, `RAW_OVERFLOW` | Evidenza tecnica a supporto della semantica | si', per le famiglie runtime-routed | Debug, audit tecnico, spiegazione di come si arriva a un evento semantico | Non sono decisioni policy e non vanno confusi con i fatti kernel `IN_*` pubblici |
| Raw sintetici Alfred | create directory scoperte da scan ricorsivo, overflow sintetico nei test | Evidenza normalizzata prodotta da Alfred per non perdere un fatto utile | si', se attraversa il callback applicativo o il test JSONL dedicato | Spiegare che Alfred ha generato un raw coerente con il modello comune | Ogni nuovo sintetico va documentato; non deve diventare stringa libera |
| Diagnostica watch base | `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, `WATCH_STALE_EVENT_DROPPED` | Affidabilita' del monitoraggio | si' | Dire quando Alfred sta osservando un path, lo ha perso o non puo' fidarsi del mapping `wd -> path` | Non sono azioni dell'agente e non sono eventi filesystem utente |
| Diagnostica resync locale | `WATCH_RESYNC_BEGIN`, `WATCH_RESYNC_SCAN_DONE`, `WATCH_RESYNC_FAILED`, `WATCH_RESYNC_END`, reinstall/rollback | Stato di recupero locale | si' per la famiglia migrata | Spiegare se Alfred ha provato a recuperare un watch stale e con quale esito | Non prova che il workspace sia completo se il resync fallisce |
| Diagnostica lost-scope | `WATCH_LOST_QUEUED`, `WATCH_LOST_FOUND`, `WATCH_LOST_REINSTALLED`, retry, gave-up, end | Stato di recupero fuori dal vecchio path | si' per il percorso migrato | Dire quando Alfred cerca una directory persa tramite identita' e quando la ritrova | Non equivale a process/session attribution |
| Fatti kernel/backend osservati | `IN_CREATE`, `IN_DELETE`, `IN_MOVED_FROM`, `IN_Q_OVERFLOW` | Solo debug backend storico | no | Usarli per debug locale in `raw.log` | Non sono ledger pubblico v0 finche' non esiste `backend_observed` multi-backend |
| Audit inotify opt-in | `IN_OPEN`, `IN_ACCESS`, `IN_CLOSE_NOWRITE` | Diagnostica raw rumorosa | no | Verificare localmente audit backend opt-in | Non sono file read ledger: mancano modello pubblico, contesto processo e golden JSONL |
| Lifecycle/app | startup, shutdown, config, logger initialized | Contesto umano applicativo | no | Leggere nei log umani per diagnosi runtime | Non e' contratto JSONL/ledger v0 |
| Errori runtime generici | config failure, output writer failure, backend init failure | Diagnostica applicativa futura | no, salvo errori gia' modellati dentro famiglie watch/recovery | Oggi restano in `errors.log` e nello status fail-closed | Serve schema `diagnostic/error` o `lifecycle/error` prima del ledger pubblico |
| Trace/performance | tracepoint pipeline, benchmark, metriche | Futuro debugging/quality evidence | no | Usare nei report benchmark o Lab, non nel ledger v0 | Serve layer trace/pipeline stabile |
| Security/policy/Agent Guard | allow, warn, would-block, block, approval | Futuro livello decisionale | no | Solo direzione progettuale | Richiede sessione agente, policy, decisioni, test e possibilmente backend enforcement |

La conseguenza pratica e':

```text
observe ledger v0 pubblico = sottoinsieme JSONL opt-in dei record gia'
runtime-routed, letto come evidenza osservazionale.
```

Non e':

```text
raw.log completo
events.log completo
lista completa delle azioni dell'agente
contratto policy/would-block
prova di attribuzione processo -> effetto
```

Questa mappa non richiede nuovi test perche' non cambia comportamento. I test
JSONL esistenti proteggono le famiglie pubbliche gia' scelte per Event Model v0.
Se una futura PR aggiunge una famiglia alla colonna `Runtime JSONL oggi = si'`,
allora dovra' aggiornare anche golden JSONL, contratto log e questa mappa.

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
