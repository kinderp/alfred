# Debugging, test e strumenti

Questo capitolo spiega gli strumenti usati per trovare bug, controllare la
qualita' del codice e verificare il comportamento del programma.

## Compilazione di sviluppo

Il comando principale e':

```bash
make
```

La build di sviluppo usa:

- simboli di debug
- ottimizzazioni disabilitate
- AddressSanitizer
- UndefinedBehaviorSanitizer
- warning severi

Questa configurazione e' pensata per trovare errori durante lo sviluppo.

## Sanitizer

I sanitizer sono controlli automatici inseriti dal compilatore nel programma.
Quando il programma gira, questi controlli intercettano errori che in C spesso
sono difficili da vedere.

### AddressSanitizer

Trova problemi di memoria.

Esempi:

```c
char buf[4];
buf[10] = 'x'; /* errore: fuori dai limiti */
```

Oppure:

```c
free(p);
printf("%s\n", p); /* errore: uso dopo free */
```

Quando AddressSanitizer trova un errore, stampa:

- tipo di errore
- file e riga
- stack trace
- indirizzo di memoria coinvolto

### UndefinedBehaviorSanitizer

Trova comportamenti indefiniti.

In C, "comportamento indefinito" significa che il linguaggio non garantisce
cosa succede. Il programma potrebbe sembrare funzionare oppure rompersi in modo
imprevedibile.

Esempi:

- overflow di interi signed
- shift con numero di bit non valido
- accesso a puntatori non validi

## Valgrind

Valgrind esegue il programma dentro un ambiente controllato e controlla come
usa la memoria.

Comando:

```bash
make valgrind
```

Il target usa opzioni severe:

```text
--leak-check=full
--show-leak-kinds=all
--track-origins=yes
```

### Differenza tra sanitizer e Valgrind

| Strumento | Punti forti | Limiti |
| --- | --- | --- |
| Sanitizer | veloce, ottimo durante sviluppo | richiede ricompilazione |
| Valgrind | molto dettagliato sui leak | molto piu' lento |

Di solito si usa prima la build con sanitizer. Valgrind si usa per controlli
piu' approfonditi.

## GDB

GDB e' il debugger.

Comando:

```bash
make gdb
```

Comandi GDB utili:

```text
run
break app_init
next
step
print app
backtrace
quit
```

Significato:

- `break`: mette un breakpoint
- `run`: avvia il programma
- `next`: esegue la prossima riga senza entrare nelle funzioni
- `step`: entra dentro una funzione
- `print`: stampa una variabile
- `backtrace`: mostra la catena di chiamate

## Browser del codice

Oltre a test e debugger, il progetto mantiene alcuni strumenti locali per
navigare il codice dal browser. Sono utili quando uno studente deve orientarsi
in una codebase non piccola: invece di aprire file a caso, puo' cercare una
funzione, una struttura dati o un evento e poi collegare i risultati alla
documentazione in `docs/it`.

Gli strumenti sono volutamente separati:

- Bootlin Elixir in `docs/code-browser/`
- Kythe in `docs/kythe-browser/`
- Sourcebot in `docs/sourcebot-browser/`

Esiste anche uno strato comune per studenti e contributori:

```text
tools/code-browsing/
```

Questa cartella contiene script aggregati che chiamano gli script specifici dei
singoli strumenti. L'obiettivo e' pratico: chi vuole solo preparare o avviare
tutti i browser del codice non deve ricordare tre percorsi diversi.

### Provisioning comune

Il progetto usa container Docker per Sourcebot, Elixir e Kythe. Questo riduce i
vincoli sulla distribuzione Linux usata dallo studente, ma non elimina il
prerequisito Docker: Docker Engine o Docker Desktop devono essere gia'
installati e funzionanti sull'host.

Il progetto non installa Docker automaticamente. La scelta e' intenzionale:
l'installazione del daemon dipende dalla distribuzione, dai permessi
dell'utente, dal gruppo `docker`, da systemd o dal runtime usato nella macchina.
Uno script del repository non dovrebbe modificare questi aspetti di sistema.

Per controllare il prerequisito:

```sh
tools/code-browsing/check-docker.sh
```

Per preparare tutti gli strumenti:

```sh
tools/code-browsing/setup-all.sh
```

`setup-all.sh` esegue:

1. controllo Docker
2. setup Sourcebot, cioe' pull dell'immagine e controllo del repository Git
3. setup Elixir, cioe' clone di Bootlin Elixir, build immagine e database
4. setup Kythe e indicizzazione, cioe' download release, build immagine,
   compilazione strumentata e generazione delle serving tables

E' un comando volutamente pesante. Va usato alla prima preparazione o quando si
vuole ricreare l'ambiente completo.

Per avviare, fermare, riavviare e controllare tutti i servizi:

```sh
tools/code-browsing/start-all.sh
tools/code-browsing/status-all.sh
tools/code-browsing/restart-all.sh
tools/code-browsing/stop-all.sh
```

URL predefiniti:

```text
Sourcebot: http://127.0.0.1:3000
Elixir:    http://127.0.0.1:8080/alfred/workspace/source
Kythe API: http://127.0.0.1:9898
```

Graphify non e' ancora incluso in questi script. Per ora resta una voce della
roadmap: prima bisogna fare uno spike tecnico e decidere se produce davvero
mappe utili del codice e della documentazione. Solo dopo avra' senso aggiungere
container, setup e comandi aggregati.

La cartella contiene anche `docker-compose.yml`. Compose e' utile per chi
preferisce descrivere i tre server come servizi Docker, ma e' opzionale:
richiede il plugin `docker compose` o il binario `docker-compose`. Gli script
restano il percorso consigliato nella guida didattica perche' distinguono meglio
setup, reindex, start, stop e diagnosi degli errori.

### Elixir

Elixir e' il browser leggero e specifico per codice C/Linux. La guida si trova
in:

```text
docs/code-browser/README.md
```

Uso tipico:

```bash
docs/code-browser/setup-elixir.sh
docs/code-browser/reindex-elixir.sh
docs/code-browser/start-elixir.sh
docs/code-browser/status-elixir.sh
```

Elixir e' utile per leggere il codice in modo tradizionale: file, simboli,
riferimenti e navigazione dal browser. E' una buona scelta quando serve uno
strumento stabile e abbastanza semplice.

### Kythe

Kythe e' documentato in:

```text
docs/kythe-browser/README.md
```

Nel nostro progetto e' soprattutto un esperimento di backend semantico: riesce
a indicizzare il codice C e a produrre dati interrogabili, ma la release binaria
non fornisce una GUI pronta. Per questo non e' lo strumento consigliato agli
studenti per leggere Alfred giorno per giorno.

Resta utile per capire concetti piu' avanzati:

- extractor
- unita' `.kzip`
- graphstore
- serving tables
- cross-reference semantiche

Dal punto di vista pratico, Kythe puo' servire se vogliamo costruire strumenti
automatici sopra il codice. Per esempio puo' aiutare a generare mappe
`funzione -> chiamanti -> chiamati`, controllare se la documentazione cita
funzioni ancora esistenti, produrre viste mirate sui simboli o ridurre il lavoro
di esplorazione iniziale di un agente. Un agente puo' usare Kythe per
interrogare definizioni e riferimenti prima di aprire molti file, risparmiando
contesto e concentrando la lettura sui punti davvero rilevanti.

Questo non sostituisce la revisione del codice sorgente: prima di modificare
Alfred bisogna comunque leggere i file reali e lanciare i test. Kythe e' quindi
uno strumento di orientamento e automazione, non una fonte unica di verita'.

### Sourcebot

Sourcebot e' documentato in:

```text
docs/sourcebot-browser/README.md
```

E' una alternativa moderna per leggere e cercare il codice via web. Si avvia
con Docker:

```bash
docs/sourcebot-browser/start-sourcebot.sh
```

Poi si apre:

```text
http://127.0.0.1:3000
```

Comandi principali:

```bash
docs/sourcebot-browser/setup-sourcebot.sh
docs/sourcebot-browser/status-sourcebot.sh
docker logs -f alfred-sourcebot
docs/sourcebot-browser/restart-sourcebot.sh
docs/sourcebot-browser/stop-sourcebot.sh
```

Sourcebot usa un Docker named volume chiamato `alfred-sourcebot-data` per i dati
interni. Il volume non viene cancellato dallo stop normale, cosi' il database e
l'indice possono essere riusati al riavvio. Per cancellare tutto:

```bash
docs/sourcebot-browser/stop-sourcebot.sh
docker volume rm alfred-sourcebot-data
```

Query utili da provare nella UI:

```text
app_run
lang:c app_run
sym:app_run
path:app/src app_run
recursive_create_nested_dir
FILE_RELOCATED
```

Sourcebot e' probabilmente lo strumento piu' comodo se l'obiettivo e' cercare
rapidamente nel codice e aprire file dal browser. La sua code navigation
completa, pero', richiede una licenza Enterprise; per il nostro uso didattico
restano comunque molto utili file explorer, ricerca indicizzata, syntax
highlighting e filtri.

### Quale scegliere

Per la lettura quotidiana del codice:

- usa Sourcebot se vuoi una UI moderna e una ricerca comoda
- usa Elixir se vuoi uno strumento piu' leggero e specifico per il codice C
- usa Kythe solo per esperimenti avanzati su dati semantici e cross-reference

Gli studenti non devono partire da Kythe. Prima devono imparare a seguire il
codice con Elixir o Sourcebot, poi possono usare la documentazione italiana per
capire responsabilita', strutture dati e flusso degli eventi.

La sequenza umana consigliata e':

```text
leggi la documentazione -> cerca nel codice con Sourcebot/Elixir/rg
-> apri i file reali -> fai la modifica -> esegui la procedura pre-commit
```

I browser del codice aiutano nella fase di comprensione, non nella validazione
finale. Dopo ogni modifica restano necessari build e test descritti nella
sezione successiva.

## Come sono strutturati i test shell

I test Bash non sono script isolati senza struttura. Sono organizzati a livelli,
in modo che ogni parte abbia una responsabilita' chiara:

```text
Makefile target
-> tests/<suite>/run_all.sh
-> tests/<suite>/test_*.sh
-> tests/core/test_lib.sh
-> alfred + filesystem reale + log
```

Il target Makefile e' il punto di ingresso usato da sviluppatori, studenti e CI.
Per esempio:

```make
test-core:
	$(MAKE) all
	cd tests/core && bash run_all.sh
```

Questo significa:

1. ricompila il progetto con `make all`
2. entra nella directory `tests/core`
3. esegue il runner `run_all.sh`

Il cambio directory e' importante. Gli script usano path relativi come
`../../alfred`, `./events.log` e `./raw.log`; questi path funzionano perche' il
runner viene lanciato dalla directory della suite.

### Il ruolo di `run_all.sh`

Ogni suite shell ha un runner:

```text
tests/core/run_all.sh
tests/backend/run_all.sh
tests/scanner/run_all.sh
```

Il runner stampa un'intestazione leggibile, poi esegue tutti gli script
`test_*.sh`:

```bash
for test_script in test_*.sh; do
    echo "Running $test_script"
    bash "$test_script"
    echo ""
done
```

Questo meccanismo rende semplice aggiungere un test: se il nuovo file si chiama
`test_qualcosa.sh` e si trova nella directory giusta, il runner lo esegue
automaticamente. La suite core contiene anche `test_lib.sh`, quindi il runner
core lo salta esplicitamente:

```bash
if [[ "$test_script" == "test_lib.sh" ]]; then
    continue
fi
```

`test_lib.sh` non e' uno scenario. E' una libreria di funzioni condivise; se
venisse eseguita come test, non avrebbe un caso concreto da verificare.

I runner usano:

```bash
set -euo pipefail
```

Significato:

- `-e`: se un comando fallisce, lo script termina
- `-u`: usare una variabile non definita e' errore
- `-o pipefail`: una pipeline fallisce se fallisce uno dei comandi interni

Queste opzioni rendono i test piu' severi. Senza di esse, un errore potrebbe
passare inosservato e il runner potrebbe stampare `PASSED` anche se un comando
intermedio e' fallito.

### Il ruolo di un singolo `test_*.sh`

Un test core tipico ha questa forma:

```bash
#!/usr/bin/env bash

source ./test_lib.sh
trap cleanup EXIT

start_alfred_core

printf "rename\n" > "$TEST_ROOT/old.txt"
sleep 1
mv "$TEST_ROOT/old.txt" "$TEST_ROOT/new.txt"
sleep 1

assert_contains "FILE_CREATED path=.*/old.txt"
assert_contains "FILE_RENAMED from=.*/old.txt to=.*/new.txt"
assert_order "FILE_CREATED path=.*/old.txt" \
             "FILE_RENAMED from=.*/old.txt to=.*/new.txt"
```

Le fasi sono:

1. caricare la libreria con `source ./test_lib.sh`
2. registrare `cleanup` con `trap cleanup EXIT`
3. avviare Alfred sulla directory temporanea
4. produrre azioni reali sul filesystem
5. aspettare brevemente che inotify consegni gli eventi
6. controllare i log con gli assert

`source` esegue il file indicato nella shell corrente. Questo e' diverso da
`bash ./test_lib.sh`: se eseguissimo la libreria in un nuovo processo, le
funzioni definite dentro non sarebbero disponibili nel test chiamante.

`trap cleanup EXIT` dice alla shell: "quando questo script termina, anche in
caso di errore, chiama `cleanup`". Questo evita di lasciare Alfred in esecuzione
o directory temporanee sporche.

### Variabili condivise della libreria

`tests/core/test_lib.sh` definisce alcune variabili comuni:

```bash
ALFRED_BIN="${ALFRED_BIN:-../../alfred}"
TEST_ROOT="${TEST_ROOT:-/tmp/alfred_core_test}"
LOG_FILE="${LOG_FILE:-./events.log}"
ALFRED_PID=""
```

`ALFRED_BIN` indica il binario da avviare. La sintassi:

```bash
${ALFRED_BIN:-../../alfred}
```

significa: se la variabile `ALFRED_BIN` e' gia' definita nell'ambiente, usa quel
valore; altrimenti usa `../../alfred`. Questo permette di sovrascrivere il
binario da testare senza modificare lo script.

`TEST_ROOT` e' la directory temporanea osservata da Alfred. I test creano,
modificano, spostano e cancellano file dentro questa directory.

`LOG_FILE` e' il log semantico principale, di solito `events.log`. Alcuni test
leggono anche `raw.log` ed `errors.log`, che vengono preparati dalla libreria.

`ALFRED_PID` conserva il process id del processo Alfred avviato dal test. Serve
per fermarlo in modo controllato alla fine.

### `reset_env()`

`reset_env()` prepara un ambiente pulito:

```bash
reset_env() {
    rm -rf "$TEST_ROOT"
    mkdir -p "$TEST_ROOT"
    : > "$LOG_FILE"
    : > ./raw.log
    : > ./errors.log
}
```

Passaggi:

- rimuove la vecchia directory temporanea
- la ricrea vuota
- svuota `events.log`
- svuota `raw.log`
- svuota `errors.log`

La sintassi:

```bash
: > "$LOG_FILE"
```

usa il comando shell `:`. `:` non fa nulla e termina con successo. La
redirezione `>` pero' tronca il file, cioe' lo crea vuoto o lo svuota se esiste
gia'. E' un modo compatto per dire: "prepara un file vuoto".

### `start_alfred_core()`

`start_alfred_core()` pulisce l'ambiente e avvia Alfred:

```bash
start_alfred_core() {
    reset_env

    ALFRED_EVENT_ENGINE=core "$ALFRED_BIN" "$TEST_ROOT" >/dev/null 2>&1 &
    ALFRED_PID=$!

    sleep 1
}
```

La variabile `ALFRED_EVENT_ENGINE=core` forza il percorso core ufficiale. Oggi
e' anche il default, ma nei test resta esplicita per rendere chiaro quale
contratto stiamo verificando.

Il simbolo `&` avvia Alfred in background. Il test deve continuare a eseguire
comandi sul filesystem mentre Alfred resta attivo e osserva la directory.

`$!` contiene il PID dell'ultimo processo avviato in background. Lo salviamo in
`ALFRED_PID` per poterlo fermare dopo.

`>/dev/null 2>&1` manda stdout e stderr di Alfred fuori dall'output del test. I
test non leggono lo stdout del processo: leggono i file di log prodotti da
Alfred.

`sleep 1` lascia ad Alfred il tempo di inizializzare inotify e installare i
watch. Senza questa attesa, il test potrebbe creare file prima che il backend
sia pronto.

### `stop_alfred()`

`stop_alfred()` ferma Alfred se e' ancora in esecuzione:

```bash
stop_alfred() {
    if [[ -n "$ALFRED_PID" ]] && kill -0 "$ALFRED_PID" 2>/dev/null; then
        kill -INT "$ALFRED_PID"
        wait "$ALFRED_PID" || true
    fi
}
```

`[[ -n "$ALFRED_PID" ]]` controlla che la variabile non sia vuota.

`kill -0 "$ALFRED_PID"` non invia un vero segnale di terminazione. Serve a
chiedere al sistema: "questo processo esiste ancora ed e' raggiungibile?".

`kill -INT "$ALFRED_PID"` invia `SIGINT`, lo stesso tipo di interruzione che si
ottiene con `Ctrl-C`. Alfred puo' cosi' uscire in modo controllato.

`wait "$ALFRED_PID"` aspetta la fine del processo. `|| true` evita che un codice
di uscita non zero durante lo shutdown faccia fallire la pulizia del test.

### `cleanup()`

`cleanup()` e' la funzione registrata con `trap`:

```bash
cleanup() {
    stop_alfred
    rm -rf "$TEST_ROOT"

    if [[ "${ALFRED_KEEP_TEST_LOGS:-0}" != "1" ]]; then
        rm -f "$LOG_FILE" ./raw.log ./errors.log
    fi
}
```

Prima ferma Alfred, poi rimuove la directory temporanea. Infine cancella i log,
a meno che l'utente abbia chiesto di conservarli:

```bash
ALFRED_KEEP_TEST_LOGS=1 make test
```

Questa variabile e' utile quando un test fallisce e si vuole leggere con calma
`events.log`, `raw.log` ed `errors.log`.

### `fail_with_log()`

`fail_with_log()` centralizza il messaggio di errore:

```bash
fail_with_log() {
    local message="$1"

    echo "FAIL: $message"
    echo "----- events.log -----"
    cat "$LOG_FILE" || true
    exit 1
}
```

Ogni assert lo usa per stampare il motivo del fallimento e il contenuto di
`events.log`. Questo rende il debug piu' rapido: lo studente vede subito quali
eventi sono stati prodotti davvero.

### `assert_contains()`

`assert_contains()` verifica che almeno una riga di `events.log` corrisponda a
una regex:

```bash
assert_contains() {
    local pattern="$1"

    if ! grep -Eq "$pattern" "$LOG_FILE"; then
        fail_with_log "missing pattern: $pattern"
    fi
}
```

Esempio:

```bash
assert_contains "FILE_CREATED path=.*/old.txt"
```

Il test passa se `events.log` contiene una riga compatibile con quel pattern.
Se non la trova, fallisce e stampa il log.

### `assert_not_contains()`

`assert_not_contains()` fa il controllo opposto:

```bash
assert_not_contains() {
    local pattern="$1"

    if grep -Eq "$pattern" "$LOG_FILE"; then
        fail_with_log "unexpected pattern: $pattern"
    fi
}
```

Serve quando un evento non deve essere prodotto. Per esempio, un test puo'
verificare che un move+rename core non emetta due eventi legacy separati:

```bash
assert_not_contains "FILE_MOVED from=.*/src/old.txt to=.*/dst/new.txt"
assert_not_contains "FILE_RENAMED from=.*/src/old.txt to=.*/dst/new.txt"
```

### `assert_count()`

`assert_count()` verifica quante righe corrispondono a una regex:

```bash
assert_count() {
    local pattern="$1"
    local expected="$2"
    local actual

    actual=$(grep -Ec "$pattern" "$LOG_FILE" || true)

    if [[ "$actual" != "$expected" ]]; then
        fail_with_log "wrong count for pattern: $pattern (expected $expected, got $actual)"
    fi
}
```

`grep -Ec` conta le righe che corrispondono. `|| true` e' necessario perche'
`grep` restituisce errore quando non trova match; per un conteggio, zero match
e' un risultato valido, non un errore di shell.

### `assert_order()`

`assert_order()` controlla che un evento compaia prima di un altro:

```bash
assert_order() {
    local first="$1"
    local second="$2"
    local first_line
    local second_line

    first_line=$(grep -En "$first" "$LOG_FILE" | head -n 1 | cut -d: -f1 || true)
    second_line=$(grep -En "$second" "$LOG_FILE" | head -n 1 | cut -d: -f1 || true)

    if [[ -z "$first_line" || -z "$second_line" ]]; then
        fail_with_log "cannot check order: $first before $second"
    fi

    if (( first_line >= second_line )); then
        fail_with_log "wrong order: $first must appear before $second"
    fi
}
```

`grep -En` stampa le righe nel formato:

```text
numero:riga
```

`head -n 1` prende il primo match. `cut -d: -f1` tiene solo il numero di riga.
Alla fine la funzione confronta i numeri:

```bash
if (( first_line >= second_line )); then
```

Questa sintassi e' aritmetica Bash. Il test fallisce se il primo evento appare
dopo il secondo o sulla stessa riga.

`assert_order` non richiede che le due righe siano consecutive. Richiede solo
che l'ordine relativo sia quello atteso.

### Differenza fra test core e test backend

I test core usano `events.log` come contratto semantico ufficiale. Sono quelli
che verificano eventi come:

```text
FILE_CREATED
FILE_READY
DIR_RELOCATED
WATCH_RESYNC_END
```

I test backend possono leggere anche `raw.log`, perche' controllano eventi
inotify grezzi o diagnostica interna del backend. Per esempio:

```bash
grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_identity_match name=" \
    ./raw.log
```

Questa differenza e' importante: un test core deve proteggere cio' che
l'applicazione espone come semantica; un test backend puo' proteggere anche
dettagli tecnici necessari al resync, ai watch e alla diagnosi.

## Leggere le regex nei test shell

Molti test end-to-end di Alfred sono script Bash. Questi script non confrontano
quasi mai una riga di log completa carattere per carattere: cercano invece
pattern dentro `events.log` o `raw.log`. Il motivo e' pratico. Alcune parti del
log cambiano a ogni esecuzione:

- directory temporanee sotto `/tmp`
- watch descriptor, cioe' numeri `wd`
- cookie inotify
- ordine di alcuni eventi ravvicinati

Per questo i test usano espressioni regolari, spesso abbreviate in "regex". Una
regex descrive una famiglia di stringhe accettabili. Per esempio:

```text
DIR_CREATED path=.*/one$
```

non significa "cerca letteralmente i caratteri punto e asterisco". Significa:
"trova una riga che contenga `DIR_CREATED path=`, poi qualunque prefisso di
path, e che finisca con `/one`".

### Dove vengono usate

Nei test core gli helper principali sono in `tests/core/test_lib.sh`:

```bash
assert_contains "FILE_RENAMED from=.*/old.txt to=.*/new.txt"
assert_not_contains "core seq="
assert_order "FILE_CREATED path=.*/old.txt" "FILE_READY path=.*/old.txt"
```

Questi helper usano `grep -E`:

- `grep` cerca testo dentro un file
- `-E` abilita le regex estese, chiamate ERE
- `-q` rende la ricerca silenziosa e usa solo il codice di uscita
- `-n` stampa anche il numero di riga, utile per controllare l'ordine
- `-c` conta quante righe corrispondono al pattern

Esempi dal codice:

```bash
grep -Eq "$pattern" "$LOG_FILE"
grep -Ec "$pattern" "$LOG_FILE"
grep -En "$first" "$LOG_FILE"
```

`assert_contains` passa se almeno una riga di `events.log` corrisponde alla
regex. `assert_not_contains` passa se nessuna riga corrisponde. `assert_order`
trova la prima riga che corrisponde al primo pattern e la prima riga che
corrisponde al secondo pattern; poi verifica che la prima venga prima della
seconda.

Alcuni test backend controllano direttamente `raw.log`, perche' vogliono
verificare che il kernel abbia prodotto un evento inotify specifico:

```bash
grep -Eq "IN_MOVE_SELF .*path=.*/alfred_backend_test_identity_match name=" \
    ./raw.log
```

In questo caso non stiamo controllando un evento semantico utente. Stiamo
controllando la diagnostica grezza del backend.

### Regex e glob della shell non sono la stessa cosa

Un errore comune e' confondere regex e glob.

Il glob e' usato dalla shell per espandere nomi di file:

```bash
ls *.c
```

Qui `*.c` significa "tutti i file che finiscono con `.c`" nella directory
corrente.

La regex e' usata da `grep` per riconoscere testo:

```bash
grep -E "FILE_CREATED path=.*/a.txt" events.log
```

Qui `.*` significa "qualunque sequenza di caratteri" dentro una riga di log.
Nei test Alfred, quando parliamo di regex, parliamo dei pattern passati a
`grep`, non dei glob usati dalla shell per trovare file.

### Caratteri speciali usati nei test

| Simbolo | Significato in regex | Esempio dai test |
| --- | --- | --- |
| `.` | un qualunque singolo carattere | `FILE_CREATED path=.*/a.txt` |
| `*` | ripete il token precedente zero o piu' volte | `.*` |
| `.*` | qualunque sequenza di caratteri, anche vuota | `.*/one/two` |
| `[0-9]` | un carattere numerico da `0` a `9` | `wd=[0-9]+` |
| `+` | ripete il token precedente una o piu' volte | `[0-9]+` |
| `$` | fine della riga | `path=.*/one$` |
| `^` | inizio della riga | `^DIR_CREATED` |
| `|` | alternativa, cioe' "oppure" | `WATCH_REMOVED|DIR_DELETED` |
| `()` | raggruppa parti della regex | `(FILE|DIR)_CREATED` |
| `\` | rende letterale un carattere speciale | `app_t \*app` |

Il pattern piu' frequente e':

```text
.*
```

E' composto da:

- `.`: un carattere qualunque
- `*`: ripeti il carattere precedente zero o piu' volte

Quindi `.*` significa "qualunque testo". Nei test serve per ignorare parti
variabili, per esempio il prefisso assoluto della directory temporanea:

```bash
assert_contains "DIR_CREATED path=.*/one$"
```

Questa regex puo' riconoscere:

```text
DIR_CREATED path=/tmp/alfred_core_test/one
DIR_CREATED path=/tmp/qualunque-altro-root/one
```

Non riconosce invece:

```text
DIR_CREATED path=/tmp/alfred_core_test/one/two
```

perche' il `$` richiede che la riga finisca subito dopo `/one`.

### Perche' usare `$`

Il simbolo `$` e' importante quando un path puo' essere prefisso di un altro
path. Per esempio:

```bash
assert_contains "DIR_CREATED path=.*/one$"
assert_contains "DIR_CREATED path=.*/one/two$"
```

Senza `$`, il primo pattern potrebbe trovare anche:

```text
DIR_CREATED path=/tmp/alfred_core_test/one/two
```

Questo renderebbe il test piu' debole, perche' non dimostrerebbe davvero che
esiste un evento separato per `one`.

### Perche' usare `[0-9]+`

I watch descriptor inotify sono numeri assegnati dal kernel. Non possiamo
sapere in anticipo se un test vedra' `wd=1`, `wd=2` o un altro valore. Quindi i
test cercano:

```bash
assert_contains "WATCH_ADDED wd=[0-9]+ path=.*/a$"
```

`[0-9]` significa "una cifra". `+` significa "una o piu' ripetizioni". Quindi
`[0-9]+` riconosce:

```text
1
12
2048
```

Questa sintassi richiede `grep -E`. Con `grep` base, senza `-E`, il `+` non ha
lo stesso significato.

### Perche' quotare le regex

Nei test i pattern sono quasi sempre tra virgolette:

```bash
assert_contains "FILE_RELOCATED from=.*/src/old.txt to=.*/dst/new.txt"
```

Le virgolette servono a passare tutta la regex come un solo argomento alla
funzione Bash. Senza virgolette, gli spazi dividerebbero il pattern in piu'
argomenti e la shell potrebbe interpretare alcuni caratteri prima che arrivino
a `grep`.

Le virgolette doppie permettono anche l'espansione di variabili shell se
necessaria. Le virgolette singole sono piu' rigide: proteggono il testo senza
espandere variabili. Nei test Alfred usiamo spesso virgolette doppie per
coerenza con gli script esistenti.

### Come leggere alcuni pattern reali

```bash
assert_contains "FILE_RENAMED from=.*/old.txt to=.*/new.txt"
```

Lettura:

- cerca un evento `FILE_RENAMED`
- il vecchio path deve finire con `old.txt`
- il nuovo path deve finire con `new.txt`
- non importa quale sia la directory temporanea usata dal test

```bash
assert_contains "WATCH_RESYNC_FAILED wd=[0-9]+ path=.*/alfred_backend_test_identity_mismatch reason=IN_MOVE_SELF error=identity-mismatch"
```

Lettura:

- cerca una diagnostica backend `WATCH_RESYNC_FAILED`
- accetta qualunque numero di watch descriptor
- il path deve finire con `alfred_backend_test_identity_mismatch`
- il motivo deve essere `IN_MOVE_SELF`
- l'errore deve essere `identity-mismatch`

```bash
assert_order "WATCH_STALE wd=[0-9]+ path=.*/root reason=IN_MOVE_SELF" \
             "WATCH_RESYNC_BEGIN wd=[0-9]+ path=.*/root reason=IN_MOVE_SELF"
```

Lettura:

- entrambe le righe devono esistere in `events.log`
- la prima riga deve comparire prima della seconda
- il test non richiede che siano consecutive

### Regola pratica per scrivere nuove regex nei test

Quando scrivi un test shell:

- usa testo letterale per la parte semantica importante
- usa `.*` solo per parti davvero variabili
- usa `[0-9]+` per numeri assegnati dal kernel o dal runtime
- usa `$` quando vuoi fissare la fine del path
- non rendere il pattern troppo generico, altrimenti il test passa anche con
  eventi sbagliati
- non rendere il pattern troppo specifico su dettagli instabili, altrimenti il
  test diventa fragile

Un buon test non deve dimostrare che il log e' identico byte per byte. Deve
dimostrare che il contratto importante e' rispettato.

## Esempio di ricognizione prima di un micro-refactor

Questa sezione mostra un esempio concreto del metodo da usare prima di
modificare codice. L'esempio riguarda il backend inotify, ma il metodo vale per
qualunque parte della codebase.

Obiettivo dell'esempio:

```text
capire quali dipendenze da app_t restano nel backend inotify
```

La sequenza e':

```text
Kythe -> rg -> sed/editor -> documentazione -> proposta di modifica
```

### 1. Controllare il perimetro con Kythe

Se Kythe e' gia' stato configurato e il server e' avviato, si puo' chiedere
quali file vede dentro il modulo inotify:

```bash
source docs/kythe-browser/.work/env.sh
kythe --api=http://127.0.0.1:9898 ls 'kythe://alfred?path=modules/inotify/src'
```

Spiegazione:

- `source docs/kythe-browser/.work/env.sh` carica nell'ambiente le variabili
  generate dal setup Kythe, per esempio `KYTHE_DIR` e `PATH`. Senza questo
  passaggio la shell potrebbe non trovare il comando `kythe`.
- `kythe` e' il client CLI di Kythe.
- `--api=http://127.0.0.1:9898` dice al client dove si trova il server HTTP/API
  Kythe locale.
- `ls` e' il sottocomando che lista directory o file conosciuti dall'indice.
- `'kythe://alfred?path=modules/inotify/src'` e' una URI Kythe:
  - `kythe://` e' lo schema
  - `alfred` e' il corpus, cioe' il nome logico del progetto indicizzato
  - `?path=modules/inotify/src` restringe la richiesta a quel path
- Le virgolette singole proteggono `?` dalla shell. Senza virgolette, alcuni
  caratteri possono essere interpretati dalla shell invece che passati a Kythe.

Output atteso:

```text
inotify_adapter.c
inotify_backend.c
watch_manager.c
watcher.c
```

Questo comando non decide cosa modificare. Serve solo a verificare che l'indice
veda il perimetro giusto. Se Kythe non e' aggiornato o non risponde, si puo'
continuare con `rg`, ma non bisogna basare decisioni su un indice vecchio.

### 2. Cercare pattern architetturali con `rg`

Durante i micro-refactor del backend, per trovare le dipendenze residue da
`app_t` si usava:

```bash
rg -n "app->|app_t \*app|config_set_event_engine|inotify_backend_context|inotify_backend_" \
  modules/inotify/src/inotify_backend.c \
  modules/inotify/include/inotify_backend.h \
  app/src/app.c
```

`rg` significa ripgrep. E' uno strumento di ricerca testuale molto veloce, piu'
adatto di `grep -R` su codebase grandi.

Opzioni usate:

- `-n`: mostra il numero di riga accanto a ogni match. Questo rende immediato
  aprire il file nel punto giusto.

La stringa tra virgolette e' una espressione regolare. Il carattere `|` significa
"oppure". Quindi questa ricerca trova qualunque riga che contenga uno dei
pattern indicati.

Pattern usati:

- `app->`
  - trova accessi diretti ai campi di `app_t`
  - esempio: `app->config`, `app->inotify`, `app->logger`
  - utile per capire dove un modulo vede ancora troppo stato applicativo

- `app_t \*app`
  - trova firme o dichiarazioni che ricevono ancora un puntatore ad `app_t`
  - `\*` significa asterisco letterale
  - in regex, `*` da solo ha un significato speciale: "ripeti il token
    precedente"; per cercare proprio il carattere `*` bisogna scrivere `\*`
  - esempio trovato: `int inotify_backend_poll(app_t *app, ...)`

- `config_set_event_engine`
  - trova il punto che valida `ALFRED_EVENT_ENGINE` e la chiave
    `event_engine=...` nei file di configurazione
  - oggi non salva piu' un modo runtime: accetta solo `core` e rifiuta valori
    storici come `shadow`
  - e' importante perche' l'accettazione o il rifiuto di un valore
    `ALFRED_EVENT_ENGINE` appartiene alla configurazione applicativa, non alla
    semantica del backend inotify

- `inotify_backend_context`
  - trova il context ristretto passato al backend
  - serve a verificare che il backend riceva solo runtime, configurazione e
    logger, non l'intero `app_t`

- `inotify_backend_`
  - trova funzioni e tipi pubblici del backend inotify
  - utile per vedere il confine API del modulo

I file passati dopo la regex restringono la ricerca:

```text
modules/inotify/src/inotify_backend.c
modules/inotify/include/inotify_backend.h
app/src/app.c
```

Questo evita rumore. Invece di cercare in tutto il repository, si cercano solo i
file coinvolti dal problema:

- implementazione del backend
- header pubblico del backend
- applicazione che chiama il backend

### 3. Leggere il codice reale con `sed`

Dopo `rg`, bisogna leggere il codice reale intorno ai match. Per esempio:

```bash
sed -n '250,390p' modules/inotify/src/inotify_backend.c
```

Spiegazione:

- `sed` e' uno stream editor. Qui lo usiamo solo per stampare un intervallo di
  righe.
- `-n` disabilita la stampa automatica di tutte le righe.
- `'250,390p'` significa:
  - parti dalla riga `250`
  - arriva alla riga `390`
  - `p` significa print, cioe' stampa
- Il path finale e' il file da leggere.

Questo comando non modifica il file. Serve a leggere una finestra precisa senza
aprire tutto il sorgente.

Esempio di uso:

```text
rg trova un match a riga 358
sed legge da 250 a 390
lo sviluppatore vede tutto il contesto della funzione
```

Per modifiche reali, dopo aver identificato il blocco, si puo' usare l'editor
abituale. `sed` qui e' solo uno strumento di lettura.

### 4. Leggere la documentazione collegata

Prima di proporre una modifica architetturale, bisogna confrontare il codice con
la documentazione esistente. Per il refactor backend/core:

```bash
sed -n '185,375p' docs/it/15-todo-switch-core.md
```

Il significato del comando e' lo stesso visto sopra: stampa solo le righe da
`185` a `375`.

Questo passaggio evita un errore comune: modificare il codice dimenticando una
decisione gia' discussa. Nel nostro caso, `15-todo-switch-core.md` contiene la
mappa delle dipendenze residue, la strategia a micro-refactor e la distinzione
tra backend core e vecchio ponte legacy ormai rimosso.

### 5. Formulare la proposta

Dopo questi passaggi, la proposta deve essere piccola e verificabile.

Esempio storico, non piu' presente nel codice corrente, ma valido per capire la
forma di un micro-refactor:

```text
Durante la migrazione, dentro `inotify_backend_poll()`, il passo era:
app->config.event_engine_mode
con:
ctx.config->event_engine_mode

```

Motivo:

- `event_engine_mode` era configurazione e poteva passare dal context
- il passo non cambiava API pubblica
- il passo non cambiava comportamento osservabile
- oggi quel debito e' stato chiuso: il poll path non legge piu'
  `event_engine_mode`, il campo e' stato rimosso da `config_t` e il backend non
  contiene piu' un ponte legacy

Questa e' la forma desiderata di una ricognizione tecnica: non basta trovare
righe con `rg`; bisogna interpretarle, leggere il contesto e collegarle alla
direzione architetturale documentata.

## Procedura manuale prima di un commit

Ogni modifica al codice deve essere verificata con una procedura riproducibile
anche senza strumenti di AI. L'obiettivo e' permettere a studenti e
contributori di fare gli stessi controlli manualmente, con gli stessi criteri
usati durante lo sviluppo assistito.

La sequenza standard e':

```bash
git diff --check
make
make test
make test-backend-diagnostics
make test-scanner
make test-watcher
```

Questi comandi non sono intercambiabili: l'ordine serve a controllare prima i
problemi piu' semplici, poi il comportamento core e infine la diagnostica
backend che non fa parte dello stream semantico. `make test-scanner` e'
separato perche' verifica il componente di attraversamento filesystem, non il
runtime inotify -> core. `make test-watcher` e' ancora piu' mirato: controlla la
tabella `wd -> path` e lo stato di affidabilita' dei watch senza avviare Alfred.

### 1. `git diff --check`

Questo comando controlla il diff prima ancora di compilare.

Serve a trovare problemi come:

- spazi finali a fine riga
- righe vuote con spazi
- whitespace problematico introdotto dalla patch

E' il controllo piu' economico. Se fallisce, correggi subito il diff e non
passare alla build: non ha senso spendere tempo sui test se la patch contiene
gia' difetti meccanici.

### 2. Primo `make`

Il primo `make` costruisce la build normale del progetto:

```bash
make
```

Questa build e' core-only: non compila `events.c` e `move_cache.c`, perche'
questi file appartengono al confronto legacy/shadow. E' la build da considerare
predefinita.

Questo passo risponde alla domanda:

```text
il progetto compila nella configurazione normale?
```

Se fallisce, fermati. Prima bisogna correggere errori di compilazione, warning
trattati come errori, include mancanti, firme incoerenti o problemi di linking.

### 3. `make test`

Il comando:

```bash
make test
```

ricostruisce il binario core-only ed esegue la suite in:

```text
tests/core/
```

Questa suite avvia Alfred reale, usa il backend inotify reale e controlla lo
stream semantico prodotto dal core. Non e' un test unitario isolato: e' il test
end-to-end del percorso core.

Questo passo risponde alla domanda:

```text
il percorso ufficiale inotify -> raw -> core -> events.log funziona ancora?
```

E' importante eseguirlo prima dei funzionali storici perche' il core e' il
runtime ufficiale di default.

`make test-core` resta disponibile come nome esplicito della stessa suite.

### 4. `make test-backend-diagnostics`

Il comando:

```bash
make test-backend-diagnostics
```

ricostruisce il binario core-only ed esegue la suite in:

```text
tests/backend/
```

Questa suite controlla diagnostica del backend inotify. Per esempio verifica
che la creazione di una directory aggiunga un watch (`WATCH_ADDED`) e che la
rimozione di una directory osservata pulisca il watch (`WATCH_REMOVED`). Queste
righe sono log diagnostici: aiutano a capire se il backend mantiene
correttamente la tabella dei watch, ma non sono eventi semantici del core.

Questo passo risponde alla domanda:

```text
il backend inotify mantiene correttamente il proprio stato interno?
```

### 5. `make test-scanner`

Il comando:

```bash
make test-scanner
```

esegue i test del componente `fs_scanner`.

Nel primo step dello scanner, il test costruisce un piccolo albero:

```text
root/
    a/
        b/
        file.txt
    c/
    pipe.fifo
    volatile/
    link_to_a -> a/
```

Poi verifica che lo scanner:

- emetta la root
- emetta le directory `a`, `a/b` e `c`
- non emetta il file, perche' la configurazione iniziale e' directory-only
- non segua il symlink, perche' `follow_symlinks` e' disabilitato di default
- rispetti `emit_root=0`
- rispetti `max_depth=1`
- si fermi dopo `max_entries=2`
- emetta il file quando `include_files=1`
- emetta il symlink come entry `FS_SCAN_SYMLINK` quando `include_symlinks=1`
- emetta la FIFO come entry `FS_SCAN_OTHER` quando `include_other=1`
- continui se una directory figlia sparisce tra emissione e discesa ricorsiva

Questo target serve alla futura progettazione resync e indicizzazione. Non
sostituisce `make test` o `make test-backend-diagnostics`.

### 6. `make test-watcher`

Il comando:

```bash
make test-watcher
```

compila ed esegue un test C diretto in:

```text
tests/watcher/
```

Questa suite verifica la watcher table, cioe' la struttura che il backend usa
per tradurre un `wd` in path e, da ora, per ricordare se quel mapping e'
affidabile. Non usa inotify reale e non produce log semantici.

Il primo test controlla che:

- `watcher_store()` renda lo slot `WATCHER_STATE_VALID`
- `watcher_store_identity()` salvi anche identita' `st_dev/st_ino`
- `watcher_set_state()` possa passare a `WATCHER_STATE_STALE`
- `watcher_set_state()` possa passare a `WATCHER_STATE_RESYNCING`
- `watcher_remove()` riporti lo slot a `WATCHER_STATE_REMOVED`
- `WATCHER_STATE_REMOVED` non venga impostato su uno slot ancora attivo
- `watcher_count_state()` conti solo watch attivi nello stato richiesto
- `watcher_foreach_state()` visiti solo watch attivi nello stato richiesto e
  rispetti lo stop anticipato della callback

Questo serve alla futura gestione `IN_MOVE_SELF`: prima fissiamo il contratto
della struttura dati, poi colleghiamo gli eventi critici alla policy di resync.

### 7. Nessun `test-legacy-shadow`

Il target `make test-legacy-shadow` e la variante
`ENABLE_LEGACY_SHADOW=1` sono stati rimossi dal Makefile. I test funzionali
storici in `tests/functional/` restano nel repository come memoria della fase
legacy/shadow, ma non sono piu' una verifica corrente.

Da questo punto della migrazione i controlli diagnostici utili sono stati
migrati in `tests/backend/` e il contratto semantico ufficiale vive in
`tests/core/`.

Per evitare uso accidentale, `tests/lib/test_lib.sh` fallisce subito con un
messaggio esplicito se qualcuno prova a lanciare la vecchia suite funzionale.

### Cosa fare se un comando fallisce

Non proseguire meccanicamente. La regola e':

```text
primo errore -> fermarsi -> capire -> correggere -> rilanciare la sequenza
```

Esempi:

- se fallisce `git diff --check`, correggi whitespace o patch
- se fallisce `make`, correggi compilazione o linking
- se fallisce `make test`, analizza il comportamento semantico del core
- se fallisce `make test-backend-diagnostics`, controlla log diagnostici,
  watch descriptor e manutenzione della tabella dei watch
- se fallisce `make test-scanner`, controlla attraversamento directory,
  symlink, limiti e callback dello scanner

Per modifiche solo documentali, questa sequenza completa puo' essere eccessiva:
in quel caso almeno `git diff --check` resta obbligatorio. Se pero' la
documentazione descrive codice, test, Makefile o comportamento runtime, conviene
comunque eseguire i comandi rilevanti per evitare di documentare qualcosa di non
piu' vero.

### Conservare i log dei test

Di default le suite in `tests/core/` e `tests/backend/` puliscono i log alla fine
di ogni test. Questo evita di sporcare la working tree durante lo sviluppo
normale.

Quando pero' devi analizzare un fallimento puo' essere utile conservare:

```text
raw.log
events.log
errors.log
```

Per farlo, esegui il test con:

```bash
ALFRED_KEEP_TEST_LOGS=1 make test
```

oppure:

```bash
ALFRED_KEEP_TEST_LOGS=1 make test-backend-diagnostics
```

La variabile `ALFRED_KEEP_TEST_LOGS=1` viene letta dagli script di supporto dei
test. Quando e' impostata a `1`, la funzione di cleanup ferma Alfred e rimuove
la directory temporanea del test, ma non cancella i file `.log` generati nella
directory della suite.

Dopo `make test`, i log possono trovarsi in:

```text
tests/core/raw.log
tests/core/events.log
tests/core/errors.log
```

Dopo `make test-backend-diagnostics`, i log possono trovarsi in:

```text
tests/backend/raw.log
tests/backend/events.log
tests/backend/errors.log
```

Significato dei file:

- `raw.log`: eventi grezzi letti dal backend inotify
- `events.log`: eventi semantici del core e diagnostica backend come
  `WATCH_ADDED` o `WATCH_REMOVED`
- `errors.log`: errori applicativi o diagnostica di fallimento

Questa modalita' e' pensata per debugging manuale e per GitHub Actions. Nel
workflow CI la variabile viene impostata automaticamente, cosi' se un test
fallisce lo step finale puo' caricare i log come artifact.

Dopo averli letti localmente, puoi cancellarli con:

```bash
rm -f tests/core/*.log tests/backend/*.log
```

I log sono ignorati da Git e non devono essere committati.

## Test funzionali

Il progetto contiene test in:

```text
tests/functional/
```

`make test` non esegue piu' questi test: ora punta alla suite core ufficiale.
Per studiarli durante un audit storico:

```bash
tests/functional/README.md
```

I test funzionali verificano il comportamento del programma dall'esterno.
Per esempio possono creare, spostare o cancellare file e poi controllare che il
programma abbia registrato gli eventi corretti.

Nota importante: i test funzionali storici sono nati quando lo stream legacy era
ancora il riferimento. Alcuni script possono quindi cercare eventi o diagnostica
che non rappresentano piu' il contratto semantico ufficiale. Il percorso
supportato per il prodotto finale e' la suite core.

La descrizione dettagliata degli scenari, con operazioni filesystem ed eventi
attesi nei log, e' raccolta in
[Scenari di test](14-scenari-test.md).

## Test core end-to-end

La suite core end-to-end si trova in:

```text
tests/core/
```

Si esegue con:

```bash
make test-core
```

Il comando storico `make test` e' ora un alias di questa suite.

Questi test avviano Alfred con:

```text
ALFRED_EVENT_ENGINE=core
```

Il target `make test-core` ricostruisce prima il binario core-only. Oggi questa
e' l'unica variante di build supportata dal Makefile.

e verificano lo stream semantico ufficiale plain prodotto dal core. Non cercano
righe `core seq=...`, perche' quelle appartenevano allo shadow mode storico.

Importante: `make test-core` non e' un test unitario isolato del solo core.
Esegue Alfred reale su una directory temporanea, usa il kernel inotify reale,
passa dal backend inotify e controlla l'`events.log` prodotto dal core. E'
quindi gia' il test end-to-end del percorso core; il nome serve solo a
distinguerlo dai funzionali storici legacy-shadow.

La copertura iniziale include scenari base e alcuni casi semantici critici:

- `test_create_dir.sh`: verifica `DIR_CREATED`
- `test_create_file.sh`: verifica `FILE_CREATED`, `FILE_MODIFIED` e
  `FILE_READY`
- `test_delete_dir.sh`: verifica `DIR_CREATED` e `DIR_DELETED`, senza
  promuovere `WATCH_REMOVED` a evento semantico
- `test_delete_file.sh`: verifica `FILE_CREATED`, `FILE_MODIFIED`,
  `FILE_READY` e `FILE_DELETED`
- `test_modify_file.sh`: verifica che una seconda scrittura produca una nuova
  coppia `FILE_MODIFIED` / `FILE_READY` senza un secondo `FILE_CREATED`
- `test_move_dir.sh`: verifica `DIR_MOVED` quando cambia il contenitore ma il
  nome della directory resta lo stesso
- `test_move_file.sh`: verifica `FILE_MOVED` quando cambia la directory ma il
  nome del file resta lo stesso
- `test_move_rename_dir.sh`: verifica `DIR_RELOCATED` invece di `DIR_MOVED` +
  `DIR_RENAMED`
- `test_move_rename_file.sh`: verifica `FILE_RELOCATED` invece di
  `FILE_MOVED` + `FILE_RENAMED`
- `test_recursive_create_nested_dir.sh`: verifica il recupero dei
  `DIR_CREATED` in una creazione rapida stile `mkdir -p`
- `test_rename_dir.sh`: verifica `DIR_RENAMED` quando cambia solo il nome della
  directory nello stesso contenitore
- `test_rename_file.sh`: verifica `FILE_RENAMED` dopo la fase
  `CREATED/MODIFIED/READY`

Questa suite fissa il comportamento end-to-end futuro del percorso core senza
cambiare subito i test funzionali storici.

## Test backend diagnostics

```text
tests/backend/
```

Si esegue con:

```bash
make test-backend-diagnostics
```

Questi test avviano Alfred in `ALFRED_EVENT_ENGINE=core`, ma non controllano il
contratto semantico utente. Controllano invece la diagnostica del backend
inotify che oggi viene ancora scritta in `events.log` tramite `logger_event()`.

La copertura iniziale include:

- `test_watch_added_create_dir.sh`: crea una directory e verifica che il backend
  aggiunga un watch diagnostico con `WATCH_ADDED`
- `test_watch_removed_delete_dir.sh`: crea e rimuove una directory osservata e
  verifica che il backend registri `WATCH_REMOVED`
- `test_recursive_slow_watch_tree.sh`: crea lentamente `a`, `a/b` e `a/b/c` e
  verifica che ogni directory riceva il proprio `WATCH_ADDED`

Questi test sono separati dalla suite core per evitare un equivoco: una riga
`WATCH_ADDED` e' utile per il manutentore del backend, ma non e' un evento che
il core dovrebbe esporre come semantica filesystem.

La libreria `tests/core/test_lib.sh` include anche `assert_count`, usato quando
uno scenario deve fissare quante volte un evento puo' comparire. Per esempio,
nel test `modify_file` `FILE_CREATED` deve comparire una sola volta, mentre
`FILE_MODIFIED` e `FILE_READY` devono comparire due volte: una per la creazione
con scrittura iniziale e una per la modifica successiva.

### Perche' l'overflow resta fuori dalla suite iniziale

L'overflow della coda inotify non viene ancora trasformato in un test
end-to-end stabile della suite core. Non e' stato dimenticato: lo rimandiamo a
dopo lo switch completo al core perche' ha una natura diversa dagli scenari
base.

Gli scenari core attuali rispondono alla domanda:

```text
dato un evento filesystem normale, quale evento semantico produce Alfred?
```

L'overflow risponde invece a una domanda di recovery:

```text
se il backend perde affidabilita' perche' la coda kernel trabocca,
come ricostruiamo lo stato dell'albero osservato?
```

Renderlo riproducibile in modo affidabile in un test end-to-end e' difficile,
perche' dipende da timing, carico della macchina, velocita' del filesystem,
dimensione della coda kernel e capacita' del processo di drenare gli eventi.
Un test che prova solo a saturare inotify rischia quindi di essere fragile.

Quando il core sara' il percorso ufficiale, l'overflow dovra' essere progettato
come feature separata. Le opzioni da discutere sono:

- evento diagnostico backend
- evento semantico di tipo resync richiesto
- scan completo dell'albero con eventi sintetici
- combinazione di log diagnostico e procedura controllata di resync

Per questo, nella suite core iniziale l'overflow resta documentato come lavoro
futuro e non come test mancante.

## Test shadow storici

Durante l'integrazione del core e' esistito anche un tool diagnostico in:

```text
tests/shadow/compare_shadow_output.py
```

Questo tool non sostituisce i test ufficiali. Dopo la rimozione del dispatcher
legacy, la modalita' shadow non e' piu' una verifica ordinaria: il tool resta
utile solo come materiale storico e, dove possibile, come runner di scenari in
core mode. Durante la migrazione serviva a confrontare due percorsi interni:

```text
legacy dispatcher inotify -> events.log
core in shadow mode       -> events.log con prefisso core
```

Non usare questo comando come verifica corrente: oggi la forma storica senza
`--event-engine core` tenta di usare shadow mode e quindi fallisce.

Durante la fase storica di confronto, il tool stampava quattro sezioni:

- `Legacy`: eventi prodotti dal vecchio dispatcher
- `Core`: eventi prodotti dal core
- `Only in legacy`: eventi presenti solo nel vecchio percorso
- `Only in core`: eventi presenti solo nel nuovo percorso

Una differenza non era automaticamente un bug. Durante shadow mode poteva essere
una differenza attesa, per esempio quando il legacy emetteva `DIR_MOVED` piu'
`DIR_RENAMED` ma il core emetteva un solo `DIR_RELOCATED`.

`--strict` appartiene alla fase storica di confronto legacy/core:

```bash
python3 tests/shadow/compare_shadow_output.py create_file --strict
```

Per gli scenari correnti non bisogna piu' usare shadow come blocco di merge:
aggiungere o aggiornare invece test in `tests/core/` o `tests/backend/`.

Un esempio di scenario storico era:

```bash
python3 tests/shadow/compare_shadow_output.py recursive_create_nested_dir
```

Questo scenario riproduce una creazione rapida in stile `mkdir -p`. Serve a
studiare quali directory vengono notificate come `DIR_CREATED` e quali vengono
solo scoperte dopo, tramite aggiunta ricorsiva dei watch.

La vecchia opzione per conservare i log completi era:

```bash
python3 tests/shadow/compare_shadow_output.py recursive_create_nested_dir --keep-logs
```

I file vengono conservati in:

```text
tests/shadow/last-run/
```

In particolare:

- `events.log` contiene sia eventi semantici sia diagnostica backend come
  `WATCH_ADDED`
- `raw.log` contiene gli eventi grezzi arrivati davvero da inotify

Questa distinzione resta didatticamente utile per capire casi come `mkdir -p`,
ma la verifica corrente vive nella suite core e nella suite backend diagnostics.

La sola modalita' ancora utile per ispezione manuale e':

```bash
python3 tests/shadow/compare_shadow_output.py <scenario> --event-engine core
```

In questa forma il tool non confronta piu' legacy e core: avvia solo il runtime
ufficiale core e aiuta a ispezionare lo scenario. La verifica ufficiale resta
`make test` e `make test-backend-diagnostics`.

## Provare un file di configurazione

Alfred carica i default con `config_defaults()` e puo' poi applicare un file di
configurazione tramite la variabile d'ambiente `ALFRED_CONFIG`.

Esempio:

```bash
cat > /tmp/alfred.conf <<'EOF'
inotify_watch_mask=default,-IN_ATTRIB
EOF

ALFRED_CONFIG=/tmp/alfred.conf ./alfred /tmp/cartella-da-osservare
```

Questa variabile e' il modo supportato oggi per provare opzioni runtime senza
aggiungere ancora un parser CLI. La precedenza pratica e':

```text
config_defaults()
    -> ALFRED_CONFIG, se presente
    -> ALFRED_EVENT_ENGINE, se presente
```

Quindi il file caricato da `ALFRED_CONFIG` puo' cambiare opzioni come
`inotify_watch_mask`, `inotify_recursive`, `inotify_watcher_capacity` e i path
dei log. `ALFRED_EVENT_ENGINE` resta un override separato e viene validato dopo
il file per rifiutare esplicitamente valori vecchi come `shadow`.

Per debug conviene usare anche:

```bash
ALFRED_KEEP_TEST_LOGS=1 make test-backend-diagnostics
```

cosi' si possono ispezionare `raw.log`, `events.log` ed `errors.log` dopo uno
scenario che usa una configurazione temporanea.

## Provare il core come stream ufficiale

Il core e' lo stream ufficiale di default. Per forzare esplicitamente la stessa
modalita', usare l'override d'ambiente:

```bash
ALFRED_EVENT_ENGINE=core ./alfred /tmp/cartella-da-osservare
```

Il confronto shadow legacy/core non e' piu' riattivabile a runtime. Il vecchio
flusso:

```bash
ALFRED_EVENT_ENGINE=shadow ./alfred /tmp/cartella-da-osservare
```

fallisce per valore non valido. Questo evita un confronto finto: il dispatch
live legacy/core e' stato spento, quindi il percorso supportato e'
`event_engine=core`.

## Strumenti futuri per la documentazione dinamica

La guida [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
contiene scenari scritti come frame testuali. Questi frame servono gia' come
lettura guidata, ma sono anche preparazione per strumenti futuri.

Possibile comando futuro:

```bash
make docs-animations
```

Questo comando non esiste ancora. Quando verra' aggiunto, dovrebbe leggere gli
scenari animabili, produrre frame intermedi e generare output didattici come:

- SVG per singoli frame
- GIF o video per lezioni
- pagine HTML interattive

Fino a quel momento, gli scenari nel Markdown sono lo strumento ufficiale: vanno
aggiornati ogni volta che cambiano funzioni, strutture dati o flussi eventi.

In questa modalita' `events.log` usa il formato plain:

```text
FILE_CREATED path=...
FILE_MODIFIED path=...
FILE_READY path=...
```

Il prefisso `core seq=...` non compare perche' non stiamo piu' scrivendo un
secondo stream shadow. Il vecchio dispatcher semantico e' stato rimosso, quindi
le differenze rispetto al legacy restano solo materiale storico documentato.

Il runner storico degli scenari puo' ancora avviare Alfred in core mode per
ispezione manuale:

```bash
python3 tests/shadow/compare_shadow_output.py create_file --event-engine core
```

In questo caso il tool stampa una sola sezione:

```text
Core official
```

perche' `events.log` contiene lo stream plain del core e non contiene lo stream
legacy da confrontare.

## Stress test

Gli stress test sono in:

```text
tests/stress/
```

Servono per produrre molti eventi rapidamente e verificare che il programma non
si rompa sotto carico.

Esempi:

```text
stress_create.py
stress_move.py
stress_recursive.py
stress_rename.py
```

## clang-format

`clang-format` formatta automaticamente il codice.

Comando:

```bash
make format
```

Serve a evitare discussioni inutili su spazi, indentazione e stile.

Prima di usarlo su grandi porzioni di codice conviene avere una configurazione
condivisa, per esempio un file `.clang-format`.

## cppcheck

`cppcheck` e' uno strumento di analisi statica.

Comando:

```bash
make scan
```

"Analisi statica" significa che lo strumento legge il codice senza eseguirlo e
cerca problemi probabili.

Puo' trovare:

- variabili non inizializzate
- codice morto
- errori di memoria potenziali
- controlli mancanti

## clang-tidy

`clang-tidy` e' un analizzatore piu' avanzato.

Comando:

```bash
make tidy
```

Puo' controllare:

- bug probabili
- modernizzazione del codice
- stile
- performance
- leggibilita'

Anche qui, in futuro conviene aggiungere una configurazione condivisa.

## Cosa fare quando qualcosa fallisce

### La compilazione fallisce

Guarda la prima riga di errore reale, non solo l'ultima.

Spesso Make stampa alla fine:

```text
make: *** [...] Error 1
```

Quella non e' la causa. La causa e' alcune righe prima.

### Il linker fallisce

Cerca messaggi come:

```text
undefined reference to `nome_funzione`
```

Significa che una funzione e' stata dichiarata o chiamata, ma il linker non ha
trovato la sua implementazione.

### Il programma fa segmentation fault

Usa:

```bash
make gdb
```

Poi dentro GDB:

```text
run
backtrace
```

### Sanitizer segnala un errore

Leggi:

- tipo di errore
- file e riga
- stack trace

Correggi prima il primo errore: molti errori successivi possono essere effetti
secondari.

### Un test fallisce

Controlla:

- quale test e' fallito
- quale output era atteso
- quale output e' stato prodotto
- se ci sono log in `raw.log`, `events.log`, `errors.log`

I log sono output di runtime. Non devono essere versionati: `raw.log`,
`events.log`, `errors.log` e le varianti dentro `tests/functional/` vengono
riscritti dai test o dalle run manuali e sono ignorati da `.gitignore`.

## Strategia consigliata per studenti

Quando modifichi il codice:

1. compila con `make`
2. correggi warning o errori
3. esegui `make test`
4. se tocchi memoria o puntatori, usa anche `make valgrind`
5. se non capisci un crash, usa `make gdb`

Non aspettare di aver scritto molto codice prima di compilare. In C conviene
fare piccoli passi e verificare spesso.
