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
-> alfred_record_output_pipeline_drain_once()
-> alfred_record_runtime_drain_once()
-> alfred_record_queue_pop()
-> alfred_record_dispatcher_dispatch_one()
-> sink concreto
```

`alfred_record_output_pipeline_drain_once()` e' importante perche' e' il
wrapper della pipeline configurata: `app_drain_output_pipeline()` chiama quello,
non direttamente il runtime drain helper. Il wrapper conserva il confine fra
applicazione e runtime output, poi delega a `alfred_record_runtime_drain_once()`
per consumare la coda attraverso dispatcher e sink.

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

## Glossario decisionale

Questa milestone usa alcuni termini architetturali. Vanno letti in modo
concreto, non come etichette astratte.

| Termine | Significato pratico in Alfred | Esempio corrente |
| --- | --- | --- |
| `raw-first` | Il core semantico riceve prima un fatto grezzo normalizzato minimale, cioe' `alfred_raw_event_t`. Il record comune viene prodotto dopo o in parallelo per output e diagnostica. | `handle_backend_event()` chiama `alfred_process(app->core, raw)`. |
| `record-first` | Il core semantico riceve direttamente `alfred_record_t`. Il record diventa l'envelope comune anche per l'input del core, non solo per output/JSONL. | Una futura versione potrebbe far consumare record `NORMALIZED_RAW` al core. |
| `bridge misurato` | Un adattatore temporaneo e dichiarato converte fra record e raw, o fra raw e record, mentre si misura costo e correttezza. Non e' una soluzione permanente senza criteri di uscita. | `backend_ops->poll()` emette record, ma il core oggi richiede raw: un bridge potrebbe adattare uno dei due lati. |
| `dual path` | Due percorsi convivono temporaneamente: uno per il core, uno per il record/output. E' accettabile solo se documentato e se ha una ragione precisa. | Oggi `app_run()` usa il raw bridge, mentre `backend_ops->poll()` esiste come poll staged record-emitting. |
| percorso caldo | Parte del codice eseguita per ogni evento osservato, dove copie, allocazioni, lock, formattazione o I/O possono pesare molto. | backend poll -> raw/core input -> eventuale enqueue. |
| benchmark gate | Condizione numerica minima prima di accettare una modifica al percorso caldo. Serve a non scegliere a sensazione. | Confrontare raw-first baseline e bridge/record-first con output counter/no-op. |
| test/contract gate | Insieme minimo di test e documentazione che deve esistere prima di cambiare un contratto runtime. | Test focused per conversione, test core per semantica, golden JSONL solo per output pubblico. |

In parole semplici: `raw-first` privilegia stabilita' e velocita' del core
attuale; `record-first` privilegia un modello interno piu' uniforme; `bridge
misurato` prova a comprare tempo e dati prima della scelta definitiva.

## Criteri di confronto

Le opzioni non vanno confrontate solo in base all'eleganza architetturale. Per
Alfred contano almeno questi criteri:

| Criterio | Domanda da fare |
| --- | --- |
| Correttezza semantica | Il core continua a produrre gli stessi eventi semantici per create, delete, modify, ready, rename, move, relocate e overflow? |
| Costo sul percorso caldo | La scelta aggiunge copie, allocazioni, validazioni pesanti, dispatch indiretti o queueing prima della semantica? |
| Ownership e lifetime | Le stringhe path restano borrowed? Serve clone owned? Chi distrugge cosa? |
| Backend futuri | Fanotify, audit o eBPF possono usare il modello senza fingere di essere inotify? |
| Affidabilita' e prove security | Overflow, diagnostica backend, provenance e segnali di errore restano visibili e non vengono persi nella conversione? |
| Testabilita' | Possiamo scrivere test focused semplici per dimostrare il contratto? |
| Compatibilita' | I log compatibili, JSONL e test esistenti restano stabili o devono essere migrati? |
| Semplicita' | La scelta riduce complessita' reale o introduce un livello in piu' solo per simmetria? |
| Uscita dal debito | La scelta chiude il raw bridge o lo rende almeno misurabile e con una data di uscita? |

Questi criteri evitano due errori:

- restare raw-first per abitudine anche quando i backend futuri ne soffrono;
- migrare record-first per eleganza anche se il percorso caldo peggiora.

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

## Matrice di confronto preliminare

La tabella seguente non e' ancora la decisione finale. Serve a rendere
espliciti i trade-off prima di scrivere benchmark o cambiare runtime.

| Opzione | Costo hot path atteso | Ownership/lifetime | Impatto backend futuri | Affidabilita' e prove | Impatto test | Rischio principale | Valutazione preliminare |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A: core raw-first | Basso, perche' resta il percorso attuale. | Semplice: il raw resta borrowed e valido durante `alfred_process()`. | Medio: backend futuri devono produrre o adattarsi a un raw comune. | Buona sul comportamento corrente, ma le prove restano divise fra raw/core/record output. | Basso: conserva test core esistenti. | Congelare il core attorno a una forma troppo vicina a inotify. | Buona baseline; non chiude il debito architetturale. |
| B: core record-first | Incerto: il record e' piu' grande e piu' generale; va misurato. | Piu' delicato: bisogna decidere quali campi sono borrowed, quali owned e quando clonare. | Alto potenziale: un envelope comune aiuta backend diversi. | Alto potenziale se il record conserva provenance/diagnostica; alto rischio se la conversione perde dettagli raw. | Alto impatto: servono test core nuovi e migrazione semantica accurata. | Rendere il core piu' generico ma piu' lento o meno chiaro. | Promettente, ma non va fatto senza benchmark e test focused. |
| C: bridge misurato | Medio e misurabile: aggiunge conversione controllata. | Dipende dalla direzione del bridge; deve documentare borrowed/owned a ogni confine. | Buono: permette di provare record-first senza forzarlo subito. | Buona se i test dimostrano equivalenza fra raw, record, overflow e diagnostica; pericolosa se il bridge filtra campi. | Medio: servono test per conversione e equivalenza. | Il bridge diventa permanente e complica il codice. | Migliore passo esplorativo se vogliamo dati prima della scelta. |
| D: dual path dichiarato | Basso nell'immediato, perche' lascia il runtime com'e'. | Semplice nel breve, ma doppio contratto da spiegare. | Debole: rimanda il problema per i backend futuri. | Buona nel breve solo se entrambi i percorsi emettono gli stessi segnali osservabili; fragile se divergono. | Basso nel breve; alto se dura troppo. | Normalizzare il debito e confondere contributori. | Accettabile solo come stato temporaneo con uscita esplicita. |

## Raccomandazione preliminare

La raccomandazione preliminare e':

```text
Non migrare subito il core a record-first.
Usare il percorso raw-first come baseline misurata.
Preparare un bridge misurato solo se serve a confrontare costi e contratti.
```

Il motivo e' pragmatico:

- il core filesystem corrente funziona e ha test;
- il percorso raw-first e' il comportamento runtime reale;
- `alfred_record_t` e' gia' essenziale per output, JSONL e ledger, ma questo
  non dimostra automaticamente che debba essere anche l'input del core;
- un record-first non misurato potrebbe introdurre copie, campi inutili o
  ownership ambigua proprio nel percorso caldo;
- un bridge misurato permette di raccogliere numeri e test prima di cambiare
  responsabilita' del core.

Questa raccomandazione non chiude la scelta finale. Dice solo quale ordine e'
piu' sicuro:

1. mantenere il comportamento corrente come baseline;
2. scrivere benchmark e test focused;
3. provare eventuale bridge o record-first in modo reversibile;
4. decidere solo dopo aver confrontato numeri, chiarezza e test.

La frase guida e':

```text
Record-first e' una direzione possibile, non una scorciatoia.
```

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

Un benchmark gate e' la condizione minima di misura prima di accettare una
modifica nel percorso caldo. Non e' un benchmark generico e non serve a dire
"Alfred e' veloce" in assoluto. Serve a rispondere a una domanda piu' stretta:

```text
Questo cambio al modello di input del core costa abbastanza poco,
preserva la semantica e conserva le prove necessarie?
```

Per questa milestone il gate deve confrontare il percorso runtime corrente con
ogni candidato che introduce record, bridge, conversioni, copie, code o
dispatch aggiuntivi prima della semantica core.

### Baseline raw-first

La prima misura deve rappresentare il comportamento corrente, senza aggiungere
JSONL, writer testuali o I/O non necessario:

```text
inotify/backend oppure sorgente raw sintetica
-> alfred_raw_event_t
-> alfred_process()
-> callback evento semantico
-> output counter/no-op opzionale
```

Questa baseline misura il costo del core semantico con input raw. E' il punto
di confronto obbligatorio per ogni proposta successiva. Se una PR propone un
bridge o un core record-first ma non riporta questa baseline, non sta
dimostrando il costo reale della migrazione.

La baseline deve essere povera di I/O. Se JSONL, file flush, socket o log
testuale entrano nel benchmark principale, il risultato misura anche il writer
e non piu' solo il core input model.

### Candidato bridge misurato

Un bridge misurato deve rendere esplicito il costo della conversione fra record
e raw, o fra raw e record:

```text
alfred_record_t NORMALIZED_RAW
-> adapter record/raw oppure bridge raw/record
-> alfred_process()
-> callback evento semantico
-> output counter/no-op opzionale
```

Questa misura serve a capire se il bridge e' un debito accettabile mentre si
decide il modello finale. Deve rispondere almeno a queste domande:

- quante conversioni avvengono per evento;
- quali campi vengono copiati;
- quali stringhe restano borrowed;
- quali prove raw, diagnostiche, overflow o errori vengono preservati;
- quanto overhead introduce rispetto alla baseline raw-first.

Un bridge non misurato non deve diventare percorso runtime stabile.

### Candidato record-first

Un vero candidato record-first esiste solo quando il core puo' consumare record
come input primario:

```text
alfred_record_t NORMALIZED_RAW
-> futuro input record del core
-> evento semantico / record semantico
-> output counter/no-op opzionale
```

Questo benchmark non va confuso con l'output pipeline attuale. Oggi Alfred sa
gia' produrre e serializzare record a valle; questo non dimostra che il core
debba consumare record a monte. Il candidato record-first deve quindi misurare
il nuovo input del core, non il writer JSONL.

### Modalita' di output del benchmark

Le modalita' di output vanno ordinate cosi':

| Modalita' | Cosa misura | Quando usarla |
| --- | --- | --- |
| counter/no-op | costo del percorso core senza formattazione o I/O pesante | prima misura obbligatoria |
| JSONL buffered | costo del formato pubblico e della pipeline output | seconda misura, dopo il core |
| testo compatibile | costo del writer/log compatibile | utile per compatibilita', non come prova primaria del core |

La regola pratica e':

```text
counter/no-op dimostra il costo del core input path;
JSONL dimostra il costo dell'output pubblico;
il testo compatibile dimostra compatibilita', non performance core.
```

### Metriche minime

Il primo gate non deve essere sofisticato come una suite statistica completa,
ma deve raccogliere abbastanza dati da evitare decisioni a sensazione.

| Metrica | Significato | Interpretazione |
| --- | --- | --- |
| `events_per_second` | quanti eventi il percorso riesce a processare al secondo | misura throughput generale |
| `elapsed_ns` o `elapsed_ms` | tempo totale del run | serve per confrontare run identici |
| `events_processed` / `records_processed` | quanti input sono entrati nel percorso misurato | evita benchmark vuoti o parziali |
| conversioni per evento | quante trasformazioni raw/record avvengono | un bridge dovrebbe renderle visibili |
| copie o byte copiati, se disponibili | quanto dato viene duplicato | utile per stimare costo hot-path e cache pressure |
| allocazioni per evento, se disponibili | quante allocazioni dinamiche entrano nel percorso | una crescita qui e' sospetta nel percorso caldo |
| `max_pending`, se entra una coda | profondita' massima della queue | misura pressione e rischio backlog |
| errori, drop, conversion failures | perdita o fallimento nel percorso | devono essere zero per scenari normali |
| equivalenza semantica | eventi semantici uguali alla baseline | requisito di correttezza, non metrica opzionale |
| preservation di overflow/diagnostica/prove | segnali non semantici conservati | requisito di affidabilita' e sicurezza |
| overhead percentuale vs baseline | costo relativo del candidato | dato principale per decidere accettazione o rifiuto |

Le metriche su allocazioni e byte copiati possono arrivare in una fase
successiva se richiedono strumenti nuovi. La PR che cambia il percorso caldo
deve pero' dichiarare esplicitamente se non le misura.

### Regola di accettazione v0

Per v0 non fissiamo ancora una soglia numerica permanente. Sarebbe arbitraria:
manca un benchmark mirato ripetuto sul core input model. La prima PR che
introduce il benchmark dovra' proporre una soglia basata sui numeri raccolti.

Fino ad allora valgono queste regole minime:

- baseline raw-first e candidato devono essere misurati nello stesso eseguibile
  o con lo stesso stile di run;
- il benchmark primario deve usare output counter/no-op;
- JSONL non puo' essere usato da solo per dimostrare il costo del core input
  path;
- il candidato deve produrre gli stessi eventi semantici della baseline negli
  scenari rappresentativi;
- il candidato deve preservare overflow, diagnostica ed evidence oppure
  dichiarare esplicitamente cosa non supporta;
- gli errori e i drop devono essere zero negli scenari normali;
- l'overhead percentuale deve essere riportato, anche se la soglia finale non e'
  ancora decisa;
- un singolo run puo' bastare per orientare una PR documentale o esplorativa,
  ma non per accettare una migrazione runtime definitiva.

Quando il benchmark sara' implementato, la regola di accettazione dovra'
evolvere verso run ripetuti e confrontabili. Per una migrazione del main loop
serviranno almeno piu' ripetizioni nello stesso ambiente o una motivazione
esplicita del perche' non siano disponibili.

### Rapporto con i benchmark esistenti

I benchmark gia' introdotti per Writer Runtime e output pipeline restano utili,
ma rispondono a domande diverse.

| Benchmark esistente | Cosa puo' provare | Cosa non puo' provare |
| --- | --- | --- |
| `core-input-raw-first` | costo baseline di `alfred_raw_event_t -> alfred_process() -> callback counter/no-op` | costo di JSONL, output runtime, backend reale o record-first non ancora implementato |
| `raw-to-record-adapter` | costo isolato di `alfred_record_from_raw()` sullo stesso workload raw sintetico | costo di un bridge completo record->core o di un core record-first |
| `raw-to-record-plus-core` | costo di produrre un record normalizzato con `alfred_record_from_raw()` e poi alimentare il core raw-first con `alfred_process()` sullo stesso raw sintetico | costo di un bridge completo record->core, output runtime o core record-first |
| `queue-dispatcher-jsonl` | costo di queue, dispatcher e JSONL su record gia' disponibili | costo di cambiare input del core |
| `output-pipeline-jsonl` | costo della pipeline output integrata con JSONL | equivalenza fra raw-first e record-first |
| counter/output runtime | costo minimo della pipeline output senza formattazione pesante | costo di un bridge record/raw a monte del core |

Questi benchmark possono segnalare se il writer e' un collo di bottiglia, ma
non bastano per decidere la migrazione di `app_poll_legacy_raw_backend_once()`.
Per quella decisione serve un benchmark mirato al confine:

```text
raw input / record input / bridge
-> core semantico
-> output no-op o counter
```

### Limiti dichiarati del primo gate

Il primo benchmark gate non copre ancora tutto:

- non misura p95/p99 latency;
- non misura I/O reale su disco o socket;
- non misura worker thread, lock o code per sink;
- non misura fanotify, audit, eBPF o backend futuri;
- non misura ancora pressione memoria con strumenti dedicati;
- non sostituisce i test di equivalenza semantica.

Questi limiti sono accettabili per la decisione iniziale, purche' siano
dichiarati nella PR che usa il benchmark.

### Checklist prima di migrare il main loop

Prima di sostituire il ponte:

```text
app_poll_legacy_raw_backend_once()
```

con un percorso basato su `backend_ops->poll()` o su record input, devono essere
veri tutti questi punti:

1. esiste un benchmark raw-first baseline;
2. esiste una misura del candidato bridge o record-first;
3. il confronto usa output counter/no-op come misura primaria;
4. i test dimostrano equivalenza semantica sugli scenari rappresentativi;
5. overflow, diagnostica ed evidence sono preservati o il debito e' dichiarato;
6. ownership e lifetime delle stringhe sono documentati;
7. la PR riporta overhead percentuale rispetto alla baseline;
8. la documentazione spiega cosa viene cambiato nel percorso caldo;
9. il rollback o il criterio di uscita dal bridge e' descritto;
10. la issue madre registra la decisione e i numeri usati.

Solo dopo questa checklist il main loop puo' essere migrato senza trasformare
una scelta architetturale in un salto al buio.

## Piano test

Il piano test serve a impedire che la migrazione del core input model venga
validata solo perche' "i log finali sembrano uguali". Il punto delicato non e'
solo l'output finale: e' il contratto fra backend, adapter, core e record
strutturati. Per questo una PR runtime dovra' dimostrare sia equivalenza
semantica sia preservazione delle prove non semantiche.

I test devono proteggere livelli diversi:

- test core: semantica filesystem da raw a eventi semantici;
- test backend: poll, target, capability, diagnostica e record borrowed;
- test formatter/JSONL: schema pubblico e campi esposti;
- test golden: scenari pubblici rappresentativi;
- test focused nuovi: eventuale bridge, conversione record/raw o input
  record-first.

Un cambio al core input model non deve essere coperto solo da golden JSONL.
JSONL e' un output pubblico; non e' il contratto interno primario del core.

### Matrice minima di copertura

| Livello | Cosa deve provare | Esempi di scenari | Perche' serve |
| --- | --- | --- | --- |
| Core raw-first baseline | Il comportamento corrente resta noto e misurabile | create, close-write, modify, delete, rename/move/relocate, overflow | Serve come riferimento per confrontare candidato bridge o record-first |
| Adapter raw -> record | Ogni raw ammesso produce il record normalized_raw corretto | `RAW_CREATE`, `RAW_DELETE`, `RAW_MOVED_FROM`, `RAW_MOVED_TO`, `RAW_OVERFLOW`, mask invalide rifiutate | Evita che il record perda informazioni prima del core |
| Bridge record -> raw, se introdotto | Il ponte ricostruisce esattamente il raw necessario al core oppure dichiara cosa non puo' preservare | path, raw mask, cookie, pid, timestamp, overflow | Un bridge che perde cookie o overflow puo' sembrare veloce ma rompere rename/move o diagnostica |
| Core record-first, se introdotto | Il core produce gli stessi eventi semantici consumando record normalized_raw | stessa suite del core raw-first, eseguita sul nuovo input | Dimostra equivalenza semantica senza dipendere da JSONL |
| Runtime app/main loop | `app_run()` usa il nuovo percorso senza cambiare comportamento utente | test core end-to-end esistenti, backend diagnostics, output compatibile | Protegge l'integrazione reale con inotify e filesystem |
| Output strutturato | JSONL resta coerente con il record model dopo la migrazione | golden JSONL rappresentativi, non tutta la suite core | Protegge il contratto pubblico, ma non sostituisce i test interni |
| Performance gate | Il candidato ha overhead dichiarato rispetto alla baseline | `perf-core-input` e futuri benchmark bridge/record-first | Evita decisioni basate solo su pulizia architetturale |

### Scenari semantici rappresentativi

I test di equivalenza non devono coprire ogni combinazione possibile prima del
primo refactor, ma devono includere scenari che esercitano i punti dove raw e
record possono divergere:

- create file e create directory;
- delete file e delete directory;
- modify e close-write/file-ready;
- rename nello stesso parent;
- move fra directory diverse;
- move+rename/relocated;
- `MOVED_FROM` senza `MOVED_TO` immediato;
- `MOVED_TO` senza sorgente nota;
- overflow;
- raw mask invalide o ambigue rifiutate dagli adapter;
- diagnostica watch/recovery che non deve diventare evento semantico.

Questi scenari esistono gia' in parte nelle suite core, backend e adapter. La
PR che introduce un bridge o un core record-first dovra' indicare quali test
esistenti riusa e quali focused test aggiunge.

### Contratti da verificare prima di una migrazione

Prima di cambiare `app_poll_legacy_raw_backend_once()` o l'input di
`alfred_process()`, la PR deve rispondere esplicitamente a queste domande:

1. quale tipo entra nel core: `alfred_raw_event_t`, `alfred_record_t` o un
   ponte dichiarato?
2. chi possiede le stringhe nel punto di ingresso?
3. il record e' borrowed e consumato sincronicamente, oppure diventa owned e
   puo' attraversare una coda?
4. quali campi sono obbligatori per preservare rename/move: path, cookie,
   raw_mask, timestamp e pid?
5. come sono rappresentati overflow, diagnostica backend, errori e prove?
6. cosa succede se l'adapter rifiuta un record o una raw mask?
7. la failure mode e' fail-closed, evento diagnostico, errore di processo o
   fallback al raw-first?
8. come si torna indietro se il candidato e' piu' lento o meno affidabile?

Se una risposta e' "non ancora", la migrazione puo' essere rimandata. Non e'
un fallimento: e' preferibile conservare il raw-first esplicito piuttosto che
introdurre un bridge ambiguo nel percorso caldo.

### Test minimi per opzione

| Opzione | Test richiesti prima di runtime change |
| --- | --- |
| Restare raw-first | Nessun nuovo test runtime obbligatorio; mantenere baseline benchmark e documentare perche' il debito resta accettabile |
| Bridge misurato | Test focused record->raw o raw->record->raw; test core equivalenti alla baseline; test overflow/cookie; benchmark overhead; documentazione rollback |
| Core record-first | Test unitari del nuovo input core; suite semantica rappresentativa; test adapter; test runtime app; benchmark raw-first vs record-first |
| Dual path temporaneo | Test che dimostrano che i due percorsi producono lo stesso output; diagnostica in caso di divergenza; criterio di rimozione del dual path |

### Regola di documentazione

Ogni PR che cambia il modello di input del core deve aggiornare:

- questo documento, se cambia la checklist o il piano test;
- [Event Model v0](29-event-model-v0.md), se cambia il significato di un
  record o campo;
- [Backend API v0](30-backend-api-v0.md), se cambia il contratto di
  `backend_ops->poll()` o dell'emit boundary;
- [Flusso eventi](07-flusso-eventi.md), se cambia il percorso runtime reale;
- [Report benchmark](34-report-benchmark-prestazioni.md), se aggiunge numeri
  o nuove righe benchmark;
- pagina man o contratto log, solo se cambia output pubblico o CLI.

## Decisione provvisoria v0

La decisione provvisoria della milestone e':

```text
Il runtime resta raw-first per ora.
Non introduciamo ancora un bridge record->core.
Non migriamo ancora il core a record-first.
```

Questo significa che il percorso caldo corrente resta:

```text
app_run()
-> app_poll_legacy_raw_backend_once()
-> inotify_backend_poll()
-> handle_backend_event(alfred_raw_event_t)
-> alfred_process(core, raw)
```

I record continuano a essere fondamentali, ma per ora restano soprattutto:

- il contratto strutturato a valle;
- il formato comune per writer, JSONL, sink e diagnostica;
- la rappresentazione emessa dal percorso staged `backend_ops->poll()`;
- una base misurabile per futuri bridge o record-first.

### Perche' non migrare subito

La scelta non e' ideologica. E' una scelta di rischio e costo sul percorso
caldo.

I dati raccolti finora dicono:

- `core-input-raw-first` misura il costo corrente del core semantico raw-first;
- `raw-to-record-adapter` mostra che `alfred_record_from_raw()` e' economico
  come conversione isolata nel micro-benchmark;
- `raw-to-record-plus-core` mostra che produrre un record normalizzato sidecar
  e poi alimentare il core raw-first resta nello stesso ordine di grandezza,
  ma con rumore sufficiente da non poter diventare una soglia o una garanzia.

Questi numeri sono utili per orientare, ma non dimostrano ancora che:

- un bridge completo record->raw sia corretto e conveniente;
- un core record-first semplifichi davvero il codice;
- ownership, lifetime, overflow, diagnostica ed evidence siano preservati in
  tutti gli scenari;
- il main loop possa essere migrato senza aumentare complessita' o rischio.

Per questo la decisione piu' prudente e' mantenere il raw-first come baseline
runtime e usare i record come envelope/output/documented boundary finche' non
esiste un motivo concreto per pagare il costo del cambio.

### Cosa resta autorizzato

Questa decisione non blocca il lavoro sui record. Restano dentro lo scope:

- migliorare adapter e record model;
- aggiungere benchmark mirati;
- aumentare copertura JSONL e golden pubblici;
- documentare meglio ownership e campi record;
- costruire prototipi isolati, non collegati al main loop, per bridge o
  record-first;
- mantenere `backend_ops->poll()` staged come prova del confine Backend API v0.

La regola e':

```text
Si puo' misurare e prototipare.
Non si cambia il percorso caldo runtime senza nuova decisione.
```

### Criteri per riaprire bridge o record-first

La decisione va riaperta se almeno uno di questi segnali diventa vero:

1. arriva un secondo backend reale e il raw-first inizia a duplicare troppa
   logica specifica;
2. il bridge record->raw ha test focused, preserva path/cookie/raw_mask/pid/
   timestamp/overflow e ha overhead accettabile rispetto alla baseline;
3. un prototipo record-first riduce complessita' del core senza perdere
   semantica o performance;
4. la Backend API staged diventa il percorso principale per piu' backend e il
   raw bridge diventa il maggiore debito di integrazione;
5. i benchmark ripetuti mostrano che il costo di conversione e' stabile e
   trascurabile nel confine core input;
6. i test di equivalenza coprono gli scenari rappresentativi elencati nel
   piano test;
7. un requisito futuro, per esempio provenance, policy o session context a
   monte del core, richiede davvero record come input primario.

Se nessuno di questi segnali e' presente, restare raw-first e' una scelta
consapevole, non inerzia tecnica.

### Implicazione per il prossimo lavoro

Il prossimo lavoro runtime non deve essere "migrare tutto a record". Deve
essere piu' stretto:

```text
mantenere raw-first nel main loop
documentare il debito
chiudere la milestone con criteri di riapertura
poi tornare a lavoro MVP sul backend/output/test
```

Eventuali PR future su bridge o record-first devono nascere come child issue
separate e devono citare questa decisione, spiegando quale criterio di
riapertura stanno soddisfacendo.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- il percorso corrente e' mappato chiaramente;
- le opzioni sono confrontate con pro, contro e rischi;
- il benchmark gate e' scritto;
- il piano test e' scritto;
- la decisione provvisoria e' documentata nella issue madre e negli MD;
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
