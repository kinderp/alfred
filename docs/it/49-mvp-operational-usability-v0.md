# MVP operational usability v0

Questa milestone serve a rendere l'MVP corrente di Alfred piu' semplice da
usare, verificare e spiegare prima di allargare il progetto con nuovi backend,
Agent Guard o visioni piu' lunghe come Observation Runtime.

Il punto di partenza e' importante: Alfred ha gia' un backend inotify di
riferimento, Backend API v0 staged, Event Model v0, record strutturati, JSONL
opt-in, benchmark iniziali, Lab documentale e una decisione esplicita sul core
input model. Pero' un utente o contributore nuovo deve ancora leggere molti
documenti per capire come avviare Alfred, quali opzioni sono reali, quali log
guardare e quali comandi dimostrano che l'MVP funziona.

Questa milestone chiude quel gap operativo.

## Obiettivo

L'obiettivo e':

```text
rendere l'MVP attuale facile da lanciare, controllare e spiegare
senza cambiare il percorso caldo e senza anticipare backend futuri.
```

In pratica significa:

- chiarire la CLI corrente e decidere quali opzioni minime implementare ora;
- distinguere bene opzioni implementate da opzioni roadmap;
- allineare README, documentazione italiana e pagine man;
- preparare, a fine milestone, la traduzione italiana del README pubblico e
  delle pagine man quando il contratto CLI/man page inglese e' stabile;
- definire un percorso di smoke test riproducibile per utenti e contributori;
- mantenere chiaro cosa Alfred supporta oggi e cosa resta rimandato;
- non riaprire decisioni appena chiuse sul core input model.

## Perche' ora

Dopo `Core Input Model / Main Loop Migration v0`, la scelta architetturale e'
deliberata:

```text
runtime raw-first per v0
nessun bridge record->core
nessun core record-first
```

Quindi non serve forzare una migrazione del percorso caldo. Il lavoro piu'
utile e' tornare all'MVP osservabile: rendere cio' che esiste davvero piu'
accessibile e verificabile.

Questa milestone prepara anche il terreno per futuri utenti reali. Prima di
aggiungere fanotify, eBPF, audit o policy, Alfred deve saper rispondere bene a
domande semplici:

- come si lancia?
- come si configura?
- come si controlla che la configurazione sia valida?
- quali file produce?
- come capisco se JSONL e' abilitato?
- quali eventi sono supportati?
- quale comando di test dimostra che l'MVP non e' rotto?

## Non obiettivi

Questa milestone non implementa:

- fanotify;
- eBPF;
- audit;
- backend Windows o macOS;
- Agent Guard;
- policy engine;
- enforcement o approval UI;
- process attribution;
- network events;
- migrazione del main loop a `backend_ops->poll()`;
- core record-first;
- worker thread;
- code per sink;
- nuovo formato binario;
- Universal Observation Runtime.

Questi temi possono influenzare la chiarezza dei contratti, ma non devono
allargare il lavoro corrente.

## Confini architetturali da preservare

La milestone deve rispettare i confini gia' decisi:

| Confine | Regola |
| --- | --- |
| Backend | Non scrive JSONL e non decide policy finali. |
| Core | Continua a consumare `alfred_raw_event_t` nel runtime v0. |
| Record | Resta il contratto strutturato per output, adapter e writer. |
| JSONL | Resta output opt-in, non Backend API. |
| Writer runtime | Resta single-threaded/opt-in; niente worker thread in questa milestone. |
| CLI | Deve orchestrare configurazione e uso, non conoscere semantica inotify interna. |

## Domande della milestone

Le domande da chiudere sono:

1. Qual e' la CLI minima utile per un MVP pubblico?
2. `--help` e `--version` vanno implementati subito?
3. `--check-config` e' abbastanza piccolo da introdurre ora o va rimandato?
4. La precedenza `ALFRED_CONFIG` vs futura `--config` va solo documentata o
   implementata?
5. Quale comando di smoke test deve poter eseguire un nuovo contributore?
6. README e man page descrivono il comportamento reale o contengono roadmap
   non chiaramente marcata?
7. I log `raw.log`, `events.log`, `errors.log` e `output.jsonl` sono spiegati
   abbastanza bene per un utente?

## Checklist proposta

| Item | Stato | Note |
| --- | --- | --- |
| Setup milestone, issue madre e roadmap | Done | Issue madre #209, issue figlia #210 e PR #211 aprono la milestone. |
| Audit CLI/user workflow corrente | Done | Issue figlia #212 e PR #213 hanno confrontato `app/src/main.c`, `app/src/app.c`, README, man page e comportamento reale del binario prima delle modifiche CLI. |
| Decidere CLI minima v0 | Done | La CLI minima v0 include `--help`, `--version` e `--check-config`; `-c`/`--config`, `--print-config` e `--` restano step futuri. |
| Implementare comportamento selezionato | Done | Issue figlie #214 e #216. `--help` e `--version` terminano prima di `app_init()`; `--check-config` valida configurazione e termina prima del runtime. |
| Aggiungere test CLI/config | Done | Issue figlie #214 e #216. `make test-cli` copre exit status, stdout/stderr e assenza di log runtime per comandi informativi e validazione config. |
| Allineare README e man page | Done | Issue figlie #214, #216 e #220. README, `alfred(1)`, `alfred.conf(5)` e `alfred-events(7)` descrivono CLI minima, smoke test, JSONL opt-in, session context e roadmap/non-goal in modo coerente. |
| Tradurre README e man page in italiano | Done | Issue figlia #222 e PR #223. Dopo la stabilizzazione del testo inglese, aggiunge `README.it.md` e pagine man italiane in layout locale `docs/man/it/man1`, `docs/man/it/man5` e `docs/man/it/man7`. |
| Definire smoke test MVP | Done | Issue figlia #218 e PR #219. Il percorso breve e' `make smoke-mvp`: build, CLI minima, runtime su tmpdir, eventi rappresentativi, log compatibili e JSONL opt-in. Merge di riferimento: 7421aed. |
| Chiusura readiness | Done | Issue figlia #224. Questa sezione chiude la milestone con sintesi di percorsi affidabili, validazione, debiti rimandati e prossimi passi consigliati. |

## Audit CLI/user workflow corrente

Questo audit fotografa il comportamento reale del binario prima di aggiungere
opzioni nuove. Serve a evitare che README, pagine man o issue promettano una
CLI diversa da quella implementata.

### Percorso osservato nel codice

Il percorso corrente e':

```text
app/src/main.c
    -> app_init(&app, argc, argv)
        -> config_defaults()
        -> getenv("ALFRED_CONFIG")
        -> config_load(), se ALFRED_CONFIG e' presente
        -> getenv("ALFRED_EVENT_ENGINE")
        -> logger_init()
        -> backend init/start/target setup
        -> argv[1..argc-1] come path posizionali da osservare
    -> app_run()
    -> app_shutdown()
```

Non esiste ancora un parser di opzioni generale. Esistono pero' tre comandi
speciali riconosciuti da `main()` prima del runtime: `--help`, `--version` e
`--check-config`. Tutti gli altri argomenti restano nel percorso storico e
vengono trattati come path da osservare.

### Comportamento osservato

| Comando | Stato corrente | Interpretazione |
| --- | --- | --- |
| `./alfred /tmp/root` | Supportato | Avvia il runtime su una o piu' directory root. Il processo resta in esecuzione finche' riceve `SIGINT`/`SIGTERM` o incontra un errore runtime. |
| `ALFRED_CONFIG=./alfred.conf ./alfred /tmp/root` | Supportato | Carica la configurazione prima di inizializzare logger, backend, core e output pipeline. |
| `./alfred` | Fallisce con exit non-zero e `startup failed` | Non ci sono path da osservare. Il messaggio e' corretto come fallimento, ma non e' ancora amichevole come usage. |
| `./alfred --help` | Supportato | Stampa usage su `stdout`, exit `0`, nessun runtime/log. |
| `./alfred --version` | Supportato | Stampa versione su `stdout`, exit `0`, nessun runtime/log. |
| `ALFRED_CONFIG=/file/mancante ./alfred /tmp/root` | Fallisce con `invalid ALFRED_CONFIG=...` e `startup failed` | La failure mode e' esplicita e deve restare coperta da test/documentazione. |
| `./alfred -c conf /tmp/root` | Non implementato | `-c` e `conf` sarebbero trattati come path separati. |
| `./alfred --config conf /tmp/root` | Non implementato | `--config` e `conf` sarebbero trattati come path separati. |
| `./alfred --check-config` | Supportato | Valida default, `ALFRED_CONFIG` e `ALFRED_EVENT_ENGINE`, poi termina senza logger/backend/core/output/watch. |
| `./alfred -- /tmp/root` | Non implementato | `--` sarebbe trattato come path, non come fine opzioni. |

### Raccomandazione

Il sottoinsieme CLI minimo viene introdotto senza un parser complesso:

1. `--help`: stampa uso breve su `stdout`, exit `0`, nessun logger/backend/core
   inizializzato. Implementato come primo comando informativo v0.
2. `--version`: stampa versione su `stdout`, exit `0`, nessun logger/backend/core
   inizializzato. Implementato come primo comando informativo v0.
3. `--check-config`: implementato nello step successivo come validazione di
   default, `ALFRED_CONFIG` e `ALFRED_EVENT_ENGINE`; termina senza logger,
   backend, core, output pipeline o watch.

`-c`/`--config` e `--` sono utili, ma toccano la semantica di parsing e la
precedenza con `ALFRED_CONFIG`; conviene affrontarli in step futuri separati.

### Localizzazione README/man page

La traduzione italiana va pianificata come lavoro di fine milestone, non come
primo passo. Il motivo e' pratico: se traduciamo ora README e pagine man, poi
dobbiamo aggiornare due volte anche la traduzione quando cambiano `--help`,
`--version`, `--check-config` e lo smoke test MVP.

Il deliverable corretto e':

```text
README pubblico inglese stabile
    -> traduzione italiana coerente

docs/man/man1/alfred.1 stabile
docs/man/man5/alfred.conf.5 stabile
docs/man/man7/alfred-events.7 stabile
    -> pagine man italiane in layout locale, per esempio docs/man/it/man1,
       docs/man/it/man5 e docs/man/it/man7
```

L'obiettivo non e' solo avere un `.md` tradotto: l'utente deve poter consultare
le pagine italiane quando usa una lingua/locale italiano o quando passa
esplicitamente il path alla pagina man italiana.

### Dopo il completamento dell'MVP

Quando l'MVP di Alfred sara' chiuso e il contratto operativo sara' piu' stabile,
bisogna fare un passaggio documentale piu' ampio:

1. usare la documentazione tecnica italiana consolidata per ampliare le pagine
   man inglesi e italiane, evitando che restino solo un riferimento minimo;
2. mantenere le pagine man concise ma piu' complete su configurazione, eventi,
   JSONL, smoke test, limiti e troubleshooting;
3. tradurre progressivamente `docs/it` in una futura `docs/en`, separando il
   contenuto didattico stabile dalle note storiche o di roadmap;
4. aggiornare README, indici, roadmap e pagine man nello stesso passaggio quando
   cambia un contratto utente o contributore.

Questa attivita' non fa parte del primo MVP operativo: e' un promemoria per la
chiusura o il primo ciclo post-MVP, quando avremo meno churn sui testi tecnici.

## Smoke test MVP

Il percorso breve della milestone e':

```bash
make smoke-mvp
```

Questo comando non e' la suite completa e non e' un benchmark. Serve come prova
operativa minima per un utente o contributore che vuole capire se l'MVP corrente
funziona end-to-end.

La ricetta fa queste cose:

1. compila Alfred;
2. verifica i comandi informativi `--help`, `--version` e `--check-config`;
3. crea una configurazione temporanea con `output_enabled=true`,
   `output_format=jsonl` e `output_log=output.jsonl`;
4. avvia Alfred su una directory temporanea reale;
5. crea un file, lo rinomina e crea una directory;
6. ferma il runtime con `SIGINT`;
7. controlla che esistano `raw.log`, `events.log` e `output.jsonl`;
8. valida righe rappresentative nei log compatibili e record JSONL parseabili.

Questa prova dimostra che il percorso utente minimo e' ancora sano:

```text
CLI minima
    -> configurazione output JSONL
    -> runtime inotify reale
    -> raw.log compatibile
    -> events.log compatibile
    -> output.jsonl strutturato
```

Non dimostra invece copertura completa di rename/move/relocate, overflow,
diagnostica backend, golden JSONL, recovery lost-scope o prestazioni. Quei
contratti restano protetti dalle suite dedicate e dai benchmark manuali.

Per conservare gli artifact:

```bash
ALFRED_KEEP_TEST_LOGS=1 make smoke-mvp
```

## Readiness audit finale

La milestone `MVP operational usability v0` e' pronta per la chiusura quando la
PR collegata alla issue #224 viene mergiata. Il risultato non rende Alfred un
prodotto completo, ma rende l'MVP corrente molto piu' facile da lanciare,
validare e spiegare.

### Percorsi affidabili oggi

| Percorso | Stato | Come verificarlo |
| --- | --- | --- |
| Build base | Affidabile per MVP | `make` |
| CLI informativa | Affidabile per MVP | `./alfred --help`, `./alfred --version`, `make test-cli` |
| Validazione configurazione | Affidabile per MVP | `./alfred --check-config`, `ALFRED_CONFIG=... ./alfred --check-config`, `make test-cli` |
| Runtime inotify su directory | Affidabile per MVP | `./alfred /tmp/root`, suite core/backend e `make smoke-mvp` |
| Log compatibili | Affidabile per MVP | `raw.log`, `events.log`, `errors.log`, `make smoke-mvp` |
| JSONL opt-in | Affidabile per il percorso cablato | `output_enabled=true`, `output_format=jsonl`, `make test-jsonl`, `make smoke-mvp` |
| README/man page inglesi | Allineati al contratto corrente | `README.md`, `docs/man/man1`, `docs/man/man5`, `docs/man/man7` |
| README/man page italiane | Disponibili localmente | `README.it.md`, `man -l docs/man/it/man*/...` |

### Comandi consigliati prima di aprire PR

Per una modifica ordinaria sull'MVP corrente:

```bash
git diff --check
make
make test
make test-cli
make smoke-mvp
make test-jsonl
make test-backend-diagnostics
```

Per modifiche alla documentazione man page:

```bash
man -l docs/man/man1/alfred.1 >/dev/null
man -l docs/man/man5/alfred.conf.5 >/dev/null
man -l docs/man/man7/alfred-events.7 >/dev/null
man -l docs/man/it/man1/alfred.1 >/dev/null
man -l docs/man/it/man5/alfred.conf.5 >/dev/null
man -l docs/man/it/man7/alfred-events.7 >/dev/null
```

### Debiti intenzionalmente rimandati

Questa milestone lascia esplicitamente fuori:

- parser CLI completo con `-c`, `--config`, `--print-config` e `--`;
- installazione o packaging delle pagine man, incluse quelle localizzate;
- fanotify, eBPF, audit, Windows e macOS;
- Agent Guard, policy engine, approval UI, would-block e process attribution;
- migrazione del main loop a `backend_ops->poll()`;
- core record-first o bridge record->core;
- worker thread, per-sink queues e output runtime asincrono;
- benchmark nuovi sul percorso caldo;
- traduzione completa di `docs/it` in `docs/en`.

Questi punti non sono buchi nascosti dell'MVP operativo. Sono lavori futuri che
devono avere issue, PR, benchmark o decisioni dedicate.

### Prossimo passo consigliato

Dopo la chiusura di questa milestone, il passo successivo non dovrebbe riaprire
subito piu' fronti insieme. Le alternative naturali sono:

1. chiudere eventuali debiti documentali piccoli emersi dalla readiness review;
2. aprire una milestone mirata sul parser CLI se vogliamo `-c`, `--config` e
   `--`;
3. tornare a una milestone architetturale solo se serve per il prossimo prodotto
   concreto, mantenendo la regola dei benchmark prima di cambiare il percorso
   caldo;
4. pianificare la traduzione `docs/en` e l'ampliamento man page come lavoro
   post-MVP, non come requisito del primo MVP operativo.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- esiste una CLI minima documentata e coperta da test, oppure e' documentata
  esplicitamente la decisione di non ampliarla ancora;
- README e man page non contraddicono il comportamento reale;
- esiste un percorso di smoke test MVP riproducibile;
- la matrice funzionalita' resta coerente con il README pubblico;
- eventuali opzioni CLI nuove hanno exit status, stdout/stderr e failure mode
  espliciti;
- la documentazione spiega cosa controllare nei log compatibili e in
  `output.jsonl`;
- i non-goal restano rispettati.

## Collegamenti

- GitHub Milestone: [MVP operational usability v0](https://github.com/kinderp/alfred/milestone/12)
- Issue madre: [#209](https://github.com/kinderp/alfred/issues/209)
- Roadmap CLI e pagina man: [19-roadmap-cli-e-man-page.md](19-roadmap-cli-e-man-page.md)
- Stato funzionalita': [26-stato-funzionalita.md](26-stato-funzionalita.md)
- Registro milestone: [37-roadmap-milestone-progetto.md](37-roadmap-milestone-progetto.md)
- Core input decision: [48-core-input-main-loop-migration-v0.md](48-core-input-main-loop-migration-v0.md)
