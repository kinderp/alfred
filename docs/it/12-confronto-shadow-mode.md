# Confronto shadow mode

Questo capitolo e' storico. Spiega come abbiamo confrontato l'output del
vecchio dispatcher inotify con l'output del core quando lo shadow mode era
ancora disponibile durante la migrazione.

Nel runtime corrente lo shadow mode e' stato rimosso: `ALFRED_EVENT_ENGINE=shadow`
fallisce con un errore esplicito, e il percorso ufficiale e' `core`.

## Perche' confrontare i due output

In shadow mode lo stesso evento percorre due strade:

```text
legacy path:
struct inotify_event -> events.c -> logger_event()

core path:
struct inotify_event -> inotify_adapter -> alfred_process()
                    -> core_logger_on_event() -> logger_event()
```

Il core e' il comportamento ufficiale di default. Lo shadow mode serve a
osservare anche il vecchio path, cosi' possiamo capire se il core interpreta gli
eventi nello stesso modo, meglio o peggio rispetto al legacy.

## Tool diagnostico

Il tool si trova in:

```text
tests/shadow/compare_shadow_output.py
```

Esegue uno scenario piccolo, legge `events.log`, separa righe legacy e righe
core, normalizza i path temporanei e stampa le differenze.

## Prerequisito

Prima compila il progetto:

```bash
make
```

Il tool cerca il binario:

```text
./alfred
```

Se hai modificato header importanti, per esempio `app/include/app.h`, puoi fare
una rebuild completa:

```bash
make re
```

Il Makefile ora traccia le dipendenze dagli header con file `.d`, quindi `make`
dovrebbe ricompilare automaticamente i file toccati. `make re` resta utile se
vuoi eliminare ogni dubbio durante una sessione di debug.

## Scenari disponibili

Per vedere gli scenari:

```bash
python3 tests/shadow/compare_shadow_output.py --help
```

Scenari disponibili:

```text
create_dir
create_file
delete_dir
delete_file
modify_close_write_file
move_dir
move_file
move_rename_dir
move_rename_file
recursive_create_nested_dir
recursive_create_slow_nested_dir
rename_dir
rename_file
```

La descrizione completa degli scenari funzionali e shadow, compresi raw log ed
event log attesi, e' raccolta in [Scenari di test](14-scenari-test.md).

## Esempio di uso

```bash
python3 tests/shadow/compare_shadow_output.py create_file
```

Output tipico:

```text
Scenario: create_file

Legacy:
  FILE_CREATED path=$ROOT/a.txt

Core:
  FILE_CREATED path=$ROOT/a.txt

Only in legacy:
  <none>

Only in core:
  <none>
```

In questo esempio legacy e core coincidono.

## Modalita' diagnostica e modalita' strict

Per default il tool non fallisce se trova differenze. Serve a osservare.

Se vuoi usarlo come test bloccante:

```bash
python3 tests/shadow/compare_shadow_output.py create_file --strict
```

Con `--strict`, il comando restituisce errore se legacy e core differiscono.

In questa fase iniziale e' meglio non usare `--strict` nei test ufficiali,
perche' sappiamo gia' che alcune differenze sono attese.

## Conservare i log

Per copiare i log dell'ultima esecuzione:

```bash
python3 tests/shadow/compare_shadow_output.py create_file --keep-logs
```

I file vengono copiati in:

```text
tests/shadow/last-run/
```

Questa cartella e' utile per ispezionare manualmente `events.log`, `raw.log` ed
eventuali errori.

`--keep-logs` e' importante quando il confronto normalizzato non basta. Il tool
mostra solo gli eventi legacy e core riconosciuti dal parser, mentre il file
`events.log` completo contiene anche diagnostica backend come:

```text
WATCH_ADDED wd=3 path=...
WATCH_REMOVED path=...
```

Per esempio, nello scenario `recursive_create_nested_dir`, `--keep-logs`
permette di vedere che `one/two` e `one/two/three` non hanno `DIR_CREATED`, ma
hanno comunque `WATCH_ADDED`.

## Come il tool normalizza gli eventi

Il tool trasforma path temporanei in `$ROOT`.

Esempio:

```text
/tmp/alfred_shadow_abc123/watched/a.txt
```

diventa:

```text
$ROOT/a.txt
```

Questo rende confrontabili run diversi.

Il tool riconosce due formati.

Legacy:

```text
FILE_CREATED path=/tmp/a.txt
FILE_RENAMED from=/tmp/a.txt to=/tmp/b.txt
```

Core:

```text
core seq=1 type=FILE_CREATED path=/tmp/a.txt pid=0
core seq=2 type=FILE_RENAMED from=/tmp/a.txt to=/tmp/b.txt pid=0
```

Entrambi vengono convertiti in una forma comune.

Questo parser e' pensato per `event_engine=shadow`, cioe' per il formato in cui
il core usa il prefisso `core`. Quando Alfred viene avviato con
`ALFRED_EVENT_ENGINE=core`, il core scrive invece lo stream ufficiale plain:

```text
FILE_CREATED path=/tmp/a.txt
FILE_RENAMED from=/tmp/a.txt to=/tmp/b.txt
```

Quella modalita' serve a provare lo switch, non a confrontare legacy e core nello
stesso `events.log`.

Il runner supporta entrambe le modalita':

```bash
python3 tests/shadow/compare_shadow_output.py create_file
python3 tests/shadow/compare_shadow_output.py create_file --event-engine core
```

La prima forma usa `event_engine=shadow` e mostra `Legacy`, `Core`,
`Only in legacy` e `Only in core`. La seconda forma usa
`ALFRED_EVENT_ENGINE=core` e mostra solo `Core official`.

## Come leggere le differenze

`Only in legacy` significa:

```text
il vecchio dispatcher ha prodotto un evento che il core non ha prodotto
```

`Only in core` significa:

```text
il core ha prodotto un evento che il vecchio dispatcher non ha prodotto
```

Una differenza puo' indicare:

- bug nel core
- bug nel vecchio dispatcher
- comportamento intenzionalmente diverso
- evento non ancora supportato
- scenario troppo rumoroso

## Strategia consigliata

1. Parti da scenari piccoli.
2. Leggi legacy e core.
3. Guarda `Only in legacy`.
4. Guarda `Only in core`.
5. Decidi se la differenza e' attesa.
6. Documenta le differenze ricorrenti.
7. Solo dopo rendi un confronto `--strict`.

## Risultati osservati

Gli scenari base su file e directory sono quasi tutti allineati:

| Scenario | Stato | Note |
| --- | --- | --- |
| `create_file` | differisce atteso | Legacy produce `FILE_CREATED`; core produce anche `FILE_MODIFIED` e `FILE_READY`. |
| `delete_file` | differisce atteso | Legacy produce `FILE_CREATED` e `FILE_DELETED`; core produce anche `FILE_MODIFIED` e `FILE_READY`. |
| `rename_file` | coincide | Entrambi producono `FILE_RENAMED`. |
| `create_dir` | coincide | Entrambi producono `DIR_CREATED`. |
| `delete_dir` | differisce | Legacy produce anche `WATCH_REMOVED`; il core no. |
| `rename_dir` | coincide | Entrambi producono `DIR_RENAMED`. |
| `move_dir` | coincide | Entrambi producono `DIR_MOVED` quando cambia solo la directory contenitore. |
| `move_file` | coincide | Entrambi producono `FILE_MOVED`. |
| `move_rename_dir` | differisce atteso | Legacy produce `DIR_MOVED` + `DIR_RENAMED`; core produce `DIR_CREATED` per la directory scoperta e poi `DIR_RELOCATED`. |
| `move_rename_file` | differisce | Legacy produce `FILE_MOVED` + `FILE_RENAMED`; core produce `FILE_RELOCATED`. |
| `modify_close_write_file` | differisce atteso | Legacy vede solo `FILE_CREATED`; core vede `FILE_CREATED`, due `FILE_MODIFIED` e due `FILE_READY`. |
| `recursive_create_nested_dir` | core recupera eventi | Legacy produce solo `DIR_CREATED $ROOT/one`; core produce anche `DIR_CREATED` per `one/two` e `one/two/three`. |
| `recursive_create_slow_nested_dir` | coincide | Crea le stesse directory con pause; legacy e core producono tre `DIR_CREATED` senza duplicati. |

Questo non significa che il core sia gia' pronto a sostituire il vecchio
dispatcher. Significa che molti casi base sono allineati e che le differenze
rimaste indicano decisioni semantiche da documentare.

Le decisioni semantiche sono raccolte in
[Semantica degli eventi](13-semantica-eventi.md).

### Differenza: delete directory

Nel caso `delete_dir`, legacy produce:

```text
DIR_CREATED path=$ROOT/delete-dir
WATCH_REMOVED path=$ROOT/delete-dir
DIR_DELETED path=$ROOT/delete-dir
```

Il core produce:

```text
DIR_CREATED path=$ROOT/delete-dir
DIR_DELETED path=$ROOT/delete-dir
```

La differenza e' `WATCH_REMOVED`.

Questo accade perche' `IN_IGNORED` e' un dettaglio specifico di inotify: indica
che un watch e' stato rimosso.

Decisione:

```text
WATCH_REMOVED deve restare diagnostica del backend, non evento semantico core.
```

Il legacy logga gia' sia `WATCH_ADDED` sia `WATCH_REMOVED`, ma oggi li scrive
attraverso `logger_event()`. In futuro potremo spostarli o duplicarli nel
raw/backend log, mantenendoli comunque fuori dalla semantica del core.

### Differenza: move and rename file

Nel caso `move_rename_file`, legacy produce due eventi:

```text
FILE_MOVED from=$ROOT/src/old.txt to=$ROOT/dst/new.txt
FILE_RENAMED from=$ROOT/src/old.txt to=$ROOT/dst/new.txt
```

Il core produce un evento:

```text
FILE_RELOCATED from=$ROOT/src/old.txt to=$ROOT/dst/new.txt
```

Qui il core esprime con un solo evento il fatto che il file ha cambiato sia
directory contenitore sia nome.

Decisione:

```text
preferiamo un evento unico FILE_RELOCATED, come core.
```

Questa e' una decisione di API semantica, non solo una differenza tecnica. Se
un'applicazione vuole trattare `FILE_RELOCATED` come "move + rename", potra'
farlo a livello applicativo.

### Differenza attesa: move and rename directory

Lo scenario `move_rename_dir` verifica lo stesso ragionamento, ma applicato a
una directory.

Scenario:

```text
mkdir $ROOT/src
mkdir $ROOT/dst
mkdir $ROOT/src/before
mv $ROOT/src/before $ROOT/dst/after
```

Dal punto di vista del path, cambiano due cose:

- la directory contenitore passa da `$ROOT/src` a `$ROOT/dst`
- il nome passa da `before` ad `after`

Il legacy puo' produrre due eventi separati:

```text
DIR_MOVED from=$ROOT/src/before to=$ROOT/dst/after
DIR_RENAMED from=$ROOT/src/before to=$ROOT/dst/after
```

Il core produce invece un evento semantico unico per lo spostamento/rinomina:

```text
DIR_RELOCATED from=$ROOT/src/before to=$ROOT/dst/after
```

In piu', se `src/before` viene creata prima che Alfred riesca ad aggiungere il
watch su `src`, il core puo' recuperare anche:

```text
DIR_CREATED path=$ROOT/src/before
```

Questo `DIR_CREATED` arriva da un raw event sintetico generato durante lo scan
ricorsivo mirato.

Decisione:

```text
anche per le directory preferiamo un evento unico DIR_RELOCATED.
```

Questa differenza non va corretta facendo emettere al core due eventi. Va
trattata come differenza attesa, perche' conferma che il core sta applicando la
semantica target descritta in `13-semantica-eventi.md`.

### Caso allineato: move directory semplice

Lo scenario `move_dir` isola il caso in cui una directory cambia contenitore ma
non cambia nome.

Scenario:

```text
mkdir $ROOT/src
mkdir $ROOT/dst
mkdir $ROOT/src/item
mv $ROOT/src/item $ROOT/dst/item
```

Le pause inserite nello scenario servono a non mischiare questo caso con il bug
delle creazioni ricorsive veloci. In questo modo Alfred ha tempo di aggiungere i
watch su `src` e `dst` prima della creazione e dello spostamento di `item`.

Risultato osservato:

```text
Legacy:
  DIR_CREATED path=$ROOT/src
  DIR_CREATED path=$ROOT/dst
  DIR_CREATED path=$ROOT/src/item
  DIR_MOVED from=$ROOT/src/item to=$ROOT/dst/item

Core:
  DIR_CREATED path=$ROOT/src
  DIR_CREATED path=$ROOT/dst
  DIR_CREATED path=$ROOT/src/item
  DIR_MOVED from=$ROOT/src/item to=$ROOT/dst/item
```

Conclusione:

```text
DIR_MOVED resta l'evento corretto quando cambia solo la directory contenitore.
```

Questo scenario e' importante perche' separa il caso `DIR_MOVED` dal caso
`DIR_RELOCATED`: se il nome rimane uguale, il core non deve produrre
`DIR_RELOCATED`.

### Differenza attesa: modify, close-write e file-ready

Lo scenario `modify_close_write_file` osserva l'integrazione tra eventi di
scrittura del backend e semantica del core.

Scenario:

```text
write $ROOT/editable.txt
append $ROOT/editable.txt
flush/fsync
close $ROOT/editable.txt
```

Il core contiene gia' la semantica:

```text
ALFRED_RAW_MODIFY      -> FILE_MODIFIED
ALFRED_RAW_CLOSE_WRITE -> FILE_READY
```

Il backend inotify ora usa una maschera di watch che include:

```text
IN_MODIFY
IN_CLOSE_WRITE
```

Per questo il risultato osservato e':

```text
Legacy:
  FILE_CREATED path=$ROOT/editable.txt

Core:
  FILE_CREATED path=$ROOT/editable.txt
  FILE_MODIFIED path=$ROOT/editable.txt
  FILE_READY path=$ROOT/editable.txt
  FILE_MODIFIED path=$ROOT/editable.txt
  FILE_READY path=$ROOT/editable.txt
```

Con `--keep-logs`, il `raw.log` conferma che arrivano eventi kernel distinti
per la creazione e per la modifica successiva:

```text
IN_CREATE wd=1 path=$ROOT name=editable.txt
IN_MODIFY wd=1 path=$ROOT name=editable.txt
IN_CLOSE_WRITE wd=1 path=$ROOT name=editable.txt
IN_MODIFY wd=1 path=$ROOT name=editable.txt
IN_CLOSE_WRITE wd=1 path=$ROOT name=editable.txt
```

Il legacy non cambia perche' `modules/inotify/src/events.c` ignora ancora
`IN_MODIFY` e `IN_CLOSE_WRITE`. Il core invece riceve quei raw event attraverso
`inotify_adapter.c` e li trasforma in eventi semantici.

Decisione semantica:

```text
FILE_MODIFIED e FILE_READY sono eventi semantici ufficiali del core.
```

Questa scelta e' delicata perche' rende piu' ricco lo stream degli eventi file:
una semplice scrittura puo' produrre `FILE_CREATED`, `FILE_MODIFIED` e
`FILE_READY`. Per applicazioni come indicizzatori o backup questo e' utile,
perche' `FILE_MODIFIED` segnala che il contenuto e' cambiato e `FILE_READY`
segnala che lo scrittore ha chiuso il file.

Questi eventi non vanno deduplicati tra loro: rappresentano fasi diverse della
vita del file. Il debounce del core resta invece utile per ridurre storm di
`FILE_MODIFIED` durante scritture ripetute o molto ravvicinate.

### Caso delicato: recursive create nested directory

Lo scenario `recursive_create_nested_dir` riproduce il bug documentato in:

```text
https://github.com/kinderp/alfred/issues/2
```

Scenario:

```text
mkdir -p $ROOT/one/two/three
```

Il punto delicato e' il tempo. Il sistema operativo crea `one`, poi `two`, poi
`three` molto rapidamente. Alfred riceve l'evento `IN_CREATE | IN_ISDIR` per
`one`, ma `two` e `three` possono essere gia' state create prima che Alfred
riesca ad aggiungere il watch su `one` e poi su `one/two`.

Il codice attuale mitiga il problema con un attraversamento ricorsivo:

```text
DIR_CREATED one
    -> watch_manager_add_recursive(one)
        -> trova one/two
        -> aggiunge WATCH_ADDED one/two
        -> trova one/two/three
        -> aggiunge WATCH_ADDED one/two/three
```

Risultato osservato prima del recupero sintetico:

```text
Legacy:
  DIR_CREATED path=$ROOT/one

Core:
  DIR_CREATED path=$ROOT/one
```

Legacy e core coincidevano, ma coincidevano su un risultato incompleto. Mancavano:

```text
DIR_CREATED path=$ROOT/one/two
DIR_CREATED path=$ROOT/one/two/three
```

Con `--keep-logs`, pero', il file `events.log` completo mostra anche:

```text
WATCH_ADDED path=$ROOT/one
WATCH_ADDED path=$ROOT/one/two
WATCH_ADDED path=$ROOT/one/two/three
```

Il `raw.log`, invece, mostra solo l'evento kernel per `one`:

```text
IN_CREATE IN_ISDIR path=$ROOT name=one
```

Dopo l'aggiunta dei raw event sintetici verso il core, il confronto diventa:

```text
Legacy:
  DIR_CREATED path=$ROOT/one

Core:
  DIR_CREATED path=$ROOT/one
  DIR_CREATED path=$ROOT/one/two
  DIR_CREATED path=$ROOT/one/two/three

Only in core:
  DIR_CREATED path=$ROOT/one/two
  DIR_CREATED path=$ROOT/one/two/three
```

Questa differenza e' voluta durante lo shadow mode: il core sta diventando piu'
completo del legacy per questo caso.

Questa e' la distinzione importante:

- lo stato di monitoraggio del backend viene aggiornato correttamente
- lo stream semantico legacy resta incompleto perche' mancano i `DIR_CREATED`
  delle directory scoperte dallo scan ricorsivo

La mitigazione originale permetteva al backend di monitorare le directory
scoperte, ma non ricostruiva automaticamente gli eventi semantici `DIR_CREATED`
mancanti. Il recupero sintetico aggiunge il pezzo mancante: quando lo scan
ricorsivo scopre una directory gia' presente, l'app invia al core un raw event
sintetico `CREATE | ISDIR`.

Per questo scenario non usiamo `--strict`: prima osserviamo l'output legacy e
core, poi decidiamo la semantica e la strategia di recupero.

Domande da discutere dopo l'osservazione:

- il core deve ricevere eventi raw sintetici per `one/two` e `one/two/three`?
- lo scan mirato deve restare sincrono nel percorso di gestione di `DIR_CREATED`
  o conviene introdurre un thread separato?
- dove va gestita l'eventuale deduplica se arriva sia l'evento reale sia quello
  sintetico?

Decisione implementata:

```text
usiamo scan mirato sincrono e raw event sintetici verso il core.
```

Non introduciamo subito un thread separato. Il thread avrebbe senso per una
risincronizzazione periodica generale o per recuperare da `OVERFLOW`, ma per
questo bug lo scan avviene gia' nel punto giusto: subito dopo la scoperta della
prima directory creata.

Nel primo passo non aggiungiamo una cache di dedup. Osserviamo il comportamento
in shadow mode. Se compaiono doppi `DIR_CREATED`, aggiungeremo una deduplica
piccola e localizzata nel layer backend/app, cioe' vicino al codice che genera
gli eventi sintetici.

### Controllo dedup: recursive create slow nested directory

Lo scenario `recursive_create_slow_nested_dir` crea lo stesso albero di
`recursive_create_nested_dir`, ma inserisce pause tra una `mkdir` e la
successiva:

```text
mkdir $ROOT/one
sleep
mkdir $ROOT/one/two
sleep
mkdir $ROOT/one/two/three
```

Scopo:

```text
verificare se il recupero sintetico produce doppi DIR_CREATED
quando inotify ha il tempo di consegnare gli eventi reali.
```

Risultato osservato:

```text
Legacy:
  DIR_CREATED path=$ROOT/one
  DIR_CREATED path=$ROOT/one/two
  DIR_CREATED path=$ROOT/one/two/three

Core:
  DIR_CREATED path=$ROOT/one
  DIR_CREATED path=$ROOT/one/two
  DIR_CREATED path=$ROOT/one/two/three
```

Il core non emette duplicati in questo scenario. Per ora la deduplica resta una
possibile protezione futura, non un requisito immediato.

## Prossimi scenari da aggiungere

Scenari utili:

- queue overflow, se riproducibile
- run core-mode sistematiche per tutti gli scenari shadow
- confronto tra suite funzionale storica legacy-shadow e `make test-core`

Ogni scenario dovrebbe essere piccolo e leggibile.
