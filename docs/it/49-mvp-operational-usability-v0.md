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
| Audit CLI/user workflow corrente | In progress | Issue figlia #212. Confronta `app/src/main.c`, `app/src/app.c`, README, man page e comportamento reale del binario. |
| Decidere CLI minima v0 | Done | La CLI informativa minima v0 e' `--help` + `--version`; `--check-config` resta step separato. |
| Implementare comportamento selezionato | In progress | Issue figlia #214. `--help` e `--version` terminano prima di `app_init()`. |
| Aggiungere test CLI/config | In progress | Issue figlia #214. `make test-cli` copre exit status, stdout/stderr e assenza di log runtime per comandi informativi. |
| Allineare README e man page | Todo | Devono distinguere implementato vs roadmap. |
| Tradurre README e man page in italiano | Todo | Da fare alla fine della milestone, quando README e man page inglesi descrivono il contratto stabile. Le pagine man italiane dovranno essere installabili/consultabili tramite lingua/locale, non solo copiate in un MD. |
| Definire smoke test MVP | Todo | Un percorso breve: build, run su tmpdir, evento, log, JSONL opt-in. |
| Chiusura readiness | Todo | Sintesi di cosa e' affidabile, cosa resta rimandato e cosa si puo' aprire dopo. |

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

Non esiste ancora un parser di opzioni. Di conseguenza, ogni argomento dopo il
nome del programma viene trattato come path da osservare. Questo e' semplice e
prevedibile, ma rende scomodi i casi base di un utente nuovo: `--help` e
`--version` non stampano informazioni, perche' oggi vengono interpretati come
path.

### Comportamento osservato

| Comando | Stato corrente | Interpretazione |
| --- | --- | --- |
| `./alfred /tmp/root` | Supportato | Avvia il runtime su una o piu' directory root. Il processo resta in esecuzione finche' riceve `SIGINT`/`SIGTERM` o incontra un errore runtime. |
| `ALFRED_CONFIG=./alfred.conf ./alfred /tmp/root` | Supportato | Carica la configurazione prima di inizializzare logger, backend, core e output pipeline. |
| `./alfred` | Fallisce con exit non-zero e `startup failed` | Non ci sono path da osservare. Il messaggio e' corretto come fallimento, ma non e' ancora amichevole come usage. |
| `./alfred --help` | Fallisce con exit non-zero e `startup failed` | `--help` viene trattato come path, non come opzione informativa. |
| `./alfred --version` | Fallisce con exit non-zero e `startup failed` | `--version` viene trattato come path, non come opzione informativa. |
| `ALFRED_CONFIG=/file/mancante ./alfred /tmp/root` | Fallisce con `invalid ALFRED_CONFIG=...` e `startup failed` | La failure mode e' esplicita e deve restare coperta da test/documentazione. |
| `./alfred -c conf /tmp/root` | Non implementato | `-c` e `conf` sarebbero trattati come path separati. |
| `./alfred --config conf /tmp/root` | Non implementato | `--config` e `conf` sarebbero trattati come path separati. |
| `./alfred --check-config` | Non implementato | Sarebbe trattato come path. |
| `./alfred -- /tmp/root` | Non implementato | `--` sarebbe trattato come path, non come fine opzioni. |

### Raccomandazione

Il prossimo micro-step dovrebbe decidere e poi implementare il sottoinsieme CLI
minimo, senza introdurre un parser complesso:

1. `--help`: stampa uso breve su `stdout`, exit `0`, nessun logger/backend/core
   inizializzato. Implementato come primo comando informativo v0.
2. `--version`: stampa versione su `stdout`, exit `0`, nessun logger/backend/core
   inizializzato. Implementato come primo comando informativo v0.
3. `--check-config`: da valutare come step separato, perche' richiede definire
   quale configurazione validare, quale precedenza usare e quali subsystem non
   devono partire.

`-c`/`--config` e `--` sono utili, ma toccano la semantica di parsing e la
precedenza con `ALFRED_CONFIG`; conviene affrontarli dopo `--help` e
`--version`.

### Localizzazione README/man page

La traduzione italiana va pianificata come lavoro di fine milestone, non come
primo passo. Il motivo e' pratico: se traduciamo ora README e pagine man, poi
dobbiamo aggiornare due volte anche la traduzione quando cambiano `--help`,
`--version`, eventuale `--check-config` e lo smoke test MVP.

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
