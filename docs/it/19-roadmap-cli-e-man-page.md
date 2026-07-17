# Roadmap CLI e pagina man

Questo documento raccoglie le decisioni future sull'interfaccia da riga di
comando di Alfred. Per ora Alfred resta volutamente semplice: i path da
osservare sono argomenti posizionali e il file di configurazione puo' essere
indicato con la variabile d'ambiente `ALFRED_CONFIG`.

Non implementiamo ancora un parser CLI professionale in questo punto. Lo scopo
di questa roadmap e' evitare decisioni improvvisate quando aggiungeremo opzioni
come `-c`, `--config` o la stampa della configurazione effettiva.

## Fonti e convenzioni

Le convenzioni da seguire sono:

- GNU Coding Standards, sezione sulle command-line interfaces:
  <https://www.gnu.org/prep/standards/standards.html>
- POSIX Utility Syntax Guidelines:
  <https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html>
- Linux `man-pages(7)`:
  <https://man7.org/linux/man-pages/man7/man-pages.7.html>

Punti pratici che useremo:

- supportare `--help`
- supportare `--version`
- supportare `--check-config` per validare la configurazione senza avviare il
  runtime
- usare opzioni brevi per comandi frequenti, per esempio `-c`
- usare opzioni lunghe descrittive, per esempio `--config`
- usare `--` come fine delle opzioni quando serve distinguere path che iniziano
  con `-`
- tenere gli argomenti posizionali finali come path da osservare
- documentare chiaramente precedenza tra default, file, ambiente e CLI

## Stato attuale

Oggi l'uso reale e':

```bash
./alfred /path/da/osservare
./alfred --help
./alfred --version
./alfred --check-config
```

Con file di configurazione:

```bash
ALFRED_CONFIG=./alfred.conf ./alfred /path/da/osservare
ALFRED_CONFIG=./alfred.conf ./alfred --check-config
```

La precedenza attuale e':

```text
config_defaults()
    -> ALFRED_CONFIG, se presente
    -> ALFRED_EVENT_ENGINE, se presente
```

`ALFRED_EVENT_ENGINE` esiste solo come validazione di compatibilita': `core` e'
il runtime ufficiale; `shadow` fallisce.

`--help` e `--version` sono comandi informativi: stampano su `stdout`, escono
con codice `0` e terminano prima di inizializzare configurazione, logger,
backend, core, output pipeline o watch. Questo garantisce che non creino
`raw.log`, `events.log`, `errors.log` o `output.jsonl`.

`--check-config` e' un comando di validazione: inizializza i default in memoria,
carica `ALFRED_CONFIG` se presente e applica `ALFRED_EVENT_ENGINE` se presente,
usando gli stessi helper di `app_init()`. Poi termina prima di inizializzare
logger, backend, core, output pipeline o watch. Per questo non crea log runtime
e non valida i path da osservare: i path richiedono il backend e restano nel
percorso di startup normale.

## Comportamento desiderato futuro

Quando aggiungeremo un parser CLI, l'uso principale dovrebbe diventare:

```bash
alfred [OPTIONS] PATH...
```

Esempi:

```bash
alfred /tmp/progetto
alfred -c alfred.conf /tmp/progetto
alfred --config alfred.conf /tmp/progetto
alfred --print-config /tmp/progetto
alfred --config alfred.conf --print-config /tmp/progetto
alfred -- /tmp/path-che-inizia-con-trattino
```

`PATH...` resta obbligatorio quando Alfred deve avviare il monitoraggio.
Alcune opzioni informative, come `--help` e `--version`, terminano invece senza
avviare il backend.

## Opzioni pianificate

| Opzione | Significato | Stato |
| --- | --- | --- |
| `-c FILE` | carica configurazione da `FILE` | da implementare |
| `--config FILE` | forma lunga di `-c` | da implementare |
| `--print-config` | stampa la configurazione effettiva e continua o termina, da decidere | da discutere |
| `--check-config` | valida default, `ALFRED_CONFIG` e `ALFRED_EVENT_ENGINE`, poi termina senza avviare logger/backend/core/output/watch | implementato |
| `--help` | stampa uso breve e opzioni | implementato |
| `--version` | stampa versione del programma | implementato |
| `--` | fine opzioni, tutto cio' che segue e' path | da implementare |

## Stampa della configurazione effettiva

La funzione futura potrebbe chiamarsi, per esempio:

```c
void config_dump_effective(const config_t *cfg, FILE *out);
```

Oppure, se vogliamo tenere separato il backend:

```c
void config_dump_effective(const config_t *cfg, FILE *out);
void inotify_config_dump_effective(const inotify_config_t *cfg, FILE *out);
```

La stampa deve mostrare i valori dopo aver applicato default, file di
configurazione, variabili d'ambiente e opzioni CLI. Per esempio:

```text
recursive=true
inotify_watcher_capacity=128
inotify_watch_mask=IN_CREATE,IN_DELETE,IN_MODIFY,IN_ATTRIB,IN_CLOSE_WRITE,...
raw_log=raw.log
event_log=events.log
error_log=errors.log
event_engine=core
```

Domanda ancora aperta: `--print-config` deve solo stampare e terminare, oppure
stampare e poi avviare Alfred?

Scelta provvisoria consigliata: farla terminare senza avviare il backend. Se in
futuro serve stampare e continuare, possiamo aggiungere una opzione diversa,
per esempio `--verbose-config`.

## Precedenza futura consigliata

Quando esisteranno sia ambiente sia CLI, la precedenza dovrebbe essere:

```text
1. config_defaults()
2. ALFRED_CONFIG, se presente
3. -c FILE / --config FILE, se presente
4. variabili d'ambiente di override specifiche
5. opzioni CLI specifiche
```

Questo significa che la CLI vince sul file indicato dall'ambiente. Esempio:

```bash
ALFRED_CONFIG=base.conf alfred --config debug.conf /tmp/progetto
```

In questo caso `debug.conf` dovrebbe essere il file usato.

## Errori e messaggi

Le regole future dovrebbero essere:

- opzione sconosciuta: errore su `stderr`, exit non-zero
- opzione che richiede argomento ma non lo riceve: errore su `stderr`, exit
  non-zero
- file configurazione non leggibile: errore su `stderr`, exit non-zero
- configurazione invalida: errore su `stderr`, exit non-zero
- `--help`: output su `stdout`, exit `0`
- `--version`: output su `stdout`, exit `0`

Esempio di errore:

```text
alfred: unknown option '--prin-config'
Try 'alfred --help' for usage.
```

## Struttura futura del codice

Per non appesantire `main.c`, il parsing dovrebbe stare nel livello
applicazione:

```text
main()
    -> app_init()
        -> app_parse_args()
        -> config_defaults()
        -> config_load()
        -> logger_init()
        -> backend init
```

Possibile struttura:

```c
typedef struct app_options {
    const char *config_path;
    int print_config;
    int check_config;
    int show_help;
    int show_version;
    int first_path_index;
} app_options_t;
```

Il parser CLI deve solo decidere quali opzioni sono state richieste e dove
iniziano i path. Non deve conoscere la semantica inotify e non deve modificare
direttamente il core.

## Pagina man

Alfred ora ha una prima bozza di pagine man dentro `docs/man/`. Sono piu'
formali degli MD e servono come contratto consultabile da terminale, ma restano
bozze: Event Model v0 e la futura CLI con opzioni vere potranno modificarle.

Per leggerle senza installarle:

```bash
man -l docs/man/man1/alfred.1
man -l docs/man/man5/alfred.conf.5
man -l docs/man/man7/alfred-events.7
```

Le pagine iniziali sono:

```text
alfred(1)
alfred.conf(5)
alfred-events(7)
```

`alfred(1)` documenta l'uso utente corrente: path posizionali, variabile
`ALFRED_CONFIG`, log prodotti, ambiente e limiti attuali.

`alfred.conf(5)` documenta il formato key-value reale, le chiavi supportate, i
default, la maschera inotify e `inotify_audit_events`.

`alfred-events(7)` documenta il modello architetturale corrente: raw backend
log, `ALFRED_RAW_*`, eventi semantici core, diagnostica backend, self events e
lost-scope recovery.

Quando l'interfaccia CLI sara' stabile, queste pagine dovranno essere aggiornate
insieme a eventuali opzioni come:

```text
--help
--version
-c FILE
--config FILE
--print-config
--check-config
```

Struttura consigliata per `alfred(1)`:

```text
NAME
SYNOPSIS
DESCRIPTION
OPTIONS
ENVIRONMENT
FILES
EXIT STATUS
EXAMPLES
SEE ALSO
```

Esempio di synopsis futuro:

```text
alfred [OPTIONS] PATH...
alfred --help
alfred --version
```

`ENVIRONMENT` deve documentare almeno:

```text
ALFRED_CONFIG
ALFRED_EVENT_ENGINE
ALFRED_KEEP_TEST_LOGS
```

`FILES` dovrebbe citare eventuali path standard solo se li introduciamo davvero.
Per ora non promettiamo `/etc/alfred.conf`.

## Decisioni rimandate

- scegliere se usare `getopt_long()` direttamente o una piccola astrazione
  locale
- decidere se `--print-config` termina o continua
- decidere se introdurre un path standard come `/etc/alfred.conf`
- decidere se supportare file config multipli
- decidere se aggiungere output macchina leggibile, per esempio
  `--print-config=json`
- decidere come gestire opzioni non disponibili su sistemi non Linux quando
  arriveranno backend futuri diversi da inotify
