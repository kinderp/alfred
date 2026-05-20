# Scenari di test

Questo capitolo descrive gli scenari usati nei test del progetto. L'obiettivo
non e' solo sapere "quale comando eseguire", ma capire quali eventi ci aspettiamo
nel `raw.log` e nell'`events.log`.

I test sono divisi in due famiglie:

- test funzionali: verificano il comportamento esterno del programma
- test shadow: confrontano il vecchio dispatcher inotify con il nuovo core

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

- eventi semantici legacy, per esempio `DIR_CREATED`
- eventi semantici core in shadow mode, con prefisso `core`
- diagnostica backend, per esempio `WATCH_ADDED`

Esempio:

```text
DIR_CREATED path=/tmp/root/dir
WATCH_ADDED wd=2 path=/tmp/root/dir
core seq=1 type=DIR_CREATED path=/tmp/root/dir pid=0
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

Si eseguono con:

```bash
make test
```

Oppure direttamente:

```bash
cd tests/functional
bash run_all.sh
```

Questi test usano `tests/lib/test_lib.sh`, avviano `alfred` su una directory
temporanea e cercano pattern nell'output.

Importante: i test funzionali storici controllano soprattutto il comportamento
legacy visibile in `events.log`. Durante l'integrazione core/shadow mode, lo
stesso file puo' contenere anche righe `core ...`.

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

Stato attuale: il file esiste ma non contiene ancora uno scenario. Lo scenario
equivalente e' coperto dal tool shadow con `move_rename_file`.

Scenario consigliato:

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

## Test shadow

Il tool shadow si trova in:

```text
tests/shadow/compare_shadow_output.py
```

Comando base:

```bash
python3 tests/shadow/compare_shadow_output.py <scenario>
```

Il tool crea una directory temporanea, avvia `alfred`, esegue lo scenario e
separa l'output in quattro sezioni:

```text
Legacy
Core
Only in legacy
Only in core
```

Le differenze non fanno fallire il comando, a meno che non si usi:

```bash
--strict
```

Per conservare i log completi:

```bash
--keep-logs
```

I log vengono copiati in:

```text
tests/shadow/last-run/
```

### create_file

Operazioni:

```text
write $ROOT/a.txt
```

Risultato osservato:

```text
Legacy: FILE_CREATED path=$ROOT/a.txt
Core:   FILE_CREATED path=$ROOT/a.txt
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
Core:   FILE_CREATED, FILE_DELETED
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
Core:   FILE_RENAMED
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
