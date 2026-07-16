# Core Input Model / Main Loop Migration v0

Questo documento definisce la milestone successiva alla chiusura MVP del
backend inotify di riferimento.

Il problema non e' aggiungere subito un nuovo backend. Il problema e' decidere
quale modello di input deve consumare il core semantico di Alfred prima di
cambiare il main loop e prima di introdurre fanotify, audit, eBPF o altri
sensori.

## Perche' ora

La milestone inotify MVP ha chiuso il backend Linux come riferimento per il
sottoinsieme staged di Backend API v0. Questo significa che Alfred ha gia':

- Backend API v0 documentata;
- `inotify_backend_ops()` come tabella ops statica;
- capability conservative per inotify;
- lifecycle e target management via ops;
- `backend_ops->poll(timeout_ms = 0)` staged;
- record strutturati comuni;
- writer/sink/JSONL fuori dal contratto backend primario.

Resta pero' un debito importante: il main loop runtime usa ancora un ponte raw
intenzionale verso il core.

```text
app_run()
-> app_poll_legacy_raw_backend_once()
-> inotify_backend_poll()
-> handle_backend_event(alfred_raw_event_t)
-> alfred_process(core, raw)
-> alfred_event_t
-> alfred_record_t
-> sink / writer
```

In parallelo esiste il percorso staged della Backend API:

```text
backend_ops->poll(timeout_ms = 0)
-> inotify poll esistente
-> alfred_raw_event_t
-> alfred_record_from_raw()
-> emit_record(alfred_record_t borrowed)
```

Questi due percorsi convivono volutamente. Non e' un errore nascosto: e' il
punto in cui Alfred deve decidere se il core resta raw-first, se diventa
record-first o se serve un ponte misurato tra i due modelli.

## Obiettivo

L'obiettivo della milestone e' decidere e preparare il modello di input del
core e il percorso di migrazione del main loop.

La domanda principale e':

```text
Il core semantico deve consumare alfred_raw_event_t,
alfred_record_t,
oppure un ponte esplicito e misurato fra i due?
```

La risposta deve includere:

- impatto sul percorso caldo;
- impatto su ownership e lifetime dei dati;
- impatto su test core, backend, JSONL e golden;
- impatto su Backend API v0 e Event Model v0;
- benchmark necessario prima di cambiare runtime;
- criterio di rollback o di accettazione.

## Mappa corrente del runtime

Prima di scegliere una delle opzioni bisogna distinguere tre percorsi che oggi
sono collegati, ma non identici:

1. lifecycle e target management passano gia' dalla Backend API v0;
2. il main loop runtime usa ancora il ponte raw verso il core;
3. l'output strutturato sta a valle e riceve record borrowed da callback.

### Composition root e lifecycle

Durante `app_init()` l'applicazione sceglie il backend inotify statico e usa la
tabella ops per inizializzazione, target e start:

```text
app_init()
-> app->backend_ops = inotify_backend_ops()
-> alfred_backend_ops_is_minimally_valid(app->backend_ops)
-> app->backend_ops->init(...)
-> app->backend_ops->add_target(...)
-> app->backend_ops->start(...)
```

Le funzioni coinvolte hanno responsabilita' diverse:

| Funzione | Responsabilita' |
| --- | --- |
| `inotify_backend_ops()` | restituisce la tabella statica `alfred_backend_ops_t` del backend inotify. |
| `alfred_backend_ops_is_minimally_valid()` | valida descriptor, capability e callback obbligatorie; non sta nel percorso caldo. |
| `inotify_backend_ops_init()` | costruisce il runtime inotify staged, copia `emit`/`userdata` dal chiamante e chiama `inotify_backend_init()`. |
| `inotify_backend_ops_add_target()` | valida un target filesystem-path v0 e delega a `inotify_backend_add_startup_watch()`. |
| `inotify_backend_ops_start()` | marca il runtime staged come avviato; non possiede nuove risorse. |

Questo significa che Alfred non e' piu' completamente inotify-specific nel
lifecycle. Pero' il ciclo eventi principale non usa ancora `backend_ops->poll()`.

### Percorso runtime effettivo: raw bridge

Il percorso realmente usato da `app_run()` per alimentare il core e':

```text
app_run()
-> app_build_inotify_backend_context()
-> app_poll_legacy_raw_backend_once()
-> inotify_backend_poll()
-> backend_poll()
-> adapter inotify -> alfred_raw_event_t
-> handle_backend_event(alfred_raw_event_t)
-> alfred_process(app->core, raw)
-> core_logger_on_event()
-> alfred_record_from_event()
-> app_emit_output_record_callback()
-> app_emit_output_record()
-> app_enqueue_output_record()
```

Le responsabilita' principali sono:

| Funzione | Responsabilita' |
| --- | --- |
| `app_run()` | possiede il loop, decide quando drenare l'output e quando fermarsi. |
| `app_build_inotify_backend_context()` | costruisce un contesto inotify stretto con puntatori borrowed a runtime, config, logger e callback record. |
| `app_poll_legacy_raw_backend_once()` | isola l'unica chiamata applicativa diretta rimasta a `inotify_backend_poll()`. |
| `inotify_backend_poll()` | legge un batch non bloccante dal fd inotify e consegna raw event tramite callback. |
| `backend_poll()` | implementa la lettura reale, diagnostica, conversione raw e manutenzione watch/recovery. |
| `handle_backend_event()` | adatta alcuni raw a record per output/log compatibile, poi passa il raw originale al core. |
| `alfred_process()` | e' il core semantico corrente: consuma `alfred_raw_event_t` e produce zero o un evento semantico. |

Questo e' il percorso piu' importante per la milestone. Finche'
`alfred_process()` consuma `alfred_raw_event_t`, sostituire meccanicamente
`app_poll_legacy_raw_backend_once()` con `backend_ops->poll()` non e' corretto:
la poll ops non consegna raw event al core, consegna record normalizzati alla
callback Backend API.

### Percorso staged: `backend_ops->poll(timeout_ms = 0)`

Il percorso staged della Backend API v0 e':

```text
backend_ops->poll(timeout_ms = 0)
-> inotify_backend_ops_poll()
-> inotify_backend_poll()
-> inotify_backend_ops_emit_raw_record()
-> alfred_record_from_raw()
-> runtime->context.emit_record(alfred_record_t borrowed)
```

Le responsabilita' principali sono:

| Funzione | Responsabilita' |
| --- | --- |
| `inotify_backend_ops_poll()` | valida runtime staged, `started`, `emit_record` e `timeout_ms == 0`, poi delega al poll inotify esistente. |
| `inotify_backend_ops_emit_raw_record()` | riceve il raw event dal poll inotify, lo converte in record e lo emette alla callback Backend API. |
| `alfred_record_from_raw()` | produce un `alfred_record_t` `NORMALIZED_RAW` borrowed, copiando solo campi scalari e prendendo in prestito le stringhe raw. |
| `runtime->context.emit_record()` | callback applicativa generica che riceve record borrowed; il ricevente deve clonare se vuole conservarli. |

Questo percorso serve a rendere reale la Backend API v0, ma non sostituisce
ancora il core input path. E' una prova compilata e testata del confine
backend->record, non il main loop definitivo.

### Percorso output: downstream, non input del core

L'output strutturato e' a valle del problema core input model.

```text
app_emit_output_record_callback()
-> app_emit_output_record()
-> app_enqueue_output_record()
-> alfred_record_output_pipeline_enqueue()
-> alfred_record_queue_push()
-> alfred_record_clone_owned()

app_run()
-> app_drain_output_pipeline()
-> alfred_record_runtime_drain_once()
-> alfred_record_queue_pop()
-> alfred_record_dispatcher_dispatch_one()
-> sink concreto
```

Questo percorso non deve guidare da solo la scelta del core input model. E'
importante per JSONL, ledger, counter sink e writer futuri, ma il core deve
prima ricevere un input semantico corretto e veloce. I writer restano fuori dal
percorso caldo target.

### Perche' i due poll convivono

La convivenza attuale e' intenzionale:

- `app_run()` deve ancora alimentare il core con `alfred_raw_event_t`;
- `backend_ops->poll()` e' gia' il contratto backend staged, ma produce
  `alfred_record_t`;
- l'output pipeline sa gia' ricevere record, ma non decide la semantica core;
- cambiare il main loop senza decidere il core input model rischierebbe una
  migrazione solo apparente.

La milestone deve quindi scegliere fra tre direzioni reali:

1. mantenere il core raw-first e trattare i record come output/envelope;
2. rendere il core record-first con test e benchmark adeguati;
3. introdurre un bridge misurato con criteri di uscita espliciti.

## Non obiettivi

Questa milestone non implementa:

- fanotify;
- audit;
- eBPF;
- backend Windows o macOS;
- registry multi-backend completo;
- Agent Guard;
- policy engine;
- enforcement;
- process tree;
- rete;
- worker thread reale;
- code per sink separate;
- nuovo formato di writer.

Questi temi restano dipendenze future. Possono influenzare la decisione
architetturale, ma non devono allargare il passo corrente.

## Opzioni sul tavolo

### Opzione A: core raw-first

Il core continua a consumare `alfred_raw_event_t`.

```text
backend
-> alfred_raw_event_t
-> alfred_process()
-> alfred_event_t
-> alfred_record_t
-> output pipeline
```

Vantaggi:

- e' il percorso corrente;
- riduce il rischio di regressioni;
- mantiene corto il percorso caldo gia' testato;
- evita copie o conversioni aggiuntive prima della semantica;
- protegge i test core esistenti.

Svantaggi:

- la Backend API staged produce record, ma il core usa raw;
- i backend futuri devono adattarsi al modello raw anche se non sono inotify;
- il core resta piu' vicino ai dettagli del backend corrente;
- il confine comune di Alfred resta diviso tra raw input e record output.

Quando avrebbe senso:

- se i benchmark mostrano che la conversione record-first costa troppo;
- se i backend futuri possono produrre un raw comune minimale senza ambiguita';
- se vogliamo rimandare la migrazione finche' esistono almeno due backend reali.

### Opzione B: core record-first

Il core consuma direttamente `alfred_record_t`.

```text
backend
-> alfred_record_t borrowed
-> core semantic stage
-> alfred_record_t semantic/diagnostic
-> output pipeline
```

Vantaggi:

- unifica il contratto interno attorno al record strutturato;
- rende piu' naturale l'arrivo di backend diversi;
- permette a raw, semantica e diagnostica di attraversare lo stesso envelope;
- avvicina Event Model v0 al runtime reale.

Svantaggi:

- rischia di portare nel percorso caldo campi non necessari;
- richiede contratti forti su ownership, borrowed/owned e lifetime;
- puo' costringere il core a interpretare record troppo generici;
- puo' rendere piu' complessa la semantica filesystem gia' stabile;
- richiede test di migrazione molto accurati.

Quando avrebbe senso:

- se il record puo' rappresentare l'input minimo senza copie inutili;
- se il costo di conversione e dispatch resta misurabilmente accettabile;
- se il core puo' distinguere chiaramente record raw, semantici e diagnostici;
- se la migrazione semplifica davvero backend futuri e non solo la teoria.

### Opzione C: ponte misurato record/raw

Alfred mantiene un bridge esplicito fra record e raw mentre misura il costo e
chiarisce i contratti.

Esempio:

```text
backend_ops->poll()
-> alfred_record_t raw normalized
-> bridge record -> core input
-> alfred_process()
-> semantic record
```

Oppure, nella direzione opposta:

```text
backend legacy poll
-> alfred_raw_event_t
-> core
-> record
-> output pipeline
```

con un bridge documentato che dichiara cosa e' temporaneo e cosa e' stabile.

Vantaggi:

- permette una migrazione graduale;
- rende esplicito il debito invece di nasconderlo;
- consente benchmark comparativi;
- riduce il rischio di cambiare troppo in un'unica PR.

Svantaggi:

- introduce duplicazione temporanea;
- puo' aumentare complessita' se non ha una data di uscita;
- richiede test che proteggano entrambe le direzioni;
- puo' nascondere ambiguita' se non viene documentato bene.

Quando avrebbe senso:

- se vogliamo misurare prima di decidere;
- se il record-first e' promettente ma ancora troppo rischioso;
- se serve mantenere compatibilita' con test e semantica core esistenti.

### Opzione D: dual path temporaneo dichiarato

Alfred continua temporaneamente con due percorsi dichiarati:

```text
runtime semantic core:
  alfred_raw_event_t -> alfred_process()

staged Backend API/output path:
  backend_ops->poll() -> alfred_record_t
```

La differenza rispetto a lasciare le cose come sono e' che il dual path diventa
un contratto temporaneo con criteri di uscita.

Vantaggi:

- rischio minimo sul comportamento corrente;
- utile se la milestone deve solo stabilire criteri e non implementare subito;
- lascia tempo per benchmark e test mirati.

Svantaggi:

- non chiude il debito;
- puo' confondere contributori e studenti;
- i backend futuri restano bloccati o costretti a scegliere un lato del ponte.

Quando avrebbe senso:

- se nessuna opzione e' ancora sufficientemente provata;
- se serve prima rafforzare benchmark, test o documentazione;
- se vogliamo una milestone puramente decisionale prima della migrazione.

## Regole per il percorso caldo

Questa milestone tocca una zona delicata: il percorso caldo.

Regola guida:

```text
Non cambiare il percorso caldo senza contratto e numeri.
```

Per Alfred il percorso caldo target resta:

```text
evento OS
-> collector/backend
-> normalizzazione minima
-> record strutturato o input core minimo
-> enqueue / consegna controllata
```

Non devono finire nel percorso caldo:

- serializzazione JSONL;
- writer testuali;
- flush su file;
- socket I/O;
- report;
- dashboard;
- Lab;
- policy pesante;
- allocazioni evitabili;
- copie profonde non necessarie;
- lock pesanti.

Se una opzione introduce copie, conversioni, dispatch indiretto, queueing,
allocazioni o campi piu' grandi nel percorso caldo, la PR deve indicare quale
benchmark lo misura e quale baseline usa.

## Gate benchmark

Prima di migrare il main loop bisogna avere almeno un confronto esplicito tra:

- percorso corrente raw-first;
- eventuale percorso record-first;
- eventuale bridge record/raw;
- output no-op o counter;
- output JSONL solo se il writer resta fuori dal confronto caldo.

Metriche candidate:

- eventi al secondo;
- record processati;
- conversioni per evento;
- allocazioni o copie per evento, quando misurabili;
- tempo totale del bench;
- `max_pending` e statistiche queue se la coda entra nel percorso;
- differenza percentuale rispetto alla baseline.

La prima baseline deve essere povera di I/O. Se JSONL e disco entrano nel
benchmark, il risultato misura anche il writer e non solo il core input model.

## Piano test

I test devono proteggere livelli diversi:

- test core: semantica filesystem da raw a eventi semantici;
- test backend: poll, target, capability, diagnostica e record borrowed;
- test formatter/JSONL: schema pubblico e campi esposti;
- test golden: scenari pubblici rappresentativi;
- test focused nuovi: eventuale bridge, conversione record/raw o input
  record-first.

Un cambio al core input model non deve essere coperto solo da golden JSONL.
JSONL e' un output pubblico; non e' il contratto interno primario del core.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- il percorso corrente e' mappato chiaramente;
- le opzioni sono confrontate con pro, contro e rischi;
- il benchmark gate e' scritto;
- il piano test e' scritto;
- la decisione scelta e' documentata nella issue madre e negli MD;
- eventuali PR runtime sono divise in micro-step;
- i debiti rimandati sono espliciti.

Se la milestone decide di non migrare subito, deve comunque spiegare quale
segnale fara' riaprire la migrazione: nuovo backend, benchmark, test mancanti o
semplificazione architetturale.

## Collegamenti

- GitHub Milestone: [Core Input Model / Main Loop Migration v0](https://github.com/kinderp/alfred/milestone/11)
- Issue madre: [#187](https://github.com/kinderp/alfred/issues/187)
- Setup roadmap: [#188](https://github.com/kinderp/alfred/issues/188)
- Inotify reference backend: [31-milestone-inotify-reference-backend.md](31-milestone-inotify-reference-backend.md)
- Backend API v0: [30-backend-api-v0.md](30-backend-api-v0.md)
- Event Model v0: [29-event-model-v0.md](29-event-model-v0.md)
- Writer Runtime Roadmap v0: [33-writer-runtime-roadmap-v0.md](33-writer-runtime-roadmap-v0.md)
- Performance report: [34-report-benchmark-prestazioni.md](34-report-benchmark-prestazioni.md)
