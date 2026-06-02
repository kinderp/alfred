# Scenari di test

Questo capitolo descrive gli scenari usati nei test del progetto. L'obiettivo
non e' solo sapere "quale comando eseguire", ma capire quali eventi ci aspettiamo
nel `raw.log` e nell'`events.log`.

I test sono divisi in tre famiglie:

- test core: verificano il core come stream semantico ufficiale plain
- test backend diagnostics: verificano log tecnici del backend, come i watch
- test storici: documentano la fase legacy/shadow e non sono verifica corrente

## File di log

Alfred scrive principalmente tre file:

```text
raw.log
events.log
errors.log
```

`raw.log` contiene eventi grezzi arrivati davvero da inotify. Esempio:

```text
IN_CREATE IN_ISDIR wd=1 path=/tmp/root name=dir
```

`events.log` contiene:

- eventi semantici core, per esempio `DIR_CREATED`
- diagnostica backend, per esempio `WATCH_ADDED`

Esempio:

```text
DIR_CREATED path=/tmp/root/dir
WATCH_ADDED wd=2 path=/tmp/root/dir
```

`errors.log` contiene errori e diagnostica di fallimento.

Gli esempi di log in questo capitolo sono schemi didattici. Non includono
timestamp, numeri di sequenza completi, tutti i `wd` reali o tutti i dettagli
del formato su disco. Valori come `wd=<root-wd>` e `cookie=<n>` cambiano a ogni
esecuzione. Anche l'ordine di alcune righe puo' variare quando il filesystem
produce eventi molto ravvicinati.

## Test funzionali

I test funzionali si trovano in:

```text
tests/functional/
```

`make test` non esegue piu' questa suite: ora punta al percorso core ufficiale.
Gli script non vanno lanciati come verifica corrente. Sono archivio storico:

```bash
tests/functional/README.md
```

Questi test usano `tests/lib/test_lib.sh`, avviano `alfred` su una directory
temporanea e cercano pattern nell'output. Il loro helper storico chiedeva
`ALFRED_EVENT_ENGINE=shadow`; oggi questo percorso fallisce intenzionalmente
per evitare confronti finti.

Importante: i test funzionali storici controllavano soprattutto il comportamento
legacy visibile in `events.log`. Se uno scenario descrive ancora comportamento
utente utile, va portato in `tests/core/` o `tests/backend/`.

## Test core end-to-end

I test core si trovano in:

```text
tests/core/
```

Si eseguono con:

```bash
make test-core
```

Questa suite avvia Alfred con `ALFRED_EVENT_ENGINE=core` e controlla solo lo
stream semantico ufficiale prodotto dal core:

```text
FILE_CREATED path=...
FILE_MODIFIED path=...
FILE_READY path=...
```

Non controlla `raw.log` come contratto stabile. `raw.log` e' importante per
diagnosi backend e per capire cosa arriva da inotify, ma eventi grezzi come
`IN_MODIFY`, `IN_CLOSE_WRITE`, `wd` e ordine di consegna possono essere piu'
sensibili al kernel, al filesystem e al timing. Per questo i test core fissano
il contratto semantico di Alfred.

Il nome `test-core` non significa "test unitario del solo core in memoria".
Questi test sono gia' end-to-end sul percorso core reale:

```text
filesystem reale
-> kernel inotify reale
-> backend inotify Alfred
-> alfred_raw_event_t
-> core Alfred
-> events.log ufficiale core
```

La parola "core" indica quale stream semantico viene considerato ufficiale, non
quali livelli del programma vengono saltati. Per questo non serve introdurre
ora una suite separata chiamata `test-functional-core`: sarebbe quasi un doppione
di `make test-core`.

Gli eventi raw Alfred (`alfred_raw_event_t`) hanno valore di test, ma sono piu'
adatti a test unitari dell'adapter o del core, dove possiamo costruire input
controllati senza dipendere dal timing del kernel. Gli scenari end-to-end in
`tests/core/` verificano invece il risultato finale che useranno le
applicazioni.

Gli script shell usano regex per cercare eventi nei log senza dipendere da path
temporanei, `wd` o altri valori instabili. La sintassi dei pattern usati da
`assert_contains`, `assert_not_contains`, `assert_count`, `assert_order` e
`grep -E` e' spiegata in `docs/it/10-debugging-test-e-strumenti.md`, nella
sezione "Leggere le regex nei test shell".

## Audit storico pre-rimozione dello shadow legacy

Lo switch e' stato totale: lo shadow legacy non e' una modalita' conservata nel
prodotto finale. Prima di cancellarlo, abbiamo usato i test esistenti come
inventario. L'idea era:

```text
prima si dimostra che il core copre il comportamento utile,
poi si rimuove il confronto legacy,
non il contrario.
```

La suite legacy/shadow ha ancora valore solo come fotografia storica della
migrazione, ma non definisce piu' la semantica target. La semantica target e'
quella della suite core end-to-end.

I criteri usati nell'audit sono tre:

- se uno scenario rappresenta comportamento utente, deve avere un test core
- se uno scenario controlla solo diagnostica backend, non deve bloccare lo
  switch semantico
- se uno scenario documenta una differenza legacy/core, deve essere spiegato
  come comportamento storico, non come requisito futuro

## Mappa storica funzionali legacy e core

Questa mappa documenta l'audit fatto prima della rimozione del legacy. I due
gruppi di test non avevano lo stesso scopo:

- `tests/functional/` nasce quando il dispatcher legacy era lo stream
  principale; oggi resta come memoria storica e non ha piu' un target Makefile
  ufficiale
- `tests/core/` nasce per fissare lo stream semantico ufficiale del core e oggi
  viene eseguito da `make test` e `make test-core` in build core-only, usando
  comunque filesystem, inotify e backend reali

Per questo la domanda non era: "quale suite e' migliore?". La domanda corretta
era:

```text
quale scenario deve restare test end-to-end del prodotto finale,
quale deve restare solo confronto legacy/shadow,
e quale e' gia' coperto meglio dalla suite core?
```

Mappa corrente:

| Scenario | Test funzionale legacy | Test core end-to-end | Stato |
| --- | --- | --- | --- |
| create file | `tests/functional/test_create_file.sh` | `tests/core/test_create_file.sh` | Coperto da entrambe; core controlla anche `FILE_MODIFIED` e `FILE_READY` quando il file viene scritto. |
| create directory | `tests/functional/test_create_dir.sh` | `tests/core/test_create_dir.sh` | Coperto da entrambe; funzionale controlla anche diagnostica `WATCH_ADDED`. |
| delete file | `tests/functional/test_delete_file.sh` | `tests/core/test_delete_file.sh` | Coperto da entrambe; il funzionale storico controlla soprattutto la creazione, il core controlla anche delete e ordine semantico. |
| delete directory | `tests/functional/test_delete_dir.sh` | `tests/core/test_delete_dir.sh` | Coperto da entrambe; core verifica che `WATCH_REMOVED` non sia evento semantico. |
| rename file | `tests/functional/test_rename_file.sh` | `tests/core/test_rename_file.sh` | Coperto da entrambe; core controlla anche write lifecycle precedente al rename. |
| rename directory | `tests/functional/test_rename_dir.sh` | `tests/core/test_rename_dir.sh` | Coperto da entrambe; core esclude esplicitamente `DIR_MOVED` e `DIR_RELOCATED`. |
| move file | `tests/functional/test_move_file.sh` | `tests/core/test_move_file.sh` | Coperto da entrambe; core controlla anche che non sia classificato come rename/relocated. |
| move directory | non presente nei funzionali storici | `tests/core/test_move_dir.sh` | Coperto solo dal core; utile mantenerlo come contratto semantico ufficiale. |
| move + rename file | `tests/functional/test_move_rename_file.sh` | `tests/core/test_move_rename_file.sh` | Coperto da entrambe, ma con semantica diversa: legacy emette `FILE_MOVED + FILE_RENAMED`, core emette un solo `FILE_RELOCATED`. |
| move + rename directory | `tests/functional/test_move_rename_dir.sh` | `tests/core/test_move_rename_dir.sh` | Coperto da entrambe, ma con semantica diversa: legacy emette `MOVED + RENAMED`, core emette un solo `RELOCATED`. |
| recursive slow directory tree | `tests/functional/test_recursive.sh` | non identico | Lo scenario diagnostico watch e' stato migrato in `tests/backend/test_recursive_slow_watch_tree.sh`; il funzionale storico resta legacy/shadow. |
| recursive fast nested directory | non presente nei funzionali storici | `tests/core/test_recursive_create_nested_dir.sh` | Coperto dal core; verifica raw sintetici e recupero delle directory create prima dei watch. |
| modify / close-write | non presente nei funzionali storici | `tests/core/test_modify_file.sh` | Coperto solo dal core; fissa `FILE_MODIFIED` e `FILE_READY`. |
| engine shadow invalido | non presente nei funzionali storici | `tests/core/test_invalid_event_engine_shadow.sh` | Coperto dal core; verifica che `ALFRED_EVENT_ENGINE=shadow` sia un valore non valido e non faccia fallback a core. |

### Risultato dell'audit storico

La copertura core e' stata sufficiente per rimuovere lo shadow legacy, con una
distinzione importante: la suite core copre il contratto semantico finale,
mentre alcuni test funzionali storici coprivano anche log diagnostici del
backend.

Scenari semantici gia' coperti dal core:

- create/delete file
- create/delete directory
- rename file/directory
- move file/directory
- move+rename file/directory come singolo evento `RELOCATED`
- modify e close-write come `FILE_MODIFIED` e `FILE_READY`
- recursive fast nested directory con recupero tramite raw sintetici

Scenari legacy che non devono sopravvivere come contratto finale:

- doppia emissione legacy `MOVED + RENAMED` quando cambia sia directory sia nome
- dipendenza da `events.c` per costruire eventi semantici finali
- uso di `move_cache.c` nel modulo inotify legacy

Scenari diagnostici ricollocati o archiviati:

- controllo di `WATCH_ADDED`
- controllo di `WATCH_REMOVED`
- controllo dell'aggiunta progressiva dei watch in una creazione ricorsiva lenta
- smoke test del vecchio percorso legacy/shadow

Questa distinzione e' essenziale per gli studenti: un test puo' essere utile
durante una migrazione senza essere parte del contratto futuro del programma.
Nel nostro caso, `WATCH_ADDED` e `WATCH_REMOVED` descrivono il lavoro interno
del backend inotify; non sono eventi semantici prodotti dal core per chi usa
Alfred.

### Decisione storica sugli scenari legacy

Questa tabella spiega perche' `make test` ora punta al percorso core. Classifica
che cosa resta nel prodotto e che cosa e' solo memoria diagnostica della
migrazione.

| Gruppo | Decisione futura | Motivo |
| --- | --- | --- |
| Create/delete/rename file e directory | Tenere nel core | Sono comportamento utente fondamentale. |
| Move file e move directory | Tenere nel core | Fissano la differenza tra move e rename. |
| Move + rename file/directory | Tenere nel core | Il core deve emettere un solo `RELOCATED`. |
| Modify / close-write | Tenere nel core | Fissa `FILE_MODIFIED` e `FILE_READY`. |
| Recursive fast nested directory | Tenere nel core | Protegge il recupero con raw sintetici. |
| `WATCH_ADDED` / `WATCH_REMOVED` nei funzionali | Tenere come diagnostica o test mirati | Sono log backend, non eventi semantici utente. |
| Doppia emissione legacy `MOVED + RENAMED` | Archiviare con lo shadow | Documenta il comportamento storico, non il target. |
| Shadow mode rimosso | Tenere nel core | Protegge il contratto di configurazione runtime. |

Conclusione operativa:

- la suite `make test` / `make test-core` e' oggi la sorgente piu' precisa per
  il comportamento end-to-end futuro del percorso core
- la suite funzionale storica resta solo memoria della fase legacy/shadow; dopo
  lo spegnimento del dispatch legacy nel poll path non e' piu' una verifica
  ordinaria
- il vecchio target `make test-legacy-shadow` e' stato rimosso; i controlli
  storici su `WATCH_ADDED`, `WATCH_REMOVED` e differenze legacy/core non sono
  piu' verifica ordinaria; i controlli watch utili sono gia' in
  `tests/backend/`
- `tests/functional/test_move_rename_file.sh` resta materiale storico per
  mostrare la vecchia doppia emissione `FILE_MOVED + FILE_RENAMED`, non una
  verifica corrente

## Test backend diagnostics

Prima di archiviare la suite funzionale storica, abbiamo salvato gli scenari che
non sono semantica core ma descrivono la salute del backend inotify. Questi test
non vivono nella suite core, perche' non controllano il contratto utente di
Alfred. Vivono in una suite separata:

```text
tests/backend/
make test-backend-diagnostics
```

Questa suite ha un obiettivo diverso:

```text
tests/core/     -> eventi semantici ufficiali
tests/backend/  -> diagnostica e manutenzione interna del backend inotify
tests/functional/ -> storico legacy/shadow archiviato
```

Scenari diagnostici riesaminati:

| Scenario diagnostico | Origine attuale | Cosa controlla | Perche' e' utile |
| --- | --- | --- | --- |
| create directory watch | `tests/backend/test_watch_added_create_dir.sh` | `WATCH_ADDED` dopo la creazione di una directory | verifica che il backend aggiunga un watch alla nuova directory osservabile |
| delete directory watch | `tests/backend/test_watch_removed_delete_dir.sh` | `WATCH_REMOVED` dopo `IN_IGNORED` | verifica che il backend non lasci una voce watch valida per una directory sparita |
| recursive slow watch tree | `tests/backend/test_recursive_slow_watch_tree.sh` | `WATCH_ADDED` per `a`, `a/b`, `a/b/c` | verifica la manutenzione progressiva dei watch quando le directory nascono una dopo l'altra |
| attrib raw log | `tests/backend/test_attrib_raw_log.sh` | `IN_ATTRIB` nel raw log dopo `chmod` | verifica che la maschera osservi cambiamenti di attributi e che `events.log` non riceva nuove righe semantiche o diagnostiche dopo `chmod` |
| watch mask disable attrib | `tests/backend/test_watch_mask_disable_attrib.sh` | assenza di `IN_ATTRIB` dopo `inotify_watch_mask=default,-IN_ATTRIB` | verifica che la maschera configurata cambi davvero gli eventi osservati dal backend |
| watch mask invalid token | `tests/backend/test_watch_mask_invalid_token.sh` | avvio fallito con `IN_ATRIB` | verifica che i typo nella maschera siano errori di configurazione, non token ignorati |
| self events root watch | `tests/backend/test_self_events_root_watch.sh` | `IN_DELETE_SELF`, `IN_IGNORED`, `IN_MOVE_SELF`, `WATCH_STALE`, `WATCH_RESYNC_BEGIN`, `WATCH_RESYNC_FAILED`, `WATCH_REMOVED` e comportamento della root spostata | verifica osservativa per distinguere eventi sul path osservato da eventi sui figli; delete-self marca il watch stale e attende cleanup, move-self avvia un probe resync conservativo senza inventare delete dei figli o relocation semantica |
| self move identity match | `tests/backend/test_self_move_identity_match.sh` | `IN_MOVE_SELF`, path osservato ripristinato con la stessa identita', `WATCH_RESYNC_SCAN_DONE ... dirs=3 watched=1 missing=2`, `WATCH_RESYNC_SCAN_CLASS ... result=needs-reinstall`, due `WATCH_RESYNC_REINSTALLED`, due `FILE_CREATED` nelle directory riparate | verifica il ramo positivo del probe: se la root osservata viene spostata e rimessa nello stesso path prima del resync, `(st_dev, st_ino)` coincidono, Alfred misura la copertura, reinstalla tutti i watch mancanti nello scope affidabile e dimostra che i nuovi watch vedono file creati in entrambe le directory riparate |
| self move identity mismatch | `tests/backend/test_self_move_identity_mismatch.sh` | `IN_MOVE_SELF`, path osservato ricreato, `WATCH_RESYNC_FAILED ... error=identity-mismatch`, nessun `WATCH_RESYNC_SCAN_DONE` | dimostra perche' il path da solo non basta: se la root osservata viene spostata e una nuova directory nasce nello stesso path, Alfred confronta `(st_dev, st_ino)`, non scansiona il path riusato e lascia il watch `STALE` |
| recursive fast synthetic raw | `tests/core/test_recursive_create_nested_dir.sh` | effetto semantico finale dei raw sintetici | per ora basta il test core: il formato raw diagnostico non e' un contratto stabile da fissare end-to-end |

Gli scenari diagnostici sui watch sono gia' in `tests/backend/`.

## Test scanner

La suite scanner si esegue con:

```bash
make test-scanner
```

Questi test non avviano Alfred come processo di monitoraggio e non controllano
eventi semantici. Servono a verificare il componente di attraversamento
filesystem che useremo per resync e, in futuro, indicizzazione.

| Scenario | Script | Cosa controlla | Perche' serve |
| --- | --- | --- | --- |
| scanner directories and options | `tests/scanner/test_fs_scanner_dirs.sh` | default directory-only, `emit_root=0`, `max_depth=1`, `max_entries=2`, `include_files=1`, `include_symlinks=1`, `include_other=1`, symlink non seguito, directory figlia rimossa durante lo scan | fissa il contratto iniziale dello scanner, delle opzioni pubbliche e della prima policy sugli errori parziali prima di usarlo per resync |

## Test watcher table

La suite watcher si esegue con:

```bash
make test-watcher
```

Questi test non avviano Alfred e non usano eventi reali del kernel. Servono a
verificare direttamente la struttura dati che conserva il mapping `wd -> path`
e lo stato di affidabilita' del watch.

| Scenario | Script | Cosa controlla | Perche' serve |
| --- | --- | --- | --- |
| watcher reliability states | `tests/watcher/test_watcher_state.c` | store iniziale `VALID`, identita' `st_dev/st_ino`, transizioni a `STALE` e `RESYNCING`, remove a `REMOVED`, rifiuto di `REMOVED` su slot attivo, conteggio per stato, iterazione per stato e stop anticipato | prepara la futura gestione `IN_MOVE_SELF` e resync senza dipendere da timing o dettagli del kernel |

Lo scenario `attrib raw log` usa `chmod` come caso rappresentativo di
`IN_ATTRIB`. La documentazione Linux elenca anche timestamp, attributi estesi,
numero di hard link, proprietario e gruppo. Non li testiamo tutti subito perche'
alcuni casi dipendono da permessi, filesystem o strumenti esterni. Verranno
aggiunti alla suite solo se decidiamo una semantica utente ufficiale per i
cambiamenti di metadati.

Decisione attuale sui raw sintetici: non aggiungiamo un test backend separato
solo sul formato raw. Il test core esistente verifica gia' il contratto utente:
`DIR_CREATED` per tutta la catena `one/two/three`. Un test raw dedicato avrebbe
senso solo se il formato raw diagnostico diventasse un contratto interno stabile
da proteggere. Oggi resta uno strumento di diagnosi backend.

La regola pratica e':

- se il test parla di `FILE_CREATED`, `DIR_RELOCATED`, `FILE_READY`, va in
  `tests/core/`
- se il test parla di `WATCH_ADDED`, `WATCH_REMOVED`, watch descriptor o raw
  diagnostici, va in `tests/backend/`
- se il test parla di scanner, opzioni di attraversamento, symlink o errori
  parziali di scan, va in `tests/scanner/`
- se il test parla direttamente di `watcher_table_t`, stati `VALID/STALE` o
  identita' `st_dev/st_ino` senza avviare Alfred, va in `tests/watcher/`
- se il test parla di differenze legacy/core, va archiviato con la storia dello
  shadow e non deve sopravvivere come controllo automatico ordinario

La guida contributori contiene la procedura operativa completa per aggiungere
un test: scelta della suite, runner, documentazione e verifiche locali.

### Scenario core: engine shadow invalido

File:

```text
tests/core/test_invalid_event_engine_shadow.sh
```

Questo scenario non controlla eventi filesystem. Controlla invece un contratto
di configurazione runtime nato durante lo switch al core:

```text
runtime: ALFRED_EVENT_ENGINE=shadow
```

Il risultato atteso e' un fallimento di startup. Alfred non deve fare fallback
silenzioso a `event_engine=core`, perche' in quel caso lo sviluppatore
crederebbe di stare usando una modalita' che non esiste piu'.

Il test verifica due cose:

- il processo esce con status diverso da zero
- `startup.log` contiene il messaggio di valore invalido e indica che l'unico
  valore ammesso e' `core`

Il test vive in `tests/core/` perche' il rifiuto di engine non supportati fa
parte del contratto runtime ufficiale del percorso core.

Strategia consigliata per i prossimi refactor dei test:

1. usare `make test` come contratto ufficiale core
2. mantenere `make test-core` come nome esplicito della stessa suite
3. non usare piu' `make test-legacy-shadow` come verifica ordinaria: il
   dispatch legacy live e' stato spento
4. non aggiungere per ora una terza suite `test-functional-core`, perche'
   `make test` e `make test-core` sono gia' end-to-end sul percorso core
5. lasciare i funzionali legacy come archivio storico finche' servono alla
   lettura della migrazione; non devono rientrare nella suite ufficiale

Risultato della revisione finale scenari/test: non emerge un buco semantico
immediato. La suite core copre il contratto utente corrente, la suite backend
copre la diagnostica watch utile e i test funzionali/shadow restano archivio
storico.

## Collegamento con la lettura guidata del codice

Questo capitolo dice quali scenari esistono e quali eventi ci aspettiamo nei
log. Per capire come quegli eventi attraversano funzioni e strutture dati, usa
anche [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md).

Mappa rapida:

| Scenario | Lettura guidata utile |
| --- | --- |
| create file | `Scenario animabile: create file` |
| close-write / file ready | `Scenario animabile: close-write / file ready` |
| modify file | `Scenario animabile: modify debounced` |
| move and rename file/directory | `Scenario animabile: move + rename nel core` |
| recursive create nested directory | `Scenario animabile: recursive mkdir veloce` |
| delete directory con rimozione watch | `Scenario animabile: rimozione watch` |
| startup watch | `Scenario animabile: aggiunta watch iniziale` |

La differenza e' questa:

- qui fissiamo il contratto osservabile dei test
- nella lettura guidata seguiamo il percorso interno: funzioni, campi delle
  strutture dati e frame animabili

### Core: create file

File:

```text
tests/core/test_create_file.sh
```

Operazioni:

```bash
printf "hello\n" > "$TEST_ROOT/a.txt"
```

Event log core atteso:

```text
FILE_CREATED path=$ROOT/a.txt
FILE_MODIFIED path=$ROOT/a.txt
FILE_READY path=$ROOT/a.txt
```

### Core: delete file

File:

```text
tests/core/test_delete_file.sh
```

Operazioni:

```bash
printf "temporary\n" > "$TEST_ROOT/delete-me.txt"
rm "$TEST_ROOT/delete-me.txt"
```

Event log core atteso:

```text
FILE_CREATED path=$ROOT/delete-me.txt
FILE_MODIFIED path=$ROOT/delete-me.txt
FILE_READY path=$ROOT/delete-me.txt
FILE_DELETED path=$ROOT/delete-me.txt
```

Questo test conferma che una cancellazione file conserva la storia precedente
del file: prima il path viene creato e scritto, poi viene eliminato.

### Core: rename file

File:

```text
tests/core/test_rename_file.sh
```

Operazioni:

```bash
printf "rename\n" > "$TEST_ROOT/old.txt"
mv "$TEST_ROOT/old.txt" "$TEST_ROOT/new.txt"
```

Event log core atteso:

```text
FILE_CREATED path=$ROOT/old.txt
FILE_MODIFIED path=$ROOT/old.txt
FILE_READY path=$ROOT/old.txt
FILE_RENAMED from=$ROOT/old.txt to=$ROOT/new.txt
```

`FILE_RENAMED` e' corretto perche' cambia solo il nome nello stesso contenitore.

### Core: create directory

File:

```text
tests/core/test_create_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/new-dir"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/new-dir
```

### Core: delete directory

File:

```text
tests/core/test_delete_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/delete-dir"
rmdir "$TEST_ROOT/delete-dir"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/delete-dir
DIR_DELETED path=$ROOT/delete-dir
```

Il test controlla anche che `WATCH_REMOVED` non compaia come evento semantico
core. La rimozione del watch resta diagnostica backend.

### Core: rename directory

File:

```text
tests/core/test_rename_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/old-dir"
mv "$TEST_ROOT/old-dir" "$TEST_ROOT/new-dir"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/old-dir
DIR_RENAMED from=$ROOT/old-dir to=$ROOT/new-dir
```

`DIR_RENAMED` e' corretto perche' la directory resta nello stesso contenitore e
cambia solo il nome. Il test controlla anche che lo stesso evento non venga
classificato come `DIR_MOVED` o `DIR_RELOCATED`.

### Core: move file

File:

```text
tests/core/test_move_file.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
printf "move\n" > "$TEST_ROOT/src/moved.txt"
mv "$TEST_ROOT/src/moved.txt" "$TEST_ROOT/dst/moved.txt"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/src
DIR_CREATED path=$ROOT/dst
FILE_CREATED path=$ROOT/src/moved.txt
FILE_MODIFIED path=$ROOT/src/moved.txt
FILE_READY path=$ROOT/src/moved.txt
FILE_MOVED from=$ROOT/src/moved.txt to=$ROOT/dst/moved.txt
```

`FILE_MOVED` e' corretto perche' cambia il contenitore ma il basename resta lo
stesso. Il test controlla anche che lo stesso evento non venga classificato come
`FILE_RENAMED` o `FILE_RELOCATED`.

### Core: move directory

File:

```text
tests/core/test_move_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
mkdir "$TEST_ROOT/src/item"
mv "$TEST_ROOT/src/item" "$TEST_ROOT/dst/item"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/src
DIR_CREATED path=$ROOT/dst
DIR_CREATED path=$ROOT/src/item
DIR_MOVED from=$ROOT/src/item to=$ROOT/dst/item
```

`DIR_MOVED` e' corretto perche' cambia il contenitore ma il nome della directory
resta lo stesso. Il test controlla anche che lo stesso evento non venga
classificato come `DIR_RENAMED` o `DIR_RELOCATED`.

### Core: move and rename directory

File:

```text
tests/core/test_move_rename_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
mkdir "$TEST_ROOT/src/before"
mv "$TEST_ROOT/src/before" "$TEST_ROOT/dst/after"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/src
DIR_CREATED path=$ROOT/dst
DIR_CREATED path=$ROOT/src/before
DIR_RELOCATED from=$ROOT/src/before to=$ROOT/dst/after
```

`DIR_RELOCATED` e' corretto perche' cambiano sia il contenitore sia il nome
della directory. Il core deve emettere un solo evento semantico, non la coppia
`DIR_MOVED` + `DIR_RENAMED` usata dal percorso legacy.

### Core: move and rename file

File:

```text
tests/core/test_move_rename_file.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
printf "move and rename\n" > "$TEST_ROOT/src/old.txt"
mv "$TEST_ROOT/src/old.txt" "$TEST_ROOT/dst/new.txt"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/src
DIR_CREATED path=$ROOT/dst
FILE_RELOCATED from=$ROOT/src/old.txt to=$ROOT/dst/new.txt
```

Il test controlla anche che non compaiano `FILE_MOVED` e `FILE_RENAMED` per lo
stesso spostamento: nel core lo spostamento con cambio nome e' un solo
`FILE_RELOCATED`.

### Core: recursive create nested directory

File:

```text
tests/core/test_recursive_create_nested_dir.sh
```

Operazioni:

```bash
mkdir -p "$TEST_ROOT/one/two/three"
```

Event log core atteso:

```text
DIR_CREATED path=$ROOT/one
DIR_CREATED path=$ROOT/one/two
DIR_CREATED path=$ROOT/one/two/three
```

Questo test verifica che il core recuperi anche le directory create prima che il
watch del padre fosse installato. Il recupero passa da scan ricorsivo backend e
raw event sintetici verso il core.

### Core: modify file / close-write

File:

```text
tests/core/test_modify_file.sh
```

Operazioni:

```bash
printf "initial\n" > "$TEST_ROOT/editable.txt"
printf "second\n" >> "$TEST_ROOT/editable.txt"
```

Event log core atteso:

```text
FILE_CREATED path=$ROOT/editable.txt
FILE_MODIFIED path=$ROOT/editable.txt
FILE_READY path=$ROOT/editable.txt
FILE_MODIFIED path=$ROOT/editable.txt
FILE_READY path=$ROOT/editable.txt
```

Il primo `printf` crea il file e quindi produce anche `FILE_CREATED`. Entrambe
le scritture producono invece una fase `FILE_MODIFIED` seguita da
`FILE_READY`, perche' `FILE_MODIFIED` rappresenta la modifica dei contenuti e
`FILE_READY` rappresenta la chiusura in scrittura (`close-write`). Il test usa
un controllo di conteggio: `FILE_CREATED` deve comparire una sola volta,
`FILE_MODIFIED` e `FILE_READY` due volte.

### create file

File:

```text
tests/functional/test_create_file.sh
```

Operazioni:

```bash
touch "$TEST_ROOT/a.txt"
```

Raw log atteso:

```text
IN_CREATE wd=<root-wd> path=$ROOT name=a.txt
```

Event log atteso:

```text
FILE_CREATED path=$ROOT/a.txt
core ... type=FILE_CREATED path=$ROOT/a.txt
```

### create dir

File:

```text
tests/functional/test_create_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/dir1"
```

Raw log atteso:

```text
IN_CREATE IN_ISDIR wd=<root-wd> path=$ROOT name=dir1
```

Event log atteso:

```text
DIR_CREATED path=$ROOT/dir1
WATCH_ADDED wd=<new-wd> path=$ROOT/dir1
core ... type=DIR_CREATED path=$ROOT/dir1
```

`WATCH_ADDED` e' diagnostica backend: indica che il modulo inotify ha iniziato a
monitorare la nuova directory.

### delete file

File:

```text
tests/functional/test_delete_file.sh
```

Operazioni:

```bash
touch "$TEST_ROOT/a.txt"
rm "$TEST_ROOT/a.txt"
```

Raw log atteso:

```text
IN_CREATE wd=<root-wd> path=$ROOT name=a.txt
IN_DELETE wd=<root-wd> path=$ROOT name=a.txt
```

Event log atteso:

```text
FILE_CREATED path=$ROOT/a.txt
FILE_DELETED path=$ROOT/a.txt
core ... type=FILE_CREATED path=$ROOT/a.txt
core ... type=FILE_DELETED path=$ROOT/a.txt
```

Nota: il test storico controlla almeno la creazione. La cancellazione e' parte
dello scenario e va considerata quando si estende la copertura.

### delete dir

File:

```text
tests/functional/test_delete_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/dir"
rm -rf "$TEST_ROOT/dir"
```

Raw log atteso:

```text
IN_CREATE IN_ISDIR wd=<root-wd> path=$ROOT name=dir
IN_DELETE IN_ISDIR wd=<root-wd> path=$ROOT name=dir
IN_IGNORED wd=<dir-wd> path=$ROOT/dir name=
```

Event log atteso:

```text
DIR_CREATED path=$ROOT/dir
WATCH_ADDED wd=<dir-wd> path=$ROOT/dir
DIR_DELETED path=$ROOT/dir
WATCH_REMOVED path=$ROOT/dir
core ... type=DIR_CREATED path=$ROOT/dir
core ... type=DIR_DELETED path=$ROOT/dir
```

`WATCH_REMOVED` resta diagnostica backend. Non deve diventare un evento
semantico del core.

### rename file

File:

```text
tests/functional/test_rename_file.sh
```

Operazioni:

```bash
touch "$TEST_ROOT/a.txt"
mv "$TEST_ROOT/a.txt" "$TEST_ROOT/b.txt"
```

Raw log atteso:

```text
IN_CREATE wd=<root-wd> path=$ROOT name=a.txt
IN_MOVED_FROM wd=<root-wd> path=$ROOT name=a.txt cookie=<n>
IN_MOVED_TO wd=<root-wd> path=$ROOT name=b.txt cookie=<n>
```

Event log atteso:

```text
FILE_CREATED path=$ROOT/a.txt
FILE_RENAMED from=$ROOT/a.txt to=$ROOT/b.txt
core ... type=FILE_CREATED path=$ROOT/a.txt
core ... type=FILE_RENAMED from=$ROOT/a.txt to=$ROOT/b.txt
```

### rename dir

File:

```text
tests/functional/test_rename_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/one"
mv "$TEST_ROOT/one" "$TEST_ROOT/two"
```

Raw log atteso:

```text
IN_CREATE IN_ISDIR wd=<root-wd> path=$ROOT name=one
IN_MOVED_FROM IN_ISDIR wd=<root-wd> path=$ROOT name=one cookie=<n>
IN_MOVED_TO IN_ISDIR wd=<root-wd> path=$ROOT name=two cookie=<n>
```

Event log atteso:

```text
DIR_CREATED path=$ROOT/one
WATCH_ADDED wd=<dir-wd> path=$ROOT/one
DIR_RENAMED from=$ROOT/one to=$ROOT/two
core ... type=DIR_CREATED path=$ROOT/one
core ... type=DIR_RENAMED from=$ROOT/one to=$ROOT/two
```

### move file

File:

```text
tests/functional/test_move_file.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
touch "$TEST_ROOT/src/a.txt"
mv "$TEST_ROOT/src/a.txt" "$TEST_ROOT/dst/a.txt"
```

Raw log atteso:

```text
IN_CREATE IN_ISDIR path=$ROOT name=src
IN_CREATE IN_ISDIR path=$ROOT name=dst
IN_CREATE path=$ROOT/src name=a.txt
IN_MOVED_FROM path=$ROOT/src name=a.txt cookie=<n>
IN_MOVED_TO path=$ROOT/dst name=a.txt cookie=<n>
```

Event log atteso:

```text
DIR_CREATED path=$ROOT/src
DIR_CREATED path=$ROOT/dst
FILE_CREATED path=$ROOT/src/a.txt
FILE_MOVED from=$ROOT/src/a.txt to=$ROOT/dst/a.txt
core ... type=FILE_MOVED from=$ROOT/src/a.txt to=$ROOT/dst/a.txt
```

### move and rename directory

File:

```text
tests/functional/test_move_rename_dir.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/src/before"
mkdir "$TEST_ROOT/dst"
mv "$TEST_ROOT/src/before" "$TEST_ROOT/dst/after"
```

Raw log atteso:

```text
IN_CREATE IN_ISDIR path=$ROOT name=src
IN_CREATE IN_ISDIR path=$ROOT/src name=before
IN_CREATE IN_ISDIR path=$ROOT name=dst
IN_MOVED_FROM IN_ISDIR path=$ROOT/src name=before cookie=<n>
IN_MOVED_TO IN_ISDIR path=$ROOT/dst name=after cookie=<n>
```

Event log legacy atteso:

```text
DIR_MOVED from=$ROOT/src/before to=$ROOT/dst/after
DIR_RENAMED from=$ROOT/src/before to=$ROOT/dst/after
```

Event log core target:

```text
core ... type=DIR_CREATED path=$ROOT/src/before
core ... type=DIR_RELOCATED from=$ROOT/src/before to=$ROOT/dst/after
```

Questa differenza e' intenzionale: il core considera move+rename come una sola
operazione semantica `DIR_RELOCATED`. Inoltre il core puo' recuperare
`DIR_CREATED` per `src/before` tramite raw event sintetico, se quella directory
e' stata creata prima che il watch su `src` fosse installato.

### move and rename file

File:

```text
tests/functional/test_move_rename_file.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/src"
mkdir "$TEST_ROOT/dst"
touch "$TEST_ROOT/src/old.txt"
mv "$TEST_ROOT/src/old.txt" "$TEST_ROOT/dst/new.txt"
```

Event log legacy atteso:

```text
FILE_MOVED from=$ROOT/src/old.txt to=$ROOT/dst/new.txt
FILE_RENAMED from=$ROOT/src/old.txt to=$ROOT/dst/new.txt
```

Event log core target:

```text
core ... type=FILE_RELOCATED from=$ROOT/src/old.txt to=$ROOT/dst/new.txt
```

Il test funzionale controlla esplicitamente il comportamento legacy, cioe' la
doppia emissione `FILE_MOVED` e `FILE_RENAMED`. Il test core controlla invece
la semantica target: una sola operazione logica `FILE_RELOCATED`.

### recursive create

File:

```text
tests/functional/test_recursive.sh
```

Operazioni:

```bash
mkdir "$TEST_ROOT/a"
mkdir "$TEST_ROOT/a/b"
mkdir "$TEST_ROOT/a/b/c"
touch "$TEST_ROOT/a/b/c/file.txt"
```

Questo test inserisce pause tra le `mkdir`, quindi Alfred ha tempo di aggiungere
i watch prima della creazione della directory successiva.

Event log atteso:

```text
DIR_CREATED path=$ROOT/a
WATCH_ADDED path=$ROOT/a
DIR_CREATED path=$ROOT/a/b
WATCH_ADDED path=$ROOT/a/b
DIR_CREATED path=$ROOT/a/b/c
WATCH_ADDED path=$ROOT/a/b/c
FILE_CREATED path=$ROOT/a/b/c/file.txt
```

Questo scenario non riproduce il bug `mkdir -p`: per quello usiamo lo scenario
shadow `recursive_create_nested_dir`.

## Test shadow storici

Il tool shadow storico si trova in:

```text
tests/shadow/compare_shadow_output.py
```

La modalita' shadow non e' piu' eseguibile. Il comando storico:

```bash
python3 tests/shadow/compare_shadow_output.py <scenario>
```

fallisce con un errore esplicito perche' il confronto legacy/core e' stato
rimosso.

Durante la migrazione il tool creava una directory temporanea, avviava
`alfred`, eseguiva lo scenario e separava l'output in quattro sezioni:

```text
Legacy
Core
Only in legacy
Only in core
```

Le differenze non facevano fallire il comando, a meno che non si usasse:

```bash
--strict
```

La vecchia opzione per conservare i log completi era:

```bash
--keep-logs
```

I log vengono copiati in:

```text
tests/shadow/last-run/
```

Per ispezione manuale storica, il runner puo' ancora avviare il core plain:

```bash
python3 tests/shadow/compare_shadow_output.py <scenario> --event-engine core
```

In questa modalita' il tool avvia Alfred con `ALFRED_EVENT_ENGINE=core` e
stampa:

```text
Core official
```

Non c'e' confronto con il legacy nella stessa run, perche' il vecchio dispatcher
non esiste piu' nel codice corrente. Questo runner non sostituisce `make test`.

Nota importante: anche in core mode l'app continua ad aggiornare i watch del
backend quando vede `IN_CREATE | IN_ISDIR`. Questa logica non appartiene a
`events.c`: serve al backend per continuare a monitorare nuove directory. Senza
questo aggiornamento, scenari come `move_dir` o `recursive_create_nested_dir`
perderebbero eventi dentro directory create da poco.

## Mappa a tre livelli degli scenari core

Questa tabella riassume il percorso piu' importante per l'integrazione:

```text
evento inotify -> alfred_raw_event_t -> alfred_event_t
```

La colonna "inotify" descrive gli eventi kernel principali. La colonna "raw
Alfred" descrive i flag prodotti dall'adapter o dai raw event sintetici. La
colonna "semantica core" descrive gli eventi `alfred_event_t` che vogliamo
considerare riferimento finale.

| Scenario shadow | inotify principale | raw Alfred | semantica core target |
| --- | --- | --- | --- |
| `create_file` | `IN_CREATE`, `IN_MODIFY`, `IN_CLOSE_WRITE` | `ALFRED_RAW_CREATE`, `ALFRED_RAW_MODIFY`, `ALFRED_RAW_CLOSE_WRITE` | `FILE_CREATED`, `FILE_MODIFIED`, `FILE_READY` |
| `delete_file` | `IN_CREATE`, `IN_MODIFY`, `IN_CLOSE_WRITE`, `IN_DELETE` | `ALFRED_RAW_CREATE`, `ALFRED_RAW_MODIFY`, `ALFRED_RAW_CLOSE_WRITE`, `ALFRED_RAW_DELETE` | `FILE_CREATED`, `FILE_MODIFIED`, `FILE_READY`, `FILE_DELETED` |
| `rename_file` | `IN_CREATE`, `IN_MODIFY`, `IN_CLOSE_WRITE`, `IN_MOVED_FROM`, `IN_MOVED_TO` | `ALFRED_RAW_CREATE`, `ALFRED_RAW_MODIFY`, `ALFRED_RAW_CLOSE_WRITE`, `ALFRED_RAW_MOVED_FROM`, `ALFRED_RAW_MOVED_TO` | `FILE_CREATED`, `FILE_MODIFIED`, `FILE_READY`, `FILE_RENAMED` |
| `create_dir` | `IN_CREATE | IN_ISDIR` | `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR` | `DIR_CREATED` |
| `delete_dir` | `IN_CREATE | IN_ISDIR`, `IN_DELETE | IN_ISDIR`, `IN_IGNORED` | `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR`, `ALFRED_RAW_DELETE | ALFRED_RAW_ISDIR` | `DIR_CREATED`, `DIR_DELETED`; `WATCH_REMOVED` resta diagnostica backend |
| `rename_dir` | `IN_CREATE | IN_ISDIR`, `IN_MOVED_FROM | IN_ISDIR`, `IN_MOVED_TO | IN_ISDIR` | `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR`, `ALFRED_RAW_MOVED_FROM | ALFRED_RAW_ISDIR`, `ALFRED_RAW_MOVED_TO | ALFRED_RAW_ISDIR` | `DIR_CREATED`, `DIR_RENAMED` |
| `move_file` | directory create, file write, `IN_MOVED_FROM`, `IN_MOVED_TO` | create directory raw, file write raw, moved-from/to raw con stesso cookie | `DIR_CREATED`, `FILE_CREATED`, `FILE_MODIFIED`, `FILE_READY`, `FILE_MOVED` |
| `move_dir` | directory create, `IN_MOVED_FROM | IN_ISDIR`, `IN_MOVED_TO | IN_ISDIR` | create directory raw, moved-from/to raw con stesso cookie e `ALFRED_RAW_ISDIR` | `DIR_CREATED`, `DIR_MOVED` |
| `move_rename_file` | directory create, file write, `IN_MOVED_FROM`, `IN_MOVED_TO` con nome diverso | moved-from/to raw con stesso cookie, directory contenitore diversa e nome diverso | `FILE_RELOCATED` per il file; eventi di creazione/write precedenti restano separati |
| `move_rename_dir` | directory create, `IN_MOVED_FROM | IN_ISDIR`, `IN_MOVED_TO | IN_ISDIR` con nome diverso | moved-from/to raw con stesso cookie, `ALFRED_RAW_ISDIR`, directory contenitore diversa e nome diverso | `DIR_RELOCATED`; eventuale `DIR_CREATED` sintetico se la directory e' scoperta dallo scan |
| `recursive_create_nested_dir` | spesso solo `IN_CREATE | IN_ISDIR` per la prima directory osservabile | raw reale per la prima directory, raw sintetici `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR` per le directory scoperte | `DIR_CREATED` per ogni directory dell'albero |
| `recursive_create_slow_nested_dir` | `IN_CREATE | IN_ISDIR` per ogni directory | `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR` per ogni directory | `DIR_CREATED` per ogni directory, senza duplicati |
| `modify_close_write_file` | `IN_CREATE`, `IN_MODIFY`, `IN_CLOSE_WRITE`, poi nuovo `IN_MODIFY`, nuovo `IN_CLOSE_WRITE` | `ALFRED_RAW_CREATE`, due `ALFRED_RAW_MODIFY`, due `ALFRED_RAW_CLOSE_WRITE` | `FILE_CREATED`, due `FILE_MODIFIED`, due `FILE_READY` |

Questa tabella non sostituisce i dettagli degli scenari sotto. Serve come mappa
veloce per capire dove nasce ogni evento e quale livello del sistema lo deve
interpretare. I dettagli di path, cookie e watch descriptor restano nei log
osservati con `--keep-logs`.

### create_file

Operazioni:

```text
write $ROOT/a.txt
```

Risultato osservato:

```text
Legacy: FILE_CREATED path=$ROOT/a.txt
Core:   FILE_CREATED path=$ROOT/a.txt
        FILE_MODIFIED path=$ROOT/a.txt
        FILE_READY path=$ROOT/a.txt
```

### delete_file

Operazioni:

```text
write $ROOT/delete-me.txt
unlink $ROOT/delete-me.txt
```

Atteso:

```text
Legacy: FILE_CREATED, FILE_DELETED
Core:   FILE_CREATED, FILE_MODIFIED, FILE_READY, FILE_DELETED
```

### rename_file

Operazioni:

```text
write $ROOT/old.txt
rename $ROOT/old.txt -> $ROOT/new.txt
```

Atteso:

```text
Legacy: FILE_RENAMED
Core:   FILE_CREATED, FILE_MODIFIED, FILE_READY, FILE_RENAMED
```

### create_dir

Operazioni:

```text
mkdir $ROOT/new-dir
```

Atteso:

```text
Legacy: DIR_CREATED
Core:   DIR_CREATED
```

### delete_dir

Operazioni:

```text
mkdir $ROOT/delete-dir
rmdir $ROOT/delete-dir
```

Atteso:

```text
Legacy: DIR_CREATED, WATCH_REMOVED, DIR_DELETED
Core:   DIR_CREATED, DIR_DELETED
```

Differenza attesa: `WATCH_REMOVED` e' diagnostica backend, non evento semantico
core.

### rename_dir

Operazioni:

```text
mkdir $ROOT/old-dir
rename $ROOT/old-dir -> $ROOT/new-dir
```

Atteso:

```text
Legacy: DIR_RENAMED
Core:   DIR_RENAMED
```

### move_file

Operazioni:

```text
mkdir $ROOT/src
mkdir $ROOT/dst
write $ROOT/src/moved.txt
rename $ROOT/src/moved.txt -> $ROOT/dst/moved.txt
```

Atteso:

```text
Legacy: FILE_MOVED
Core:   FILE_MOVED
```

### move_dir

Operazioni:

```text
mkdir $ROOT/src
mkdir $ROOT/dst
sleep
mkdir $ROOT/src/item
sleep
rename $ROOT/src/item -> $ROOT/dst/item
```

Raw log atteso:

```text
IN_CREATE IN_ISDIR path=$ROOT name=src
IN_CREATE IN_ISDIR path=$ROOT name=dst
IN_CREATE IN_ISDIR path=$ROOT/src name=item
IN_MOVED_FROM IN_ISDIR path=$ROOT/src name=item
IN_MOVED_TO IN_ISDIR path=$ROOT/dst name=item
```

Event log normalizzato osservato:

```text
Legacy: DIR_CREATED path=$ROOT/src
        DIR_CREATED path=$ROOT/dst
        DIR_CREATED path=$ROOT/src/item
        DIR_MOVED from=$ROOT/src/item to=$ROOT/dst/item

Core:   DIR_CREATED path=$ROOT/src
        DIR_CREATED path=$ROOT/dst
        DIR_CREATED path=$ROOT/src/item
        DIR_MOVED from=$ROOT/src/item to=$ROOT/dst/item
```

Questo scenario fissa il caso semplice: il nome della directory resta `item`,
ma cambia la directory contenitore. L'evento semantico corretto e' quindi
`DIR_MOVED`, non `DIR_RENAMED` e non `DIR_RELOCATED`.

### move_rename_file

Operazioni:

```text
mkdir $ROOT/src
mkdir $ROOT/dst
write $ROOT/src/old.txt
rename $ROOT/src/old.txt -> $ROOT/dst/new.txt
```

Atteso:

```text
Legacy: FILE_MOVED + FILE_RENAMED
Core:   FILE_RELOCATED
```

Differenza attesa: il core rappresenta una singola operazione logica.

### move_rename_dir

Operazioni:

```text
mkdir $ROOT/src
mkdir $ROOT/dst
mkdir $ROOT/src/before
rename $ROOT/src/before -> $ROOT/dst/after
```

Atteso:

```text
Legacy: DIR_MOVED + DIR_RENAMED
Core:   DIR_CREATED path=$ROOT/src/before
        DIR_RELOCATED
```

Differenza attesa: il core rappresenta move+rename come una singola operazione
logica `DIR_RELOCATED` e puo' recuperare anche la creazione iniziale della
directory tramite raw event sintetico.

### modify_close_write_file

Operazioni:

```text
write $ROOT/editable.txt
sleep
append $ROOT/editable.txt
flush/fsync
sleep while file is still open
close $ROOT/editable.txt
```

Scopo:

```text
osservare come il core interpreta IN_MODIFY e IN_CLOSE_WRITE.
```

Semantica core:

```text
ALFRED_RAW_MODIFY      -> FILE_MODIFIED
ALFRED_RAW_CLOSE_WRITE -> FILE_READY
```

Output normalizzato osservato oggi:

```text
Legacy: FILE_CREATED path=$ROOT/editable.txt
Core:   FILE_CREATED path=$ROOT/editable.txt
        FILE_MODIFIED path=$ROOT/editable.txt
        FILE_READY path=$ROOT/editable.txt
        FILE_MODIFIED path=$ROOT/editable.txt
        FILE_READY path=$ROOT/editable.txt
```

Raw log osservato oggi con `--keep-logs`:

```text
IN_CREATE path=$ROOT name=editable.txt
IN_MODIFY path=$ROOT name=editable.txt
IN_CLOSE_WRITE path=$ROOT name=editable.txt
IN_MODIFY path=$ROOT name=editable.txt
IN_CLOSE_WRITE path=$ROOT name=editable.txt
```

Conclusione: il backend ora consegna al core anche gli eventi di modifica e
close-write. Il legacy dispatcher resta piu' povero perche' ignora quei flag,
mentre il core espone la vita del file in modo piu' completo.

### recursive_create_nested_dir

Operazioni:

```text
mkdir -p $ROOT/one/two/three
```

Output normalizzato osservato:

```text
Legacy: DIR_CREATED path=$ROOT/one
Core:   DIR_CREATED path=$ROOT/one
        DIR_CREATED path=$ROOT/one/two
        DIR_CREATED path=$ROOT/one/two/three
```

Con `--keep-logs`, `events.log` mostra anche:

```text
WATCH_ADDED path=$ROOT/one
WATCH_ADDED path=$ROOT/one/two
WATCH_ADDED path=$ROOT/one/two/three
```

`raw.log` mostra solo:

```text
IN_CREATE IN_ISDIR path=$ROOT name=one
```

Conclusione: il backend aggiunge i watch correttamente e il core recupera gli
eventi semantici mancanti tramite raw event sintetici. Il legacy resta
incompleto per questo scenario, quindi `Only in core` contiene `one/two` e
`one/two/three`.

### recursive_create_slow_nested_dir

Operazioni:

```text
mkdir $ROOT/one
sleep
mkdir $ROOT/one/two
sleep
mkdir $ROOT/one/two/three
```

Atteso:

```text
Legacy: DIR_CREATED path=$ROOT/one
        DIR_CREATED path=$ROOT/one/two
        DIR_CREATED path=$ROOT/one/two/three

Core:   DIR_CREATED path=$ROOT/one
        DIR_CREATED path=$ROOT/one/two
        DIR_CREATED path=$ROOT/one/two/three
```

Questo scenario serve a controllare la deduplica. Le pause danno tempo ad Alfred
di aggiungere i watch prima della creazione successiva, quindi inotify consegna
gli eventi reali. Il core non produce duplicati in questo caso.

## Come usare questo capitolo

Quando si aggiunge o modifica uno scenario:

1. scrivere le operazioni filesystem
2. indicare gli eventi raw attesi
3. indicare gli eventi semantic legacy attesi
4. indicare gli eventi core attesi
5. documentare se la differenza e' un bug, una decisione semantica o diagnostica
   backend

Questo mantiene allineati test, documentazione e decisioni architetturali.
