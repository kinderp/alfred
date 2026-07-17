# Stato documentazione didattica

Questo file traccia lo stato corrente dei capitoli in `docs/it`.

Non e' piu' un diario completo della migrazione storica. La cronologia
dettagliata dei vecchi passaggi, incluso lo shadow mode, resta recuperabile da
Git. Qui devono restare solo informazioni utili per capire quali documenti sono
attivi, quali sono incompleti e quali sono stati rimossi perche' superati.

## Stati usati

- `Completo`: capitolo leggibile e gia' utile agli studenti.
- `Parziale`: capitolo presente ma da espandere.
- `Rimosso`: capitolo eliminato perche' storico o superato.

## Capitoli attivi

| Stato | Capitolo |
| --- | --- |
| Completo | `README.md` |
| Completo | `00-regole-operative.md` |
| Completo | `01-panoramica-progetto.md` |
| Completo | `02-architettura-generale.md` |
| Parziale | `03-struttura-cartelle.md` |
| Completo | `04-livello-applicazione.md` |
| Completo | `05-modulo-inotify.md` |
| Completo | `06-core-engine.md` |
| Completo | `07-flusso-eventi.md` |
| Completo | `08-guida-c-usato-nel-progetto.md` |
| Completo | `09-makefile-e-build-system.md` |
| Completo | `10-debugging-test-e-strumenti.md` |
| Completo | `11-come-contribuire.md` |
| Completo | `13-semantica-eventi.md` |
| Completo | `14-scenari-test.md` |
| Parziale | `16-mappa-codice-e-strutture.md` |
| Parziale | `17-roadmap-documentazione-avanzata.md` |
| Parziale | `18-modello-licenze.md` |
| Parziale | `19-roadmap-cli-e-man-page.md` |
| Completo | `20-matrice-eventi-inotify.md` |
| Parziale | `21-roadmap-scanner-resync.md` |
| Parziale | `22-contratto-log.md` |
| Parziale | `23-roadmap-plugin-backend.md` |
| Parziale | `24-roadmap-ai-agent-guardrail.md` |
| Completo | `25-roadmap-unificata-dossier.md` |
| Completo | `26-stato-funzionalita.md` |
| Completo | `27-guida-lettura-documentazione.md` |
| Completo | `28-audit-documentazione-e-debiti.md` |
| Parziale | `29-event-model-v0.md` |
| Parziale | `30-backend-api-v0.md` |
| Completo | `31-milestone-inotify-reference-backend.md` |
| Parziale | `32-writer-api-v0.md` |
| Parziale | `33-writer-runtime-roadmap-v0.md` |
| Parziale | `34-report-benchmark-prestazioni.md` |
| Parziale | `35-qualita-prodotto-software.md` |
| Parziale | `36-use-cases-posizionamento-integrazioni.md` |
| Parziale | `37-roadmap-milestone-progetto.md` |
| Parziale | `38-visione-observation-runtime.md` |
| Parziale | `39-principi-architetturali-futuri.md` |
| Parziale | `40-audit-inotify-backend-api-v0.md` |
| Completo | `41-tracepoint-lab-roadmap-mvp.md` |
| Completo | `42-tracepoint-model-v0.md` |
| Completo | `43-agent-workspace-observe-ledger-v0.md` |
| Parziale | `44-workspace-session-runtime-schema-v0.md` |
| Completo | `45-workspace-session-runtime-context-v0.md` |
| Parziale | `46-metadata-session-record-jsonl-v0.md` |
| Completo | `47-inotify-reference-backend-mvp-closure-audit.md` |
| Parziale | `48-core-input-main-loop-migration-v0.md` |
| Completo | `lab/README.md` |
| Completo | `lab/scenarios/create-file.md` |
| Completo | `lab/scenarios/file-ready.md` |
| Completo | `lab/scenarios/rename-move-relocate.md` |
| Completo | `lab/scenarios/watch-stale-recovery.md` |
| Parziale | `audit/README.md` |
| Parziale | `audit/2026-06-25-audit-notturno.md` |
| Parziale | `audit/2026-07-01-audit-notturno.md` |
| Parziale | `audit/nightly-playbook.md` |
| Parziale | `audit/maturity-matrix.md` |
| Parziale | `audit/maturity-data-template.csv` |
| Parziale | `glossario.md` |

## Capitoli rimossi

| Capitolo | Motivo |
| --- | --- |
| `12-confronto-shadow-mode.md` | Documento storico della migrazione legacy/core. Lo shadow mode non e' piu' una modalita' runtime e non deve restare nel percorso di lettura corrente. |
| `15-todo-switch-core.md` | TODO storico dello switch al core. I debiti ancora utili sono stati migrati nella roadmap unificata, nella matrice inotify, nella roadmap scanner/resync e nei capitoli architetturali correnti. |

## Documenti tecnici fuori da `docs/it`

| Stato | Capitolo |
| --- | --- |
| Completo | `docs/code-browser/README.md` |
| Completo | `docs/sourcebot-browser/README.md` |
| Parziale | `docs/kythe-browser/README.md` |
| Parziale | `docs/man/man1/alfred.1` |
| Parziale | `docs/man/man5/alfred.conf.5` |
| Parziale | `docs/man/man7/alfred-events.7` |
| Completo | `AGENTS.md` |
| Parziale | `tests/exploratory/nightly/README.md` |

## Aggiornamento audit esplorativi notturni

`docs/it/audit/README.md` definisce il metodo per gli audit esplorativi
notturni: issue madre, issue figlie, report nel repository, script
riproducibili e regole per non committare log completi. Il primo report storico
e' `docs/it/audit/2026-06-25-audit-notturno.md`, collegato alle issue GitHub
`#29` e `#30`.

Il secondo report storico e'
`docs/it/audit/2026-07-01-audit-notturno.md`, collegato alla issue GitHub
`#68`. Ripete gli scenari principali dopo il lavoro recente su Backend API v0 e
Writer Runtime, conferma che il known failure `#30` e' ancora riproducibile e
aggiorna la freschezza della matrice di maturita' per gli scenari rieseguiti.

`tests/exploratory/nightly/` contiene scenari rilanciabili che non fanno ancora
parte della suite ufficiale. Servono a ripetere audit precedenti e a decidere
quali scenari promuovere a test contrattuali.

`docs/it/audit/maturity-matrix.md` introduce una prima matrice qualitativa per
stimare la maturita' osservata delle funzionalita' provate negli audit. Il
documento chiarisce anche perche' gli audit attuali sono scenario-based tests e
non fuzzy test in senso tecnico.

`docs/it/audit/nightly-playbook.md` e' il playbook operativo per il prompt breve
"inizia sessione test notturni seguendo regole operative". Definisce bootstrap,
issue madre, scenari base, scenari extra, analisi log, issue figlie, sub-issue,
upload artifact, aggiornamento maturita' e report finale.

`35-qualita-prodotto-software.md` spiega agli studenti il significato delle
dimensioni usate nella matrice di maturita': correttezza funzionale,
robustezza, affidabilita', stabilita', performance, leggerezza, sicurezza,
coerenza, semplicita', manutenibilita', operabilita' e documentazione.

## Aggiornamento Core Input Model / Main Loop Migration v0

La milestone corrente e' `Core Input Model / Main Loop Migration v0`,
collegata alla GitHub Milestone #11, alla issue madre #187 e alla issue figlia
di setup #188. Il riferimento principale e'
`48-core-input-main-loop-migration-v0.md`.

Il documento non implementa ancora una migrazione runtime. Serve a decidere
quale input deve consumare il core semantico dopo la chiusura MVP del backend
inotify: `alfred_raw_event_t`, `alfred_record_t`, un bridge misurato o un dual
path temporaneo dichiarato. La regola guida e' non cambiare il percorso caldo
senza un contratto esplicito e numeri di benchmark.

Il debito da chiudere e' il ponte raw isolato nel main loop:
`app_poll_legacy_raw_backend_once()` continua ad alimentare
`alfred_process()` con `alfred_raw_event_t`, mentre la poll staged della
Backend API v0 emette record normalizzati. Questa convivenza e' intenzionale,
ma deve diventare una decisione architetturale misurata prima di aggiungere
backend futuri.

Aggiornamento successivo: `48-core-input-main-loop-migration-v0.md` contiene
ora anche la mappa del runtime corrente. `app_init()` usa gia'
`inotify_backend_ops()` per lifecycle, target e start; `app_run()` invece usa
ancora `app_poll_legacy_raw_backend_once()` -> `inotify_backend_poll()` ->
`handle_backend_event()` per consegnare `alfred_raw_event_t` al core. Il
percorso staged `backend_ops->poll(timeout_ms = 0)` delega allo stesso poll
inotify, ma converte il raw in `alfred_record_t` con
`alfred_record_from_raw()` e lo emette come record borrowed. La mappa rende
esplicito perche' il main loop non puo' essere migrato meccanicamente senza
decidere prima il core input model.

Aggiornamento successivo: lo stesso documento chiarisce i termini `raw-first`,
`record-first`, `bridge misurato`, `dual path`, percorso caldo, benchmark gate
e test/contract gate. Aggiunge anche una matrice di confronto preliminare. La
raccomandazione provvisoria e' prudente: mantenere il raw-first come baseline,
non migrare il core a record-first senza benchmark e usare un bridge misurato
solo se serve a confrontare costi, ownership e test prima della decisione
finale. La matrice include esplicitamente anche affidabilita' e prove security:
overflow, diagnostica backend, provenance e segnali di errore non devono
perdersi durante una eventuale conversione fra raw e record.

Aggiornamento successivo: `48-core-input-main-loop-migration-v0.md` definisce
ora il benchmark gate della milestone. Il gate separa la baseline raw-first dal
candidato bridge misurato e dal futuro candidato record-first; chiarisce che la
misura primaria deve usare output counter/no-op e non JSONL; elenca metriche
minime come eventi al secondo, tempo totale, record processati, conversioni,
copie/allocazioni quando disponibili, errori, drop, equivalenza semantica,
preservation di overflow/diagnostica/evidence e overhead percentuale. Il
documento dichiara anche che i benchmark `queue-dispatcher-jsonl`,
`output-pipeline-jsonl` e counter/output runtime misurano soprattutto la
pipeline di output: sono utili, ma non bastano per decidere la migrazione del
core input model. Prima di sostituire `app_poll_legacy_raw_backend_once()`
servono benchmark mirati, test di equivalenza e documentazione di ownership,
lifetime, rollback e numeri usati nella decisione.

Aggiornamento successivo: la baseline raw-first del gate benchmark e' stata
resa eseguibile con `make perf-core-input`. Il nuovo benchmark manuale
`tests/perf/bench_core_input.c` misura solo
`alfred_raw_event_t -> alfred_process() -> callback semantica counter/no-op`,
senza inotify reale, filesystem I/O, JSONL, writer testuali, queue, dispatcher
o runtime app. `34-report-benchmark-prestazioni.md` documenta la nuova riga
CSV `core-input-raw-first`, i campi `raw_events`,
`raw_events_per_sec_avg`, `semantic_events_last`, `errors_last`,
`conversions_per_event` e `output_mode`, e chiarisce che questa baseline dovra'
essere confrontata solo con futuri benchmark bridge/record-first sullo stesso
confine core input. Un aggiornamento successivo dello stesso benchmark aggiunge
la riga `raw-to-record-adapter`, che misura il costo isolato
`alfred_raw_event_t -> alfred_record_from_raw()` senza implementare un bridge
record/raw o un core record-first. Un ulteriore aggiornamento aggiunge
`raw-to-record-plus-core`, cioe'
`alfred_raw_event_t -> alfred_record_from_raw() -> alfred_process()`, per
misurare il costo sidecar di produrre il record normalizzato mentre il core
resta raw-first. Anche questa riga non implementa ancora un bridge completo,
un core record-first o una migrazione del main loop.

## Aggiornamento Tracepoint e Lab MVP

La cartella `docs/it/lab/` contiene gli scenari Lab Markdown v0. Gli scenari
attualmente disponibili coprono create file, close-write/file-ready,
rename/move/relocate e watch stale/recovery. Il quarto scenario chiude il primo
set MVP documentando la diagnostica `WATCH_*`: path stale, resync locale,
handoff lost-scope e differenza tra affidabilita' del monitoraggio e semantica
filesystem. Per v0 resta documentazione `stable-doc`: non introduce parser,
`trace.jsonl`, UI o nuovo I/O nel percorso caldo.

## Aggiornamento Performance suite v0

La milestone `Performance suite v0` e' chiusa come contratto documentale
iniziale. Il riferimento principale e'
`34-report-benchmark-prestazioni.md`: il documento conserva i numeri storici
gia' raccolti e definisce tassonomia dei benchmark, significato dei campi CSV,
politica di refresh, limiti metodologici, debiti accettati e gate test/script.
L'obiettivo non e' ottimizzare a sensazione, ma impedire decisioni su worker
thread, code per sink, writer binari o nuovi formati senza numeri espliciti e
confrontabili.

Restano rimandati i benchmark piu' forti: ripetizioni statistiche, p95/p99,
ambiente normalizzato, I/O reale, socket, thread/lock, backpressure asincrona,
code per sink, writer binari, allocazioni e backend non-inotify. Questi debiti
sono ora espliciti e devono diventare benchmark mirati prima delle decisioni
architetturali corrispondenti.

## Aggiornamento Agent workspace observe ledger

La milestone operativa successiva e' `Agent workspace observe ledger`, collegata
alla GitHub Milestone #6 e alla issue madre #130. Il riferimento principale e'
`24-roadmap-ai-agent-guardrail.md`, ma il perimetro corrente e' volutamente
molto piu' stretto della visione completa: definire un ledger observe-mode per
effetti filesystem di workspace senza promettere enforcement, policy engine,
process tree, rete, fanotify/eBPF/audit o integrazione LLM.

Il primo micro-step #131 e' documentale: aggiornare registro milestone,
tracciabilita' e stato documentazione. I passi successivi dovranno separare
con precisione cosa Alfred puo' osservare oggi tramite record/JSONL da cio' che
richiede campi futuri di sessione agente, processo, workspace e decisione
policy. Ogni proposta runtime dovra' rispettare Event Model v0, Writer Runtime
v0 e i vincoli emersi nella Performance suite v0.

`43-agent-workspace-observe-ledger-v0.md` definisce questo primo contratto:
il ledger v0 e' una vista osservazionale sui record correnti, non un nuovo tipo
C e non una nuova semantica JSONL. Il documento dichiara quali effetti
filesystem e diagnostici sono credibili oggi, quali campi agent/session/process
sono futuri e quali claim sono vietati finche' non esistono backend, policy e
test dedicati. Il documento contiene anche la mappa record/JSONL corrente:
semantica filesystem, raw Alfred normalizzati e diagnostica watch/recovery
possono contribuire al ledger osservazionale; fatti kernel `IN_*`, audit
inotify opt-in, lifecycle, errori generici, trace e policy restano non-goal o
future famiglie di contratto.

Aggiornamento successivo: il documento definisce anche i campi minimi
workspace/session candidati per i prossimi micro-step. `workspace_root`,
`workspace_id` e `ledger_session_id` sono il set meno rischioso per descrivere
una sessione osservazionale Alfred senza fingere una vera agent session.
`agent_session_id`, `agent_name`, `task_id`, `declared_intent`, process tree e
decisioni policy restano futuri finche' non esistono una fonte esplicita,
backend adatti o un policy engine. La regola principale e': ogni campo deve
dichiarare la propria fonte di verita' e non deve essere derivato
implicitamente da path, PID, timestamp o contenuto dei file.

Aggiornamento successivo: il documento definisce anche la strategia di test e
golden coverage per il ledger observe-mode. La scelta e' di non duplicare tutte
le suite core/backend: i test core proteggono la semantica filesystem, i test
backend proteggono watch/recovery/output failure, i test formatter proteggono
la forma JSONL e i golden JSONL proteggono solo scenari pubblici
rappresentativi. Un nuovo golden diventa obbligatorio quando una PR aggiunge
campi pubblici del ledger, cambia una famiglia runtime-routed, introduce
decisioni policy visibili o modifica queue/dispatcher/writer in modo rilevante
per `output.jsonl`.

Aggiornamento successivo: il documento definisce anche il gate benchmark per il
ledger observe-mode. I micro-step documentali e i golden che verificano
comportamento gia' runtime-routed non richiedono un refresh dei benchmark. Un
refresh o un benchmark mirato diventa invece necessario quando una modifica
ledger cambia schema pubblico, dimensione/copia dei record, volume JSONL,
classificazione runtime, queue, drain, dispatcher, sink, writer, buffering,
fail-closed o introduce backend/attribution non coperti dai numeri inotify
correnti.

Aggiornamento successivo: la milestone `Agent workspace observe ledger` e'
chiusa come contratto documentale observe-mode. L'esito non implementa Agent
Guard, policy, enforcement, process tree, rete o campi runtime agent/session.
Stabilizza invece cosa Alfred puo' chiamare ledger oggi: una lettura
osservazionale dei record strutturati gia' prodotti e runtime-routed, con
claim vietati, campi futuri, strategia golden, gate benchmark e debiti
espliciti per i prossimi micro-step.

## Aggiornamento Workspace/session runtime schema

La milestone operativa successiva e' `Workspace/session runtime schema v0`,
collegata alla GitHub Milestone #7 e alla issue madre #145. Il riferimento
principale e' `44-workspace-session-runtime-schema-v0.md`.

Il perimetro e' volutamente piccolo: trasformare i candidati
`workspace_root`, `workspace_id` e `ledger_session_id` in una decisione
documentata su significato, fonte di verita', posizionamento runtime,
ownership, JSONL, test e benchmark gate. La milestone non implementa ancora
Agent Guard, policy, enforcement, process tree, rete, file read affidabili,
attribuzione agente/processo o integrazione LLM.

Aggiornamento successivo: il primo micro-step di design della milestone
definisce semantica e source of truth dei tre campi. `workspace_root` e'
contesto dichiarato esplicitamente, `workspace_id` e' identificatore opaco del
workspace dichiarato o generato da Alfred, `ledger_session_id` identifica la
run osservazionale Alfred e non una sessione agente. Sono vietate in v0
inferenze silenziose da path osservati, prefissi comuni, directory corrente,
PID, timestamp, nome agente, prompt o contenuto dei file. Restano successivi
runtime placement, ownership C, forma JSONL, golden test e benchmark refresh.
Il documento chiarisce anche che `workspace_root` e `workspace_id` non sono
equivalenti: la root descrive il perimetro filesystem dichiarato, mentre l'id e'
solo una label opaca di correlazione e da solo non prova containment.

Aggiornamento successivo: il secondo micro-step di design decide il
posizionamento runtime v0. I valori workspace/sessione devono vivere in un
contesto runtime immutabile owned da app/runtime, creato prima
dell'osservazione e distrutto durante shutdown dopo writer/sink interessati.
In quella fase non vengono ancora aggiunti a ogni `alfred_record_t` e non
aumentano il clone owned della queue. I backend non possiedono questi campi, il
core filesystem non li usa per la semantica corrente e un futuro writer/record
di sessione dovra' leggere il contesto con API esplicita, golden JSONL e
benchmark dedicati prima di renderlo pubblico. Il contesto separato deve inoltre
essere pubblicato come read-only verso eventuali reader asincroni futuri e
distrutto solo dopo stop/join/drain dei componenti che possono ancora leggerlo.

Aggiornamento successivo: il terzo micro-step di design decide il gate JSONL e
test. Lo schema JSONL runtime corrente non cambia ancora: `workspace_root`,
`workspace_id` e `ledger_session_id` non vengono emessi sui record evento senza
una PR dedicata. La forma pubblica futura preferita e' un record
metadata/sessione separato, perche' questi campi descrivono il contesto della
run e non un fatto filesystem per-evento. I valori vuoti non devono apparire nel
JSONL valido; i golden dovranno coprire assenza, escaping, ordine del metadata
record e assenza di attribuzione agente/policy implicita.

Aggiornamento successivo: il quarto micro-step di design decide il gate
benchmark. La milestone documentale non richiede un refresh dei numeri perche'
non cambia runtime, record, queue, writer, JSONL o workload. Un refresh diventa
obbligatorio quando una futura PR cambia il percorso runtime, ripete stringhe
per evento, arricchisce JSONL per-record, cambia writer/sink/pipeline o genera
identificatori dentro il percorso caldo. Il report benchmark ora contiene una
tabella dedicata a workspace/session schema.

Aggiornamento successivo: la milestone `Workspace/session runtime schema v0` e'
chiusa come contratto documentale. L'esito non implementa ancora struct runtime,
opzioni CLI/config, JSONL pubblico o campi in `alfred_record_t`. Stabilizza
pero' significato, fonte di verita', runtime placement, ownership/lifetime,
pubblicazione read-only per reader futuri, forma JSONL preferita, test richiesti
e benchmark gate. L'implementazione C/JSONL futura dovra' partire dai debiti
espliciti nel documento 44.

## Aggiornamento Workspace/session runtime context

La milestone `Workspace/session runtime context v0` e' il primo passo di codice
dopo il contratto documentale `44-workspace-session-runtime-schema-v0.md`.
Introduce il parsing opzionale di `workspace_root`, `workspace_id` e
`ledger_session_id` e una copia app-owned immutabile dentro `app_t`, senza
modificare `alfred_record_t`, queue, dispatcher, sink, writer JSONL o schema
pubblico.

`45-workspace-session-runtime-context-v0.md` e' il riferimento didattico per
questo micro-step: spiega chi possiede le stringhe, perche' i valori vuoti
vengono rifiutati, quali funzioni sono coinvolte e quali debiti restano
rimandati prima di rendere questi campi visibili in JSONL o nei record.

Aggiornamento successivo: la milestone `Workspace/session runtime context v0`
e' chiusa come primo sottoinsieme runtime. L'esito introduce
`config_t.workspace_session`, `app_t.workspace_session`, parsing opzionale da
configurazione, rifiuto di valori vuoti o identificatori troppo lunghi e test
focused. Restano deliberatamente fuori `alfred_record_t`, queue, dispatcher,
sink, writer JSONL, metadata/session record, generazione automatica id, policy
e Agent Guard.

## Aggiornamento Metadata/session record JSONL

La milestone operativa successiva e' `Metadata/session record JSONL v0`,
collegata alla GitHub Milestone #9 e alla issue madre #163. Il riferimento
principale e' `46-metadata-session-record-jsonl-v0.md`.

Il perimetro resta stretto: pubblicare il contesto workspace/sessione gia'
presente in `app_t` come record metadata/sessione separato quando l'output
JSONL e' abilitato. Non e' enrichment per-evento, non e' Agent Guard e non e'
attribuzione agente/processo. La forma preferita e'
`diagnostic + lifecycle + SESSION_CONTEXT`, con golden JSONL dedicati e
benchmark gate quando il lavoro passa da documento a codice.

Aggiornamento successivo: il primo micro-step di codice della milestone
metadata/sessione ha ammesso il nome controllato
`diagnostic + lifecycle + SESSION_CONTEXT` nel modello record e nel formatter
JSONL, senza payload runtime.

Aggiornamento successivo: il micro-step payload JSONL stabilizza
`alfred_record_session_t` dentro `alfred_record_t` e definisce il payload
formatter pubblico: `workspace.root`, `workspace.id` e `ledger.session_id`.
Le stringhe vuote sono trattate come assenti, gli oggetti vuoti non vengono
emessi e il formatter rifiuta payload sessione su record diversi da
`SESSION_CONTEXT` per evitare per-event enrichment accidentale. Questo step non
emetteva ancora il record in runtime: builder, enqueue, ordine nello stream e
golden end-to-end sono stati coperti dal child issue successivo.

Aggiornamento successivo: il micro-step runtime emette `SESSION_CONTEXT` una
sola volta quando `output_enabled=true` e almeno un campo workspace/sessione e'
configurato. Il record viene costruito come borrowed view di
`app_t.workspace_session`, poi attraversa `app_emit_output_record()`,
`app_enqueue_output_record()` e `alfred_record_output_pipeline_enqueue()`, dove
la queue clona owned il payload. Se il contesto e' assente, Alfred non produce
un record lifecycle vuoto; gli eventi filesystem successivi non ricevono
automaticamente payload `workspace` o `ledger`.

Aggiornamento successivo: l'audit finale della milestone verifica che
documentazione, man page, test focused e registro milestone descrivano lo stato
implementato dopo la PR runtime. `SESSION_CONTEXT` e' un record one-shot di
contesto dichiarato della run, non una prova per-evento e non attribuzione di
agente/processo. Restano fuori dal perimetro agent session, process tree,
policy, id generation e enrichment automatico degli eventi filesystem.

Aggiornamento successivo: la milestone `Metadata/session record JSONL v0` viene
chiusa come sottoinsieme implementato. Alfred pubblica il contesto
workspace/sessione configurato con un record `SESSION_CONTEXT` separato, passato
da queue, dispatcher e sink. La chiusura non cambia i non-obiettivi: niente
arricchimento automatico degli eventi filesystem, niente `agent_session_id`,
niente process tree, niente policy e niente generazione automatica di id.

## Aggiornamento Inotify reference backend MVP closure audit

Dopo la chiusura delle milestone JSONL/sessione, il lavoro torna al debito
storico piu' importante: la riga `Inotify reference backend` e' ancora
`in progress` nel registro, anche se molti strati costruiti sopra inotify sono
ormai chiusi. La nuova milestone
`Inotify reference backend MVP closure audit` serve a verificare documentazione,
matrice eventi, stato funzionalita', test e limiti del sottoinsieme staged di
Backend API v0 prima di dichiarare inotify chiuso per MVP.

Il perimetro resta di audit e closure: non introduce fanotify, eBPF, audit,
policy, process attribution, network telemetry o Agent Guard. Se emergono bug
piccoli possono diventare issue figlie; se emergono temi grandi devono essere
registrati come debito o roadmap futura.

Aggiornamento successivo: la milestone `Inotify reference backend MVP closure
audit` chiude il debito storico della riga `Inotify reference backend` nel
registro milestone. L'esito e' una chiusura per MVP, non una promessa di
runtime multi-backend completo: inotify resta il primo backend Linux di
riferimento, con documentazione runtime allineata, mappa test/contratti,
registro debiti residui e criteri blocker espliciti. Restano rimandati
main-loop migration, benchmark comparativi per il ponte raw/record, worker e
backpressure pubblica, raw audit/self-event JSONL dedicati, lifecycle/error
JSONL, process attribution, rete, policy, enforcement e backend futuri come
fanotify, audit, eBPF, Windows e macOS.

## Aggiornamento registro milestone

`37-roadmap-milestone-progetto.md` introduce il registro cronologico delle
milestone di Alfred. Il documento separa GitHub Milestone/Project come tracking
operativo dalla documentazione versionata come memoria storica. Per ogni
milestone prevede data di fine orientativa, durata stimata, durata reale,
dipendenze, motivazione della priorita', cosa viene sbloccato e link a issue,
PR e documenti stabili.

Aggiornamento successivo: dopo la chiusura di Backend API v0 come staged
subset, la milestone operativa successiva e' `Inotify backend conforms to
Backend API v0`. Il registro milestone punta alla GitHub Milestone #3, alla
issue madre #71, alla prima issue figlia tecnica #72 e alla issue/PR di setup
documentale #73/#74. Il primo passo tecnico non e' una riscrittura del main
loop, ma un audit dei gap tra implementazione inotify corrente e contratto
Backend API v0, con particolare attenzione a lifecycle, target management,
capabilities, poll/emit boundary, ownership, diagnostica, test e
documentazione.

Aggiornamento successivo: `40-audit-inotify-backend-api-v0.md` raccoglie il
primo audit della issue #72. Il documento distingue cosa e' gia' conforme per
il subset staged, cosa resta ponte intenzionale e quali micro-step conviene
aprire dopo. La decisione operativa e' che il raw bridge del main loop non e'
una non-conformita' nascosta: resta deliberato finche' non viene deciso e
misurato il modello di input del core.

Aggiornamento successivo: la issue #76 rende esplicito l'endpoint della
milestone `Inotify backend conforms to Backend API v0`. La conformita' inotify
vale per il sottoinsieme staged: ops statiche, capabilities conservative,
lifecycle, target management, emit boundary, poll staged e test focused. Non
include ancora runtime backend-agnostic end-to-end, migrazione del main loop o
core semantico basato su `alfred_record_t`.

Aggiornamento successivo: la issue #78 dettaglia il lifecycle inotify v0 dentro
`40-audit-inotify-backend-api-v0.md`. La nuova mappa collega `init`, `start`,
`add_target`, `remove_target`, `poll`, `stop` e `destroy` alle funzioni reali,
chiarendo che per lo staged subset `init/destroy` possiedono le risorse kernel
e backend mentre `start/stop` sono marker di stato idempotenti. Il documento
rimanda anche ai test focused che proteggono queste regole.

Aggiornamento successivo: la issue #80 avvia la mappa target-management inotify
v0 dentro `40-audit-inotify-backend-api-v0.md`. La sezione chiarisce che
`add_target` e `remove_target` gestiscono target API filesystem-path, non watch
descriptor arbitrari: documenta validazione, path borrowed, copie
backend-owned, duplicati idempotenti, overlap ricorsivi rifiutati, rollback
degli add falliti, autorita' delle root configurate, cleanup ricorsivo e
propagazione degli errori diagnostici dopo la pulizia dello stato.

Aggiornamento successivo: la issue #82 avvia la mappa capabilities inotify v0
dentro `40-audit-inotify-backend-api-v0.md`. La sezione collega
`inotify_backend_capabilities()`, `alfred_backend_capabilities_has()` e
`alfred_backend_ops_is_minimally_valid()` ai flag dichiarati e a quelli
intenzionalmente assenti. Il punto centrale e' che inotify dichiara solo
capability osservazionali filesystem/recovery e non promette audit API-level,
process context, network context, permission events o blocking/enforcement.

Aggiornamento successivo: la issue #84 avvia la mappa poll/emit boundary
inotify v0 dentro `40-audit-inotify-backend-api-v0.md`. La sezione distingue il
runtime principale ancora raw-oriented
`app_run()` -> `app_poll_legacy_raw_backend_once()` -> `inotify_backend_poll()`
-> `handle_backend_event()` dal percorso staged Backend API v0
`backend_ops->poll()` -> `inotify_backend_ops_poll()` ->
`inotify_backend_ops_emit_raw_record()` -> `alfred_record_from_raw()` ->
`emit(alfred_record_t)`. Documenta precondizioni, ownership, `timeout_ms == 0`,
raw `NULL` come no-op, propagazione errori e motivo per cui la migrazione del
main loop resta una decisione separata da misurare.

Aggiornamento successivo: la issue #86 avvia la mappa errori e diagnostica
backend inotify v0 dentro `40-audit-inotify-backend-api-v0.md`. La sezione
separa errore tecnico, record diagnostico osservabile ed evento semantico:
watch lifecycle, resync, lost-scope, overflow, poll/runtime I/O e callback
output hanno responsabilita' diverse. Il punto centrale e' che Alfred deve
rendere visibile la diagnostica gia' modellata come record, propagare gli errori
di output strutturato in modalita' fail-closed e non trasformare diagnostica
backend in falsi eventi filesystem semantici.

Aggiornamento successivo: la issue #88 audita la copertura focused del subset
staged Backend API v0 dentro `40-audit-inotify-backend-api-v0.md`. La sezione
non introduce nuovo comportamento: collega descriptor, capabilities,
lifecycle, target management, poll staged, ownership, diagnostica, overflow,
lost-scope recovery e compatibilita' runtime ai test gia' esistenti. La
decisione e' che nuovi test vanno aggiunti quando cambia un contratto o si
chiude un gap reale; per questo micro-step la copertura viene resa esplicita
in una matrice invece di duplicare assert gia' protetti.

Aggiornamento successivo: la issue #90 chiude la documentazione della milestone
`Inotify backend conforms to Backend API v0`. `31-milestone-inotify-reference-backend.md`
ora distingue il risultato chiuso dal perimetro rimandato: inotify e' reference
backend per il subset staged, ma non esiste ancora runtime multi-backend
end-to-end. `37-roadmap-milestone-progetto.md` registra la milestone come done
e `40-audit-inotify-backend-api-v0.md` elenca micro-step completati e debiti
rimandati: main-loop migration, `timeout_ms != 0`, diagnostica backend
generica, registry multi-backend e backend futuri.

Aggiornamento successivo: la milestone operativa passa a `Tracepoint and Lab
MVP`, con GitHub Milestone #4 e issue madre #92. Il primo micro-step #93 e'
documentale: creare una roadmap dedicata, aggiornare il registro milestone e
chiarire che il Lab MVP non e' ancora una dashboard o un tracing runtime
pesante. `41-tracepoint-lab-roadmap-mvp.md` definisce tracepoint logici,
scenari MVP, formato scenario Lab v0, non-obiettivi e criteri di chiusura.

Aggiornamento successivo: la issue #95 definisce il primo Tracepoint Model v0
come contratto documentale, non come codice runtime. `42-tracepoint-model-v0.md`
chiarisce naming, stati `candidate` / `stable-doc` / `public-output` /
`deprecated`, metadati minimi, rapporto con `alfred_record_t`, vincoli del
percorso caldo, esempi, anti-pattern e criteri prima di introdurre qualunque
output trace pubblico.

Aggiornamento successivo: la issue #97 seleziona il primo set scenari
Tracepoint/Lab MVP. Il set resta piccolo: create file, close-write/file-ready,
rename/move/relocate e watch stale/recovery. `41-tracepoint-lab-roadmap-mvp.md`
spiega perche' questi scenari entrano nell'MVP e perche' overflow, recursive
mkdir, output pipeline end-to-end, policy/agent guardrail e performance trace
restano rimandati. `42-tracepoint-model-v0.md` promuove solo i tracepoint
necessari a `stable-doc`, distinguendo `RAW_EVENT_NORMALIZED` dal candidato
`RAW_RECORD_BUILT`, senza introdurre output trace runtime.

Aggiornamento successivo: la issue #99 collega i tracepoint `stable-doc` a
funzioni, strutture dati e test rappresentativi. `42-tracepoint-model-v0.md`
contiene la mappa principale tracepoint -> funzioni -> dati -> copertura,
`41-tracepoint-lab-roadmap-mvp.md` la riassume per scenario e
`16-mappa-codice-e-strutture.md` aggiunge una vista didattica per seguire il
percorso nel codice. Anche questo passo resta documentale: nessun output trace
runtime e nessun cambio al percorso caldo.

Aggiornamento successivo: la issue #101 decide il formato scenario Lab v0.
`41-tracepoint-lab-roadmap-mvp.md` stabilisce che il formato iniziale e'
Markdown-first con sezioni strutturate, non YAML/JSON e non parser runtime.
La scelta mantiene gli scenari leggibili per studenti e contributor, ma usa
campi stabili per poter generare in futuro un formato macchina.
`42-tracepoint-model-v0.md` chiarisce che gli scenari possono citare tracepoint `stable-doc`
senza trasformarli in output pubblico o contratto interno primario.

Aggiornamento successivo: la issue #103 decide di non implementare runtime
trace output nel Tracepoint/Lab MVP corrente. `41-tracepoint-lab-roadmap-mvp.md`
documenta la scelta: prima si scrivono scenari Lab concreti in Markdown v0,
usando tracepoint `stable-doc`, record, output e test gia' esistenti; solo se
gli scenari mostrano una mancanza reale si riaprira' una issue per un output
trace opt-in, versionato, testato e misurato rispetto al percorso caldo.

Aggiornamento successivo: la issue #105 introduce il primo scenario Lab concreto.
`docs/it/lab/scenarios/create-file.md` applica il formato Markdown v0 al
percorso minimo `IN_CREATE -> RAW_CREATE -> FILE_CREATED -> sink/output`.
`docs/it/lab/README.md` diventa l'indice degli scenari Lab concreti e
`41-tracepoint-lab-roadmap-mvp.md` linka lo scenario dalla roadmap.

Aggiornamento successivo: la issue #107 aggiunge lo scenario Lab
close-write/file-ready. `docs/it/lab/scenarios/file-ready.md` chiarisce la
differenza tra `RAW_MODIFY`/`FILE_MODIFIED` e
`RAW_CLOSE_WRITE`/`FILE_READY`, senza introdurre runtime trace, parser o nuovo
codice.

Aggiornamento successivo: le issue #109 e #111 completano il primo set di
scenari Lab concreti. `docs/it/lab/scenarios/rename-move-relocate.md` spiega
correlazione `MOVED_FROM`/`MOVED_TO`, cookie, move table e classificazione
rename/move/relocate. `docs/it/lab/scenarios/watch-stale-recovery.md` spiega
diagnostica `WATCH_*`, path stale, resync locale e lost-scope recovery senza
inventare semantica filesystem falsa.

Aggiornamento successivo: la issue #113 chiude la milestone `Tracepoint and Lab
MVP` come MVP documentale `stable-doc`. Il risultato stabile e': Tracepoint
Model v0, quattro scenari Markdown v0, mappa tracepoint/funzioni/test e
decisione esplicita di non introdurre ancora `trace.jsonl`, parser, UI,
tracepoint `public-output`, nuovo I/O nel percorso caldo o test focused per
formati macchina non esistenti.

## Aggiornamento visione Observation Runtime

`38-visione-observation-runtime.md` documenta la visione lunga di Alfred come
runtime di osservazioni: eventi, misure, percezioni, inferenze, previsioni,
azioni, outcome e feedback. Il documento chiarisce che questa e' una bussola
architetturale, non scope corrente.

`39-principi-architetturali-futuri.md` traduce la visione in principi pratici:
provenance obbligatoria, separazione tra osservazione/inferenza/decisione,
backend come sensori con capabilities, stato derivato dal log, percorso caldo
corto, versioning separato dei contratti e LLM come adapter/interfaccia, non
come fonte di verita'.

## Aggiornamento recente

La pulizia corrente ha:

- rimosso i due capitoli storici sullo shadow mode e sul TODO dello switch
- aggiornato `README.md` e `docs/it/README.md`
- sostituito i riferimenti operativi a `15-todo-switch-core.md` con
  `25-roadmap-unificata-dossier.md`
- aggiornato i capitoli attivi che descrivevano ancora l'archiviazione dei
  documenti storici come prossimo passo
- mantenuto solo le note storiche necessarie a capire perche' `shadow` e'
  invalido e perche' il core e' l'unico riferimento semantico corrente

## Aggiornamento feature matrix

`26-stato-funzionalita.md` raccoglie in un unico posto le funzionalita'
supportate dal backend inotify, dal raw log, dagli `ALFRED_RAW_*`, dal core
semantico, dalla diagnostica backend e dalla lost-scope recovery. Il documento
rimanda ai test principali ed e' stato usato come base per Event Model v0. Deve
restare il controllo da leggere prima di progettare Backend API v0, JSONL o
nuovi record.

## Aggiornamento pagine man

Sono state aggiunte tre bozze di pagine man:

- `alfred(1)`: uso utente corrente, ambiente, log, limiti e esempi
- `alfred.conf(5)`: formato key-value, chiavi supportate e default
- `alfred-events(7)`: modello eventi corrente, self events, diagnostica e
  lost-scope recovery

Le pagine sono marcate come parziali perche' dovranno essere riallineate dopo
Event Model v0, Backend API v0, JSONL e futura CLI con opzioni esplicite.

## Aggiornamento guida di lettura

`27-guida-lettura-documentazione.md` e' stato aggiunto come documento d'insieme.
Non sostituisce gli altri capitoli: spiega in quale ordine leggerli, separa i
percorsi per utente, contributore, studente e sviluppatore, e fornisce un indice
tematico rapido per ritrovare argomenti come eventi, test, log, scanner,
resync, plugin backend, configurazione e roadmap.

## Aggiornamento audit documentazione

`28-audit-documentazione-e-debiti.md` raccoglie il controllo corrente su link
locali, diagrammi Mermaid, animazioni rimandate e debiti dichiarati nella
documentazione. Serve come punto di discussione per distinguere cosa e' rotto,
cosa e' volutamente rimandato e quali decisioni architetturali conviene
affrontare per prime.

## Aggiornamento Event Model v0

`29-event-model-v0.md` definisce il modello eventi approvato per il prossimo
filone architetturale: un record comune identificato da
`layer + category + type`, con `layer` e `category` controllati e `type`
controllato per ogni coppia layer/category. Il documento include campi comuni,
campi filesystem, campi watch/recovery, campi security futuri opzionali, un
diagramma Mermaid dei record implementati oggi e una tabella riassuntiva con
rimandi a matrice inotify, contratto log, semantica eventi e scenari di test.
Resta parziale perche' `alfred_record_t` esiste come contratto dati in
`core/include/alfred_record.h` e il primo adapter raw esiste in
`core/src/alfred_record_adapter.c`. Lo stesso file contiene anche l'adapter
semantico `alfred_event_t -> alfred_record_t`, usato da `core_logger_on_event()`
prima del formatter testuale. Esiste anche il builder diagnostico `WATCH_*` in
`core/src/alfred_record_diagnostic.c`, il formatter testuale in
`core/src/alfred_record_text.c`, il writer JSONL buffered e i sink/runtime
output introdotti dal Writer Runtime v0.

## Aggiornamento Backend API v0

`30-backend-api-v0.md` definisce la proposta documentale della Backend API v0.
Il documento stabilisce lifecycle, target management, emit sink basato su Event
Model v0, capabilities, error model, ownership, mapping del backend inotify
corrente e roadmap implementativa. Il primo tipo C comune,
`core/include/alfred_record.h`, e' stato aggiunto come contratto dati. Esiste
anche l'adapter `alfred_raw_event_t -> alfred_record_t` per i record
`normalized_raw + filesystem + RAW_*`, l'adapter semantico
`alfred_event_t -> alfred_record_t` e il builder diagnostico per i principali
record `WATCH_*`. Esiste anche `alfred_record_format_text()` per produrre il
payload testuale da record. Il primo sink comune `alfred_record_sink_t` e il
text sink compatibile `alfred_record_text_sink_t` sono stati aggiunti come
ponte `record -> emit(record) -> payload callback` e sono ora usati dal percorso
semantico ufficiale `core_logger_on_event()`. I diagnostici runtime inotify
`WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE`, `WATCH_RESYNC_*` e
`WATCH_LOST_*` passano ora dal sink comune; `WATCH_RESYNC_SCAN_FAILED` e
`WATCH_LOST_QUEUE_FAILED` conservano il canale error tramite un bridge
event/error. Il documento
include uno schema Mermaid della pipeline C introdotta finora. La policy Event
Model v0 per errori OS ora distingue `error`, `os_error_code`,
`os_error_name` e `os_error_message`, e la struttura C `alfred_record_t`
contiene il payload `os_error`. Il builder diagnostico puo' gia' popolare
questi campi tramite `alfred_record_build_watch_diagnostic_with_os_error()`.
Il formatter testuale puo' gia' renderli nella forma compatibile
`errno=N (...)`. Il runtime inotify usa gia' questo percorso per
`WATCH_RESYNC_FAILED` con `errno`, conservando codice OS e messaggio nel record.
Il primo frammento C della Backend API v0 lato input e' ora il modello di
capabilities:

- `core/include/alfred_backend_capabilities.h`
- `core/src/alfred_backend_capabilities.c`
- `modules/inotify/src/inotify_backend_capabilities.c`

Il descriptor `alfred_backend_capabilities_t` dichiara nome backend, versione
API e bitmask delle capability. Il backend inotify dichiara capability
osservazionali filesystem, recursive watch, metadata raw, self events,
overflow, identity tracking e lost-scope recovery. Non dichiara audit events,
permission events, process context, network context o blocking. L'opt-in audit
inotify corrente resta raw-log-only finche' non produce record audit
strutturati. Il contratto e' coperto da
`tests/backend/test_backend_capabilities.c`.

Il contratto Backend API v0 ora ha anche il primo scheletro C compilabile per
`alfred_backend_ops_t` in `core/include/alfred_backend_ops.h` e
`core/src/alfred_backend_ops.c`. Lo scheletro definisce tipi opachi per
backend/config/target, emit boundary basato su `alfred_record_t`, puntatore al
descriptor `alfred_backend_capabilities_t`, callback lifecycle e helper
`alfred_backend_ops_is_minimally_valid()`. Il runtime inotify non e' ancora
migrato a questa tabella: il test `tests/backend/test_backend_ops.c` blocca
solo la forma minima del contratto. La docstring di `alfred_backend_emit_t` e
la Backend API v0 chiariscono anche la ownership dell'emit boundary: il backend
puo' copiare function pointer e `userdata`, ma non deve conservare il puntatore
alla busta `alfred_backend_emit_t` ricevuta da `init()`.
I casi rifiutati da `alfred_backend_ops_is_minimally_valid()` sono ora
documentati esplicitamente nel contratto Backend API v0, nella guida C per
studenti, nella guida contributori e nella man page `alfred-events(7)`, oltre
che nei test.
La roadmap plugin backend non duplica piu' la vecchia bozza di
`alfred_backend_ops_t`: rimanda al contratto Backend API v0, all'header
compilato e ai test, cosi' il prossimo adapter refactor ha una sola fonte di
verita'.
Il backend inotify espone ora il primo descriptor
`inotify_backend_ops()` in `modules/inotify/src/inotify_backend_ops.c`. Questo
descriptor collega nome, versione API e capabilities inotify alla forma comune
`alfred_backend_ops_t` ed e' coperto da
`tests/backend/test_backend_inotify_ops.c`. Il primo micro-step successivo ha
reso reali `init` e `destroy` nella tabella ops usando
`inotify_backend_ops_runtime_t` e `inotify_backend_ops_config_t` come tipi
concreti passati attraverso l'API opaca. `init` costruisce il contesto inotify
esistente, copia function pointer e `userdata` dell'emit boundary e chiama
`inotify_backend_init()`. `destroy` chiama `inotify_backend_shutdown()` solo per
runtime inizializzati e poi azzera i puntatori borrowed del contesto. Dopo i
passi target, anche `add_target` e `remove_target` sono reali per target
filesystem-path minimi. Il passo lifecycle successivo ha reso reali anche
`start` e `stop` come transizioni idempotenti dell'adapter ops: richiedono un
runtime inizializzato e un contesto completo, impostano o azzerano `started`,
ma non aprono fd, non chiudono risorse e non leggono eventi. `poll` resta
placeholder fail-fast finche' non sara' migrato nel suo passo dedicato. Il
runtime normale continua a usare le funzioni inotify-specifiche finche' `app.c`
non verra' migrato.

Il passo successivo ha reso concreto anche il target model minimo in
`alfred_backend_ops.h`: `alfred_backend_target_t` supporta per ora solo
`ALFRED_BACKEND_TARGET_TYPE_FILESYSTEM_PATH`, nessuna flag e nessuna opzione
backend-specifica. L'adapter inotify implementa `add_target` validando quel
target e delegando a `inotify_backend_add_startup_watch()`.

Il micro-step successivo ha implementato anche `remove_target` nella tabella
ops inotify. Il callback accetta lo stesso sottoinsieme di target, rifiuta
runtime incoerenti e target non supportati, poi delega a
`inotify_backend_remove_startup_watch()`. La rimozione passa da
`watch_manager_remove()` per conservare i record diagnostici `WATCH_REMOVED`.
`remove_target` accetta solo root configurate esatte: non puo' essere usato per
rimuovere un child watch creato internamente dalla ricorsivita' di un target
padre, perche' quello romperebbe la copertura del target padre lasciando
`configured_roots` invariato.
Quando la configurazione inotify e' ricorsiva, rimuovere una root rimuove anche
i watch figli sotto quella root con confronto di prefisso a boundary `/`.
`configured_roots` resta pero' l'autorita' per la ownership del target API:
se un watch attivo e' gia' sparito per manutenzione backend o per un evento
kernel come `IN_IGNORED`, `remove_target` deve comunque poter rimuovere la root
configurata esatta. L'assenza di watch attivi non rende invalido il target;
significa solo che non ci sono descriptor kernel da pulire prima di aggiornare
il registro dei target configurati.
Se una diagnostica `WATCH_REMOVED` fallisce durante `remove_target`, il backend
ricorda l'errore ma continua a rimuovere i watch raccolti e poi rimuove la root
configurata esatta. Il chiamante riceve comunque `ERR_INOTIFY`, ma
`configured_roots` e watcher table non restano in uno stato target parziale.
Il helper watcher che raccoglie i descriptor per prefisso azzera `count` su
errore, cosi' il percorso di rimozione non puo' riusare accidentalmente un
conteggio vecchio o un risultato parziale dopo un fallimento della raccolta.
Per v0, `add_target` rifiuta target ricorsivi sovrapposti padre/figlio e
mantiene idempotenti solo i duplicati esatti, per evitare ownership ambigua dei
watch finche' non esisteranno refcount o owner espliciti. `add_target` ha anche
un contratto atomic-like sul target. Il duplicato esatto e' idempotente sia in
modalita' ricorsiva sia non ricorsiva: non reinstalla watch e non emette un
secondo `WATCH_ADDED`. Questa idempotenza e' registry-based: se la root resta
in `configured_roots` ma i watch attivi sono gia' spariti, `add_target(path)`
restituisce comunque `ERR_OK` senza riparare automaticamente la copertura
kernel. Per forzare una reinstallazione bisogna fare `remove_target(path)` e poi
`add_target(path)`. I path target devono avere lunghezza minore di
`PATH_MAX`, perche' inotify v0 conserva configured roots e watcher path in
buffer fissi: un path troppo lungo viene rifiutato con `ERR_INVALID_ARG`, non
con `ERR_ALLOC`. Il target filesystem v0 usa identita' lessicale ristretta:
non canonicalizza ancora symlink, `..`, mount boundary o path relativi, ma
rifiuta gli alias con slash finale tranne `/`. Quindi `/tmp/root` e' valido,
`/tmp/root/` e' `ERR_INVALID_ARG` e `/` resta valido. Per i nuovi target validi,
il backend registra la root prima di installare i watch e, se l'installazione
fallisce, rimuove eventuali watch parziali e annulla la root registrata. In
questo modo un errore di `add_target`
non lascia una root configurata o watch nuovi visibili al chiamante.

Il micro-step lifecycle successivo ha implementato `start` e `stop` nella
tabella ops inotify senza cambiare il runtime normale. `start()` prima di
`init()` o su runtime incoerente restituisce `ERR_INVALID_ARG`; dopo `init()`
restituisce `ERR_OK`, imposta `started=1` ed e' idempotente. `stop()` prima di
`init()` o su runtime incoerente restituisce `ERR_INVALID_ARG`; dopo `init()`
restituisce `ERR_OK`, imposta `started=0` ed e' idempotente anche se `start()`
non era stato chiamato. `destroy()` resta il confine che rilascia fd, watcher
table, configured roots e puntatori borrowed. Per ora `start/stop` non
governano `add_target` e `remove_target`: target management resta vincolato a
runtime inizializzato, mentre la semantica piu' restrittiva del lifecycle verra'
decisa quando `poll` e l'event loop saranno migrati.
`poll` resta placeholder fail-fast.

Il raw runtime bridge e' ora completo per i raw principali di questo branch:
`RAW_CREATE`, `RAW_DELETE`, `RAW_ATTRIB`, `RAW_MODIFY`, `RAW_CLOSE_WRITE`,
`RAW_MOVED_FROM`, `RAW_MOVED_TO` e `RAW_OVERFLOW`. `app.c` converte
`ALFRED_RAW_CREATE`, `ALFRED_RAW_DELETE`, `ALFRED_RAW_ATTRIB`,
`ALFRED_RAW_MODIFY`, `ALFRED_RAW_CLOSE_WRITE`, `ALFRED_RAW_MOVED_FROM`,
`ALFRED_RAW_MOVED_TO` e `ALFRED_RAW_OVERFLOW` con `alfred_record_from_raw()`,
li invia al sink comune, scrive il payload normalizzato su `raw.log` e poi
passa comunque il raw originale ad `alfred_process()`. I diagnostici runtime
`WATCH_ADDED`/`WATCH_REMOVED`/`WATCH_STALE`/`WATCH_RESYNC_*`/`WATCH_LOST_*`
usano gia' record Event Model v0, sink comune e formatter testuale compatibile.
Il contratto di `alfred_record_from_raw()` e' stato reso esplicito: ogni raw
record deve contenere una sola azione primaria, mentre `ALFRED_RAW_ISDIR` resta
un qualificatore. Le mask ambigue vengono rifiutate dall'adapter e sono coperte
da test dedicati.
La prima API di ownership per i record e' stata aggiunta come passo
preparatorio: `alfred_record_clone_owned()` e
`alfred_record_destroy_owned()` clonano e liberano le stringhe presenti in
`alfred_record_t`. La API non e' ancora collegata al runtime hot path; serve a
fissare il contratto prima di introdurre code, dispatcher e writer asincroni.
Il contratto di riuso della destinazione e' stato reso esplicito: la clone API
scrive in un `dst` vuoto/non-owned e chi vuole riusare lo stesso record deve
prima chiamare `alfred_record_destroy_owned()`. Questo evita leak da
sovrascrittura e, allo stesso tempo, evita di trasformare la clone in una
replace API che potrebbe fare `free()` su stringhe borrowed.
`29-event-model-v0.md` e `32-writer-api-v0.md` documentano in dettaglio le
quattro strategie discusse: deep copy per record, storage inline fisso,
pool/arena per batch e string/path table.
La prima `alfred_record_queue_t` e' stata aggiunta come micro-step successivo:
riceve record borrowed, conserva record owned in una coda bounded
single-threaded e trasferisce ownership al chiamante durante `pop()`. Anche
questa API non e' ancora collegata al runtime; serve a fissare FIFO, overflow,
cleanup e lifetime prima di introdurre dispatcher, backpressure e benchmark.
Il contratto di `pop()` e' stato reso esplicito: la destinazione deve essere
zeroed o gia' distrutta, perche' `pop()` trasferisce ownership in una
destinazione vuota e non sostituisce automaticamente record owned precedenti.
Anche `alfred_record_queue_init()` e' ora difensiva: rifiuta la reinit di una
queue attiva, preservando il vecchio buffer e gli owned record finche' il
chiamante non esegue `alfred_record_queue_destroy()`. Il contratto e' stato
stretto ulteriormente: `init()` accetta solo queue zeroed o gia' distrutte, non
variabili automatiche davvero non inizializzate, perche' il codice deve leggere
`queue.items` prima di azzerare la struct.
`32-writer-api-v0.md` ora contiene una tabella riepilogativa delle API di
ownership/queue v0, con input, output, proprietario finale, cleanup richiesto ed
errori tipici. `08-guida-c-usato-nel-progetto.md` collega la stessa logica a una
freccia di ownership borrowed -> queue owned -> caller owned -> destroy.
Il primo `alfred_record_dispatcher_t` e' stato aggiunto come micro-step
successivo: registra sink in uno storage bounded fornito dal chiamante, chiama i
sink in ordine di registrazione e propaga il primo errore. Il documento chiarisce
che "bounded" puo' indicare limiti diversi: nella queue limita i record in
attesa, nel dispatcher limita il numero di sink destinatari.
La sezione `Record Dispatcher v0` di `32-writer-api-v0.md` e' stata poi ampliata
in forma didattica: ora spiega il ruolo del dispatcher, cosa sono i sink, come
`alfred_record_sink_emit()` chiama `sink->emit(userdata, record)`, in quale
ordine vengono contattati i sink, cosa succede al primo errore e perche' il
dispatcher non deve diventare un writer.
Il primo helper di drain `alfred_record_dispatcher_drain_queue()` e' stato
aggiunto come contratto testabile queue -> dispatcher -> sink -> destroy. La
documentazione ora spiega che drain significa estrarre, consegnare e liberare i
record owned, che `max_records` limita il batch e che retry/requeue/dead-letter
sono decisioni future di backpressure.
Prima della PR e' stato aggiunto un pass didattico sulle strutture e sui
callback: `08-guida-c-usato-nel-progetto.md` ora spiega in modo esteso
puntatori a funzione, callback, `void *userdata`, `emit + userdata` e i principali
typedef callback usati da core, backend, scanner, watcher, sink e text sink.
`16-mappa-codice-e-strutture.md` ora contiene una mappa delle nuove strutture
Event Model/output: `alfred_record_sink_t`, `alfred_record_text_sink_t`,
`alfred_record_queue_t`, `alfred_record_dispatcher_sink_t` e
`alfred_record_dispatcher_t`, con campi, responsabilita' e ciclo drain.
`32-writer-api-v0.md` ora contiene anche un diagramma architetturale Mermaid
della pipeline completa backend -> record -> copia owned -> queue -> dispatcher
-> sink -> writer, piu' un sequence diagram che mostra il passaggio temporale
del record. Le animazioni vere restano rimandate alla roadmap documentazione
avanzata, dove la pipeline record/output e' stata aggiunta agli scenari
animabili.

## Aggiornamento Writer API v0

`32-writer-api-v0.md` definisce la proposta documentale della Writer API v0. Il
documento chiarisce che text, JSONL, protobuf, MessagePack, socket, Unix socket
e futuri formati binari sono writer/serializzazioni di `alfred_record_t`, non
il contratto interno primario. La regola architetturale centrale e' che il
percorso caldo deve restare:

```text
evento OS
-> collector/backend
-> normalizzazione minima
-> alfred_record_t
-> enqueue su coda/ring buffer
```

Writer, serializzazione, I/O, flush, dashboard, Lab, report e policy pesante
devono stare fuori dal percorso caldo. Il documento marca i bridge sincroni
correnti verso i logger come passaggi temporanei di migrazione, non come
architettura finale ad alte prestazioni. Il documento ora include anche la
roadmap per coda/ring buffer, possibile coda per sink, classi sink
`critical`/`best-effort`/`debug`, profili operativi, no-op benchmark, plugin
statici plugin-like e rinvio dei plugin `.so` o out-of-process a fasi
successive.

Aggiornamento successivo: `29-event-model-v0.md`, `30-backend-api-v0.md`,
`32-writer-api-v0.md` e `24-roadmap-ai-agent-guardrail.md` ora esplicitano i
primi punti critici da chiudere prima di stabilizzare il confine
`emit(record)`:

- ownership memoria fra record borrowed e record owned accodabile;
- dispatcher/coda come confine fra hot path e writer;
- drop, backpressure e record diagnostici di perdita;
- lifecycle writer e registry statico plugin-like;
- lifecycle/capabilities dei backend;
- contesto futuro agente/processo/workspace/policy senza implementare ancora
  Agent Guard.

Queste sezioni sono contratto architetturale, non stato runtime completo. Il
codice usa ancora bridge sincroni compatibili verso i logger mentre migriamo
gradualmente i raw log e i diagnostici verso record e sink.

Le domande successive su `alfred_raw_event_t ->
alfred_record_from_raw() -> alfred_record_sink_emit() ->
alfred_record_text_sink_emit() -> raw.log`, sulla normalizzazione e sulla
correlazione futura multi-backend sono state consolidate in:

- `07-flusso-eventi.md`, che ora spiega normalizzazione, semantica, deduplica,
  percorso sincrono raw -> record -> text sink e percorso finale asincrono;
- `29-event-model-v0.md`, che ora descrive la correlazione futura fra
  inotify, fanotify, eBPF, processo, workspace, agent session e policy;
- `30-backend-api-v0.md`, che ora ribadisce il confine fra backend,
  core semantico, correlation engine, policy engine e writer.

## Aggiornamento bootstrap agenti e milestone corrente

Sono stati aggiunti:

- `AGENTS.md` nella root del repository come entrypoint breve per Codex e altri
  agenti AI;
- `31-milestone-inotify-reference-backend.md` per definire il perimetro
  corrente: chiudere inotify come backend di riferimento senza rendere inotify
  il centro concettuale del prodotto;
- una sezione di bootstrap in `00-regole-operative.md`;
- un percorso dedicato alle sessioni agente in
  `27-guida-lettura-documentazione.md`;
- nuovi concetti strategici in `24-roadmap-ai-agent-guardrail.md`, tra cui
  Agent Session, Agent Action Ledger, Intent-to-Action Verification,
  would-block mode e Workspace Boundary.

La regola architetturale da conservare e':

```text
inotify deve validare il contratto di Alfred,
non definire il confine finale del prodotto.
```

## Aggiornamento controllo complessita' architetturale

Le regole operative ora esplicitano che una modifica non banale deve aggiornare
almeno uno tra documentazione architetturale, contratto API, scenario/test,
diagramma, ADR o checklist di review. `30-backend-api-v0.md` contiene una
tabella `modulo -> puo' fare -> non deve fare` per backend, adapter, core,
dispatcher/sink, writer, policy futura e Lab/tooling. Lo stesso documento
contiene anche il diagramma del prossimo confine `emit(record)`, che prepara il
passaggio tecnico successivo senza anticipare JSONL.

`28-audit-documentazione-e-debiti.md` registra come debiti dichiarati gli ADR
brevi, la review architetturale periodica, i golden test JSONL e i tag
architetturali cercabili nel codice.

`00-regole-operative.md` e `11-come-contribuire.md` ora fissano anche la regola
per i finding di review: i finding importanti vanno inseriti come commenti
inline nella PR; il commit di fix deve citare PR e link al finding; dopo il fix
bisogna rispondere al commento inline con lo SHA-1 del commit e una spiegazione
in inglese della soluzione. In questo modo la review resta una traccia storica
leggibile e collegata ai commit effettivi.

## Aggiornamento didattico lifetime e ownership C

`08-guida-c-usato-nel-progetto.md` ora contiene una spiegazione estesa del
lifetime della memoria in C: memoria automatica/stack, memoria statica, memoria
dinamica/heap, borrowed pointer, owned pointer, memory leak, double free,
use-after-free, `free()` su memoria borrowed e contratti di ownership delle
funzioni. La sezione usa il caso reale di `alfred_record_clone_owned(src, dst)`
per spiegare perche' il clone richiede una destinazione vuota/non-owned e perche'
il riuso corretto e' `clone -> destroy -> clone -> destroy`.
La stessa sezione chiarisce anche i termini zeroed, non-owned e owned con esempi
minimi su `alfred_record_t`, cosi' i finding sulle API di ownership non
richiedono conoscenza C implicita.

`27-guida-lettura-documentazione.md` ora rimanda direttamente a questa sezione
dall'indice tematico rapido, cosi' uno studente che legge i finding della PR su
owned record, queue e dispatcher puo' recuperare prima i concetti C necessari.

## Aggiornamento JSONL writer v0

`alfred_record_format_jsonl()` e `alfred_record_jsonl_sink_emit()` introducono
il primo supporto JSONL testabile sopra `alfred_record_t`. Il codice non e'
ancora collegato al runtime: formatta record raw, semantic e diagnostic in un
oggetto JSON senza newline e consegna il payload a una callback caller-owned.
`29-event-model-v0.md` e `32-writer-api-v0.md` documentano il mapping
`alfred_record_t -> JSONL`, gli esempi raw/semantic/diagnostic, l'escaping delle
stringhe e i limiti intenzionali v0. La documentazione chiarisce anche che non
usiamo librerie JSON esterne e che la serializzazione lossless di path Linux con
byte non UTF-8 resta un tema futuro. Il mapping `identity` e' stato fissato come
coppia atomica: JSONL emette `device_id` e `inode_id` solo se sono entrambi
presenti, evitando identita' parziali ambigue. `32-writer-api-v0.md` contiene
ora anche una tabella esplicita di debito tecnico JSONL v0: omissione dei campi
zero/`NULL`, assenza di `null` espliciti, path Linux non UTF-8 non lossless,
sink sincrono, assenza di backpressure, assenza di framing file/socket e schema
JSON non ancora formalizzato.

Il contratto del JSONL buffered writer e' stato chiarito anche sul ciclo di
vita: `alfred_record_jsonl_writer_init()` accetta solo writer inattivi con
`used == 0`. Se il writer contiene bytes pendenti, `init()` fallisce e preserva
buffer e contatore. La scelta evita che una reinit accidentale cancelli righe
JSONL non ancora flushate. `32-writer-api-v0.md` e
`16-mappa-codice-e-strutture.md` spiegano ora la differenza tra stato inattivo,
stato buffered attivo e riuso corretto del writer.

## Aggiornamento counter sink v0

`alfred_record_counter_sink_t` introduce il primo sink no-op/counter per misure
baseline. Il sink si collega al confine generico `alfred_record_sink_t`, conta
record totali, layer principali e category principali, ma non formatta testo,
non genera JSONL, non scrive file/socket, non alloca memoria e non conserva
puntatori borrowed. `32-writer-api-v0.md` lo documenta come baseline per
benchmark futuri, `29-event-model-v0.md` lo inserisce fra gli helper C del
percorso record -> sink, e `10-debugging-test-e-strumenti.md` spiega il test
dedicato `test_record_counter_sink.sh`.

## Aggiornamento benchmark record sink

`tests/perf/bench_record_sinks.c` e `tests/perf/run_record_sinks.sh`
introducono il primo micro-benchmark manuale per confrontare `counter`, `text`
e `jsonl` sink con record sintetici in memoria. Il nuovo target
`make perf-record-sinks` stampa CSV con `sink`, `records`, `runs`, `min_us`,
`avg_us`, `max_us`, `records_per_sec_avg`, `bytes_last` e
`counter_total_last`. Lo script accetta `--records` e `--runs`, cosi' si possono
fare misure ripetute senza cambiare codice. Il benchmark non usa inotify, non
scrive file/socket e non misura backpressure: serve come baseline locale per
separare il costo del confine `record -> sink` dal costo di formattazione text o
JSONL. La guida test e la Writer API spiegano come lanciarlo e come leggere le
colonne. La guida test ora include anche esempi di interpretazione per righe
`counter` e `jsonl`, la formula del throughput medio e le indicazioni pratiche
per capire se il costo e' nel core/sink boundary oppure nella formattazione.
Il target `make perf` e' stato aggiunto come indice testuale dei benchmark
manuali: non esegue misure, ma spiega in inglese cosa fanno
`perf-record-sinks` e `perf-lost-scope`. Non contiene ancora riferimenti a man
page perche' le pagine manuale dei benchmark non sono state scritte.

Aggiornamento successivo: `make perf-record-sinks` produce ora anche la riga
`queue-counter`. Questa riga misura il primo confine runtime documentato nella
roadmap writer: record borrowed, clone owned, push nella queue, pop, emit al
counter sink e destroy del record owned. Non misura text, JSONL, file, socket o
flush. Serve a capire il costo del passaggio `record -> queue -> sink` prima di
collegare worker, dispatcher runtime o JSONL buffered writer.

Aggiornamento successivo: lo stesso benchmark produce ora anche le righe
`dispatcher-counter`, `dispatcher-text`, `dispatcher-jsonl` e
`dispatcher-counter-text-jsonl`. Queste righe misurano
`alfred_record_dispatcher_dispatch_one()` verso sink registrati, senza coda,
thread o I/O reale. La guida test contiene ora un diagramma Mermaid del percorso
backend -> adapter -> record -> queue -> dispatcher -> sink/writer e una tabella
che collega ogni riga CSV alle funzioni interessate.

Aggiornamento successivo: il benchmark produce ora anche le righe
`queue-dispatcher-counter`, `queue-dispatcher-text`,
`queue-dispatcher-jsonl` e `queue-dispatcher-counter-text-jsonl`. Queste righe
misurano `alfred_record_dispatcher_drain_queue()` con record owned in queue,
pop, dispatch, sink emit e destroy del record owned. Sono la prima misura del
percorso single-threaded piu' vicino al runtime writer target, ancora senza
thread, socket, file flush o backpressure reale.

Aggiornamento successivo: `alfred_record_runtime_drain_once()` introduce il
worker/drain simulato single-threaded sopra queue e dispatcher. Il nuovo helper
non crea thread e non collega writer runtime reali: chiama il drain basso
livello e restituisce `max_records`, `dispatched`, `remaining` e `status`.
`test_record_runtime_drain.sh` copre coda vuota, batch limit, drain completo,
errore sink e input invalidi.

Aggiornamento successivo: `alfred_record_jsonl_writer_t` introduce il primo
writer JSONL buffered. Il writer resta fuori dal backend e fuori da thread reali:
usa un buffer di formattazione per un singolo oggetto JSON, un buffer output per
una o piu' righe JSONL complete e una callback `write(data, size)` chiamata solo
al flush o quando serve spazio per una nuova riga. `test_record_jsonl_writer.sh`
copre batching, flush esplicito, auto-flush, errore di flush con bytes
preservati, riga singola troppo grande, esposizione come sink generico e input
invalidi.

Aggiornamento successivo: `config_t.output` introduce la configurazione minima
del runtime output. I default sono `output_enabled=false`,
`output_format=jsonl`, `output_buffer_size=65536` e `output_log=output.jsonl`;
il parser accetta anche `text` per configurazioni disabilitate/future, rifiuta
formati non implementati e rifiuta buffer sotto `8192` o malformati. La
documentazione chiarisce che `enabled=false` mantiene solo il path compatibile
`raw.log`/`events.log`/`errors.log`, mentre `enabled=true` collega in modo
sincrono il primo percorso JSONL aggiuntivo per i raw record gia' migrati al
record sink. `test_output_config.sh` copre default, valori validi e invalidi.

Aggiornamento successivo: `alfred_record_output_pipeline_t` introduce la prima
pipeline composta single-writer. La pipeline non crea thread: quando e'
abilitata, fa `enqueue` di record owned nella queue, `drain_once` verso
dispatcher e JSONL writer, e `flush` separato verso callback bytes. Quando e'
disabilitata, tutti i passaggi sono no-op riusciti. Il test
`test_record_output_pipeline.sh` copre disabled mode, pipeline JSONL abilitata,
batch drain, queue full e flush failure con bytes preservati.

Aggiornamento successivo: `app.c` e `core_logger.c` collegano la pipeline dietro
`output_enabled=true` e `output_format=jsonl`. Il path e' ancora sincrono e
aggiuntivo: i log compatibili restano attivi, mentre i raw record normalizzati
gia' candidati al record sink e gli eventi semantici core vengono convertiti in
`alfred_record_t`, scritti sui log compatibili tramite text sink e accodati nella
pipeline JSONL per `output_log`. `test_output_pipeline_runtime.sh` verifica il
comportamento end-to-end con `ALFRED_CONFIG`.

Aggiornamento successivo: `inotify_backend_context_t` espone un callback
generico `emit_record` per diagnostica backend strutturata. `watch_manager.c`
lo usa per offrire `WATCH_ADDED` e `WATCH_REMOVED` alla stessa output pipeline
JSONL dopo aver preservato `events.log`. Il watch manager continua a non
conoscere `app_t`, file di output o writer: costruisce solo il record
diagnostico e lo consegna al callback borrowed. `test_output_pipeline_runtime.sh`
ha iniziato a controllare anche `WATCH_ADDED` e `WATCH_REMOVED` in
`output.jsonl`.

Aggiornamento successivo: `WATCH_STALE` usa lo stesso callback `emit_record`
dopo aver preservato `events.log`, quindi entra in `output.jsonl` quando
`output_enabled=true`. Il routing resta volutamente limitato: `WATCH_RESYNC_*`,
`WATCH_LOST_*` e, in quel momento, `WATCH_STALE_EVENT_DROPPED` richiedevano
ancora micro-step dedicati.

Aggiornamento successivo: `WATCH_STALE_EVENT_DROPPED` completa la famiglia
diagnostica watch base nel writer runtime. Il backend usa
`alfred_record_build_stale_event_dropped()` per conservare in modo strutturato
la mask e il nome dell'evento kernel scartato (`watch.event_mask` e
`watch.event_name`), preserva prima `events.log`, poi offre il record borrowed
alla output pipeline JSONL. Restano fuori dal writer runtime i diagnostici
`WATCH_RESYNC_*` e `WATCH_LOST_*`.

Aggiornamento successivo: la pipeline JSONL runtime adotta una policy
fail-closed quando `output_enabled=true`. Se enqueue, drain o writer falliscono,
`app_emit_output_record()` marca `app.output_failed` e il callback applicativo
propaga `ERR_IO` verso `app_run()`. Questo evita che Alfred continui a processare
eventi producendo un `output.jsonl` parziale e non affidabile. Il test runtime
usa `/dev/full` per simulare un writer che fallisce durante il flush del buffer
JSONL e verifica che Alfred termini con stato non-zero e diagnostica in
`errors.log`.

Aggiornamento successivo: il contratto fail-closed copre anche il flush finale
di shutdown. Un record puo' restare nel buffer JSONL se il buffer non si riempie
durante `app_run()`: in quel caso l'errore di I/O puo' comparire solo quando
`app_shutdown()` chiama il flush conclusivo. `app_shutdown()` ora restituisce
`ERR_IO` in quel caso e `main()` lo converte in exit non-zero quando il loop era
terminato senza errori. `test_output_pipeline_runtime.sh` aggiunge uno scenario
con pochi eventi e `output_log=/dev/full` per bloccare questa regressione.

Aggiornamento successivo: la stessa policy e' stata estesa alla diagnostica
watch base prodotta direttamente dal backend. `watch_manager_add()`,
`watch_manager_remove()`, `WATCH_STALE` e `WATCH_STALE_EVENT_DROPPED` propagano
ora il fallimento del callback `emit_record` invece di limitarsi a loggarlo. Il
backend resta indipendente da JSONL e non decide la policy di processo: espone
l'errore all'applicazione, che applica `output_enabled=true` come fail-closed.
`test_watch_diagnostic_output_failure` verifica il caso `WATCH_ADDED` con
rollback del watch appena installato.

Aggiornamento successivo: il ramo `WATCH_STALE` ha ora una copertura dedicata.
`WATCH_STALE` nasce durante il poll dei self-event (`IN_MOVE_SELF`,
`IN_DELETE_SELF`, `IN_UNMOUNT`), non durante l'installazione del watch: per
questo il test separato `test_watch_stale_output_failure` fa fallire solo il
callback strutturato su quel tipo di record e verifica che il backend scriva il
log compatibile, registri l'errore e restituisca fallimento al chiamante.

Aggiornamento successivo: `22-contratto-log.md` contiene ora una mappa di
copertura per tutte le famiglie loggabili: fatti kernel/backend `IN_*`, audit
inotify, raw Alfred normalizzati, raw sintetici, eventi semantici core,
diagnostica watch/resync/lost-scope, lifecycle, errori, trace e security futura.
La mappa chiarisce per ogni famiglia se oggi appare nei log testuali, se esiste
un adapter/builder verso `alfred_record_t`, se il record e' sink-capable, se e'
gia' runtime-routed verso un sink o una pipeline, e se entra in `output.jsonl`.
La regola fissata e' che `output.jsonl` oggi e' un sottoinsieme strutturato
opt-in, non una copia completa di Alfred.

Aggiornamento successivo: `make perf-record-sinks` produce anche la riga
`output-pipeline-jsonl`. Questa misura la pipeline composta con record sintetici:
enqueue owned, runtime drain, dispatcher, JSONL buffered writer e flush finale
verso callback in memoria. Non misura ancora file I/O, socket, thread, lock o
backpressure reale. La guida test spiega come confrontarla con
`queue-dispatcher-jsonl` per capire se il wrapper pipeline aggiunge overhead
rilevante prima del collegamento ad `app_run()`.

Aggiornamento successivo: `34-report-benchmark-prestazioni.md` raccoglie i run
manuali gia' eseguiti, i valori CSV osservati, l'interpretazione delle righe
`counter`, `queue-dispatcher-jsonl` e `output-pipeline-jsonl`, e le conclusioni
provvisorie sul costo della pipeline. Il report chiarisce che i numeri sono
indicativi, che `runs=1` non basta per dichiarazioni prestazionali definitive e
che il risultato utile per ora e' l'assenza di overhead macroscopico del wrapper
pipeline rispetto a queue + dispatcher + JSONL.

Aggiornamento successivo: la documentazione Writer API v0 e la mappa codice sono
state riallineate allo stato runtime corrente. JSONL non e' piu' descritto come
formatter non collegato al runtime: oggi `app_run()` puo' instradare i record
gia' migrati nella pipeline `record -> queue -> dispatcher -> JSONL writer`
quando `output_enabled=true` e `output_format=jsonl`. Resta invece non
implementato il writer asincrono finale con worker thread, code per sink,
backpressure avanzata, target multipli e profili operativi.

Aggiornamento successivo: il routing dei record semantici verso JSONL e' stato
separato dal successo del text sink compatibile. La v0 resta sincrona, ma un
record semantico strutturato valido viene ora offerto alla output pipeline prima
del formatter testuale di `events.log`, cosi' un path molto lungo non puo' far
perdere silenziosamente il record JSONL solo perche' la riga legacy supera il
buffer umano. `test_output_pipeline_runtime.sh` copre questo caso con un path
profondo creato prima dello startup e un file creato dopo l'avvio.

Aggiornamento successivo: la mappa del contratto log chiarisce il prossimo
micro-step sui diagnostici `WATCH_RESYNC_*`. Questi record sono gia'
`sink-capable` perche' hanno builder Event Model v0 e formatter text/JSONL, ma
oggi sono `runtime-routed` solo verso il text sink compatibile del backend
inotify. Non attraversano ancora `emit_record` e quindi non compaiono in
`output.jsonl`. La documentazione ora distingue esplicitamente text sink legacy,
output pipeline e prossimo routing fail-closed verso JSONL.

Aggiornamento successivo: i diagnostici `WATCH_RESYNC_*` sono stati collegati
alla output pipeline. Il backend inotify conserva prima la riga compatibile in
`events.log` o `errors.log`, poi offre lo stesso record borrowed a
`ctx->emit_record`. Se il callback fallisce, il percorso resync propaga errore
e il poll puo' fallire chiuso quando l'utente ha abilitato l'output
strutturato. `tests/backend/test_resync_output_routing.c` fissa il contratto per
`WATCH_RESYNC_BEGIN`, `WATCH_RESYNC_FAILED` e il ramo di fallimento callback. I
diagnostici `WATCH_LOST_*` restano il prossimo gruppo da migrare.

Aggiornamento successivo: il primo sottoinsieme `WATCH_LOST_*` e' stato
collegato alla output pipeline. `WATCH_LOST_QUEUED`,
`WATCH_LOST_QUEUE_SKIPPED` e `WATCH_LOST_QUEUE_FAILED` conservano il log
compatibile e poi attraversano `ctx->emit_record`, quindi possono comparire in
`output.jsonl` quando l'output strutturato e' abilitato. Il test mirato
`tests/backend/test_lost_scope_queue_output_routing.c` copre successi e
fallimento callback; `test_output_pipeline_runtime.sh` verifica il record
`WATCH_LOST_QUEUED` nel runtime end-to-end. I record lost-scope di scan,
coverage, reinstallazione, retry e fine recovery restano text-only fino al
prossimo micro-step.

Aggiornamento successivo: anche la fase scan/classificazione lost-scope e'
stata collegata alla output pipeline. `WATCH_LOST_SCAN_BEGIN`,
`WATCH_LOST_FOUND`, `WATCH_LOST_PREFIX_UPDATED`,
`WATCH_LOST_COVERAGE_DONE`, `WATCH_LOST_COVERAGE_MISSING`,
`WATCH_LOST_COVERAGE_CLASS`, `WATCH_LOST_NOT_FOUND` e
`WATCH_LOST_RECOVERY_FAILED` attraversano ora `ctx->emit_record` dopo il log
compatibile. `tests/backend/test_lost_scope_scan_output_routing.c` verifica i
campi strutturati principali. Restano text-only i record che installano o
rimuovono watch durante recovery e quelli che schedulano/concludono la
recovery: reinstall, rollback, retry scheduled, gave-up e recovery end.

Aggiornamento successivo: il routing `WATCH_LOST_*` e' completo. Anche
`WATCH_LOST_REINSTALLED`, `WATCH_LOST_REINSTALL_FAILED`,
`WATCH_LOST_ROLLBACK`, `WATCH_LOST_RETRY_SCHEDULED`,
`WATCH_LOST_RECOVERY_GAVE_UP` e `WATCH_LOST_RECOVERY_END` vengono offerti a
`ctx->emit_record` dopo il log compatibile. Il test
`tests/backend/test_lost_scope_completion_output_routing.c` verifica i campi
strutturati della fase finale: path installato, watch rimosso in rollback,
retry count, delay, pending count, stato finale e numero di watch validati.

Aggiornamento successivo: la review della PR sui `WATCH_LOST_*` ha chiarito il
contratto di errore del confine `emit_record`. I call-site della recovery
lost-scope non possono piu' ignorare il valore di ritorno del helper
diagnostico: se il log compatibile e' stato scritto ma il record strutturato
viene rifiutato da `emit_record`, il backend propaga `output-failed` e il poll
runtime si ferma. Questo evita che `events.log` sembri completo mentre
`output.jsonl` perde un record diagnostico della recovery. Il test
`tests/backend/test_lost_scope_recovery.c` copre il caso in cui
`WATCH_LOST_FOUND` viene rifiutato dal callback e la recovery si interrompe
prima di `WATCH_LOST_RECOVERY_END`.

Aggiornamento successivo: la stessa review ha evidenziato che il text sink non
puo' essere il gate del ledger strutturato. Se `alfred_record_text_sink_emit()`
fallisce per buffer testuale insufficiente, i helper `WATCH_RESYNC_*` e
`WATCH_LOST_*` scrivono comunque il fallback legacy su `events.log` /
`errors.log` e poi chiamano `emit_record` con il record gia' costruito. I test
`tests/backend/test_resync_output_routing.c` e
`tests/backend/test_lost_scope_scan_output_routing.c` coprono path lunghi che
forzano il fallback e verificano che il record strutturato attraversi ancora la
output pipeline.

Aggiornamento successivo: una seconda review della PR ha trovato lo stesso
pattern nel helper generico `backend_log_watch_diagnostic_record()`, usato da
`WATCH_STALE` e dal `WATCH_RESYNC_FAILED` semplice. Anche quel helper ora
separa compatibilita' testuale e output strutturato: se il text sink fallisce
per payload troppo lungo, viene scritto il fallback legacy e poi viene chiamato
`emit_record`. I test `test_watch_stale_output_failure.c` e
`test_resync_output_routing.c` includono casi con path lungo e callback
fallito/riuscito per fissare il contratto.

Aggiornamento successivo: una terza review della stessa PR ha individuato il
residuo su `WATCH_STALE_EVENT_DROPPED`. Anche questo diagnostico e' gia'
sink-capable e runtime-routed, quindi il fallback del text sink non puo'
impedire al record strutturato di attraversare `emit_record`. Il helper
`backend_log_stale_event_dropped()` ora segue lo stesso schema
`record_built`/`compat_logged`: scrive prima la riga compatibile, eventualmente
tramite fallback legacy, e poi offre comunque il record gia' costruito al
callback strutturato. `tests/backend/test_watch_stale_output_failure.c` copre
path lungo, emissione riuscita e fallimento fail-closed anche per
`WATCH_STALE_EVENT_DROPPED`.

Aggiornamento successivo: una quarta review ha completato la stessa regola per
la diagnostica watch lifecycle in `watch_manager.c`. `WATCH_ADDED` e
`WATCH_REMOVED` ora separano anche loro il log compatibile dal confine
strutturato: se il text sink non riesce a formattare la riga umana, viene
scritto il fallback legacy e il record gia' costruito attraversa comunque
`emit_record`. `tests/backend/test_watch_diagnostic_output_failure.c` include
un caso con path sintetico lungo per forzare questo fallback senza modificare la
API pubblica del watch manager.

## Aggiornamento Writer Runtime v0

`33-writer-runtime-roadmap-v0.md` separa la Writer API v0 dalla roadmap runtime
dei writer. Il nuovo documento chiarisce che il percorso caldo deve fermarsi a
`record -> enqueue`, mentre text, JSONL, protobuf, MessagePack, socket, Lab,
flush, report e policy pesante devono stare fuori dal percorso caldo. La
roadmap documenta lo stato corrente di sink, queue, dispatcher e benchmark,
spiega il confine borrowed -> owned prima della coda, confronta coda comune e
future code per sink, introduce le classi `critical`, `best_effort` e `debug`,
descrive il problema della backpressure e fissa un ordine di micro-step:
benchmark queue -> counter, dispatcher -> sink, queue -> dispatcher -> sink,
worker simulato, JSONL buffered writer e solo dopo eventuali thread o code per
sink. `README.md`, `27-guida-lettura-documentazione.md`,
`31-milestone-inotify-reference-backend.md` e `26-stato-funzionalita.md` sono
stati aggiornati per rimandare al nuovo capitolo.

Aggiornamento successivo: la roadmap Writer Runtime v0 dichiara esplicitamente
la decisione di chiusura della v0 come runtime single-threaded documentato. Il
confine corrente comprende record owned al limite della coda, coda bounded,
drain esplicito, dispatcher, sink JSONL, counter sink, contatori runtime, test
backend, golden JSONL e benchmark di orientamento. Restano fuori dalla v0 worker
thread, code per sink, policy `critical`/`best_effort`/`debug`, retry/drop
avanzati, writer socket/binari e garanzie di latenza su workload reali. Questa
scelta evita di introdurre concorrenza reale prima di stabilizzare ownership,
backpressure minima e contratto JSONL.

Aggiornamento successivo: `00-regole-operative.md` e
`10-debugging-test-e-strumenti.md` chiariscono la regola di scelta dei test.
I contratti interni fra moduli, ownership, queue, dispatcher, sink e writer
vanno protetti con test C unitari o di integrazione mirata. I comportamenti
pubblici end-to-end devono essere fissati progressivamente con golden test
JSONL, perche' `output.jsonl` e' il contratto esterno strutturato. I test
testuali su `raw.log`, `events.log` ed `errors.log` restano in parallelo come
compatibilita' storica, debug umano e supporto didattico.

Aggiornamento successivo: nasce la prima suite golden JSONL end-to-end in
`tests/jsonl`. Il target `make test-jsonl` compila Alfred e lancia
`test_create_file_and_dir_jsonl.sh`, che abilita `output_enabled=true`, crea un
file e una directory, verifica i log compatibili e poi fa parsing reale di
`output.jsonl` con la libreria standard Python `json`. Il test fissa il primo
contratto esterno strutturato per `RAW_CREATE`, `FILE_CREATED`, `DIR_CREATED` e
`WATCH_ADDED`. La GitHub Action esegue ora anche `make test-jsonl` e carica i
log/JSONL della suite in caso di fallimento.

Aggiornamento successivo: la suite JSONL contiene ora anche
`test_rename_file_jsonl.sh`. Lo scenario crea `old-jsonl.txt`, lo rinomina in
`new-jsonl.txt` e verifica il contratto strutturato di `RAW_MOVED_FROM`,
`RAW_MOVED_TO` e `FILE_RENAMED`. Il test controlla che i due raw move espongano
lo stesso cookie non nullo e che il record semantico usi `old_path` e
`new_path`, fissando il primo caso di correlazione raw -> semantica nel formato
JSONL pubblico.

Aggiornamento successivo: `test_self_move_recovery_jsonl.sh` aggiunge il primo
golden JSONL per diagnostica recovery. Lo scenario sposta una directory
osservata fuori dal vecchio path e verifica `WATCH_STALE`,
`WATCH_RESYNC_BEGIN`, `WATCH_RESYNC_FAILED` con `watch.error=path-unreachable`
e `WATCH_LOST_QUEUED` con `recovery.pending_count`. Il test controlla anche che
non vengano prodotti record semantici `DIR_MOVED`, `DIR_RENAMED` o
`DIR_RELOCATED`, perche' `IN_MOVE_SELF` non contiene il nuovo path.

Aggiornamento successivo: il golden self-move recovery ora controlla anche gli
eventi figli ricevuti su un watch stale. Lo scenario crea
`proof-after-move.txt` dopo lo spostamento della directory osservata e verifica
due lati del contratto: Alfred deve produrre diagnostica
`WATCH_STALE_EVENT_DROPPED`, ma non deve emettere record filesystem
`normalized_raw` o `semantic` per quel file usando il vecchio path stale.

Aggiornamento successivo: la suite JSONL copre ora anche il caso di directory
relocated con `test_dir_relocated_jsonl.sh`. Lo scenario crea `src`, `dst` e
`src/before`, poi sposta e rinomina la directory in `dst/after`. Il test
verifica i due record raw `RAW_MOVED_FROM` e `RAW_MOVED_TO` con lo stesso cookie
non nullo, controlla le mask da directory `288` e `320`, e fissa il contratto
semantico pubblico `DIR_RELOCATED` con `old_path` e `new_path`. Lo stesso test
controlla anche che l'operazione non venga degradata o duplicata come
`DIR_MOVED` o `DIR_RENAMED`.

Aggiornamento successivo: `test_dir_renamed_jsonl.sh` aggiunge il golden JSONL
per rename directory nello stesso parent. Lo scenario crea `old-dir`, lo
rinomina in `new-dir`, verifica i due raw move da directory con cookie comune e
mask `288`/`320`, e richiede esattamente un `DIR_RENAMED`. Il test rifiuta
`DIR_MOVED` e `DIR_RELOCATED` sia nei log compatibili sia nel contratto JSONL,
per proteggere la distinzione semantica parent uguale + basename diverso.

Aggiornamento successivo: `test_dir_moved_jsonl.sh` aggiunge il golden JSONL
per move directory con basename invariato. Lo scenario crea `src`, `dst` e
`src/item`, poi sposta la directory in `dst/item`. Il test verifica i due raw
move da directory con cookie comune e mask `288`/`320`, e richiede esattamente
un `DIR_MOVED`. Il test rifiuta `DIR_RENAMED` e `DIR_RELOCATED` sia nei log
compatibili sia nel contratto JSONL, per proteggere la distinzione semantica
parent diverso + basename uguale.

Aggiornamento successivo: `test_file_moved_jsonl.sh` aggiunge il golden JSONL
per move file con basename invariato. Lo scenario crea `src`, `dst` e
`src/item.txt`, poi sposta il file in `dst/item.txt`. Il test verifica i due raw
move da file con cookie comune e mask `32`/`64`, e richiede esattamente un
`FILE_MOVED`. Il test rifiuta `FILE_RENAMED` e `FILE_RELOCATED` sia nei log
compatibili sia nel contratto JSONL, per estendere anche ai file la distinzione
parent diverso + basename uguale gia' fissata per le directory.

Aggiornamento successivo: `test_file_relocated_jsonl.sh` aggiunge il golden
JSONL per relocated file. Lo scenario crea `src`, `dst` e `src/before.txt`, poi
sposta e rinomina il file in `dst/after.txt`. Il test verifica i due raw move da
file con cookie comune e mask `32`/`64`, e richiede esattamente un
`FILE_RELOCATED`. Il test rifiuta `FILE_MOVED` e `FILE_RENAMED` sia nei log
compatibili sia nel contratto JSONL, completando la triade file rename, move e
relocation.

Aggiornamento successivo: `test_modify_file_jsonl.sh` aggiunge il golden JSONL
per il ciclo modify / close-write. Lo scenario scrive `editable.txt` una prima
volta e poi lo modifica con una seconda append. Il test verifica una
`RAW_CREATE`, due `RAW_MODIFY` con mask `4`, due `RAW_CLOSE_WRITE` con mask
`16`, una `FILE_CREATED`, due `FILE_MODIFIED` e due `FILE_READY`. La
documentazione chiarisce che `FILE_MODIFIED` indica contenuto cambiato, mentre
`FILE_READY` indica chiusura dopo scrittura e non e' un duplicato del modify.

Aggiornamento successivo: `test_delete_file_jsonl.sh` aggiunge il golden JSONL
per cancellazione file. Lo scenario scrive `delete-me.txt`, aspetta il ciclo
write, poi cancella il file. Il test verifica `RAW_CREATE`, `RAW_MODIFY`,
`RAW_CLOSE_WRITE`, `RAW_DELETE` con mask `2`, `FILE_CREATED`,
`FILE_MODIFIED`, `FILE_READY` e `FILE_DELETED`. Il test rifiuta `DIR_DELETED`
per mantenere esplicita la distinzione tra cancellazione file e cancellazione
directory.

Aggiornamento successivo: `test_delete_dir_jsonl.sh` aggiunge il golden JSONL
per cancellazione directory. Lo scenario crea `delete-dir`, aspetta
l'installazione del watch ricorsivo e poi rimuove la directory. Il test verifica
`RAW_CREATE` con mask `257`, `RAW_DELETE` con mask `258`, `DIR_CREATED`,
`DIR_DELETED`, `WATCH_ADDED` e `WATCH_REMOVED`. La documentazione chiarisce che
`WATCH_REMOVED` e' diagnostica di backend sulla pulizia del watch, non una
seconda cancellazione semantica, e il test rifiuta `FILE_DELETED`.

Aggiornamento successivo: `test_attrib_raw_jsonl.sh` aggiunge il golden JSONL
per `RAW_ATTRIB`. Lo scenario prepara `metadata.txt` prima dello startup, forza
il modo iniziale `0644` e poi usa `chmod 600` dopo lo startup per generare
`IN_ATTRIB`. Il test verifica `RAW_ATTRIB` con mask `8` nei log compatibili e in
`output.jsonl`, e controlla che `events.log` non cresca dopo il `chmod 600`. La
documentazione chiarisce che il cambio attributi e' osservato come fatto
raw/backend, ma non produce ancora una semantica core ufficiale.

Aggiornamento successivo: `test_overflow_raw_jsonl.sh` aggiunge il golden JSONL
per `RAW_OVERFLOW`. Lo scenario non prova a generare un overflow reale del
kernel: compila un helper C che costruisce un `IN_Q_OVERFLOW` sintetico con
`wd=-1`, passa da `backend_build_overflow_raw()`, converte in `alfred_record_t`
e formatta JSONL. Il test verifica `raw_mask=128` e `path=""`, chiarendo che
l'overflow riguarda l'intera istanza inotify e non un singolo path osservato.

Aggiornamento successivo: `22-contratto-log.md` e' stato riallineato allo stato
corrente della suite JSONL golden. La tabella
degli eventi semantici non descrive piu' `output.jsonl` come assente per
`FILE_CREATED`, `DIR_CREATED`, `FILE_MODIFIED`, `FILE_READY`, delete, rename,
move e relocated: questi record sono runtime-routed verso la pipeline JSONL
quando `output_enabled=true`. Il documento distingue ora esplicitamente tra
`runtime-routed`, cioe' un record che il codice puo' portare alla pipeline, e
`golden JSONL`, cioe' uno scenario stabile che blocca quel contratto. La stessa
sezione elenca gli scenari `tests/jsonl` coperti e i gap allora rimasti:
recovery lost-scope completa, errori strutturati, eventuale semantic
`OVERFLOW`, lifecycle/app, trace/performance e security/policy. Anche
`26-stato-funzionalita.md` e' stato aggiornato per chiarire che i raw principali
passano dal record sink e che `RAW_OVERFLOW` e' coperto come golden sintetico.

Aggiornamento successivo: `test_lost_scope_runtime_recovery_jsonl.sh` aggiunge
il golden JSONL runtime per la recovery lost-scope completa. Lo scenario avvia
Alfred con due root configurate, crea una directory watched sotto root A, la
sposta sotto root B, lascia che il watch figlio riceva `IN_MOVE_SELF`, poi
verifica la sequenza strutturata `WATCH_STALE`,
`WATCH_RESYNC_FAILED`, `WATCH_LOST_QUEUED`, scan root A,
`WATCH_LOST_NOT_FOUND`, scan root B, `WATCH_LOST_FOUND` e
`WATCH_LOST_RECOVERY_END`. Il test fissa anche il comportamento successivo:
`proof.txt` creato dopo la recovery deve uscire come `RAW_CREATE` e
`FILE_CREATED` sul path recuperato root B, non sul vecchio path root A ormai
stale. La documentazione chiarisce che, poiche' entrambe le root sono osservate,
puo' esistere anche un `DIR_MOVED` semantico da parent-level move pair; questo
non sostituisce la recovery del watch figlio, che resta necessaria per riparare
la watcher table.

Aggiornamento successivo: dopo il golden sintetico `test_overflow_core_jsonl.sh`
la matrice JSONL golden v0 e' stata chiusa per il perimetro corrente del backend
inotify di riferimento. `22-contratto-log.md` distingue ora tra famiglie
coperte, famiglie volutamente non-goal v0 e famiglie future da progettare.
`RAW_OVERFLOW` e `OVERFLOW` core sono entrambi coperti da test dedicati; la
recovery lost-scope completa e' coperta dallo scenario runtime root A -> root B.
Gli elementi rimasti fuori (`backend_observed` `IN_*`, audit read-only,
lifecycle/app, errori generici, trace/performance e security/policy) non sono
buchi del JSONL golden v0: richiedono prima un modello pubblico separato.

Aggiornamento successivo: la documentazione contributiva ora spiega anche la
struttura GitHub usata per pianificare Alfred. Sono stati creati la milestone
`Writer Runtime v0`, il Project `Alfred Roadmap`, la issue madre
`Writer Runtime v0: implementation plan` e tre Discussions iniziali dedicate a
code comuni vs code per sink, backpressure/failure policy e introduzione del
worker thread. `11-come-contribuire.md`, `27-guida-lettura-documentazione.md` e
`00-regole-operative.md` chiariscono la regola: GitHub Discussions conserva il
processo di ragionamento e le alternative, mentre gli `.md` del repository
conservano decisioni consolidate, contratti stabili e spiegazioni didattiche.

Aggiornamento successivo: `00-regole-operative.md` ora specifica anche come
scrivere le sezioni documentali nelle issue madri. I riferimenti agli MD non
devono essere liste di path: devono essere link GitHub cliccabili, possibilmente
con link ai paragrafi rilevanti e con una sintesi che permetta di capire il
contesto della issue senza leggere subito tutta la documentazione. La issue
resta il piano operativo; gli MD restano la fonte stabile del contratto.

Aggiornamento successivo: sono state create le label GitHub iniziali per
classificare roadmap, issue e PR. La tassonomia usa `area:*` per il modulo
toccato, `kind:*` per il tipo di lavoro, `priority:*` per l'urgenza e
`status:*` per lo stato operativo. `11-come-contribuire.md` spiega la tabella
completa per studenti e contributori; `00-regole-operative.md` fissa la regola
pratica: almeno una area e almeno un kind, con priority e status solo quando
servono davvero.

Aggiornamento successivo: la visione futura di Alfred include ora una sezione
su Deep Runtime Inspection in `24-roadmap-ai-agent-guardrail.md`. Il documento
chiarisce che Alfred non deve leggere continuamente registri, stack e heap come
un debugger permanente: i segnali profondi vanno attivati in modo mirato quando
sessione e rischio lo giustificano. La discussione progettuale e' stata salvata
in GitHub Discussion `#37`, mentre la roadmap stabile ribadisce che prima
servono Backend API v0 reale, capabilities runtime, subject/process context,
primitive process/memory leggere, session engine e risk engine.

Aggiornamento successivo: e' stato aggiunto
`36-use-cases-posizionamento-integrazioni.md`. Il capitolo chiarisce il
posizionamento corrente di Alfred come motore semantico filesystem Linux,
separa use case supportati, parziali e futuri, confronta Alfred con Watchman,
Fluent Bit, OpenTelemetry Collector, Wazuh, Falco, Tracee, Tetragon, Cilium e
osquery, e stabilisce che la prima integrazione pratica da rendere semplice e'
`Alfred JSONL -> Fluent Bit -> destinazioni esterne`.

Aggiornamento successivo: `36-use-cases-posizionamento-integrazioni.md` e
`26-stato-funzionalita.md` sono stati collegati esplicitamente. Il documento sui
use case contiene ora una matrice `famiglia funzionale -> use case`, esempi di
lettura e una scheda di brainstorming periodico. La regola di manutenzione e':
quando una nuova funzionalita' importante entra in `26-stato-funzionalita.md`,
bisogna controllare se abilita, rafforza o modifica un use case in `36`.

Aggiornamento successivo: la documentazione degli audit distingue ora maturita'
storica e freschezza della validazione. `audit/maturity-matrix.md`,
`audit/nightly-playbook.md` e `audit/README.md` spiegano che un refactor o una
modifica importante del codice collegato puo' rendere una funzionalita'
`stale` o `needs-revalidation` anche se gli audit precedenti erano positivi.
La maturita' non viene sempre azzerata, ma la confidence deve riflettere il
codice corrente.

Aggiornamento successivo: `README.md` in `docs/it` non e' piu' solo una lista
numerata. L'indice breve e' stato trasformato in una tabella ragionata con una
descrizione sintetica di ogni capitolo, chi dovrebbe leggerlo e quando. Gli
audit esplorativi sono stati messi in una sezione separata per distinguere la
documentazione stabile del progetto dal materiale operativo degli audit.

Aggiornamento successivo: su GitHub e' stata aggiunta la label `kind:audit` per
audit esplorativi, audit notturni e follow-up collegati. `00-regole-operative.md`,
`11-come-contribuire.md`, `audit/README.md` e `audit/nightly-playbook.md`
documentano ora che le issue madre degli audit usano `area:tests` +
`kind:audit`, mentre le issue figlie mantengono `kind:audit` e aggiungono
`kind:bug`, `kind:test` o `kind:debt` secondo il risultato.

Aggiornamento successivo: `33-writer-runtime-roadmap-v0.md` contiene ora la
mappa della pipeline strutturata corrente. Il documento distingue il percorso
sincrono oggi implementato (`record -> enqueue -> drain -> dispatcher ->
JSONL writer`) dal target della milestone, in cui il percorso caldo deve
terminare al solo enqueue bounded. `32-writer-api-v0.md` rimanda a questa mappa
per evitare ambiguita' fra API gia' presenti, bridge runtime transitori e
runtime writer asincrono futuro.

Aggiornamento successivo: `33-writer-runtime-roadmap-v0.md` spiega ora anche il
buffer circolare `alfred_record_queue_t`: campi (`items`, `capacity`, `head`,
`tail`, `count`), avanzamento con modulo, esempio con capacita' tre, funzioni
di gestione e motivo architetturale della coda bounded. Questa spiegazione
serve agli studenti e sara' riusata come nota architetturale nella PR della
milestone Writer Runtime v0.

Aggiornamento successivo: `33-writer-runtime-roadmap-v0.md` documenta anche le
funzioni candidate del prossimo micro-refactor:
`app_enqueue_output_record()` e `app_drain_output_pipeline()`. Il capitolo
spiega le sottofunzioni chiamate nei due percorsi, dalla clone owned al push in
coda, e dal drain bounded fino a dispatcher, JSONL writer, callback byte e
destroy del record owned.

Aggiornamento successivo: `00-regole-operative.md` e
`11-come-contribuire.md` chiariscono la regola per le issue madri: ogni issue
madre deve linkare vicino al goal la roadmap MD principale della milestone con
un blocco `Primary roadmap`. La lista piu' ampia dei documenti da leggere resta
utile, ma non sostituisce il link esplicito al documento operativo principale.

Aggiornamento successivo: `00-regole-operative.md` aggiunge una regola per i
commit che modificano call path rilevanti. Il body deve spiegare in inglese il
punto di ingresso, gli helper principali, le responsabilita' delle sottofunzioni
e gli eventuali effetti su ownership, I/O, hot path, API o comportamento
osservabile. I commit banali restano esclusi per non appesantire la history.

Aggiornamento successivo: `00-regole-operative.md` chiarisce che le issue madri
devono mantenere una tabella `Implementation traceability`. Ogni elemento della
checklist deve essere collegato a commit, PR o issue figlie rilevanti, cosi' si
puo' capire quali modifiche implementano o preparano ciascun punto del piano.

Aggiornamento successivo: la documentazione dei test runtime output chiarisce
che `test_output_pipeline_runtime.sh` copre anche il nuovo wrapper sincrono
`app_emit_output_record() -> app_enqueue_output_record() ->
app_drain_output_pipeline()`. Il caso `/dev/full` resta il contratto principale
per verificare che errori di enqueue, drain/write e flush finale non producano
un ledger JSONL incompleto con exit status di successo.

Aggiornamento successivo: `04-livello-applicazione.md` allinea la descrizione
didattica di `app_t` al codice corrente. `running` e' ora spiegata come
`volatile sig_atomic_t` usata per shutdown cooperativo da signal handler, mentre
`output_failed` e le risorse della pipeline strutturata sono elencate fra i
campi importanti del runtime applicativo.

Aggiornamento successivo: `make perf-runtime-output` introduce il primo
benchmark manuale del runtime output reale. A differenza di
`perf-record-sinks`, avvia Alfred, crea file reali sotto inotify e confronta
`output_enabled=false` con `output_enabled=true`, lasciando artifact separati
per ogni run. `10-debugging-test-e-strumenti.md`, `26-stato-funzionalita.md` e
`33-writer-runtime-roadmap-v0.md` spiegano scopo, CSV e limiti della misura.

Aggiornamento successivo: il benchmark runtime output disabilita LeakSanitizer
nel processo Alfred misurato, perche' LSAN puo' fallire in ambienti ristretti e
sporcare `process_status` anche quando `errors.log` e lo shutdown sono puliti.
Lo script aspetta inoltre almeno `files * 3` righe evento prima di fermare
Alfred, cosi' il confronto non interrompe il workload prima dei record
`FILE_CREATED`, `FILE_MODIFIED` e `FILE_READY` attesi.

Aggiornamento successivo: il runtime output separa meglio producer e consumer.
`app_emit_output_record()` ora accoda soltanto tramite
`app_enqueue_output_record()`, mentre `app_run()` chiama
`app_drain_output_pipeline()` dopo ogni poll backend. Il runtime resta
single-threaded: se una burst riempie la coda prima che il poll ritorni,
`app_enqueue_output_record()` esegue un drain di pressione e ritenta l'enqueue
una sola volta. Il punto che un futuro worker thread dovra' sostituire e' ora
esplicito nel loop applicativo e nella valvola di backpressure v0.

Aggiornamento successivo: `app_t` espone contatori locali
`output_stats` per osservare Writer Runtime v0 senza promuovere ancora metriche
pubbliche. A shutdown, con `output_enabled=true`, Alfred scrive in `events.log`
una riga `output runtime stats ...` con enqueue tentati/riusciti/falliti,
pressure drain, drain call, record drenati e massimo pending osservato. Il
benchmark `make perf-runtime-output` include questi campi nel CSV per capire
quando la coda bounded sta assorbendo una burst.

Aggiornamento successivo: `tests/backend/test_output_queue_pressure.sh` verifica
in modo mirato la valvola di pressione v0. Il test crea una burst abbastanza
grande da riempire la coda bounded, poi controlla che `pressure_drains>0`,
`max_pending` raggiunga la capacita' corrente, `enqueue_failures=0` e
`drained_records=enqueue_success`. `10-debugging-test-e-strumenti.md` e
`34-report-benchmark-prestazioni.md` spiegano i nuovi campi benchmark con
esempi didattici.

Aggiornamento successivo: il runtime output supporta anche
`output_format=counter` come formato benchmark/no-op. `counter` non e' un writer
utente: attraversa `record -> queue -> drain -> dispatcher -> counter sink`
senza formattazione JSONL, senza `output_log` e senza file I/O. `make
perf-runtime-output` produce ora tre righe per run: `compat-only`,
`counter-output` e `jsonl-output`. Il confronto `counter-output` vs
`compat-only` misura il costo della pipeline strutturata senza writer reale; il
confronto `jsonl-output` vs `counter-output` isola il costo aggiuntivo di JSONL,
buffering e scrittura file.

Aggiornamento successivo: `tests/backend/test_output_counter_runtime.sh`
promuove `output_format=counter` da sola baseline benchmark a contratto runtime
testato end-to-end. Il test avvia Alfred reale, genera eventi filesystem,
controlla che i log compatibili restino presenti, verifica la riga
`output runtime stats` e conferma che il file `output_log` non venga creato.
In questo modo il benchmark manuale misura le prestazioni, mentre il test
backend verifica la correttezza del percorso counter.

Aggiornamento successivo: `33-writer-runtime-roadmap-v0.md` registra prima e
poi chiude il debito di test non bloccante su
`test_output_pipeline_runtime.sh`. Il test non usa piu' `sleep` ampi per
sincronizzare le fasi principali: aspetta `application startup complete` e
record concreti come `FILE_CREATED`, `DIR_CREATED`, `WATCH_ADDED`,
`WATCH_REMOVED`, `WATCH_RESYNC_*` e `WATCH_LOST_QUEUED`. I record JSONL restano
verificati dopo lo shutdown perche' il writer e' buffered e il flush finale fa
parte del contratto testato.

Aggiornamento successivo: `00-regole-operative.md` rende esplicita la regola di
tracciabilita' GitHub per milestone strutturate. Ogni micro-step non banale di
una issue madre deve avere una issue figlia e una PR dedicata, con link
bidirezionali. Quando disponibile, la issue figlia va aggiunta anche come
sub-issue nativa GitHub; se gli strumenti automatici non lo permettono, il
collegamento deve essere registrato almeno nella issue madre e nella issue
figlia.

Aggiornamento successivo: audit finale di coerenza Writer Runtime v0. L'indice,
lo stato funzionale e la roadmap Writer Runtime distinguono ora in modo piu'
netto lo stato implementato v0 dal lavoro futuro: il runtime output e'
collegato come percorso opt-in single-threaded con queue bounded, drain
esplicito, dispatcher, JSONL/counter, statistiche runtime e valvola di
pressione; worker thread, code per sink, socket/binary writer e backpressure
pubblica restano fasi successive.

Aggiornamento successivo: la review della PR Writer Runtime v0 final audit ha
evidenziato una contraddizione residua in `22-contratto-log.md` e
`32-writer-api-v0.md`: alcune sezioni introduttive descrivevano ancora la
diagnostica JSONL collegata come sola diagnostica watch base, mentre la matrice
di chiusura indicava gia' resync e lost-scope come runtime-routed. Le sezioni
sono state riallineate: nel perimetro JSONL v0 rientrano raw normalizzati,
semantica core e diagnostica watch/resync/lost-scope; lifecycle, errori
runtime generici, trace/performance e security/policy restano futuri o non-goal
v0.

Aggiornamento successivo: la seconda review della stessa PR ha corretto il
diagramma del percorso diagnostico in `22-contratto-log.md`. Il blocco ora usa
solo funzioni reali (`alfred_record_build_watch_diagnostic()`,
`alfred_record_build_watch_diagnostic_with_os_error()` e
`alfred_record_build_stale_event_dropped()`) e spiega in prosa che il builder
diagnostico watch copre anche molti record `WATCH_RESYNC_*` e `WATCH_LOST_*`.

Aggiornamento successivo: lo staged adapter inotify per Backend API v0 collega
anche `poll()`. Il callback comune resta non bloccante e accetta solo
`timeout_ms == 0`; valori diversi sono rifiutati finche' la semantica comune dei
timeout non sara' definita. La poll della tabella ops delega alla
`inotify_backend_poll()` esistente, ma converte ogni `alfred_raw_event_t` con
`alfred_record_from_raw()` e consegna il record normalizzato al callback
`alfred_backend_emit_t` copiato da `init()`. Il test
`tests/backend/test_backend_inotify_ops.c` copre ora anche il requisito di
`start()`, l'emit obbligatorio per la poll comune e un evento reale `RAW_CREATE`
emesso attraverso la tabella ops.

Aggiornamento successivo: `app.c` usa la tabella statica
`inotify_backend_ops()` come composition root parziale per lifecycle e target
management. `app_init()` chiama ora `init`, `add_target` e `start` tramite
`alfred_backend_ops_t`; `app_shutdown()` chiama `stop` e `destroy` tramite la
stessa tabella. In quel momento il loop `app_run()` restava intenzionalmente sul
ponte raw diretto `inotify_backend_poll()` per continuare ad alimentare il core
semantico con `alfred_raw_event_t`; il passo successivo lo ha poi isolato in un
helper dedicato. La migrazione del loop principale alla `poll` Backend API v0
resta un micro-step separato perche' il path ops emette record normalizzati, non
raw event per `alfred_process()`. Prima di chiamare `init`, `app.c` valida la
tabella con `alfred_backend_ops_is_minimally_valid()` per fallire in modo
esplicito se il descriptor statico non rispetta il contratto minimo.

Aggiornamento successivo: il ponte runtime raw rimasto in `app_run()` e' stato
isolato in `app_poll_legacy_raw_backend_once()`. Il comportamento non cambia:
il helper chiama ancora `inotify_backend_poll()` con `handle_backend_event()` e
`app` come userdata, cosi' il core semantico continua a ricevere
`alfred_raw_event_t`. La differenza e' architetturale: `app_run()` non contiene
piu' direttamente la chiamata al backend inotify-specifico. Questo rende piu'
visibile l'unico punto legacy da sostituire quando il loop principale potra'
consumare il percorso `backend_ops->poll()` basato su record normalizzati.

Aggiornamento successivo: la issue GitHub `#67` ha chiarito che la migrazione
del main loop a `backend_ops->poll()` non e' un cambio meccanico. Il percorso
ops staged emette `alfred_record_t`, mentre il core semantico consuma ancora
`alfred_raw_event_t`. Backend API v0 viene quindi trattata come sottoinsieme
staged reale: lifecycle, target management, capabilities, emit boundary e poll
comune sono implementati e testati, ma il raw bridge isolato resta il percorso
runtime principale finche' non viene decisa e misurata la migrazione del core
input model.

Aggiornamento successivo: `30-backend-api-v0.md` contiene ora la sezione di
chiusura del milestone endpoint. Backend API v0 e' documentata come staged
subset: ops statiche, validator, capabilities, emit boundary, lifecycle,
target management, poll non bloccante e app lifecycle wiring sono implementati
e coperti per il sottoinsieme corrente. La migrazione del main loop, il core
input model basato su record e il benchmark hot-path restano debiti espliciti,
non ambiguita' nascoste.

Aggiornamento successivo: la issue #109 aggiunge il terzo scenario Lab
concreto, `rename / move / relocate`. Lo scenario documenta la correlazione
`MOVED_FROM` + `MOVED_TO` tramite cookie, la move table `moves[1024]`, le
funzioni `alfred_move_insert()`, `alfred_move_take()` e `classify_move()`, e la
regola che produce un solo evento semantico tra rename, move e relocate. Il
lavoro resta documentation-only: nessun `trace.jsonl`, parser, UI, cambio
record o cambio al percorso caldo.
