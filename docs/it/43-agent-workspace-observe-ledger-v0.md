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

## Campi minimi workspace/session

Questa sezione decide il set minimo concettuale per i prossimi micro-step. Non
aggiunge ancora campi a `alfred_record_t` e non cambia JSONL: definisce solo
quali nomi e responsabilita' useremo quando arrivera' il momento di progettare
schema e codice.

La regola principale e':

```text
il campo deve dichiarare da dove arriva.
```

Per un ledger di sicurezza non basta avere un valore. Bisogna sapere se quel
valore e' stato osservato dal sistema operativo, configurato dall'utente,
dichiarato dall'agente, derivato da Alfred o prodotto da una policy futura.

| Campo | Stato | Fonte di verita' | Significato | Regola v0 |
| --- | --- | --- | --- | --- |
| `workspace_root` | candidato v0 | configurazione utente o CLI futura | Root filesystem che Alfred considera workspace operativo | Non va dedotto automaticamente dal path del primo evento |
| `workspace_id` | candidato v0 | Alfred/configurazione | Identificatore stabile del workspace, separato dal path leggibile | Deve restare opzionale finche' non esiste configurazione workspace |
| `ledger_session_id` | candidato v0 | Alfred/runtime locale | Sessione osservazionale Alfred, anche senza agente AI | Puo' raggruppare record di una run senza promettere agent attribution |
| `agent_session_id` | futuro vicino | adapter agente o orchestratore fidato | Sessione specifica di un runtime agente | Non va inventato da Alfred guardando solo il filesystem |
| `agent_name` | futuro vicino | adapter agente, CLI o config | Nome del runtime agente, per esempio `codex` | E' contesto dichiarato, non prova di causalita' |
| `human_user` | futuro | OS, config o orchestratore | Utente umano responsabile o proprietario della sessione | Richiede regole privacy prima di entrare nel JSONL pubblico |
| `task_id` | futuro | orchestratore/adapter agente | Identificatore del task dichiarato | Non deve contenere necessariamente il prompt completo |
| `task_summary` | futuro | utente, agente o LLM come riassunto non autorevole | Sintesi leggibile del task | Deve essere trattato come contesto dichiarato, non fonte di verita' |
| `declared_intent` | futuro | agente/utente | Intenzione dichiarata | Utile per confronto futuro, ma non affidabile da sola |
| `subject_pid` | futuro backend | backend process/audit/eBPF/fanotify/procfs | Processo che ha prodotto l'effetto | Non affidabile con solo inotify corrente |
| `process_tree_root` | futuro backend/session engine | process backend + correlazione | Radice dell'albero processi attribuito alla sessione | Richiede session engine, non singolo record filesystem |
| `decision` | futuro policy | policy engine | `observed`, `allowed`, `would_block`, `blocked`, ecc. | Per v0 il significato resta implicitamente `observed` |
| `policy_rule_id` | futuro policy | policy engine | Regola che ha prodotto una decisione | Assente finche' non esiste policy valutata |

### Set minimo candidato

Il primo set da considerare, quando decideremo di aggiungere campi reali, e':

```text
workspace_root
workspace_id
ledger_session_id
```

Questi tre campi sono i meno rischiosi perche' possono descrivere una sessione
osservazionale di Alfred senza fingere che esista gia' una vera agent session.

- `workspace_root` dice qual e' il perimetro dichiarato dall'utente o dalla
  configurazione.
- `workspace_id` permette di riferirsi allo stesso workspace senza dipendere
  sempre dal path testuale.
- `ledger_session_id` raggruppa record prodotti durante una run o una sessione
  Alfred, anche se non sappiamo ancora quale agente o processo li abbia causati.

`agent_session_id`, `agent_name`, `task_id` e `declared_intent` sono molto
importanti, ma devono arrivare da un adapter agente, da una CLI esplicita o da
un orchestratore. Con il solo inotify sarebbero supposizioni.

### Campi da non derivare implicitamente

Alfred non deve derivare automaticamente:

- `workspace_root` dal prefisso comune dei path osservati;
- `agent_name` dal nome del processo o dal nome della directory;
- `agent_session_id` dal PID, dal timestamp o dal file modificato;
- `task_summary` dal contenuto dei file;
- `decision` dalla posizione del path dentro/fuori una directory.

Queste inferenze possono sembrare comode, ma creano un ledger ambiguo: il
lettore non sa piu' se sta guardando un fatto osservato, una configurazione
utente o una supposizione di Alfred.

### Privacy e minimizzazione

I campi workspace/session possono contenere dati sensibili. Un path workspace
puo' rivelare nome utente, cliente, repository, progetto o directory privata.
Un task summary puo' contenere informazioni aziendali o credenziali incollate
per errore.

Per questo v0 deve seguire tre regole:

- non salvare prompt completi come campo minimo;
- preferire identificatori (`workspace_id`, `ledger_session_id`, `task_id`) a
  testo lungo quando basta un riferimento;
- documentare sempre se un campo e' osservato, configurato, dichiarato o
  derivato.

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

## Strategia test e golden coverage

Il ledger observe-mode v0 non deve duplicare tutti i test core, backend e
JSONL. Deve invece usare quei test come strati diversi di evidenza.

La regola e':

```text
testiamo ogni responsabilita' nel livello in cui nasce;
nel ledger testiamo solo gli scenari pubblici rappresentativi.
```

Questo evita due errori:

- copiare tutti gli scenari core in una seconda suite ledger quasi identica;
- credere che basti un formatter JSONL unitario per dimostrare un ledger
  runtime end-to-end.

### Strati di copertura esistenti

| Strato | Test principali | Cosa protegge per il ledger | Cosa non deve promettere |
| --- | --- | --- | --- |
| Core end-to-end | `tests/core/` | Semantica filesystem stabile: create, delete, modify, file-ready, rename, move, relocate, recursive mkdir | JSONL, queue, writer, watch diagnostics o agent attribution |
| Backend diagnostics | `tests/backend/` | Salute del backend: watch aggiunti/rimossi, stale, resync, lost-scope, audit raw, output failure | Contratto pubblico completo del ledger o decisioni policy |
| Record/formatter JSONL | `tests/backend/test_record_jsonl.*`, sink/writer JSONL | Forma esatta degli oggetti JSONL, escaping, identity completa, oggetti nested | Percorso runtime reale e ordine end-to-end |
| Output pipeline runtime | `tests/backend/test_output_pipeline_runtime.sh`, `test_output_counter_runtime.sh`, `test_output_queue_pressure.sh` | Collegamento runtime `record -> queue -> drain -> dispatcher -> sink`, fail-closed e coda bounded | Semantica completa di ogni scenario filesystem |
| JSONL golden end-to-end | `tests/jsonl/` | Contratto pubblico rappresentativo di `output.jsonl` su scenari reali o sintetici controllati | Copertura esaustiva di ogni combinazione core/backend |
| Audit esplorativi | `tests/exploratory/nightly/` | Uso realistico, regressioni inattese, scenari da promuovere | Contratto stabile o sostituto della suite CI |

### Golden JSONL rappresentativi

Per v0 i golden JSONL devono coprire famiglie rappresentative, non ogni test
possibile.

| Famiglia ledger | Copertura esistente | Interpretazione |
| --- | --- | --- |
| Effetti filesystem base | `tests/jsonl/test_create_file_and_dir_jsonl.sh`, delete, modify, move, rename, relocate | Dimostra che gli effetti workspace osservati entrano nel JSONL pubblico |
| File-ready / write lifecycle | `tests/jsonl/test_modify_file_jsonl.sh` | Protegge il caso utile per agenti e automazioni: il file e' pronto dopo scrittura |
| Rename/move/relocate | `tests/jsonl/test_file_moved_jsonl.sh`, `test_file_relocated_jsonl.sh`, varianti directory | Protegge la semantica che distingue cambio nome, cambio directory e cambio completo |
| Raw normalizzati | test JSONL base e formatter raw, inclusi overflow | Mostra l'evidenza tecnica Alfred, non il fatto kernel `IN_*` |
| Diagnostica watch/recovery | `tests/backend/test_output_pipeline_runtime.sh`, `tests/jsonl/test_self_move_recovery_jsonl.sh`, `tests/jsonl/test_lost_scope_runtime_recovery_jsonl.sh` | Mostra quando la fiducia nel monitoraggio cambia o viene recuperata |
| Overflow | `tests/jsonl/test_overflow_raw_jsonl.sh`, `tests/jsonl/test_overflow_core_jsonl.sh` | Protegge il caso globale senza path, importante per audit e affidabilita' |

Questa copertura e' sufficiente per il contratto documentale corrente perche'
il micro-step non aggiunge campi a `alfred_record_t`, non cambia lo schema
JSONL e non cambia il routing runtime.

### Quando aggiungere un nuovo golden

Una PR futura deve aggiungere o aggiornare un golden JSONL quando:

- aggiunge un campo pubblico del ledger, per esempio `workspace_root`,
  `workspace_id` o `ledger_session_id`;
- rende runtime-routed una famiglia oggi esclusa da JSONL;
- cambia il significato di una famiglia gia' runtime-routed;
- aggiunge un nuovo tipo `layer/category/type` serializzato in JSONL;
- introduce una decisione osservabile come `would_block`, `blocked`,
  `allowed` o `requires_approval`;
- aggiunge una classificazione workspace come `inside_workspace` o
  `outside_workspace`;
- cambia la policy fail-closed di output incompleto;
- modifica queue, dispatcher, sink o writer in un modo che puo' cambiare quali
  record arrivano a `output.jsonl`.

### Quando non aggiungere un nuovo golden

Non serve un nuovo golden ledger quando:

- la modifica e' solo documentale;
- il comportamento e' gia' protetto dal test core e non cambia JSONL;
- il comportamento e' diagnostica backend interna senza output pubblico v0;
- il test sarebbe solo una copia di uno scenario core gia' coperto;
- il formatter JSONL unitario copre gia' la forma del campo e nessun percorso
  runtime cambia;
- il tema riguarda process tree, rete, policy o agent session ma resta
  esplicitamente futuro.

### Casi futuri da non anticipare

Questi test non devono essere scritti finche' il relativo comportamento non
esiste:

| Test futuro | Perche' non ora |
| --- | --- |
| `agent_session_id` in JSONL | Non esiste ancora fonte affidabile o adapter agente |
| `workspace_root` / `workspace_id` runtime | I campi sono candidati concettuali, non schema implementato |
| `outside_workspace` | Non esiste workspace boundary runtime |
| `would_block` / `blocked` | Non esiste policy engine o enforcement |
| processo che causa l'effetto | Inotify corrente non fornisce attribution affidabile |
| file read ledger | `IN_ACCESS` resta audit raw opt-in, non record pubblico v0 |
| network/exfiltration | Nessun backend rete implementato |

### Regola pratica per una nuova feature ledger

Quando una PR introduce una feature ledger reale, la checklist minima e':

1. test core se cambia la semantica filesystem;
2. test backend se cambia diagnostica, watch, recovery o routing dal backend;
3. test formatter/sink se cambia la forma del record;
4. golden JSONL se cambia il contratto pubblico del ledger;
5. aggiornamento del contratto log e di questa sezione;
6. benchmark o nota esplicita se cambia il percorso caldo, la coda, il
   dispatcher, il writer o il volume di output.

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

Il motivo e' semplice: finche' il ledger v0 resta una vista documentale sui
record gia' runtime-routed, non cambia il percorso misurato. Non cambiano
`alfred_record_t`, clone owned, queue, drain, dispatcher, sink, writer JSONL,
dimensione dei record o volume di output. In questo caso i benchmark storici
restano il riferimento di contesto e non serve produrre numeri nuovi.

Un micro-step futuro deve chiedersi:

1. il campo e' osservato, derivato o dichiarato?
2. la fonte e' il sistema operativo, la config, l'utente o l'agente?
3. il campo e' generale o legato a inotify?
4. cambia il percorso caldo?
5. cambia queue, dispatcher, writer o JSONL?
6. serve un benchmark refresh secondo
   [Report benchmark prestazioni](34-report-benchmark-prestazioni.md)?
7. serve un golden test JSONL o un test core/backend separato?

### Gate benchmark per il ledger observe-mode

La regola pratica e':

```text
se una modifica ledger cambia solo documentazione, mappa concettuale o test che
assertano comportamento gia' esistente, non serve un nuovo benchmark;
se cambia il percorso misurato, il volume del record o il costo del writer,
serve almeno una nota esplicita e spesso un refresh mirato.
```

| Tipo di modifica | Benchmark richiesto? | Perche' |
| --- | --- | --- |
| Documento che chiarisce claim, campi futuri, test strategy o use case | No | Non cambia codice, output o percorso caldo |
| Nuovo golden JSONL che verifica uno scenario gia' runtime-routed | Di norma no | Aumenta copertura contrattuale, non costo runtime |
| Aggiunta di `workspace_root`, `workspace_id` o `ledger_session_id` al JSONL pubblico | Si', almeno formatter/output | Aumenta byte per record, escaping, clone/ownership se entra nel record |
| Aggiunta di campi stringa dichiarati da CLI/config/sessione | Si', se entrano in `alfred_record_t` o vengono clonati per record | Puo' aumentare allocazioni, copia profonda, dimensione record e volume JSONL |
| Nuova classificazione runtime come `inside_workspace` / `outside_workspace` | Si', se calcolata nel percorso evento | Introduce normalizzazione o policy leggera per evento |
| Decisione osservabile `would_block`, `blocked`, `allowed`, `requires_approval` | Si' | Cambia schema pubblico, possibile policy path, routing e volume output |
| Nuova famiglia runtime-routed verso JSONL | Si', golden + benchmark mirato | Aumenta numero di record scritti e puo' cambiare backpressure |
| Modifica a queue, drain, dispatcher, sink, writer, buffering, flush o fail-closed | Si' | Cambia direttamente i percorsi misurati dalla Performance suite v0 |
| Worker thread, code per sink, socket, writer binari, hashing/signing, compressione | Si', nuovo benchmark dedicato prima della scelta | Sono decisioni architetturali non coperte dai benchmark v0 |
| Backend process/audit/eBPF/fanotify o attribution processo | Si', ma con benchmark nuovo | I benchmark attuali non misurano quei backend o quella correlazione |

### Quale benchmark usare

Il benchmark deve seguire il punto in cui nasce il costo.

| Costo introdotto | Misura minima consigliata |
| --- | --- |
| Solo forma JSONL di un campo nuovo | Formatter/sink JSONL e golden JSONL |
| Campo nuovo dentro record e copia owned | Benchmark queue/ownership e output pipeline sintetica |
| Nuovo routing runtime verso `output.jsonl` | Runtime output benchmark, piu' golden rappresentativo |
| Nuova logica per-event nel percorso caldo | Benchmark dedicato del percorso interessato prima/dopo |
| Nuovo writer o formato | Riga benchmark dedicata piu' confronto con `counter`, `text` e `jsonl` |
| Nuova backpressure o bounded behavior | Benchmark pressione/coda e test funzionale sui contatori |
| Nuovo backend o attribution processo | Benchmark backend-specifico; non riusare numeri inotify come prova generale |

Metriche da guardare, quando il benchmark esiste:

- tempo totale e tempo medio per record;
- record/sec o eventi/sec;
- byte prodotti per record e volume totale di output;
- contatori di enqueue, drain, pressione, drop o failure;
- `max_pending` e profondita' osservabile della coda;
- allocazioni/RSS quando aumentano campi stringa o copie profonde;
- p95/p99 solo quando avremo una suite con ripetizioni e percentili.

Per questo micro-step specifico il risultato e':

```text
benchmark refresh non richiesto:
la modifica e' documentale e non cambia schema, hot path, queue, dispatcher,
sink, writer, output JSONL o workload.
```

## Endpoint del micro-step

Questo documento chiude solo il contratto concettuale iniziale:

```text
observe ledger v0 = vista documentata su record osservazionali correnti,
con campi futuri dichiarati e claim vietati espliciti.
```

Il passo successivo naturale della milestone e' mappare quali record e JSONL
correnti contribuiscono gia' a questa vista e quali famiglie restano solo
future o diagnostiche.
