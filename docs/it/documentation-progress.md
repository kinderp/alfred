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
`core/src/alfred_record_diagnostic.c` e il formatter testuale in
`core/src/alfred_record_text.c`, ma mancano ancora writer JSONL e uso runtime
nel backend inotify.

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
`29-event-model-v0.md` e `32-writer-api-v0.md` documentano in dettaglio le
quattro strategie discusse: deep copy per record, storage inline fisso,
pool/arena per batch e string/path table.
La prima `alfred_record_queue_t` e' stata aggiunta come micro-step successivo:
riceve record borrowed, conserva record owned in una coda bounded
single-threaded e trasferisce ownership al chiamante durante `pop()`. Anche
questa API non e' ancora collegata al runtime; serve a fissare FIFO, overflow,
cleanup e lifetime prima di introdurre dispatcher, backpressure e benchmark.

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
