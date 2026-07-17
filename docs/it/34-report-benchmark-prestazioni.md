# Report benchmark prestazioni

Questo documento raccoglie le misurazioni manuali fatte durante la costruzione
del percorso record, queue, dispatcher e writer. Non e' una suite performance
ufficiale e non sostituisce i test funzionali: serve a conservare memoria delle
prove eseguite, dei valori osservati e delle conclusioni architetturali che ne
abbiamo tratto.

Per la spiegazione operativa dei benchmark, dei target `make perf-*`, delle
colonne CSV e delle opzioni come `--records` e `--runs`, leggere prima
[Debugging, test e strumenti](10-debugging-test-e-strumenti.md#9-make-perf-record-sinks).
Questo report invece registra le misure effettivamente raccolte e le
interpretazioni che ne derivano.

La regola piu' importante e' non leggere questi numeri come una promessa di
prestazioni di Alfred in produzione. Le misure storiche iniziali usavano record
sintetici e giravano in memoria. Il benchmark `make perf-runtime-output`
attraversa invece `app_run()`, inotify reale, file creati sul filesystem e
output JSONL su file temporaneo. Restano fuori dal report attuale socket,
thread, lock espliciti, code per sink, percentili e backpressure asincrona
reale.

## Obiettivo del processo

Il percorso caldo target di Alfred deve rimanere molto corto:

```text
evento OS
-> collector/backend
-> normalizzazione minima
-> alfred_record_t
-> enqueue su coda/ring buffer
```

Tutto cio' che e' costoso deve stare fuori da questo percorso:

- formattazione text;
- serializzazione JSONL;
- protobuf o MessagePack futuri;
- scrittura file;
- socket;
- flush;
- dashboard;
- Alfred Lab;
- report;
- policy pesante.

I benchmark attuali servono a rispondere a domande molto concrete:

1. Quanto costa consegnare un record a un sink quasi vuoto?
2. Quanto costa formattare text e JSONL?
3. Quanto costa fare clone owned e passare da una queue?
4. Quanto costa il dispatcher rispetto alla chiamata diretta?
5. Quanto costa comporre queue, runtime drain, dispatcher e writer buffered?
6. La nuova astrazione `alfred_record_output_pipeline_t` aggiunge overhead
   evidente rispetto ai pezzi misurati separatamente?
7. Quando il runtime reale produce una burst, la coda bounded resta osservabile
   tramite contatori locali?

## Milestone Performance suite v0

La milestone `Performance suite v0` trasforma questo report da memoria storica
di misurazioni manuali a primo contratto operativo per i benchmark di Alfred.
Non significa ancora introdurre soglie di rilascio o promettere prestazioni di
produzione. Significa decidere quali percorsi misuriamo, come li nominiamo, quali
campi CSV leggiamo e quali decisioni architetturali richiedono numeri prima di
essere implementate.

Per v0 il perimetro resta piccolo:

- benchmark sintetici in memoria per sink, formatter, queue, dispatcher e output
  pipeline;
- benchmark runtime reali gia' disponibili per il percorso inotify/app/output;
- spiegazione chiara dei campi CSV e dei limiti metodologici;
- separazione fra misure orientative e futuri contratti piu' stabili;
- regola di refresh quando cambiano queue, dispatcher, writer, JSONL, output
  pipeline o percorso runtime misurato.

Restano fuori dalla milestone:

- worker thread e code per sink;
- benchmark con lock, condition variable o concorrenza reale;
- p95/p99 latency e distribuzioni statistiche complete;
- benchmark socket, disco reale controllato, protobuf, MessagePack o writer
  binari;
- gate automatici di performance in CI;
- ottimizzazioni non giustificate da un confronto riproducibile.

Il principio e':

```text
prima misurare e rendere confrontabile,
poi decidere se cambiare architettura.
```

Questa milestone e' quindi una protezione architetturale: evita di introdurre
thread, code separate o formati nuovi senza sapere quale costo hanno oggi il
percorso record, la copia owned, la queue bounded, il dispatcher e i writer.

## Stato di chiusura Performance suite v0

Performance suite v0 e' chiusa come contratto documentale iniziale. Non chiude
il lavoro sulle prestazioni di Alfred; chiude il primo livello di disciplina:
sappiamo quali benchmark esistono, come leggerli, quando rinfrescarli e quali
debiti impediscono decisioni piu' forti.

Risultato consolidato:

- tassonomia dei benchmark v0 definita;
- campi CSV principali spiegati;
- distinzione fra benchmark sintetici e runtime benchmark documentata;
- politica di refresh definita;
- debiti e limiti accettati resi espliciti;
- decisione registrata che non servono nuovi test o script per gli step
  documentation-only della milestone;
- nessuna soglia di rilascio o gate CI introdotti.

La milestone autorizza confronti prudenti tra percorsi simili nello stesso
ambiente e con lo stesso workload. Non autorizza ancora promesse di throughput,
latenza, p95/p99, comportamento su disco reale, socket, thread, code per sink o
backend diversi da inotify.

Il prossimo lavoro sulle prestazioni dovra' partire da uno dei debiti elencati
sotto e trasformarlo in un benchmark mirato prima di usarlo per cambiare
architettura.

## Comandi usati

Le misure di questo report sono state raccolte con:

```bash
make perf-core-input
```

con:

```bash
make perf-record-sinks
```

e con:

```bash
cd tests/perf
bash run_record_sinks.sh --records 1000 --runs 1
```

`make perf-record-sinks` usa il default corrente dello script:

```text
records=100000
runs=1
```

Il run da `1000` record e' utile come smoke benchmark: verifica che tutte le
righe funzionino anche con una dimensione piccola. Il run da `100000` record e'
piu' utile per confrontare i percorsi perche' riduce il peso del rumore fisso
del processo, del timer e della compilazione appena avvenuta.

`make perf-core-input` usa il default corrente dello script:

```text
events=100000
runs=1
```

Questo benchmark e' diverso dai benchmark record/sink: misura il percorso
`alfred_raw_event_t -> alfred_process() -> callback semantica counter/no-op`.
Serve come baseline raw-first per la milestone Core Input Model / Main Loop
Migration v0.

## Avvertenze metodologiche

Per ora le misure hanno questi limiti:

- `runs=1`, quindi non abbiamo distribuzione statistica;
- non c'e' warmup;
- non fissiamo affinity CPU;
- non disabilitiamo governor dinamici della CPU;
- non misuriamo p95/p99 latency;
- non misuriamo queue depth continua nel tempo;
- misuriamo solo la valvola di pressione single-threaded v0, non backpressure
  asincrona reale;
- non misuriamo disco, socket o thread;
- non misuriamo eventi kernel reali.

Di conseguenza, le differenze piccole tra due righe non vanno interpretate come
"A e' piu' veloce di B". Vanno lette come segnali di ordine di grandezza. Se due
righe sono vicine, la conclusione corretta e' che non vediamo overhead evidente
nel run corrente.

## Politica di refresh v0

La Performance suite v0 conserva numeri storici, ma non tutti i numeri storici
restano confrontabili dopo una modifica. Un benchmark vecchio puo' restare utile
per capire il percorso che stavamo studiando in quel momento, ma non deve essere
usato come prova che il codice attuale sia piu' veloce o piu' lento se il
percorso misurato e' cambiato.

La regola base e':

```text
se cambia il percorso misurato, i numeri vanno rinfrescati prima di decidere.
```

Refresh obbligatorio:

- cambia la queue, per esempio capacita', ownership, clone owned, push/pop,
  drain o politica di overflow;
- cambia il dispatcher, per esempio ordine dei sink, fan-out, gestione errori o
  comportamento fail-closed;
- cambia un sink o writer misurato, per esempio text, JSONL, counter o un writer
  futuro;
- cambia la serializzazione JSONL, il buffering, il flush o la gestione degli
  errori di output;
- cambia il percorso runtime misurato da `make perf-runtime-output`, per esempio
  startup, shutdown, output pipeline, drain runtime o opzioni di configurazione;
- cambia il benchmark runner, il formato CSV o il significato di una colonna;
- cambia il workload di riferimento, per esempio `records`, `runs`, numero di
  file, modalita' runtime o scenario generato.

Refresh parziale:

- se cambia solo un writer, vanno rinfrescate almeno le righe che attraversano
  quel writer e le righe composte che lo includono;
- se cambia solo la queue, vanno rinfrescate almeno `queue-counter`,
  `queue-dispatcher-*` e `output-pipeline-*`;
- se cambia solo il dispatcher, vanno rinfrescate almeno `dispatcher-*`,
  `queue-dispatcher-*` e le pipeline che usano il dispatcher;
- se cambia solo il runtime output, vanno rinfrescate almeno le righe
  `runtime-output *`.

Refresh consigliato ma non sempre obbligatorio:

- cambia compilatore, flags di build o ambiente CI;
- cambia kernel, filesystem, VM, container o risorse disponibili;
- cambia il carico della macchina durante i run;
- si vuole confrontare una PR importante con il baseline piu' recente.

Refresh non necessario:

- modifica solo documentazione che non cambia comandi, significato dei campi o
  interpretazione dei risultati;
- modifica solo commenti nel codice senza cambiare comportamento compilato;
- modifica test non usati dai benchmark e non collegati al percorso misurato.

### Gate per workspace/session runtime schema

La milestone `Workspace/session runtime schema v0` usa questa politica di
refresh per evitare di introdurre campi di contesto senza numeri. I campi
coinvolti sono:

```text
workspace_root
workspace_id
ledger_session_id
```

Le decisioni documentali della milestone non richiedono benchmark refresh
finche' non cambiano codice runtime, record, queue, writer, JSONL o workload.
Il refresh diventa invece necessario quando una PR futura rende questi campi
parte del percorso misurato.

Mappa di riferimento:

| Modifica futura | Benchmark da considerare | Motivo |
| --- | --- | --- |
| Campi aggiunti a `alfred_record_t` | queue, clone owned, dispatcher e output pipeline | Cambia la dimensione del record e il lavoro fatto al confine della queue |
| Copia profonda dei campi per ogni evento | queue/owned clone e runtime output | Aumenta allocazioni, copie e cleanup per record |
| JSONL enrichment per ogni evento | formatter JSONL, sink/writer JSONL e runtime output | Aumenta byte emessi, escaping, buffering e I/O |
| Metadata/session record separato per run | smoke runtime output o benchmark mirato se passa dal runtime reale | Costo per-run, non per-evento; contano ordine, flush e compatibilita' |
| Generazione costosa di `workspace_id` | benchmark mirato solo se avviene nel percorso caldo o per evento | Un costo una tantum non va confuso con costo per-event |
| Nuovo sink/writer per session metadata | benchmark writer/output se cambia fan-out o I/O | Cambia il lavoro a valle del dispatcher |

Regola pratica:

```text
se la PR sostiene che una scelta di schema non pesa, deve indicare quale
percorso e' stato misurato o perche' il percorso misurato non cambia.
```

Questa regola protegge il percorso caldo. Un record metadata/sessione separato
puo' essere molto economico se emesso una sola volta, ma questa non e' una verita'
assoluta: se passa dalla stessa pipeline dei record osservativi, va comunque
verificato che ordine, flush e failure mode restino chiari. Al contrario,
per-record enrichment puo' essere comodo per consumer stateless, ma non deve
essere scelto senza misurare volume JSONL, escaping e costo writer.

Quando si rinfrescano i numeri, non bisogna cancellare i run vecchi se servono a
spiegare la storia del progetto. Bisogna invece aggiungere una nuova sezione con:

- data del run;
- branch o commit misurato;
- comandi eseguiti;
- ambiente essenziale, per esempio VM, kernel se rilevante e note sul carico;
- CSV prodotto o estratto rilevante;
- interpretazione breve;
- motivo del refresh, per esempio "queue changed" o "JSONL formatter changed".

I numeri vecchi diventano quindi contesto storico. I numeri nuovi diventano il
baseline corrente solo per la stessa famiglia di benchmark, stesso workload e
stesso ambiente ragionevolmente confrontabile.

Esempio pratico: se cambiamo `alfred_record_queue_push()`, non possiamo usare un
vecchio `queue-dispatcher-jsonl` come prova del costo della queue attuale. Quel
numero resta utile per capire come ragionavamo prima, ma il confronto
architetturale deve usare un run nuovo.

## Debiti benchmark v0

Performance suite v0 non prova a risolvere tutti i problemi di misurazione.
Serve prima a rendere chiaro cosa misuriamo oggi e cosa non possiamo ancora
affermare. I debiti sotto sono accettabili per v0, ma devono restare espliciti:
se li dimentichiamo, rischiamo di prendere decisioni architetturali su numeri
troppo deboli.

| Debito | Perche' conta | Stato v0 | Prima decisione bloccata |
| --- | --- | --- | --- |
| Ripetizioni insufficienti | Con `runs=1` non sappiamo quanto il risultato sia stabile tra run diversi. | Accettato come misura orientativa. | Non usare differenze piccole per scegliere ottimizzazioni o refactor. |
| Percentili mancanti | Media, minimo e massimo non descrivono p95/p99 latency. | Rimandato. | Non promettere latenze di coda o writer in produzione. |
| Confidenza statistica assente | Non registriamo ancora varianza, intervalli di confidenza o dimensione campione sufficiente. | Rimandato. | Non introdurre gate automatici o affermazioni quantitative forti. |
| Ambiente non normalizzato | CPU governor, carico VM, kernel, filesystem e cache possono cambiare i numeri. | Da annotare meglio nei refresh futuri. | Non confrontare run fatti in ambienti molto diversi come se fossero equivalenti. |
| Metadata macchina incompleti | Senza commit, kernel, CPU, filesystem e build flags e' difficile riprodurre un run. | Parzialmente coperto da note manuali. | Non usare vecchi run come baseline forte se manca contesto essenziale. |
| Disco reale non controllato | JSONL su file temporaneo non equivale a carichi disco reali, fsync, device lenti o saturazione I/O. | Rimandato. | Non decidere policy di flush o durability sulla base dei benchmark attuali. |
| Socket e integrazioni esterne assenti | Fluent Bit, OpenTelemetry, SIEM, socket e processi esterni aggiungono backpressure e failure mode diversi. | Rimandato. | Non stimare costo delle integrazioni esterne dai soli writer in memoria/file locale. |
| Thread e lock non misurati | La pipeline v0 e' ancora single-threaded; worker, mutex, condition variable e wakeup cambieranno il costo. | Rimandato finche' non si introduce un worker reale. | Non decidere worker thread o code per sink senza benchmark dedicati. |
| Backpressure asincrona assente | `pressure_drains` misura una valvola v0 single-threaded, non una politica completa di drop, retry o blocco. | Accettato come diagnostica locale. | Non scegliere policy critical/best-effort/debug solo da questi numeri. |
| Code per sink non misurate | Una coda comune e code per sink hanno costi, isolamento e failure mode diversi. | Rimandato. | Non scegliere per-sink queues senza misure comparative. |
| Writer binari assenti | MessagePack, Protobuf o formati binari avranno costi e dimensioni diverse da JSONL/text. | Rimandato. | Non assumere che JSONL rappresenti il costo dei writer futuri. |
| Memoria e allocazioni non tracciate | I benchmark attuali guardano soprattutto tempo e contatori, non allocazioni, RSS o frammentazione. | Rimandato. | Non dichiarare il percorso caldo allocation-free senza misure dedicate. |
| Kernel/backend reali limitati | Il runtime benchmark copre inotify, ma non fanotify, audit, eBPF o backend futuri. | Accettato per la milestone inotify. | Non generalizzare i risultati a backend con semantica diversa. |

Questi debiti non rendono inutili i benchmark attuali. Li rendono onesti. Oggi
possiamo usare Performance suite v0 per:

- verificare che le righe benchmark funzionino;
- capire ordini di grandezza;
- confrontare percorsi simili nello stesso ambiente;
- evitare regressioni macroscopiche;
- documentare quali numeri servono prima di cambiare architettura.

Non possiamo ancora usarla per:

- stabilire soglie CI automatiche;
- promettere throughput o latenza di produzione;
- decidere worker thread, code per sink o policy di backpressure;
- confrontare in modo forte macchine diverse;
- dimostrare costi di writer futuri non implementati.

La regola pratica e': un debito diventa blocker quando la decisione che vogliamo
prendere dipende proprio da quel dato. Per esempio, prima di introdurre code per
sink bisogna misurare almeno una coda comune contro una variante per-sink; prima
di promettere latenza bisogna avere ripetizioni e percentili; prima di cambiare
policy di flush bisogna misurare I/O reale.

## Tassonomia benchmark v0

Performance suite v0 divide i benchmark in famiglie. Questa distinzione e'
importante per non confrontare righe che misurano cose diverse.

| Famiglia | Righe | Cosa isola | Confronti sensati |
| --- | --- | --- | --- |
| Core input | `core-input-raw-first` | Input raw sintetico verso `alfred_process()` con callback counter/no-op | Baseline per futuri bridge record/raw o core record-first; non confrontare direttamente con JSONL o runtime reale |
| Baseline sink | `counter` | Costo minimo di attraversare un sink quasi vuoto | Confrontare con formatter e dispatcher per capire l'ordine di grandezza del lavoro non-I/O |
| Formatter diretti | `text`, `jsonl` | Costo della serializzazione senza queue e senza dispatcher | Confrontare text vs JSONL e confrontare formatter diretti con dispatcher equivalenti |
| Queue/ownership | `queue-counter` | Clone owned, push/pop queue e destroy senza formattazione | Confrontare con `counter` per stimare il costo del confine borrowed -> owned |
| Dispatcher | `dispatcher-counter`, `dispatcher-text`, `dispatcher-jsonl`, `dispatcher-counter-text-jsonl` | Routing e fan-out sincrono senza queue | Confrontare dispatcher con formatter diretti per stimare il costo del routing |
| Queue + dispatcher | `queue-dispatcher-counter`, `queue-dispatcher-text`, `queue-dispatcher-jsonl`, `queue-dispatcher-counter-text-jsonl` | Confine queue/ownership piu' routing verso sink | Confrontare con righe dispatcher equivalenti e con `queue-counter` |
| Output pipeline sintetica | `output-pipeline-jsonl` | Wrapper `alfred_record_output_pipeline_t` in memoria | Confrontare con `queue-dispatcher-jsonl` per vedere se la pipeline composta aggiunge overhead macroscopico |
| Runtime reale | `runtime-output compat-only`, `runtime-output counter-output`, `runtime-output jsonl-output` | `app_run()`, inotify reale, filesystem, drain runtime e output opt-in | Confrontare tra modalita' runtime; non confrontare direttamente con micro-benchmark sintetici come se misurassero lo stesso mondo |

Regole pratiche:

- `counter` e' una baseline, non una prestazione attesa di Alfred reale;
- `core-input-raw-first` e' la baseline del core, non del writer runtime;
- `text` e `jsonl` misurano soprattutto serializzazione e byte prodotti;
- `queue-counter` misura il costo di sicurezza/lifetime del record owned;
- `dispatcher-*` misura routing e fan-out senza ownership queue;
- `queue-dispatcher-*` misura il primo confine runtime single-threaded;
- `output-pipeline-jsonl` serve a controllare il costo del wrapper pipeline,
  non a dimostrare che la pipeline sia piu' veloce del percorso manuale;
- `runtime-output *` misura un percorso end-to-end piu' realistico, ma include
  anche filesystem, shell, scheduler, startup, settle e shutdown.

I confronti piu' utili per le decisioni architetturali sono:

```text
core-input-raw-first
-> futuro bridge record/raw
-> futuro core record-first
```

per decidere il Core Input Model / Main Loop Migration v0 senza confondere il
costo del core con il costo dei writer;

```text
counter
-> text/jsonl
```

per capire quanto costa serializzare rispetto a non fare quasi nulla;

```text
counter
-> queue-counter
```

per capire il costo del confine borrowed -> owned;

```text
jsonl
-> dispatcher-jsonl
-> queue-dispatcher-jsonl
-> output-pipeline-jsonl
```

per capire se dispatcher, queue e wrapper pipeline aggiungono overhead
macroscopico sopra JSONL;

```text
runtime-output compat-only
-> runtime-output counter-output
-> runtime-output jsonl-output
```

per capire il costo osservabile della pipeline nel runtime reale.

Confronti da evitare:

- non usare `records_per_sec_avg` di `counter` come promessa di throughput reale;
- non confrontare direttamente `runtime-output jsonl-output` con `jsonl` come se
  entrambi misurassero solo il formatter;
- non trarre conclusioni forti da differenze piccole quando `runs=1`;
- non usare benchmark sintetici per decidere da soli worker thread, lock,
  backpressure o code per sink;
- non introdurre ottimizzazioni se non e' chiaro quale famiglia di benchmark
  peggiora e quale contratto operativo viene protetto.

## Interpretazione campi CSV v0

I benchmark non producono tutti le stesse colonne. I micro-benchmark sintetici
misurano record in memoria; il benchmark runtime misura Alfred reale con
inotify, filesystem, processo, artifact e output opzionale. Per questo i campi
vanno letti per famiglia.

| Campo | Famiglia | Come leggerlo in v0 |
| --- | --- | --- |
| `benchmark` | Identificativo core-input | Nome della riga benchmark per il core input, per esempio `core-input-raw-first`. |
| `sink` | Identificativo sintetico | Nome della riga benchmark nei micro-benchmark. Serve a capire quale percorso viene misurato, per esempio `queue-dispatcher-jsonl`. |
| `mode` | Identificativo runtime | Nome della modalita' runtime reale, per esempio `compat-only`, `counter-output` o `jsonl-output`. |
| `run` | Identificativo runtime | Numero del run dentro una stessa modalita'. Oggi e' utile soprattutto quando aumenteremo `runs`. |
| `records` | Dimensione workload sintetico | Numero di record sintetici generati dal micro-benchmark. Permette confronti solo fra righe con stesso `records` e stesso ambiente. |
| `raw_events` | Dimensione workload core-input | Numero di `alfred_raw_event_t` sintetici passati a `alfred_process()` nel benchmark core input. |
| `files` | Dimensione workload runtime | Numero di file creati dal benchmark runtime. Non equivale al numero di record: un file puo' generare piu' raw, eventi e record. |
| `runs` | Ripetizioni sintetiche | Numero di ripetizioni aggregate dal micro-benchmark. Con `runs=1` i valori sono orientativi, non statistici. |
| `min_us`, `avg_us`, `max_us` | Timing sintetico | Tempo minimo, medio e massimo dei micro-benchmark. Con `runs=1` coincidono e non descrivono una distribuzione. |
| `startup_us` | Timing runtime | Tempo di avvio del processo Alfred e setup iniziale nel benchmark runtime. Non misura solo il percorso evento. |
| `emit_us` | Timing runtime | Tempo impiegato dallo script a generare il workload sul filesystem. Include shell, filesystem e scheduler, non solo Alfred. |
| `settle_us` | Timing runtime | Tempo atteso/osservato per lasciare smaltire gli eventi e drenare output prima dello shutdown. |
| `total_us` | Timing runtime | Tempo complessivo del run runtime. E' utile per confrontare modalita' runtime, ma include startup, workload, settle e shutdown. |
| `records_per_sec_avg` | Throughput sintetico | Derivato da `records / avg_us`. E' utile per confronti relativi fra micro-benchmark compatibili, non come promessa di throughput reale. |
| `raw_events_per_sec_avg` | Throughput core-input | Derivato da `raw_events / avg_us`. E' utile per confrontare raw-first con futuri bridge/core record-first nello stesso benchmark, non con runtime reale. |
| `files_per_sec` | Throughput runtime | Derivato dal workload di file creati. Include costo esterno di shell/filesystem e non va confrontato direttamente con `records_per_sec_avg`. |
| `bytes_last` | Output sintetico | Byte prodotti dall'ultima ripetizione del micro-benchmark. Aiuta a capire quanto output text/JSONL viene generato. |
| `counter_total_last` | Output sintetico | Numero di record osservati dal counter sink nell'ultima ripetizione. Deve corrispondere al workload atteso nei percorsi counter. |
| `semantic_events_last` | Output core-input | Numero di eventi semantici visti dalla callback counter/no-op nell'ultimo run. Puo' essere inferiore a `raw_events` perche' alcuni raw, come `MOVED_FROM`, sono stati intermedi. |
| `process_errors_last` | Diagnostico core-input | Numero di chiamate `alfred_process()` fallite nell'ultimo run. Deve essere zero per considerare valida la riga. |
| `conversions_per_event` | Diagnostico core-input | Numero dichiarato di conversioni bridge per evento. Nella baseline raw-first e' zero; servira' per confrontare bridge futuri. |
| `output_mode` | Diagnostico core-input | Modalita' di callback usata dal benchmark core input. Per la baseline e' `counter-noop`, quindi nessuna formattazione o I/O. |
| `raw_lines`, `event_lines` | Output runtime | Righe osservate nei log compatibili. Servono a verificare che il runtime abbia davvero visto e processato il workload. |
| `jsonl_lines`, `jsonl_bytes` | Output runtime | Dimensione del ledger JSONL runtime. In `counter-output` devono restare a zero; in `jsonl-output` indicano il costo dell'output strutturato. |
| `enqueue_attempts` | Queue/runtime | Record offerti alla pipeline strutturata. In `compat-only` e' zero perche' la pipeline output e' disabilitata. |
| `enqueue_success` | Queue/runtime | Record entrati nella queue bounded. Va letto insieme a `enqueue_attempts` e `enqueue_failures`. |
| `enqueue_failures` | Queue/runtime | Record non accodati. Se e' maggiore di zero, il run non dimostra un ledger completo. |
| `pressure_drains` | Queue/runtime | Numero di drain di pressione eseguiti per liberare spazio nella queue v0 single-threaded. E' diagnostico, non un obiettivo da massimizzare. |
| `drain_calls` | Queue/runtime | Numero di tentativi di drain della pipeline. Aiuta a capire quanto spesso il runtime ha provato a svuotare la queue. |
| `drained_records` | Queue/runtime | Record consegnati al dispatcher e ai sink. In un run sano deve essere coerente con `enqueue_success`. |
| `max_pending` | Queue/runtime | Massimo riempimento osservato della queue. Valori vicini alla capacita' indicano pressione sulla coda bounded. |
| `process_status` | Diagnostico runtime | Exit status del processo misurato. Non e' una metrica di performance: serve a sapere se il run e' valido. |
| `artifact_dir` | Diagnostico runtime | Directory locale con log e artifact del run. Serve per ispezione manuale e debugging, non per confronti numerici. |

Classificazione pratica:

- campi identificativi: `benchmark`, `sink`, `mode`, `run`, `artifact_dir`;
- campi dimensione workload: `raw_events`, `records`, `files`, `runs`;
- campi temporali: `min_us`, `avg_us`, `max_us`, `startup_us`, `emit_us`,
  `settle_us`, `total_us`;
- campi throughput: `records_per_sec_avg`, `files_per_sec`;
- campi output: `bytes_last`, `counter_total_last`, `raw_lines`,
  `event_lines`, `jsonl_lines`, `jsonl_bytes`;
- campi queue/runtime: `enqueue_attempts`, `enqueue_success`,
  `enqueue_failures`, `pressure_drains`, `drain_calls`, `drained_records`,
  `max_pending`;
- campi diagnostici: `process_status`, `artifact_dir`.

Per Performance suite v0 nessun campo e' ancora una soglia di rilascio. Alcuni
campi pero' sono piu' candidati di altri a diventare contrattuali in futuro:

| Tipo | Campi | Uso attuale |
| --- | --- | --- |
| Informativi | `min_us`, `max_us`, `startup_us`, `emit_us`, `settle_us`, `total_us`, `bytes_last`, `jsonl_bytes` | Aiutano a capire il run, ma non sono ancora soglie. |
| Comparativi | `avg_us`, `records_per_sec_avg`, `files_per_sec` | Utili solo fra benchmark della stessa famiglia, stesso workload e stesso ambiente. |
| Diagnostici | `process_status`, `artifact_dir`, `raw_lines`, `event_lines`, `jsonl_lines`, `counter_total_last` | Dicono se il run e gli artifact sono credibili. |
| Runtime health | `enqueue_attempts`, `enqueue_success`, `enqueue_failures`, `drained_records`, `max_pending`, `pressure_drains` | Misurano se la pipeline bounded ha retto il workload senza perdere record. |

La regola e': prima controllare i campi diagnostici, poi leggere i tempi. Un run
con `process_status != 0`, artifact mancanti o `enqueue_failures > 0` non deve
essere usato per dire che una versione e' piu' veloce o piu' lenta: prima va
capito perche' il run non e' affidabile.

## Significato delle righe principali

| Riga CSV | Cosa misura | Perche' ci interessa |
| --- | --- | --- |
| `core-input-raw-first` | `alfred_raw_event_t -> alfred_process() -> callback counter/no-op` | Baseline del core input corrente prima di misurare bridge o record-first |
| `counter` | `record -> counter sink` | Baseline minima senza formattazione e senza byte prodotti |
| `text` | `record -> text formatter` | Costo del formato leggibile compatibile con i log attuali |
| `jsonl` | `record -> JSONL formatter` | Costo della serializzazione strutturata JSONL |
| `queue-counter` | `record borrowed -> clone owned -> queue push -> pop -> counter -> destroy` | Costo del confine di ownership e queue senza formattazione |
| `dispatcher-counter` | `record -> dispatcher -> counter` | Costo del routing dispatcher verso un sink quasi vuoto |
| `dispatcher-text` | `record -> dispatcher -> text` | Routing dispatcher piu' formatter text |
| `dispatcher-jsonl` | `record -> dispatcher -> JSONL` | Routing dispatcher piu' formatter JSONL |
| `dispatcher-counter-text-jsonl` | `record -> dispatcher -> counter + text + JSONL` | Fan-out sincrono verso piu' sink |
| `queue-dispatcher-counter` | `record -> queue -> dispatcher -> counter` | Prima forma runtime single-threaded senza formattazione |
| `queue-dispatcher-text` | `record -> queue -> dispatcher -> text` | Queue piu' dispatcher piu' text |
| `queue-dispatcher-jsonl` | `record -> queue -> dispatcher -> JSONL` | Queue piu' dispatcher piu' JSONL |
| `queue-dispatcher-counter-text-jsonl` | `record -> queue -> dispatcher -> counter + text + JSONL` | Fan-out single-threaded dopo queue |
| `output-pipeline-jsonl` | `record -> output pipeline enqueue -> runtime drain -> dispatcher -> JSONL buffered writer -> flush in memoria` | Prima misura dell'oggetto pipeline che potra' essere collegato ad `app_run()` |
| `runtime-output compat-only` | `alfred reale -> inotify -> app callback -> log compatibili` | Baseline runtime reale senza pipeline JSONL |
| `runtime-output counter-output` | `alfred reale -> inotify -> app callback -> output pipeline -> counter sink` | Baseline runtime reale della pipeline senza serializzazione e senza file JSONL |
| `runtime-output jsonl-output` | `alfred reale -> inotify -> app callback -> output pipeline -> JSONL file` | Misura runtime reale con `output_enabled=true` e writer JSONL |

## Misura 0: baseline core input raw-first

Comando:

```bash
bash tests/perf/run_core_input.sh --events 100000 --runs 5
```

Questo run misura solo:

```text
alfred_raw_event_t
-> alfred_process()
-> callback semantica counter/no-op
```

Non usa inotify reale, filesystem I/O, record queue, dispatcher, JSONL, text
writer o runtime app. Per questo e' la baseline corretta del core input model:
servira' a confrontare un futuro bridge record/raw o un futuro core
record-first.

Il workload sintetico alterna create, close-write, modify, delete,
directory create/delete, coppia move, moved-to senza sorgente e overflow. La
callback conta soltanto gli eventi semantici. `MOVED_FROM` non produce subito
un evento semantico, quindi `semantic_events_last` puo' essere inferiore a
`raw_events`.

Output da aggiornare a ogni refresh:

```text
benchmark,raw_events,runs,min_us,avg_us,max_us,raw_events_per_sec_avg,semantic_events_last,process_errors_last,conversions_per_event,output_mode
core-input-raw-first,100000,5,160264,173843.40,188253,575230.35,90000,0,0,counter-noop
```

Lettura attesa:

- `process_errors_last` deve restare `0`;
- `conversions_per_event` deve restare `0` per la baseline raw-first;
- `output_mode` deve restare `counter-noop`;
- il valore `raw_events_per_sec_avg` va confrontato solo con benchmark futuri
  che misurano lo stesso confine core input.

Lettura del primo run:

- il benchmark ha processato `100000` raw event sintetici;
- la callback ha visto `90000` eventi semantici perche' ogni blocco da dieci
  raw include un `MOVED_FROM`, che e' uno stato intermedio e non emette subito
  un evento semantico;
- `process_errors_last=0` rende valido il run;
- `conversions_per_event=0` conferma che questa e' la baseline raw-first, senza
  bridge record/raw;
- `raw_events_per_sec_avg=575230.35` e' un valore orientativo su questa VM con
  `runs=5`, non una soglia CI e non una promessa di throughput runtime reale.
- la distanza tra `min_us=160264` e `max_us=188253` mostra gia' rumore di
  misura; per una soglia futura serviranno run ripetuti e ambiente piu'
  controllato.

## Misura 1: run da 100000 record

Comando:

```bash
make perf-record-sinks
```

Data del run: 2026-06-24.

Ambiente: VM di sviluppo del progetto. I valori sono indicativi e dipendono dal
carico della macchina.

```csv
sink,records,runs,min_us,avg_us,max_us,records_per_sec_avg,bytes_last,counter_total_last
counter,100000,1,3724,3724.00,3724,26852846.40,0,100000
text,100000,1,44397,44397.00,44397,2252404.44,6133313,0
jsonl,100000,1,163709,163709.00,163709,610839.97,18266602,0
queue-counter,100000,1,55087,55087.00,55087,1815310.33,0,100000
dispatcher-counter,100000,1,3674,3674.00,3674,27218290.69,0,100000
dispatcher-text,100000,1,23622,23622.00,23622,4233341.80,6133313,0
dispatcher-jsonl,100000,1,138033,138033.00,138033,724464.44,18266602,0
dispatcher-counter-text-jsonl,100000,1,153072,153072.00,153072,653287.34,24399915,100000
queue-dispatcher-counter,100000,1,34111,34111.00,34111,2931605.64,0,100000
queue-dispatcher-text,100000,1,51845,51845.00,51845,1928826.31,6133313,0
queue-dispatcher-jsonl,100000,1,170190,170190.00,170190,587578.59,18266602,0
queue-dispatcher-counter-text-jsonl,100000,1,171192,171192.00,171192,584139.45,24399915,100000
output-pipeline-jsonl,100000,1,147851,147851.00,147851,676356.60,18366602,0
```

### Interpretazione del run da 100000 record

`counter` e `dispatcher-counter` sono molto veloci. Questo e' il primo segnale
che il confine sink e il dispatcher, quando non fanno lavoro costoso, non sono il
problema principale nel benchmark corrente. Il costo di una chiamata tramite
puntatore a funzione e del routing dispatcher e' molto piu' piccolo del costo di
formattare JSONL o produrre molti byte.

`text` e' molto piu' lento di `counter`, ma molto piu' veloce di `jsonl` in
questo run. Questo e' atteso: il formato text produce meno byte e fa meno lavoro
di serializzazione strutturata.

`jsonl` produce circa 18.2 MB per 100000 record. Il costo principale qui e' la
serializzazione: escape delle stringhe, conversione numeri in testo, campi JSON e
dimensione del payload.

`queue-counter` costa molto piu' di `counter`. Questo e' il costo del confine
borrowed -> owned: copia profonda delle stringhe owned, push nella queue, pop e
destroy finale. Il dato conferma che l'ownership sicura non e' gratis. Non e'
una sorpresa e va tenuto sotto controllo per il percorso caldo.

`queue-dispatcher-counter` e' piu' veloce di `queue-counter` in questo run. Con
`runs=1` non conviene costruire una teoria su questa differenza: puo' dipendere
da rumore, layout di codice, branch prediction o dettagli del benchmark. La
conclusione prudente e' che entrambe le righe sono nello stesso ordine di
grandezza del costo queue/ownership senza formattazione.

`queue-dispatcher-jsonl` misura queue, dispatcher e JSONL insieme. Il valore e'
vicino al costo JSONL diretto, ma piu' alto. Questo e' coerente: aggiungiamo
ownership/queue/dispatcher sopra la serializzazione JSONL.

`output-pipeline-jsonl` misura il wrapper `alfred_record_output_pipeline_t`.
Nel run corrente e' vicino a `queue-dispatcher-jsonl` e addirittura inferiore,
ma non dobbiamo interpretarlo come "piu' veloce": con un solo run questa
differenza e' rumore. Il segnale utile e' un altro: la pipeline composta non
mostra overhead evidente rispetto ai pezzi gia' misurati.

Questo e' il risultato che volevamo prima di collegare la pipeline al runtime:
l'astrazione pipeline sembra ragionevole e non introduce, almeno in memoria e in
single-thread, un costo macroscopico.

## Misura 2: run da 1000 record

Comando:

```bash
cd tests/perf
bash run_record_sinks.sh --records 1000 --runs 1
```

Data del run: 2026-06-24.

```csv
sink,records,runs,min_us,avg_us,max_us,records_per_sec_avg,bytes_last,counter_total_last
counter,1000,1,666,666.00,666,1501501.50,0,1000
text,1000,1,1824,1824.00,1824,548245.61,61313,0
jsonl,1000,1,1868,1868.00,1868,535331.91,182602,0
queue-counter,1000,1,379,379.00,379,2638522.43,0,1000
dispatcher-counter,1000,1,32,32.00,32,31250000.00,0,1000
dispatcher-text,1000,1,209,209.00,209,4784689.00,61313,0
dispatcher-jsonl,1000,1,1434,1434.00,1434,697350.07,182602,0
dispatcher-counter-text-jsonl,1000,1,1456,1456.00,1456,686813.19,243915,1000
queue-dispatcher-counter,1000,1,286,286.00,286,3496503.50,0,1000
queue-dispatcher-text,1000,1,452,452.00,452,2212389.38,61313,0
queue-dispatcher-jsonl,1000,1,1475,1475.00,1475,677966.10,182602,0
queue-dispatcher-counter-text-jsonl,1000,1,1700,1700.00,1700,588235.29,243915,1000
output-pipeline-jsonl,1000,1,1499,1499.00,1499,667111.41,183602,0
```

### Interpretazione del run da 1000 record

Il run da 1000 record e' soprattutto uno smoke benchmark. I valori sono molto
sensibili al rumore fisso del processo e del timer, quindi non vanno confrontati
in modo aggressivo.

Anche qui pero' vediamo un segnale coerente:

```text
queue-dispatcher-jsonl: 1475 us
output-pipeline-jsonl: 1499 us
```

Le due righe sono praticamente allineate. Questo conferma, in piccolo, la stessa
lettura del run da 100000 record: il wrapper pipeline non sembra aggiungere un
costo evidente rispetto al percorso queue + dispatcher + JSONL.

## Misura precedente: confronto spot della pipeline

Prima del report e' stato osservato anche un run singolo in cui le due righe
principali erano:

```text
queue-dispatcher-jsonl: 137665 us
output-pipeline-jsonl: 137455 us
```

Anche in quel caso la conclusione corretta non era che la pipeline fosse piu'
veloce. La conclusione era: le due misure sono praticamente allineate e quindi
non si vede overhead macroscopico del wrapper `alfred_record_output_pipeline_t`.

## Misura 3: runtime reale con output opt-in

Comando:

```bash
make perf-runtime-output
```

Questo run non usa record sintetici: avvia Alfred reale, crea file reali sotto
una directory osservata da inotify e confronta runtime compatibile, runtime
pipeline senza writer reale e runtime JSONL opt-in.

Output osservato in questo run:

```text
mode,run,files,process_status,startup_us,emit_us,settle_us,total_us,files_per_sec,raw_lines,event_lines,jsonl_lines,jsonl_bytes,enqueue_attempts,enqueue_success,enqueue_failures,pressure_drains,drain_calls,drained_records,max_pending,artifact_dir
compat-only,1,1000,0,1056593,281258,78503,1476152,3555.45,6006,3012,0,0,0,0,0,0,0,0,0,/tmp/alfred_perf_runtime_output/compat-only/run-1
counter-output,1,1000,0,1029213,213268,268141,1540276,4688.94,6003,3014,0,0,6001,6001,0,4,94,6001,1024,/tmp/alfred_perf_runtime_output/counter-output/run-1
jsonl-output,1,1000,0,1114471,292240,406527,1838986,3421.85,6003,3014,6001,1503121,6001,6001,0,4,58,6001,1024,/tmp/alfred_perf_runtime_output/jsonl-output/run-1
```

Lettura provvisoria:

- il run dimostra che il benchmark operativo funziona e produce artifact
  separati per confrontare `raw.log`, `events.log`, `errors.log` e
  `output.jsonl`;
- `counter-output` attraversa `record -> queue -> drain -> dispatcher -> counter
  sink` e chiude con `jsonl_lines=0` e `jsonl_bytes=0`. Questo e' il risultato
  corretto: il formato counter misura la pipeline runtime senza serializzazione
  JSONL e senza file output;
- `jsonl-output` produce circa 6000 righe JSONL e circa 1.5 MB di output per
  1000 file creati;
- in `counter-output` e `jsonl-output`, `enqueue_attempts=enqueue_success` e
  `drained_records=6001` indicano che tutti i record offerti alla pipeline
  strutturata sono stati accodati e poi consegnati al dispatcher;
- `enqueue_failures=0` indica che la burst non ha prodotto un ledger incompleto;
- `pressure_drains=4` indica che il producer ha trovato la coda piena quattro
  volte e ha usato la valvola di pressione v0 per drenare e ritentare;
- `max_pending=1024` indica che la coda bounded ha raggiunto la capacita'
  corrente durante il workload;
- `emit_us` misura soprattutto il loop shell e il filesystem, non solo Alfred.
  Per questo il valore puo' risultare piu' basso in una modalita' e piu' alto
  nell'altra senza indicare automaticamente un miglioramento reale;
- `settle_us` e `total_us` sono piu' utili per osservare quanto tempo serve al
  processo completo per smaltire eventi, flushare e chiudere;
- `process_status=0` conferma che lo script ha disabilitato LeakSanitizer nel
  processo misurato e che lo shutdown cooperativo non viene sporcato da
  diagnostica sanitizer. Un run precedente mostrava `process_status=1` con
  `errors.log` vuoto proprio perche' LSAN falliva nell'ambiente di esecuzione;
- lo script aspetta almeno `files * 3` righe in `events.log`, perche' una
  creazione file semplice produce tipicamente `FILE_CREATED`, `FILE_MODIFIED` e
  `FILE_READY`. Questo evita di fermare Alfred prima che il workload sia stato
  smaltito.

Questa misura non sostituisce i micro-benchmark. Serve a confrontare la pipeline
sintetica con il runtime reale:

```text
queue-dispatcher-jsonl    -> costo interno queue + dispatcher + JSONL
output-pipeline-jsonl     -> costo della pipeline composta in memoria
runtime counter-output    -> costo osservato della pipeline senza writer reale
runtime jsonl-output      -> costo osservato della pipeline con JSONL e I/O file
```

### Come leggere i nuovi campi runtime

I campi `enqueue_*`, `pressure_drains`, `drain_calls`, `drained_records` e
`max_pending` sono contatori locali del Writer Runtime v0. Non sono ancora una
API pubblica di metriche: servono a noi per capire come si comporta il confine
`record -> queue -> dispatcher -> writer` durante i benchmark.

La lettura consigliata e':

| Campo | Domanda a cui risponde |
| --- | --- |
| `enqueue_attempts` | Quanti record sono stati offerti alla pipeline strutturata? |
| `enqueue_success` | Quanti record sono entrati davvero nella coda bounded? |
| `enqueue_failures` | Ci sono stati record persi o rifiutati dal percorso strutturato? |
| `pressure_drains` | Quante volte il producer ha trovato la coda piena e ha drenato sotto pressione? |
| `drain_calls` | Quante volte il runtime ha provato a svuotare la coda? |
| `drained_records` | Quanti record sono arrivati al dispatcher e ai sink? |
| `max_pending` | Quanto si e' riempita al massimo la coda durante il run? |

Esempi di interpretazione:

- `enqueue_attempts=0` in `compat-only`: corretto, perche' la pipeline
  strutturata e' disabilitata.
- `jsonl_lines=0` e `jsonl_bytes=0` in `counter-output`: corretto, perche' il
  sink counter conta i record ma non produce un ledger JSONL.
- `enqueue_attempts=enqueue_success=drained_records` e `enqueue_failures=0`:
  tutti i record accodati sono stati consegnati.
- `pressure_drains=0` e `max_pending` basso: il workload non ha stressato la
  coda.
- `pressure_drains>0` e `max_pending=1024`: la coda bounded e' arrivata piena.
  Nella v0 single-threaded il producer drena una volta e ritenta l'enqueue.
- `enqueue_failures>0`: il percorso output non e' affidabile per quel run; con
  `output_enabled=true` Alfred deve uscire con errore invece di fingere che il
  ledger JSONL sia completo.

## Cosa possiamo concludere oggi

Le conclusioni provvisorie sono:

1. Il costo principale non sembra essere il dispatcher in se'.
2. Il costo della serializzazione JSONL e' molto piu' rilevante del costo della
   chiamata a sink tramite interfaccia generica.
3. Il confine queue/ownership ha un costo reale, perche' copia e distrugge record
   owned. Questo costo e' accettabile per avere sicurezza di lifetime, ma va
   misurato quando il percorso caldo diventera' piu' vicino a quello reale.
4. `output-pipeline-jsonl` e' allineato a `queue-dispatcher-jsonl` nei run
   osservati. Questo indica che la composizione pipeline non aggiunge overhead
   evidente rispetto ai pezzi gia' misurati.
5. La scelta architetturale `record -> queue -> dispatcher -> writer` resta
   sensata: separa responsabilita', prepara writer diversi e non mostra ancora
   un costo strutturale preoccupante.
6. Nel runtime reale la coda bounded puo' riempirsi durante una burst
   concentrata. La valvola di pressione v0 evita fallimenti falsi, ma il dato
   conferma che il worker asincrono e le future code per sink non sono dettagli
   estetici: serviranno per isolare meglio producer e consumer.
7. La nuova riga `counter-output` permette finalmente di separare, anche nel
   runtime reale, il costo del confine queue/dispatcher dal costo del writer
   JSONL. Su questo run singolo `jsonl-output` e' piu' lento di
   `counter-output`, ma il dato va ripetuto con piu' run prima di trarre
   conclusioni quantitative.

## Cosa non possiamo concludere oggi

Non possiamo ancora dire:

- quanti eventi al secondo reggera' Alfred in produzione;
- quanto costa inotify reale;
- quanto costa `app_run()`;
- quanto costa scrivere JSONL su disco;
- quanto incide `fflush()`;
- quanto incide un socket;
- quanto incide un thread dispatcher;
- come si comporteranno backpressure asincrona, worker thread e code per sink;
- cosa succede se un sink e' lento;
- quale buffer size e' ottimale;
- quale sara' il comportamento p95/p99.

Per queste risposte servono benchmark successivi.

## Valutazione prestazionale provvisoria

Il risultato e' positivo, ma va letto con disciplina.

Il fatto che `output-pipeline-jsonl` sia vicino a `queue-dispatcher-jsonl` e' un
buon segnale per il prossimo collegamento runtime. Significa che possiamo
procedere senza avere gia' evidenza di un wrapper troppo pesante.

La parte da sorvegliare non e' tanto il puntatore a funzione del sink. La parte
da sorvegliare e':

- copia profonda dei record;
- numero di byte prodotti;
- serializzazione JSONL;
- dimensione dei buffer;
- frequenza dei flush;
- eventuale I/O reale;
- politica di backpressure;
- futura presenza di piu' sink;
- isolamento dei sink lenti.

La regola architetturale resta:

```text
il backend non deve aspettare il writer
```

Per ora i benchmark ci dicono che il modello e' promettente. Non ci autorizzano
ancora a dichiarare chiuso il tema performance.

## Prossime misure da fare

Le prossime misure utili sono:

1. Ripetere `make perf-record-sinks` con `--runs 5` o piu' per ridurre il rumore.
2. Aggiungere un benchmark con writer JSONL che scrive su file temporaneo.
3. Confrontare flush finale, flush periodico e flush per record.
4. Misurare buffer JSONL diversi: `8192`, `65536`, `262144`.
5. Eseguire `make perf-runtime-output` con `--runs 5` e file count crescente per
   confrontare runtime compatibile, runtime counter e runtime JSONL opt-in.
6. Usare `counter-output` come baseline runtime senza writer reale e confrontarlo
   con `jsonl-output` per isolare il costo di serializzazione, buffering e file
   I/O.
7. Misurare `pressure_drains`, `max_pending`, eventuali drop e latenza quando
   introdurremo backpressure reale, worker thread e code per sink.
8. Misurare un futuro writer binario, per esempio MessagePack o protocollo
   binario interno.

## Decisione pratica corrente

Alla luce dei run attuali, la pipeline e' stata collegata al runtime in modo
controllato dietro configurazione, mantenendo il percorso legacy come
riferimento. Il prossimo passo prestazionale e' misurare questo collegamento
end-to-end e confrontarlo con le baseline sintetiche.

La pipeline non deve ancora diventare asincrona in questo micro-step. Prima
conviene ottenere un percorso corretto, misurabile e disattivabile. Solo dopo ha
senso introdurre worker thread, code per sink e backpressure reale.
