# CLI parser v0

Questa milestone introduce il primo parser esplicito della CLI di Alfred dopo
la chiusura di `MVP operational usability v0`.

Il punto di partenza storico era volutamente piccolo. Prima di questa milestone
Alfred supportava:

```bash
./alfred PATH...
ALFRED_CONFIG=FILE ./alfred PATH...
./alfred --help
./alfred --version
./alfred --check-config
```

Le opzioni `--help`, `--version` e `--check-config` erano gestite prima del
runtime, mentre tutto il resto entrava nel percorso storico in cui gli argomenti
erano path posizionali. Questo ha funzionato per l'MVP, ma non bastava per un
uso piu' naturale: `-c`, `--config`, `--print-config` e `--` dovevano diventare
contratti espliciti o restare dichiaratamente rimandati.

Stato corrente dopo il parser v0 e l'audit #240: Alfred supporta `-c FILE`,
`--config FILE`, `--config=FILE`, `-- PATH...`, `-h`/`--help`,
`-V`/`--version` e `--check-config`. `--print-config` resta rimandato.

## Obiettivo

L'obiettivo e':

```text
rendere esplicita la CLI senza rendere piu' grande il runtime.
```

In pratica:

- definire una grammatica CLI v0 piccola;
- decidere la precedenza fra `ALFRED_CONFIG`, `-c FILE` e `--config FILE`;
- rendere il comportamento di `--` verificabile;
- decidere se `--print-config` e' stabile abbastanza da implementare ora;
- aggiungere test focalizzati su exit status, stdout, stderr e assenza di
  startup runtime quando il comando deve terminare prima del backend;
- aggiornare README, pagine man inglesi e pagine man italiane solo per il
  comportamento effettivamente implementato.

## Perche' ora

La milestone precedente ha reso l'MVP piu' facile da lanciare e verificare, ma
ha lasciato intenzionalmente fuori il parser CLI completo. Il debito ora e'
molto concreto:

| Comando | Stato dopo MVP | Problema |
| --- | --- | --- |
| `./alfred -c conf /tmp/root` | Implementato in parser v0 | Prima sarebbe stato trattato come path; ora seleziona config esplicita. |
| `./alfred --config conf /tmp/root` | Implementato in parser v0 | Prima sarebbe stato trattato come path; ora equivale a `-c FILE`. |
| `./alfred --config=conf /tmp/root` | Implementato dopo audit #240 | Forma GNU-style equivalente a `--config FILE`. |
| `./alfred --print-config` | Non implementato | Non esiste ancora un output stabile della configurazione effettiva. |
| `./alfred -- /tmp/root` | Implementato in parser v0 | `--` termina il parsing delle opzioni. |

Questi problemi sono visibili agli utenti e ai contributori. Risolverli e'
piu' utile, adesso, che riaprire backend futuri o percorso caldo.

Prima di implementare il parser bisogna pero' collegare opzioni e funzionalita'
reali. La tabella
[Mappa funzionalita' e superficie CLI](26-stato-funzionalita.md#mappa-funzionalita-e-superficie-cli)
parte dalle funzionalita' gia' supportate e distingue tre casi:

- funzionalita' utente che meritano una CLI (`PATH...`, `-c`, `--config`, `--`);
- funzionalita' che devono restare configurazione (`output_format`,
  `inotify_watch_mask`, `inotify_audit_events`);
- test, benchmark o roadmap futura che non devono diventare flag runtime.

## Non obiettivi

Questa milestone non implementa:

- subcommand framework;
- opzioni lunghe numerose o modalita' avanzate;
- daemon/service mode;
- installazione delle pagine man;
- refactor del runtime inotify;
- migrazione `backend_ops->poll()` nel main loop;
- modifiche a Event Model, Backend API, Writer Runtime o JSONL;
- Agent Guard, policy, enforcement, process attribution o network events.

Il parser deve orchestrare configurazione e path, non conoscere la semantica
inotify interna.

## Audit parsing storico pre-parser

Prima del parser v0 Alfred non aveva ancora un parser CLI generale. Il percorso
reale era volutamente piu' semplice:

```text
main(argc, argv)
    -> se argc == 2 e argv[1] == "--help"
       -> alfred_print_usage(stdout)
       -> exit 0

    -> se argc == 2 e argv[1] == "--version"
       -> alfred_print_version(stdout)
       -> exit 0

    -> se argc == 2 e argv[1] == "--check-config"
       -> alfred_check_config(stdout, stderr)
       -> exit 0/1

    -> altrimenti
       -> app_init(&app, argc, argv)
       -> app_run(&app)
       -> app_shutdown(&app)
```

Quella scelta aveva una conseguenza importante: i comandi informativi esistenti
erano riconosciuti solo nella forma esatta con un unico argomento. Per esempio:

```bash
./alfred --help
./alfred --version
./alfred --check-config
```

erano comandi speciali e terminavano prima del runtime.

Invece forme come:

```bash
./alfred --help /tmp/root
./alfred --check-config /tmp/root
./alfred -c conf /tmp/root
./alfred --config conf /tmp/root
./alfred -- /tmp/root
```

non erano ancora interpretate da un parser. Entravano nel percorso normale di
`app_init()` e gli argomenti venivano considerati path da osservare.

Il percorso di configurazione storico era:

```text
app_init()
    -> config_defaults(&app->config)
    -> getenv("ALFRED_CONFIG")
    -> config_load(&app->config, ALFRED_CONFIG), se presente
    -> getenv("ALFRED_EVENT_ENGINE")
    -> config_set_event_engine(&app->config, ALFRED_EVENT_ENGINE), se presente
    -> logger_init(...)
    -> backend/core/output init
    -> if argc < 2: errore "no startup paths provided"
    -> for i = 1; i < argc; i++:
           backend_ops->add_target(argv[i])
```

Questo audit confermava perche' il parser doveva stare prima della chiamata ad
`app_init()` o doveva passare ad `app_init()` una struttura gia' interpretata,
non semplicemente inoltrare `argc/argv` grezzi. La soluzione corrente segue
questa scelta: `main()` chiama `alfred_parse_args()` e poi passa path e config
gia' interpretati ad `app_init_from_paths()`.

I test CLI correnti sono concentrati in `tests/cli/test_help_version.sh` e
vengono eseguiti da `make test-cli`. Coprono:

- `--help`: stdout atteso, stderr vuoto, nessun log runtime;
- `-h`: alias side-effect-free di `--help`;
- `--version`: formato `alfred MAJOR.MINOR.PATCH`, stderr vuoto, nessun log
  runtime;
- `-V`: alias side-effect-free di `--version`;
- `--check-config` con default validi: `configuration OK`, nessun log runtime;
- `--check-config` con `ALFRED_CONFIG` valido;
- `--check-config` con `ALFRED_CONFIG` invalido;
- `--check-config` con `ALFRED_EVENT_ENGINE=shadow`, che deve fallire;
- `-c FILE`, `--config FILE` e `--config=FILE` con `--check-config`;
- precedenza della config esplicita su `ALFRED_CONFIG`;
- rifiuto di `--config=` vuoto, config duplicate, opzioni sconosciute, opzioni
  con argomento mancante, opzioni dopo path e path che iniziano con `-` senza
  `--`;
- `--` come terminatore per avviare il runtime su path interpretati.

`--print-config` resta l'unico caso CLI v0 non implementato: il test verifica
che sia rifiutato in modo esplicito finche' non esiste un output stabile della
configurazione effettiva.

## Grammatica e precedenza decise per v0

La forma principale resta:

```bash
alfred [OPTIONS] PATH...
```

La grammatica v0 deve essere intenzionalmente piccola. Non introduce
subcommand, opzioni combinate, file di configurazione multipli o override
puntuali delle chiavi di configurazione. L'unica forma `--option=value`
supportata in v0 e' `--config=FILE`, per allineare l'opzione lunga con le
convenzioni GNU senza aggiungere un framework CLI generale. Il parser deve
rispondere a una domanda sola:

```text
quali opzioni di processo sono state richieste e dove iniziano i path?
```

### Compatibilita' POSIX e convenzioni GNU

La CLI di Alfred deve restare prevedibile per utenti Unix e per script shell.
Per questo la grammatica v0 va letta rispetto a due livelli:

- le Utility Syntax Guidelines POSIX / IEEE Std 1003.1, che definiscono le
  aspettative di base per opzioni corte, option-arguments, operandi e
  terminatore `--`;
- le convenzioni storiche GNU, che rendono normali opzioni lunghe come
  `--help`, `--version` e, per opzioni lunghe con argomento, forme come
  `--config FILE` e spesso `--config=FILE`.

Lo stato dopo l'audit #240 e':

| Aspetto | Stato | Nota |
| --- | --- | --- |
| Opzione corta `-c FILE` | Allineata al sottoinsieme POSIX che ci serve | L'argomento della config e' separato dal nome opzione. |
| Terminatore `--` | Allineato | Tutto cio' che segue diventa path/operando. |
| Opzioni prima degli operandi | Allineato e intenzionalmente rigido | `alfred PATH --config conf` e' rifiutato per evitare ambiguita'. |
| `-h` / `--help` e `-V` / `--version` | Allineati alle convenzioni GNU | Terminano senza runtime e scrivono su `stdout`. |
| `--config FILE` | Estensione GNU-style supportata | Forma lunga equivalente a `-c FILE`. |
| `--config=FILE` | Supportata | Forma lunga GNU-style equivalente a `--config FILE`; `--config=` vuoto e' errore. |
| Alias `-h` / `-V` | Supportati | Non sono richiesti da POSIX, ma sono convenzioni storiche diffuse e restano side-effect-free come le forme lunghe. |

La issue #240 traccia questo audit. La decisione v0 e' pragmatica: Alfred
segue il sottoinsieme POSIX utile per opzioni corte, option-arguments separati,
operandi finali e terminatore `--`; adotta le forme GNU comuni per opzioni
lunghe, help/version e `--config=FILE`; e rifiuta ancora le opzioni dopo i path,
gli abbreviamenti lunghi e le opzioni combinate per mantenere il parser piccolo,
deterministico e facile da testare.

### Forme accettate

Le opzioni devono comparire prima dei path. Il token `--` termina il parsing
delle opzioni: tutto cio' che segue e' path. Senza `--`, un argomento che
inizia con `-` viene interpretato come opzione o come errore di parsing, non
come path.

| Forma | Comportamento v0 |
| --- | --- |
| `alfred PATH...` | Avvia il runtime osservando uno o piu' path. |
| `alfred -- PATH...` | Avvia il runtime; tutti gli argomenti dopo `--` sono path anche se iniziano con `-`. |
| `alfred -c FILE PATH...` | Carica `FILE` come configurazione e avvia il runtime sui path. |
| `alfred --config FILE PATH...` | Forma lunga equivalente a `-c FILE`. |
| `alfred --config=FILE PATH...` | Forma lunga GNU-style equivalente a `--config FILE`. |
| `alfred -c FILE -- PATH...` | Carica `FILE`; gli argomenti dopo `--` sono path. |
| `alfred --config FILE -- PATH...` | Forma lunga equivalente alla precedente. |
| `alfred --check-config` | Valida la configurazione effettiva e termina senza runtime. |
| `alfred -c FILE --check-config` | Valida default + `FILE` esplicito + env override e termina senza runtime. `ALFRED_CONFIG` non viene caricato perche' la CLI esplicita vince sull'ambiente. |
| `alfred --config FILE --check-config` | Forma lunga equivalente alla precedente. |
| `alfred --config=FILE --check-config` | Forma lunga GNU-style equivalente alla precedente. |
| `alfred -h` / `alfred --help` | Stampa usage e termina senza runtime. |
| `alfred -V` / `alfred --version` | Stampa versione e termina senza runtime. |
| `alfred --print-config` | Se implementato in v0, stampa configurazione effettiva e termina senza runtime. |
| `alfred -c FILE --print-config` | Se `--print-config` viene implementato, stampa la configurazione dopo aver caricato `FILE`. |

### Opzioni v0

| Opzione | Significato | Stato v0 |
| --- | --- | --- |
| `-c FILE` | Usa `FILE` come configurazione. | Implementata. |
| `--config FILE` | Forma lunga di `-c`. | Implementata. |
| `--config=FILE` | Forma lunga GNU-style di `--config FILE`. | Implementata dopo audit #240. |
| `--check-config` | Valida configurazione e termina senza runtime. | Implementata e integrata nel parser. |
| `--print-config` | Stampa configurazione effettiva e termina. | Rimandata: manca un output stabile della configurazione effettiva. |
| `-h`, `--help` | Stampa usage e termina. | Implementata. |
| `-V`, `--version` | Stampa versione e termina. | Implementata. |
| `--` | Fine opzioni; tutto cio' che segue e' path. | Implementata. |

### Forme rifiutate

Per v0 e' meglio rifiutare i casi ambigui invece di indovinare l'intenzione
dell'utente.

| Forma | Esito v0 | Motivo |
| --- | --- | --- |
| `alfred --unknown` | Errore su `stderr`, exit non-zero. | Evita di trattare errori di battitura come path. |
| `alfred -c` | Errore su `stderr`, exit non-zero. | `-c` richiede un valore. |
| `alfred --config` | Errore su `stderr`, exit non-zero. | `--config` richiede un valore. |
| `alfred --config=` | Errore su `stderr`, exit non-zero. | La forma con `=` deve avere un valore non vuoto. |
| `alfred -c a.conf --config b.conf PATH` | Errore su `stderr`, exit non-zero. | Un solo file config esplicito evita precedenza ambigua. |
| `alfred -c a.conf -c b.conf PATH` | Errore su `stderr`, exit non-zero. | I duplicati di config sono rifiutati in v0. |
| `alfred --help PATH` | Errore su `stderr`, exit non-zero. | `--help` e' comando esclusivo, non opzione combinabile. |
| `alfred -c a.conf --help` | Errore su `stderr`, exit non-zero. | `--help` non carica configurazione e non si combina con altre opzioni. |
| `alfred -c a.conf -h` | Errore su `stderr`, exit non-zero. | `-h` segue lo stesso contratto esclusivo di `--help`. |
| `alfred --version PATH` | Errore su `stderr`, exit non-zero. | `--version` e' comando esclusivo. |
| `alfred -c a.conf --version` | Errore su `stderr`, exit non-zero. | `--version` non carica configurazione e non si combina con altre opzioni. |
| `alfred -c a.conf -V` | Errore su `stderr`, exit non-zero. | `-V` segue lo stesso contratto esclusivo di `--version`. |
| `alfred --check-config PATH` | Errore su `stderr`, exit non-zero. | `--check-config` non deve avviare runtime e non valida path. |
| `alfred --check-config -c a.conf` | Errore su `stderr`, exit non-zero. | I comandi no-runtime non accettano opzioni successive in v0; usare `alfred -c a.conf --check-config`. |
| `alfred --print-config PATH` | Errore su `stderr`, exit non-zero, se `--print-config` viene implementato. | `--print-config` deve essere no-runtime in v0. |
| `alfred PATH --config conf` | Errore su `stderr`, exit non-zero. | Le opzioni devono precedere i path; dopo un path un token `-...` resta ambiguo senza `--`. |
| `alfred -tmp/root` | Errore su `stderr`, exit non-zero. | Path che iniziano con `-` devono passare dopo `--`. |
| `alfred --` | Errore su `stderr`, exit non-zero. | Dopo `--` serve almeno un path se si avvia runtime. |

Questa scelta e' piu' rigida di alcuni strumenti Unix, ma rende piu' chiaro il
contratto iniziale. In futuro si potranno allentare alcune regole senza rompere
gli utenti; fare il contrario sarebbe piu' difficile.

### Precedenza config v0

La precedenza decisa per v0 e':

```text
1. config_defaults()
2. -c FILE / --config FILE / --config=FILE, se presente
3. altrimenti ALFRED_CONFIG, se presente
4. ALFRED_EVENT_ENGINE, se presente
5. futuri override CLI specifici, se verranno aggiunti
```

Quindi:

```bash
ALFRED_CONFIG=base.conf ./alfred --config=debug.conf /tmp/root
```

deve usare `debug.conf`. La CLI e' una scelta esplicita dell'utente al momento
del comando e deve vincere sul file indicato dall'ambiente. In v0 questo
significa anche che, quando `-c`/`--config`/`--config=FILE` e' presente,
`ALFRED_CONFIG` non viene caricato: un ambiente shell sporco non deve impedire
a un comando esplicito di validare o usare il file scelto dall'utente.

`ALFRED_EVENT_ENGINE` resta dopo il file esplicito per compatibilita' con il
contratto corrente: e' un override/env guard storico che oggi accetta solo
`core` e rifiuta `shadow`. Non diventa una nuova opzione CLI v0.

I duplicati di `-c`/`--config`/`--config=FILE` sono errore. Questa e' una
scelta deliberata: evita una regola "last wins" implicita, semplifica i test e
rende evidente quando uno script sta componendo opzioni in modo sbagliato.

### Output ed exit status

Regole v0:

- successi informativi (`-h`/`--help`, `-V`/`--version`, `--check-config` valido,
  `--print-config` se implementato) scrivono su `stdout` ed escono con codice
  `0`;
- errori di parsing scrivono su `stderr`, non inizializzano runtime e escono
  non-zero;
- errori di configurazione scrivono su `stderr`, non inizializzano runtime e
  escono non-zero;
- comandi no-runtime non devono creare `raw.log`, `events.log`, `errors.log` o
  `output.jsonl`;
- il runtime normale mantiene la compatibilita' con `alfred PATH...`.

## Struttura codice proposta

Il parser dovrebbe stare nel livello applicazione, non nel backend:

```text
app/src/main.c
    -> app_parse_args()
    -> comandi informativi, se richiesti
    -> app_init()
        -> config_defaults()
        -> config_load() dal path deciso dal parser
        -> ALFRED_EVENT_ENGINE
        -> logger/backend/core/output
```

Una struttura minima potrebbe essere:

```c
typedef struct app_cli_options {
    const char *config_path;
    int show_help;
    int show_version;
    int check_config;
    int print_config;
    int first_path_index;
} app_cli_options_t;
```

Questa e' solo una proposta. Prima del codice bisogna leggere `app/src/main.c`,
`app/src/app.c`, `app/include/app.h`, `app/src/config.c` e i test CLI correnti.

## Test richiesti

La milestone deve ampliare `make test-cli` con casi focalizzati.

Esempi minimi:

```text
./alfred --help
./alfred --version
./alfred --check-config
./alfred -c valid.conf --check-config
./alfred --config valid.conf --check-config
./alfred --config=valid.conf --check-config
./alfred -c missing.conf --check-config
./alfred -c
./alfred --config
./alfred --config=
./alfred -c a.conf -c b.conf /tmp/root
./alfred -c a.conf --config b.conf /tmp/root
./alfred --unknown
./alfred -h
./alfred -V
./alfred -- /tmp/root
./alfred -c valid.conf -- /tmp/root
./alfred --help /tmp/root
./alfred -c valid.conf --help
./alfred --version /tmp/root
./alfred -c valid.conf --version
./alfred --check-config /tmp/root
./alfred --print-config /tmp/root   # se --print-config viene implementato
./alfred /tmp/root --config valid.conf
./alfred -tmp/root
./alfred --
```

Ogni test deve controllare:

- exit status;
- stdout/stderr;
- assenza o presenza attesa di file log;
- se il runtime viene avviato o termina prima;
- messaggi di errore abbastanza chiari per un utente.

## Checklist proposta

| Item | Stato | Note |
| --- | --- | --- |
| Setup milestone, issue madre e roadmap | Done | Issue madre #228, GitHub Milestone #13, PR #229 e questo documento. |
| Mappare funzionalita' e superficie CLI | Done | Issue figlia #230. La tabella in `26-stato-funzionalita.md` collega feature, superficie attuale e decisione CLI/config. |
| Audit parsing corrente | Done | Issue figlia #232. Documenta `main()`, `alfred_check_config()`, `app_init()`, `ALFRED_CONFIG`, `ALFRED_EVENT_ENGINE` e copertura `make test-cli`. |
| Decidere grammatica e precedenza | Done | Issue figlia #234. Specifica forme accettate/rifiutate, duplicati `-c`/`--config`, `--`, no-runtime commands, precedenza config e output/exit status. |
| Implementare parser minimo | Done | Issue figlia #236, PR #237. Il parser resta nel livello applicazione e supporta `-c`, `--config`, `--`, comandi no-runtime e errori side-effect-free. |
| Aggiungere test CLI | Done | PR #237 estende `make test-cli` con successi, errori, assenza di log runtime, precedenza config CLI e path dopo `--`. |
| Aggiornare README/man/doc | Done | PR #237 aggiorna README, pagine man inglesi/italiane, livello applicazione, roadmap CLI e matrice funzionalita'. |
| Audit POSIX/GNU CLI e man page | Done | Issue figlia #240 e PR #241. Implementa `--config=FILE`, `-h`, `-V`, test focused e allineamento delle man page inglesi/italiane; merge [`44d307f`](https://github.com/kinderp/alfred/commit/44d307f373078588a0ccc80e2e5d1ed012b0cda3). |
| Readiness review | Done | Issue figlia #238, PR #239. Ha verificato che la milestone non abbia riaperto runtime, backend, hot path, Event Model, Writer Runtime o Agent Guard. |

## Criteri di chiusura

La milestone puo' chiudersi quando:

- `-c FILE` e `--config FILE` sono implementati o esplicitamente rimandati con
  motivazione;
- `--config=FILE`, `-h` e `-V` sono implementati o esplicitamente rimandati con
  motivazione;
- `--` e' implementato o rimandato con motivazione;
- `--print-config` ha una decisione chiara: implementato, rimandato o escluso
  da v0;
- `make test-cli` copre la grammatica accettata e i casi rifiutati;
- README, pagine man inglesi e pagine man italiane non promettono opzioni non
  implementate;
- il runtime normale `./alfred PATH...` resta compatibile;
- la milestone non introduce dipendenze esterne o framework CLI pesanti.

Stato dopo l'audit #240: il parser supporta il sottoinsieme POSIX/GNU deciso
per v0: `-c FILE`, `--config FILE`, `--config=FILE`, `--`, `-h`/`--help`,
`-V`/`--version` e `--check-config`. Restano intenzionalmente rifiutati
abbreviamenti lunghi, opzioni combinate, opzioni dopo i path, duplicati di
config e `--config=` vuoto. `--print-config` resta esplicitamente rimandato
perche' non esiste ancora un output stabile della configurazione effettiva.
Dopo il merge della PR #241, questi criteri sono soddisfatti per il perimetro
v0. La milestone puo' quindi chiudersi come parser piccolo, esplicito e
documentato; `--print-config` resta un debito futuro, non una non-conformita'
nascosta della milestone.

## Progress update: PR #241 merged

PR #241 ha chiuso la issue #240 e il gap POSIX/GNU rimasto dopo il parser
minimo. Il contratto CLI v0 consolidato e':

- opzioni corte supportate: `-c FILE`, `-h`, `-V`;
- opzioni lunghe supportate: `--config FILE`, `--config=FILE`, `--help`,
  `--version`, `--check-config`;
- terminatore degli operandi: `-- PATH...`;
- config esplicita da CLI con precedenza su `ALFRED_CONFIG`;
- no-runtime commands side-effect-free;
- casi ambigui rifiutati con errore, senza avvio runtime;
- `--print-config` rimandato per mancanza di output stabile della
  configurazione effettiva.

La issue #242 traccia solo il bookkeeping finale: riallineare questo documento,
il registro milestone e la issue madre #228 dopo il merge.

## Collegamenti

- GitHub Milestone: [CLI parser v0](https://github.com/kinderp/alfred/milestone/13)
- Issue madre: [#228](https://github.com/kinderp/alfred/issues/228)
- Readiness review: [#238](https://github.com/kinderp/alfred/issues/238)
- Readiness PR: [#239](https://github.com/kinderp/alfred/pull/239)
- Audit POSIX/GNU: [#240](https://github.com/kinderp/alfred/issues/240)
- Audit POSIX/GNU PR: [#241](https://github.com/kinderp/alfred/pull/241)
- Chiusura bookkeeping: [#242](https://github.com/kinderp/alfred/issues/242)
- Roadmap CLI storica: [19-roadmap-cli-e-man-page.md](19-roadmap-cli-e-man-page.md)
- MVP usability precedente: [49-mvp-operational-usability-v0.md](49-mvp-operational-usability-v0.md)
- Livello applicazione: [04-livello-applicazione.md](04-livello-applicazione.md)
