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

## Grammatica proposta

La forma principale resta:

```bash
alfred [OPTIONS] PATH...
```

Opzioni candidate:

| Opzione | Significato | Stato da decidere |
| --- | --- | --- |
| `-c FILE` | Usa `FILE` come configurazione. | Candidata principale. |
| `--config FILE` | Forma lunga di `-c`. | Candidata principale. |
| `--check-config` | Valida configurazione e termina senza runtime. | Gia' implementata, da integrare nel parser. |
| `--print-config` | Stampa configurazione effettiva e termina. | Da decidere dopo audit output. |
| `--help` | Stampa usage e termina. | Gia' implementata, da integrare nel parser. |
| `--version` | Stampa versione e termina. | Gia' implementata, da integrare nel parser. |
| `--` | Fine opzioni; tutto cio' che segue e' path. | Candidata principale. |

Regole iniziali consigliate:

- `PATH...` resta obbligatorio per avviare il runtime;
- `--help` e `--version` ignorano i path e terminano subito;
- `--check-config` termina senza runtime e non richiede path;
- `--print-config`, se implementato, termina senza runtime e non richiede path;
- opzione sconosciuta: errore su `stderr`, exit non-zero;
- opzione che richiede valore ma non lo riceve: errore su `stderr`, exit
  non-zero;
- path che iniziano con `-` richiedono `--`.

## Precedenza config proposta

La precedenza consigliata e':

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

La milestone deve anche decidere se passare piu' volte `-c`/`--config` e'
errore o se vince l'ultima occorrenza. La scelta piu' semplice per v0 e'
renderlo errore, per evitare configurazioni ambigue.

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
| Setup milestone, issue madre e roadmap | In progress | Issue madre #228, GitHub Milestone #13 e questo documento. |
| Audit parsing corrente | Todo | Leggere codice e test prima di modificare comportamento. |
| Decidere grammatica e precedenza | Todo | Specificare `-c`, `--config`, `--`, `--print-config` e casi ambigui. |
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
