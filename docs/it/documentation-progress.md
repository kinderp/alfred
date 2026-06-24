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
del futuro output runtime. I default sono `output_enabled=false`,
`output_format=jsonl` e `output_buffer_size=65536`; il parser accetta anche
`text`, rifiuta formati non implementati e rifiuta buffer sotto `4096` o
malformati. La documentazione chiarisce che `enabled=false` mantiene il path
compatibile `raw.log`/`events.log`/`errors.log`, mentre `enabled=true` e' solo
un'intenzione validata per il futuro percorso `record -> queue -> dispatcher ->
writer`. `test_output_config.sh` copre default, valori validi e invalidi.

Aggiornamento successivo: `alfred_record_output_pipeline_t` introduce la prima
pipeline composta single-writer. La pipeline e' ancora fuori da `app_run()` e
non crea thread: quando e' abilitata, fa `enqueue` di record owned nella queue,
`drain_once` verso dispatcher e JSONL writer, e `flush` separato verso callback
bytes. Quando e' disabilitata, tutti i passaggi sono no-op riusciti. Il test
`test_record_output_pipeline.sh` copre disabled mode, pipeline JSONL abilitata,
batch drain, queue full e flush failure con bytes preservati.

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
