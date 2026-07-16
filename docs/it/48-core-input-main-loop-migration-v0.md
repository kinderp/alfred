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
