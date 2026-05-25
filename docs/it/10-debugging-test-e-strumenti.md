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
ALFRED_EVENT_RELOCATED
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
rg -n "app->|app_t \*app|event_engine_mode|inotify_backend_context|inotify_backend_" \
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

- `event_engine_mode`
  - trova dove il codice conserva ancora il valore `shadow` per rifiutarlo con
    un errore esplicito
  - e' importante perche' questa scelta appartiene alla configurazione
    applicativa, non alla semantica del backend inotify

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

Esempio storico, valido per capire la forma di un micro-refactor:

```text
Dentro inotify_backend_poll(), sostituire solo:
app->config.event_engine_mode
con:
ctx.config->event_engine_mode

```

Motivo:

- `event_engine_mode` era configurazione e poteva passare dal context
- il passo non cambiava API pubblica
- il passo non cambiava comportamento osservabile
- oggi quel debito e' stato chiuso: il poll path non legge piu'
  `event_engine_mode` e non contiene piu' un ponte legacy

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
```

Questi comandi non sono intercambiabili: l'ordine serve a controllare prima i
problemi piu' semplici, poi il comportamento core e infine la diagnostica
backend che non fa parte dello stream semantico.

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

### 5. Nessun `test-legacy-shadow`

Il target `make test-legacy-shadow` e la variante
`ENABLE_LEGACY_SHADOW=1` sono stati rimossi dal Makefile. I test funzionali
storici in `tests/functional/` restano nel repository come memoria della fase
legacy/shadow, ma non sono piu' la verifica ordinaria prima del commit.

Da questo punto della migrazione i controlli diagnostici utili sono stati
migrati in `tests/backend/` e il contratto semantico ufficiale vive in
`tests/core/`.

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

Per modifiche solo documentali, questa sequenza completa puo' essere eccessiva:
in quel caso almeno `git diff --check` resta obbligatorio. Se pero' la
documentazione descrive codice, test, Makefile o comportamento runtime, conviene
comunque eseguire i comandi rilevanti per evitare di documentare qualcosa di non
piu' vero.

## Test funzionali

Il progetto contiene test in:

```text
tests/functional/
```

`make test` non esegue piu' questi test: ora punta alla suite core ufficiale.
Per studiarli o rieseguirli manualmente durante un audit storico:

```bash
cd tests/functional
bash run_all.sh
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
righe `core seq=...`, perche' quelle appartengono allo shadow mode.

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

Esempio storico:

```bash
python3 tests/shadow/compare_shadow_output.py move_rename_dir
```

Il tool stampa quattro sezioni:

- `Legacy`: eventi prodotti dal vecchio dispatcher
- `Core`: eventi prodotti dal core
- `Only in legacy`: eventi presenti solo nel vecchio percorso
- `Only in core`: eventi presenti solo nel nuovo percorso

Una differenza non e' automaticamente un bug. Durante shadow mode puo' essere
una differenza attesa, per esempio quando il legacy emette `DIR_MOVED` piu'
`DIR_RENAMED` ma il core emette un solo `DIR_RELOCATED`.

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

fallisce con un errore esplicito. Questo evita un confronto finto: il dispatch
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
secondo stream shadow. Il vecchio dispatcher semantico non viene chiamato dal
loop principale, quindi le differenze rispetto al legacy vanno osservate
confrontando run separate oppure tornando al tool shadow.

Lo stesso runner diagnostico degli scenari puo' avviare Alfred in core mode:

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
