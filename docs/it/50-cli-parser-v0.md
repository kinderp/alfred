# CLI parser v0

Questa milestone introduce il primo parser esplicito della CLI di Alfred dopo
la chiusura di `MVP operational usability v0`.

Il punto di partenza e' volutamente piccolo. Oggi Alfred supporta:

```bash
./alfred PATH...
ALFRED_CONFIG=FILE ./alfred PATH...
./alfred --help
./alfred --version
./alfred --check-config
```

Le opzioni `--help`, `--version` e `--check-config` sono gestite prima del
runtime. Tutto il resto entra ancora nel percorso storico in cui gli argomenti
sono path posizionali. Questo ha funzionato per l'MVP, ma non basta per un uso
piu' naturale: `-c`, `--config`, `--print-config` e `--` devono diventare
contratti espliciti o restare dichiaratamente rimandati.

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
| `./alfred -c conf /tmp/root` | Non implementato | `-c` e `conf` sarebbero trattati come path. |
| `./alfred --config conf /tmp/root` | Non implementato | `--config` e `conf` sarebbero trattati come path. |
| `./alfred --print-config` | Non implementato | Non esiste ancora un output stabile della configurazione effettiva. |
| `./alfred -- /tmp/root` | Non implementato | `--` sarebbe trattato come path, non come fine opzioni. |

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

## Audit parsing corrente

Oggi Alfred non ha ancora un parser CLI generale. Il percorso reale e'
volutamente piu' semplice:

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

Questa scelta ha una conseguenza importante: i comandi informativi esistenti
sono riconosciuti solo nella forma esatta con un unico argomento. Per esempio:

```bash
./alfred --help
./alfred --version
./alfred --check-config
```

sono comandi speciali e terminano prima del runtime.

Invece forme come:

```bash
./alfred --help /tmp/root
./alfred --check-config /tmp/root
./alfred -c conf /tmp/root
./alfred --config conf /tmp/root
./alfred -- /tmp/root
```

non sono ancora interpretate da un parser. Entrano nel percorso normale di
`app_init()` e gli argomenti vengono considerati path da osservare.

Il percorso di configurazione corrente e':

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

Quindi, finche' il parser v0 non esiste, non c'e' un punto in cui `-c`,
`--config`, `--print-config` o `--` possono essere riconosciuti in modo
coerente. Questo audit conferma perche' il parser deve stare prima della
chiamata ad `app_init()` o deve passare ad `app_init()` una struttura gia'
interpretata, non semplicemente inoltrare `argc/argv` grezzi.

I test CLI correnti sono concentrati in `tests/cli/test_help_version.sh` e
vengono eseguiti da `make test-cli`. Coprono:

- `--help`: stdout atteso, stderr vuoto, nessun log runtime;
- `--version`: formato `alfred MAJOR.MINOR.PATCH`, stderr vuoto, nessun log
  runtime;
- `--check-config` con default validi: `configuration OK`, nessun log runtime;
- `--check-config` con `ALFRED_CONFIG` valido;
- `--check-config` con `ALFRED_CONFIG` invalido;
- `--check-config` con `ALFRED_EVENT_ENGINE=shadow`, che deve fallire.

Non coprono ancora:

- `-c FILE`;
- `--config FILE`;
- `--print-config`;
- `--`;
- opzioni sconosciute;
- opzioni con argomento mancante;
- path che iniziano con `-`.

Questi casi sono esattamente il perimetro dei prossimi micro-step.

## Grammatica e precedenza decise per v0

La forma principale resta:

```bash
alfred [OPTIONS] PATH...
```

La grammatica v0 deve essere intenzionalmente piccola. Non introduce
subcommand, opzioni combinate, forme `--option=value`, file di configurazione
multipli o override puntuali delle chiavi di configurazione. Il parser deve
rispondere a una domanda sola:

```text
quali opzioni di processo sono state richieste e dove iniziano i path?
```

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
| `alfred -c FILE -- PATH...` | Carica `FILE`; gli argomenti dopo `--` sono path. |
| `alfred --config FILE -- PATH...` | Forma lunga equivalente alla precedente. |
| `alfred --check-config` | Valida la configurazione effettiva e termina senza runtime. |
| `alfred -c FILE --check-config` | Valida default + `ALFRED_CONFIG` eventuale + `FILE` + env override e termina senza runtime. |
| `alfred --config FILE --check-config` | Forma lunga equivalente alla precedente. |
| `alfred --help` | Stampa usage e termina senza runtime. |
| `alfred --version` | Stampa versione e termina senza runtime. |
| `alfred --print-config` | Se implementato in v0, stampa configurazione effettiva e termina senza runtime. |
| `alfred -c FILE --print-config` | Se `--print-config` viene implementato, stampa la configurazione dopo aver caricato `FILE`. |

### Opzioni v0

| Opzione | Significato | Stato v0 |
| --- | --- | --- |
| `-c FILE` | Usa `FILE` come configurazione. | Da implementare. |
| `--config FILE` | Forma lunga di `-c`. | Da implementare. |
| `--check-config` | Valida configurazione e termina senza runtime. | Gia' implementata, da integrare nel parser. |
| `--print-config` | Stampa configurazione effettiva e termina. | Da decidere prima del codice; se instabile, rimandare esplicitamente. |
| `--help` | Stampa usage e termina. | Gia' implementata, da integrare nel parser. |
| `--version` | Stampa versione e termina. | Gia' implementata, da integrare nel parser. |
| `--` | Fine opzioni; tutto cio' che segue e' path. | Da implementare. |

### Forme rifiutate

Per v0 e' meglio rifiutare i casi ambigui invece di indovinare l'intenzione
dell'utente.

| Forma | Esito v0 | Motivo |
| --- | --- | --- |
| `alfred --unknown` | Errore su `stderr`, exit non-zero. | Evita di trattare errori di battitura come path. |
| `alfred -c` | Errore su `stderr`, exit non-zero. | `-c` richiede un valore. |
| `alfred --config` | Errore su `stderr`, exit non-zero. | `--config` richiede un valore. |
| `alfred -c a.conf --config b.conf PATH` | Errore su `stderr`, exit non-zero. | Un solo file config esplicito evita precedenza ambigua. |
| `alfred -c a.conf -c b.conf PATH` | Errore su `stderr`, exit non-zero. | I duplicati di config sono rifiutati in v0. |
| `alfred --help PATH` | Errore su `stderr`, exit non-zero. | `--help` e' comando esclusivo, non opzione combinabile. |
| `alfred -c a.conf --help` | Errore su `stderr`, exit non-zero. | `--help` non carica configurazione e non si combina con altre opzioni. |
| `alfred --version PATH` | Errore su `stderr`, exit non-zero. | `--version` e' comando esclusivo. |
| `alfred -c a.conf --version` | Errore su `stderr`, exit non-zero. | `--version` non carica configurazione e non si combina con altre opzioni. |
| `alfred --check-config PATH` | Errore su `stderr`, exit non-zero. | `--check-config` non deve avviare runtime e non valida path. |
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
2. ALFRED_CONFIG, se presente
3. -c FILE / --config FILE, se presente
4. ALFRED_EVENT_ENGINE, se presente
5. futuri override CLI specifici, se verranno aggiunti
```

Quindi:

```bash
ALFRED_CONFIG=base.conf ./alfred --config debug.conf /tmp/root
```

deve usare `debug.conf`. La CLI e' una scelta esplicita dell'utente al momento
del comando e deve vincere sul file indicato dall'ambiente.

`ALFRED_EVENT_ENGINE` resta dopo il file esplicito per compatibilita' con il
contratto corrente: e' un override/env guard storico che oggi accetta solo
`core` e rifiuta `shadow`. Non diventa una nuova opzione CLI v0.

I duplicati di `-c`/`--config` sono errore. Questa e' una scelta deliberata:
evita una regola "last wins" implicita, semplifica i test e rende evidente
quando uno script sta componendo opzioni in modo sbagliato.

### Output ed exit status

Regole v0:

- successi informativi (`--help`, `--version`, `--check-config` valido,
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
./alfred -c missing.conf --check-config
./alfred -c
./alfred --config
./alfred --unknown
./alfred -- /tmp/root
./alfred -c valid.conf -- /tmp/root
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
| Implementare parser minimo | Todo | Piccolo, testabile, nel livello applicazione. |
| Aggiungere test CLI | Todo | Successi, errori, no-runtime commands e path dopo `--`. |
| Aggiornare README/man/doc | Todo | Inglese e italiano devono restare allineati al comportamento reale. |
| Readiness review | Todo | Verificare che la milestone non abbia riaperto runtime o hot path. |

## Criteri di chiusura

La milestone puo' chiudersi quando:

- `-c FILE` e `--config FILE` sono implementati o esplicitamente rimandati con
  motivazione;
- `--` e' implementato o rimandato con motivazione;
- `--print-config` ha una decisione chiara: implementato, rimandato o escluso
  da v0;
- `make test-cli` copre la grammatica accettata e i casi rifiutati;
- README, pagine man inglesi e pagine man italiane non promettono opzioni non
  implementate;
- il runtime normale `./alfred PATH...` resta compatibile;
- la milestone non introduce dipendenze esterne o framework CLI pesanti.

## Collegamenti

- GitHub Milestone: [CLI parser v0](https://github.com/kinderp/alfred/milestone/13)
- Issue madre: [#228](https://github.com/kinderp/alfred/issues/228)
- Roadmap CLI storica: [19-roadmap-cli-e-man-page.md](19-roadmap-cli-e-man-page.md)
- MVP usability precedente: [49-mvp-operational-usability-v0.md](49-mvp-operational-usability-v0.md)
- Livello applicazione: [04-livello-applicazione.md](04-livello-applicazione.md)
