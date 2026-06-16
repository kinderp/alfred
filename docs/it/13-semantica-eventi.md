# Semantica degli eventi

Questo capitolo raccoglie le decisioni sulla semantica degli eventi prodotti da
Alfred.

La parola "semantica" indica il significato che assegniamo a un evento. Non
descrive solo cosa arriva dal sistema operativo, ma cosa Alfred vuole comunicare
alle applicazioni che useranno il core.

## Perche' serve una semantica esplicita

`inotify` produce eventi molto vicini al kernel Linux. Alcuni sono gia' facili
da capire, per esempio `IN_CREATE`; altri sono dettagli tecnici, per esempio
`IN_IGNORED`, che indica che un watch e' stato rimosso.

Il core non deve copiare automaticamente tutti i dettagli di `inotify`. Il suo
compito e' produrre eventi stabili, leggibili e utili anche se un giorno il
backend non sara' piu' `inotify`.

Questo e' il motivo principale per cui separiamo:

- eventi raw/backend
- eventi semantici del core

## Eventi raw/backend

Un evento raw/backend descrive cosa e' successo nel backend specifico.

Esempi:

```text
inotify ha ricevuto IN_CREATE
inotify ha aggiunto un watch
inotify ha rimosso un watch
inotify ha segnalato IN_IGNORED
```

Questi eventi sono utili per debug, test diagnostici e studio del backend.
Non sono necessariamente parte dell'API semantica del core.

Nel codice attuale il backend logga gia':

```text
WATCH_ADDED
WATCH_REMOVED
```

Lo fa in `modules/inotify/src/watch_manager.c` con `logger_event()`. Inoltre
il backend inotify gestisce `IN_IGNORED` rimuovendo il watch dalla tabella.

Decisione architetturale:

```text
WATCH_ADDED e WATCH_REMOVED devono restare visibili come diagnostica del
backend, ma non devono diventare eventi semantici del core.
```

In futuro potremo spostare o duplicare questi messaggi nel raw/backend log. La
cosa importante e' che non siano trattati come eventi di dominio allo stesso
livello di `FILE_CREATED`, `DIR_DELETED` o `FILE_RENAMED`.

## Scrittura file, modify e file-ready

Il core distingue tra tre concetti che spesso, nel filesystem, appaiono vicini
nel tempo:

```text
FILE_CREATED
FILE_MODIFIED
FILE_READY
```

`FILE_CREATED` indica che un nuovo path e' apparso.

`FILE_MODIFIED` indica che il contenuto di un file e' stato modificato. Nel
backend inotify questo deriva da `IN_MODIFY`, che il bridge traduce in
`ALFRED_RAW_MODIFY`.

`FILE_READY` indica che un processo ha chiuso un file dopo averlo aperto in
scrittura. Nel backend inotify questo deriva da `IN_CLOSE_WRITE`, che il bridge
traduce in `ALFRED_RAW_CLOSE_WRITE`.

La differenza didattica e' importante:

```text
modify      = il contenuto sta cambiando o e' cambiato
close-write = lo scrittore ha chiuso il file
file-ready  = Alfred considera il file pronto per essere consumato
```

Per esempio, un indicizzatore potrebbe ignorare molti `FILE_MODIFIED` intermedi
e aspettare `FILE_READY` prima di leggere il file.

Decisione semantica:

```text
FILE_CREATED, FILE_MODIFIED e FILE_READY sono eventi distinti.
```

Non sono duplicati tra loro. Raccontano fasi diverse della vita di un file:

- `FILE_CREATED`: il path e' comparso nel filesystem
- `FILE_MODIFIED`: il contenuto e' stato scritto o modificato
- `FILE_READY`: uno scrittore ha chiuso il file dopo averlo scritto

Questa scelta rende piu' ricco lo stream degli eventi. Una creazione file reale
puo' quindi produrre:

```text
FILE_CREATED
FILE_MODIFIED
FILE_READY
```

Una modifica successiva dello stesso file puo' produrre:

```text
FILE_MODIFIED
FILE_READY
```

Il motivo e' pratico. Molte applicazioni non vogliono solo sapere che un path e'
apparso: vogliono sapere anche quando il contenuto e' cambiato e quando e'
ragionevole leggerlo. Un backup, un indicizzatore o uno scanner possono usare
`FILE_MODIFIED` per sapere che un file e' sporco e `FILE_READY` per sapere che
lo scrittore ha terminato almeno una fase di scrittura.

`FILE_MODIFIED` puo' essere rumoroso durante scritture lunghe o editor che
salvano in piu' passaggi. Per questo resta valido il debounce del core:

```text
ALFRED_RAW_MODIFY -> FILE_MODIFIED, con debounce
```

`FILE_READY`, invece, deriva da un close-write e non va trattato come un
duplicato di `FILE_MODIFIED`:

```text
ALFRED_RAW_CLOSE_WRITE -> FILE_READY
```

Ogni `FILE_READY` rappresenta una chiusura in scrittura osservata dal backend.
Puo' essercene piu' di uno per lo stesso file se il file viene scritto, chiuso,
riaperto, scritto di nuovo e richiuso.

Stato attuale dell'integrazione:

```text
il core supporta FILE_MODIFIED e FILE_READY;
il backend inotify ora abilita IN_MODIFY e IN_CLOSE_WRITE nella maschera
predefinita usata da config_t.inotify.watch_mask.
```

Quindi, nella suite core corrente, lo scenario `modify_close_write_file` mostra
il comportamento completo del core: una prima scrittura puo' produrre
`FILE_CREATED`, `FILE_MODIFIED` e `FILE_READY`; una modifica successiva puo'
produrre un'altra coppia `FILE_MODIFIED` / `FILE_READY`.

## Attributi e metadati

`IN_ATTRIB` e' l'evento inotify usato per cambiamenti di attributi del file o
della directory. La fonte primaria per questa semantica e' la pagina manuale
Linux `inotify(7)`: Alfred non inventa qui un significato nuovo, ma osserva il
fatto grezzo che il kernel espone.

Secondo `inotify(7)`, `IN_ATTRIB` puo' essere generato quando cambiano:

- permessi, per esempio tramite `chmod()` o `fchmod()`
- timestamp, per esempio tramite `utimensat()` o comandi come `touch -m`
- attributi estesi, per esempio tramite `setxattr()`
- numero di hard link, per esempio tramite `link()` o `unlink()`
- proprietario o gruppo, per esempio tramite `chown()`

Esempi pratici:

```bash
chmod 600 file.txt
touch -m file.txt
setfattr -n user.example -v value file.txt
ln file.txt file-hardlink.txt
rm file-hardlink.txt
chown user:group file.txt
```

Questi esempi non hanno tutti lo stesso peso nei test automatici. `chmod` e'
semplice, portabile e non richiede privilegi particolari. `chown` puo' richiedere
permessi diversi in base all'utente che esegue la suite; gli attributi estesi
possono dipendere dal filesystem e dagli strumenti installati; i cambiamenti di
hard link sono semanticamente diversi da un cambio permessi, anche se il kernel
li notifica con lo stesso evento `IN_ATTRIB`.

Per questo motivo il test backend attuale usa `chmod` come caso rappresentativo:
serve a verificare che Alfred abbia davvero incluso `IN_ATTRIB` nella maschera
di watch e che il raw log mostri il fatto grezzo. Non stiamo ancora fissando un
contratto utente per tutti i possibili cambiamenti di metadati.

Nel backend Alfred corrente:

```text
IN_ATTRIB -> ALFRED_RAW_ATTRIB
```

Questa traduzione e' gia' supportata dall'adapter. La maschera predefinita del
backend ora include `IN_ATTRIB`, quindi il raw log puo' mostrare questi eventi.

La semantica core, pero', non e' ancora stata scelta. Oggi
`ALFRED_RAW_ATTRIB` e' un fatto raw osservabile, ma non produce ancora un evento
utente come:

```text
FILE_METADATA_CHANGED
DIR_METADATA_CHANGED
```

Questa e' una scelta intenzionale. Prima di aggiungere un evento semantico
bisogna decidere:

- se distinguere file e directory
- se il nome debba parlare di attributi o metadati
- se includere permessi, owner, timestamp e altri cambiamenti nello stesso evento
- se applicare debounce o deduplica anche ai cambiamenti di attributi
- quali scenari utente hanno davvero bisogno di questo evento
- se testare separatamente permessi, timestamp, xattr, hard link e owner
- quali casi sono abbastanza portabili per entrare nella suite ufficiale

Per ora `IN_ATTRIB` serve come osservabilita' raw/backend. La semantica ufficiale
verra' aggiunta solo dopo una decisione esplicita.

La classificazione completa degli eventi `IN_*`, compresi gli eventi che Alfred
non supporta ancora e i motivi della scelta, e' mantenuta in
[Matrice eventi inotify](20-matrice-eventi-inotify.md). Questo evita di
confondere la semantica stabile del core con tutti i dettagli tecnici esposti
dal backend Linux.

## Eventi semantici

Un evento semantico descrive il significato dell'operazione vista da Alfred.

Esempi:

```text
FILE_CREATED
FILE_READY
FILE_MODIFIED
FILE_DELETED
FILE_RENAMED
FILE_MOVED
FILE_RELOCATED
DIR_CREATED
DIR_DELETED
DIR_RENAMED
DIR_MOVED
DIR_RELOCATED
OVERFLOW
```

Questi eventi sono quelli che vogliamo offrire alle applicazioni che useranno il
core: indicizzatori, backup, strumenti di sicurezza, interfacce grafiche o altri
moduli.

## Il core e' il riferimento

Durante la migrazione abbiamo confrontato il vecchio dispatcher e il core.
Quella fase e' chiusa: oggi il runtime ufficiale e' il core.

Decisione:

```text
il core definisce la semantica target.
```

Il comportamento legacy resta solo un riferimento storico recuperabile da Git.
La documentazione corrente deve descrivere la semantica del core.

## Directory contenitore

Nel codice del core compare il concetto di "parent directory". In questa
documentazione useremo il termine italiano "directory contenitore", perche' e'
piu' chiaro.

La directory contenitore e' la directory che contiene direttamente un file o una
directory.

Esempi:

```text
/tmp/progetto/a.txt
```

La directory contenitore e':

```text
/tmp/progetto
```

Il nome dell'oggetto e':

```text
a.txt
```

Altro esempio:

```text
/home/studente/src
```

La directory contenitore e':

```text
/home/studente
```

Il nome dell'oggetto e':

```text
src
```

Questa distinzione serve soprattutto per capire move e rename.

## Rename, move e relocate

Quando un oggetto cambia posizione, Alfred guarda due informazioni:

- la directory contenitore prima e dopo
- il nome dell'oggetto prima e dopo

Se cambia solo il nome, l'evento e' un rename.

```text
prima: /tmp/progetto/a.txt
dopo:  /tmp/progetto/b.txt
```

La directory contenitore e' la stessa:

```text
/tmp/progetto
```

Il nome cambia:

```text
a.txt -> b.txt
```

Evento:

```text
FILE_RENAMED
```

Se cambia solo la directory contenitore, l'evento e' un move.

```text
prima: /tmp/progetto/a.txt
dopo:  /tmp/archivio/a.txt
```

La directory contenitore cambia:

```text
/tmp/progetto -> /tmp/archivio
```

Il nome resta uguale:

```text
a.txt
```

Evento:

```text
FILE_MOVED
```

Se cambiano sia la directory contenitore sia il nome, l'evento e' un relocate.

```text
prima: /tmp/progetto/a.txt
dopo:  /tmp/archivio/b.txt
```

La directory contenitore cambia:

```text
/tmp/progetto -> /tmp/archivio
```

Il nome cambia:

```text
a.txt -> b.txt
```

Evento:

```text
FILE_RELOCATED
```

La stessa regola vale per le directory:

```text
DIR_RENAMED
DIR_MOVED
DIR_RELOCATED
```

## Tabella decisionale

| Caso | Directory contenitore | Nome | Evento file | Evento directory |
| --- | --- | --- | --- | --- |
| Creazione | n/a | n/a | `FILE_CREATED` | `DIR_CREATED` |
| Eliminazione | n/a | n/a | `FILE_DELETED` | `DIR_DELETED` |
| Rename | uguale | diverso | `FILE_RENAMED` | `DIR_RENAMED` |
| Move | diversa | uguale | `FILE_MOVED` | `DIR_MOVED` |
| Relocate | diversa | diverso | `FILE_RELOCATED` | `DIR_RELOCATED` |
| Overflow coda kernel | n/a | n/a | `OVERFLOW` | `OVERFLOW` |
| Aggiunta watch | dettaglio backend | dettaglio backend | nessun evento core | nessun evento core |
| Rimozione watch | dettaglio backend | dettaglio backend | nessun evento core | nessun evento core |

## Creazioni ricorsive veloci

Un caso importante e' la creazione ricorsiva di directory, per esempio:

```text
mkdir -p /tmp/progetto/one/two/three
```

Dal punto di vista semantico, l'albero osservato ha acquisito tre nuove
directory:

```text
DIR_CREATED /tmp/progetto/one
DIR_CREATED /tmp/progetto/one/two
DIR_CREATED /tmp/progetto/one/two/three
```

Il fatto che `inotify` possa notificare solo la prima creazione, perche' le
sottodirectory nascono prima dell'aggiunta dei watch, e' un limite del backend.
Non cambia la semantica desiderata: una applicazione che ascolta il core e'
interessata a sapere che quelle directory esistono.

La strategia tecnica per recuperare gli eventi mancanti va discussa separatamente
dalla semantica. Una possibilita' e' generare eventi raw sintetici quando lo scan
ricorsivo mirato scopre directory gia' presenti ma non ancora notificate.

### Decisione per il bug `mkdir -p`

Per il bug delle directory annidate create velocemente scegliamo questa
strategia:

```text
scan mirato sincrono -> raw event sintetico -> core -> DIR_CREATED
```

Non introduciamo subito un thread separato. Il problema nasce nel momento in cui
gestiamo `DIR_CREATED` della prima directory osservata, quindi la soluzione piu'
semplice e' restare nello stesso flusso:

```text
IN_CREATE one
    -> Alfred aggiunge il watch su one
    -> Alfred attraversa one
    -> trova one/two gia' esistente
    -> aggiunge il watch su one/two
    -> genera un raw event sintetico CREATE|ISDIR per one/two
    -> il core emette DIR_CREATED one/two
```

Un thread separato puo' essere utile in futuro per una sincronizzazione
periodica globale o per recuperare da `OVERFLOW`, ma per questo bug aggiunge
complessita' non necessaria: gestione del ciclo di vita del thread, locking,
ordine degli eventi, shutdown e deduplica concorrente.

### Eventi raw sintetici

Un evento raw sintetico e' un `alfred_raw_event_t` creato dal programma, non
ricevuto direttamente dal kernel.

Esempio:

```text
source = ALFRED_SRC_INOTIFY
mask   = ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR
path   = /tmp/progetto/one/two
cookie = 0
pid    = 0
```

E' sintetico perche' `inotify` non ha prodotto un `IN_CREATE` per
`/tmp/progetto/one/two`; Alfred ha scoperto quella directory attraversando il
filesystem. Pero' il fatto osservato e' reale: la directory esiste ed e' nuova
rispetto allo stato monitorato.

Il backend puo' creare il raw event sintetico, ma non deve emettere direttamente
`DIR_CREATED`. Il raw event deve entrare nel core, cosi' la semantica resta
centralizzata.

### Dedup

`Dedup` significa deduplicazione: evitare di emettere due volte lo stesso evento
logico.

Il rischio nasce cosi':

```text
1. lo scan mirato scopre /tmp/progetto/one/two
2. Alfred genera un raw event sintetico DIR create
3. subito dopo arriva anche un evento reale IN_CREATE per lo stesso path
```

Se non facciamo dedup, il core potrebbe emettere:

```text
DIR_CREATED /tmp/progetto/one/two
DIR_CREATED /tmp/progetto/one/two
```

Nel runtime corrente non abbiamo aggiunto una cache di dedup dedicata per i
raw create sintetici. Se in futuro compariranno duplicati reali, la deduplica
andra' preferibilmente nel layer backend che genera gli eventi sintetici,
perche' il duplicato nasce dal recupero specifico di inotify. Il core potra'
restare concentrato sulla semantica comune.

Una possibile dedup futura e':

```text
quando genero un CREATE sintetico per path X,
memorizzo X per una piccola finestra temporale;
se arriva un IN_CREATE reale per X subito dopo,
non lo inoltro una seconda volta al core.
```

## Perche' RELOCATED e' un solo evento

Il legacy, nello scenario in cui un oggetto cambia sia directory sia nome, puo'
produrre due eventi:

```text
FILE_MOVED oppure DIR_MOVED
FILE_RENAMED oppure DIR_RENAMED
```

Il core produce invece:

```text
FILE_RELOCATED oppure DIR_RELOCATED
```

Preferiamo il comportamento del core perche' l'operazione osservata e' una sola:
un oggetto passa da un path sorgente a un path destinazione. Se un'applicazione
vuole trattare il relocate come "move + rename", potra' farlo lei. Se invece il
core emettesse sempre due eventi, sarebbe piu' difficile per l'applicazione
ricostruire che si trattava di una sola operazione logica.

Esempio su directory:

```text
prima: /tmp/progetto/src/before
dopo:  /tmp/progetto/dst/after
```

La directory contenitore cambia:

```text
/tmp/progetto/src -> /tmp/progetto/dst
```

Il nome cambia:

```text
before -> after
```

Evento core:

```text
DIR_RELOCATED
```

Anche se il vecchio dispatcher puo' descrivere questo caso come `DIR_MOVED`
piu' `DIR_RENAMED`, la semantica target del core resta un evento unico.

## Overflow

`OVERFLOW` indica che la coda del backend ha perso eventi.

Questo e' un evento semantico importante, non solo diagnostico, perche'
l'applicazione non puo' piu' fidarsi completamente della sequenza ricevuta.

Quando arriva `OVERFLOW`, una applicazione robusta dovrebbe pianificare una
risincronizzazione completa dello stato osservato.

## Regola pratica

Per decidere se qualcosa deve essere un evento semantico del core, chiediti:

```text
una applicazione portabile deve reagire a questo evento come parte del dominio?
```

Se la risposta e' si', probabilmente deve essere un evento core.

Se la risposta e':

```text
serve soprattutto a capire come si comporta il backend
```

allora deve restare diagnostica raw/backend.
