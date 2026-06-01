# Roadmap scanner e resync

Questo capitolo raccoglie la progettazione iniziale dello scanner filesystem e
della futura logica di resync. Nasce dalla discussione sugli eventi
`IN_DELETE_SELF`, `IN_MOVE_SELF`, `IN_Q_OVERFLOW` e `IN_UNMOUNT`.

L'obiettivo e' evitare di aggiungere piccole patch scollegate al backend
inotify. Serve invece un componente di attraversamento dell'albero che possa
essere riusato in due contesti:

- recovery/resync del backend quando lo stato osservato non e' piu' affidabile
- indicizzazione esplicita di un albero, eventualmente esposta in futuro dalla
  riga di comando

## Stato codice prima dello scanner

Prima di iniziare questo lavoro abbiamo verificato che non esiste ancora codice
runtime che implementi una semantica specifica per `IN_DELETE_SELF` o
`IN_MOVE_SELF`.

Stato attuale:

- `IN_DELETE_SELF` e' nella maschera predefinita del backend inotify
- `IN_DELETE_SELF` e' accettato dal parser `inotify_watch_mask`
- `IN_DELETE_SELF` viene scritto nel raw log backend
- `IN_DELETE_SELF` non viene convertito in `ALFRED_RAW_DELETE`
- `IN_DELETE_SELF` non produce direttamente eventi core
- `IN_MOVE_SELF` non e' nella maschera predefinita
- `IN_MOVE_SELF` non e' accettato dal parser `inotify_watch_mask`
- `IN_MOVE_SELF` non viene nominato dal raw log formatter
- `IN_MOVE_SELF` non produce raw Alfred e non produce semantica core

La funzione attuale:

```text
watch_manager_add_recursive_with_discovery()
```

non e' uno scanner generale. Serve a un problema piu' stretto: quando arriva
`IN_CREATE | IN_ISDIR`, il backend attraversa subito la nuova directory per
aggiungere watch alle sottodirectory gia' presenti e generare eventuali raw
create sintetici. Questa funzione ripara il caso `mkdir -p`, ma non ha ancora
le caratteristiche necessarie per un resync generale o per indicizzare file.

## Decisioni sugli eventi SELF

`IN_DELETE_SELF` e `IN_MOVE_SELF` descrivono il path osservato direttamente,
non un figlio dentro la directory osservata.

Esempio figlio:

```text
watch su /tmp/root
rm /tmp/root/a.txt
    -> IN_DELETE name=a.txt
```

Esempio watch stesso:

```text
watch su /tmp/root
rm -rf /tmp/root
    -> IN_DELETE_SELF name=
    -> IN_IGNORED name=
```

Decisione per `IN_DELETE_SELF`:

- non inventare delete per tutti i figli partendo da `IN_DELETE_SELF`
- se il kernel produce davvero `IN_DELETE` per figli immediati, inoltrare quei
  fatti al core come oggi
- in futuro, valutare mapping di `IN_DELETE_SELF` a `ALFRED_RAW_DELETE` per il
  path osservato direttamente
- la semantica candidata sarebbe `DIR_DELETED` per root directory osservate

Decisione per `IN_MOVE_SELF`:

- non produrre `DIR_RENAMED`, `DIR_MOVED` o `DIR_RELOCATED` senza nuovo path
- trattarlo prima come problema di stato backend
- se una root osservata viene spostata, la tabella `wd -> path` puo' rimanere
  legata al vecchio path mentre il watch kernel segue ancora l'inode
- gli eventi successivi potrebbero quindi essere ricostruiti con path vecchi
- serve una policy di resync o invalidazione prima di decidere una semantica
  utente

`IN_IGNORED` resta manutenzione backend: indica che il kernel ha rimosso il
watch e quindi Alfred deve aggiornare la tabella `wd -> path`. Non e' un evento
semantico core.

## Perche' uno scanner separato

Lo scanner non dovrebbe essere un'estensione pesante del watch manager attuale.
Il watch manager deve restare responsabile di:

- `inotify_add_watch()`
- `inotify_rm_watch()`
- tabella `wd -> path`
- diagnostica `WATCH_ADDED` / `WATCH_REMOVED`
- discovery mirata per nuove directory osservate

Uno scanner generale deve invece poter:

- visitare directory e file
- scegliere se includere solo directory o anche file
- leggere metadati in modo controllato
- interrompersi su richiesta
- limitare profondita' o numero di entry
- gestire errori senza bloccare tutto lo scan
- funzionare senza dipendere da inotify
- produrre callback riusabili da backend, test e CLI future

Questa separazione mantiene chiari i livelli:

```text
watch_manager     -> stato dei watch inotify
scanner filesystem -> lettura efficiente dell'albero
backend inotify   -> decide quando usare watch manager o scanner
core              -> semantica sugli eventi raw
```

## Valutazione di nftw()

`nftw()` e' una funzione POSIX per attraversare ricorsivamente alberi di file.
Puo' essere utile per un prototipo o per spiegare il problema, ma non sembra la
base migliore per lo scanner finale di Alfred.

Vantaggi:

- e' gia' disponibile su sistemi POSIX
- riduce il codice iniziale
- supporta opzioni utili come `FTW_PHYS` per non seguire symlink

Limiti:

- la callback non riceve un `userdata` esplicito
- costringe a usare stato globale/statico o workaround per passare contesto
- offre poco controllo su batching, backpressure e cancellazione
- non e' ideale per integrare statistiche, limiti, policy di errore e resync
- non si adatta bene a un futuro uso come indicizzatore performante

Decisione proposta: non usare `nftw()` come base finale. Si puo' citare come
alternativa didattica o prototipo, ma lo scanner serio dovrebbe essere nostro.

## Direzione tecnica proposta

Implementare uno scanner custom basato su API directory controllabili:

```text
openat()
fdopendir()
readdir()
fstatat()
close()
```

Motivi:

- riduce alcune race rispetto a lavorare solo con path assoluti
- permette di attraversare usando file descriptor di directory
- permette di raccogliere metadati con `fstatat()` senza ricostruire ogni volta
  una ricerca assoluta dal root del filesystem
- rende naturale aggiungere limiti, cancellazione e statistiche
- prepara un futuro output da indicizzatore

## Perche' queste funzioni

Questa sezione spiega perche' il primo scanner usa queste primitive invece di
una funzione piu' alta come `nftw()` o di una ricorsione basata solo su
`opendir()` e path assoluti.

### `openat()`

`openat()` apre un figlio partendo dal file descriptor della directory padre.
In pratica, invece di dire al kernel:

```text
apri /tmp/root/one/two/three
```

lo scanner dice:

```text
sono gia' dentro /tmp/root/one/two, apri three
```

Questo ha tre vantaggi.

Il primo e' architetturale: la scansione resta legata alla directory che il
ciclo sta davvero leggendo. Quando `readdir()` restituisce un nome, `openat()`
apre quel nome rispetto allo stesso descriptor di directory. Il codice quindi
riflette meglio la relazione padre/figlio.

Il secondo e' pratico: in un albero profondo non costringiamo il kernel a
ripartire sempre da un path assoluto lungo. Alfred costruisce comunque il path
testuale per la callback, per i log e per i test, ma l'apertura operativa del
figlio puo' usare il descriptor del padre.

Il terzo riguarda le race. Nessuno scanner userspace puo' eliminare tutte le
race con un filesystem che cambia mentre viene letto, ma lavorare con descriptor
di directory riduce la finestra in cui il codice ragiona su un path assoluto
che potrebbe essere stato rinominato da un altro processo.

### `fdopendir()`

`fdopendir()` trasforma un file descriptor gia' aperto in uno stream `DIR *`
leggibile con `readdir()`.

Questa scelta e' coerente con `openat()`: lo scanner apre la directory con le
flag che gli servono, poi consegna quel descriptor a `fdopendir()` per
iterarne le entry.

Regola importante per chi legge il codice: dopo `fdopendir()`, lo stream
`DIR *` possiede il file descriptor. Per questo `scan_dir_fd()` deve chiudere
con `closedir()` e non con `close(fd)` nei percorsi normali. Il `close(fd)`
diretto resta corretto solo se `fdopendir()` fallisce, perche' in quel caso
lo stream non e' mai stato creato.

### `readdir()`

`readdir()` legge le entry di una directory una alla volta. E' una primitive
semplice, portabile e adatta a uno scanner streaming: non abbiamo bisogno di
caricare tutta la directory in memoria prima di iniziare a lavorare.

Questo e' importante per due motivi:

- un resync puo' avvenire su directory grandi
- un futuro comando di indicizzazione potrebbe dover attraversare alberi molto
  grandi senza usare memoria proporzionale al numero totale di file

Lo scanner ignora esplicitamente `"."` e `".."`, perche' non sono veri figli
da emettere al chiamante e porterebbero a cicli inutili.

### `fstatat()`

`fstatat()` legge i metadati di una entry partendo dal descriptor della
directory padre e dal nome restituito da `readdir()`.

Nel primo scanner usiamo `fstatat()` per classificare ogni entry in:

```text
directory
file regolare
symlink
altro
```

La scelta e' leggermente piu' costosa rispetto ad affidarsi sempre a
`struct dirent.d_type`, quando il filesystem lo fornisce, ma e' piu' robusta
come primo contratto:

- `d_type` puo' essere `DT_UNKNOWN` su alcuni filesystem
- `fstatat()` fornisce anche `struct stat`, gia' esposta alla callback
- la stessa funzione gestisce in modo chiaro la policy sui symlink
- il comportamento del test non dipende da dettagli del filesystem usato

Quando `follow_symlinks` e' disabilitato, lo scanner passa
`AT_SYMLINK_NOFOLLOW`. In questo modo un symlink viene classificato come
symlink e non come tipo del suo target. Questa e' la scelta conservativa per il
resync: Alfred non deve attraversare per sorpresa un altro albero solo perche'
dentro la root osservata esiste un link simbolico.

In futuro, se servira' ottimizzare alberi enormi, potremo aggiungere una policy
ibrida:

```text
usa d_type quando e' affidabile e non servono metadati completi
usa fstatat() quando d_type e' DT_UNKNOWN o quando servono struct stat/symlink
```

Per ora preferiamo una base piu' esplicita e testabile.

### `O_NOFOLLOW`, `O_CLOEXEC` e `O_DIRECTORY`

Quando apre una directory, lo scanner usa flag esplicite.

`O_DIRECTORY` dice al kernel che ci aspettiamo una directory. Se il path non e'
una directory, l'apertura fallisce subito invece di lasciare al codice il dubbio
su cosa sia stato aperto.

`O_NOFOLLOW` viene aggiunto quando `follow_symlinks` e' falso. Serve a evitare
che l'apertura di una directory attraversi un link simbolico. Questa scelta
mantiene il comportamento coerente con `AT_SYMLINK_NOFOLLOW`.

`O_CLOEXEC` chiude automaticamente il descriptor se il processo dovesse eseguire
un futuro `exec()`. Oggi Alfred non usa questa strada nello scanner, ma e' una
buona abitudine per evitare leak di descriptor in programmi che possono crescere
e integrare comandi esterni.

### `path_join()`

Lo scanner usa `path_join()` per costruire il path testuale da passare alla
callback.

Questo path non e' usato come unica base per continuare l'attraversamento,
perche' la discesa usa `openat()` rispetto al descriptor corrente. Serve pero'
al chiamante: il backend deve poter associare il fatto osservato a un path,
i test devono poter verificare cosa e' stato emesso, e una futura modalita'
di indicizzazione dovra' stampare o serializzare i path trovati.

### Prestazioni e compromesso iniziale

La scelta attuale privilegia controllo e correttezza didattica rispetto alla
micro-ottimizzazione.

Punti favorevoli per le prestazioni:

- attraversamento streaming con `readdir()`
- nessuna lista globale di entry caricata in memoria
- apertura dei figli relativa al descriptor del padre con `openat()`
- limiti gia' presenti per profondita' e numero di entry
- callback immediata, quindi il chiamante puo' fermare lo scan appena basta

Costo accettato nel primo step:

- ogni entry viene classificata con `fstatat()`
- il path testuale viene comunque costruito per ogni entry emessa o visitata

Questo costo e' accettabile adesso perche' stiamo fissando il contratto
corretto dello scanner. Dopo avere test su file, limiti, errori parziali e
resync reale, potremo misurare e decidere se usare `d_type` come fast path.

API concettuale:

```c
typedef enum {
    FS_SCAN_DIR,
    FS_SCAN_FILE,
    FS_SCAN_SYMLINK,
    FS_SCAN_OTHER
} fs_scan_type_t;

typedef struct {
    const char *path;
    fs_scan_type_t type;
    int depth;
    const struct stat *st;
} fs_scan_entry_t;

typedef int (*fs_scan_fn)(const fs_scan_entry_t *entry, void *userdata);
```

Opzioni utili:

```c
typedef struct {
    int include_files;
    int include_dirs;
    int follow_symlinks;
    int emit_root;
    int max_depth;
    size_t max_entries;
} fs_scan_options_t;
```

Questa API e' solo una proposta. Prima di scrivere codice bisogna decidere dove
mettere il componente e quali opzioni servono nel primo passo.

## Dove metterlo

Opzioni:

```text
app/src/fs_scanner.c
app/include/fs_scanner.h
```

oppure:

```text
modules/fs_scan/
```

Io eviterei di metterlo nel core: lo scan e' I/O filesystem, non semantica
degli eventi. Il core deve continuare a ricevere raw facts e produrre eventi
semantici, senza sapere come si attraversa un albero sul filesystem.

Scelta iniziale consigliata:

```text
app/include/fs_scanner.h
app/src/fs_scanner.c
```

Motivo: e' backend-neutral ma ancora parte del runtime applicativo. Se in futuro
diventa un modulo riusabile piu' grande, potremo spostarlo in `modules/fs_scan`
senza cambiare la semantica core.

## Uso per resync

Possibili trigger:

- `IN_MOVE_SELF`
- `IN_UNMOUNT`
- `ALFRED_RAW_OVERFLOW`
- richiesta manuale futura da CLI

Politiche possibili per `IN_MOVE_SELF`:

1. marcare il watch come stale e smettere di fidarsi dei path derivati da quel
   watch
2. rimuovere il watch e loggare una diagnostica backend
3. fare uno scan di una root ancora affidabile se esiste un antenato osservato
4. richiedere un resync esplicito all'utente o all'applicazione

Per ora la scelta piu' prudente e' non inventare un nuovo path. Senza una root
affidabile da cui ripartire, Alfred non puo' sapere dove sia stata spostata la
directory osservata.

## Uso per indicizzazione

Lo stesso scanner puo' diventare utile anche fuori dal resync.

Esempi futuri:

```bash
alfred --scan /path
alfred --scan --dirs-only /path
alfred --scan --json /path
```

Possibili output:

- plain text per uso didattico
- JSON lines per integrazione con altri tool
- solo directory per debug dei watch ricorsivi
- file + directory per indicizzazione iniziale

Questa funzione non va implementata subito se stiamo lavorando al backend, ma
conviene progettare lo scanner in modo che non la renda impossibile.

## Primo passo implementato

Il primo passo e' stato implementato in:

```text
app/include/fs_scanner.h
app/src/fs_scanner.c
tests/scanner/
```

Contratto iniziale:

- attraversamento directory-only di default
- callback con `userdata`
- root emessa di default
- symlink non seguiti di default
- limiti base: `max_depth` e `max_entries`
- attraversamento basato su `openat()`, `fdopendir()`, `readdir()` e
  `fstatat()`

Il target di test e':

```bash
make test-scanner
```

Il test iniziale crea un albero con directory annidate, un file e un symlink.
Verifica che lo scanner emetta solo root e directory reali e che non segua il
symlink.

## Come usare lo scanner dal codice

Lo scanner non e' un programma separato e non e' ancora esposto dalla CLI. Per
ora e' una piccola API C usata dai test e preparata per il futuro resync.

Un chiamante deve:

1. includere `fs_scanner.h`
2. inizializzare una `fs_scan_options_t`
3. scegliere quali entry ricevere
4. passare una callback a `fs_scan_tree()`

Esempio minimo:

```c
#include "fs_scanner.h"

static int on_scan_entry(const fs_scan_entry_t *entry, void *userdata)
{
    (void)userdata;

    /*
     * entry->path and entry->st are borrowed. Copy them here if they must live
     * after the callback returns.
     */
    printf("%d %s\n", entry->depth, entry->path);

    return 0;
}

void scan_example(const char *root)
{
    fs_scan_options_t opts;

    fs_scan_options_defaults(&opts);
    opts.include_files = 1;
    opts.max_depth = 3;

    (void)fs_scan_tree(root, &opts, on_scan_entry, NULL);
}
```

Il valore di ritorno della callback ha una semantica precisa:

- `0`: continua lo scan
- diverso da `0`: ferma lo scan senza errore

Questa scelta serve per casi come:

- trovare la prima directory che soddisfa una condizione
- limitare una futura indicizzazione
- interrompere un resync quando il chiamante ha gia' raccolto abbastanza dati

Le opzioni principali sono:

| Opzione | Default | Significato |
| --- | --- | --- |
| `include_dirs` | `1` | emette directory |
| `include_files` | `0` | emette file regolari |
| `include_symlinks` | `0` | emette link simbolici come entry proprie |
| `include_other` | `0` | emette fifo, socket, device e altri tipi |
| `follow_symlinks` | `0` | segue i symlink invece di classificarli come symlink |
| `emit_root` | `1` | emette anche il path root con profondita' `0` |
| `max_depth` | `-1` | profondita' massima; `-1` significa nessun limite |
| `max_entries` | `0` | numero massimo di callback; `0` significa nessun limite |

Esempi:

```c
/* Solo root. */
opts.max_depth = 0;

/* Root e figli diretti, ma non nipoti. */
opts.max_depth = 1;

/* Prime 100 entry emesse, poi stop pulito. */
opts.max_entries = 100;

/* Directory e file regolari. */
opts.include_files = 1;
```

La distinzione importante e':

```text
scan filesystem -> produce fatti osservati
backend/core    -> decidono se quei fatti diventano watch, raw o eventi
```

Quindi una callback dello scanner non dovrebbe inventare direttamente una
semantica core. Deve raccogliere fatti, aggiornare una struttura dati del
chiamante o chiedere al backend di applicare una policy esplicita.

## Funzioni e casi del prossimo passo

Il prossimo punto da decidere riguarda la policy sugli errori parziali. Per
capirlo bisogna distinguere le funzioni coinvolte e il tipo di errore che
possono produrre.

### `readdir()`

`readdir()` legge il prossimo nome dentro una directory gia' aperta.

Nel nostro scanner viene usata dentro `scan_dir_fd()`:

```text
DIR *dir = fdopendir(fd)
while ((ent = readdir(dir)) != NULL)
    ...
```

Se la directory contiene:

```text
a
b
c
```

`readdir()` restituisce un nome alla volta. Non garantisce che il filesystem
resti fermo mentre stiamo leggendo. Un altro processo potrebbe cancellare `b`
dopo che `readdir()` ha restituito il nome ma prima che Alfred legga i suoi
metadati.

Per questo il resync deve essere pensato come una fotografia imperfetta ma
utile, non come una transazione atomica sul filesystem.

### `fstatat()`

`fstatat()` prende:

- il file descriptor della directory padre
- il nome letto da `readdir()`
- una `struct stat *` da riempire
- flag come `AT_SYMLINK_NOFOLLOW`

Serve a rispondere a domande come:

```text
questa entry e' una directory?
e' un file regolare?
e' un symlink?
quali metadati ha?
```

Caso importante:

```text
1. readdir() legge "tmp"
2. un altro processo cancella "tmp"
3. fstatat() prova a leggere i metadati di "tmp"
4. fstatat() fallisce
```

Oggi lo scanner salta quell'entry e continua. Questa scelta e' ragionevole per
entry figlie transitorie, perche' durante un resync e' normale che il mondo
cambi sotto i nostri piedi.

### `openat()`

`openat()` apre una directory figlia partendo dal descriptor della directory
padre. Lo scanner lo usa solo quando una entry e' stata classificata come
directory e deve essere attraversata.

Caso:

```text
root/
    a/
        b/
```

Lo scanner legge `a`, capisce con `fstatat()` che e' una directory, poi usa
`openat()` per aprirla e scendere dentro.

Possibili fallimenti:

- la directory e' stata cancellata dopo `fstatat()`
- i permessi sono cambiati
- il filesystem ha restituito un errore I/O
- il path e' ancora visibile ma non e' piu' apribile

Qui dobbiamo scegliere la policy. Per un resync robusto, fallire tutto per una
singola directory figlia non leggibile potrebbe essere troppo aggressivo.
Tuttavia ignorare sempre l'errore senza registrarlo potrebbe nascondere problemi
reali.

Scelta implementata per il primo passo:

```text
root non apribile             -> ERR_IO
directory figlia sparita      -> skip + continua
directory figlia senza permessi -> skip + continua
entry trasformata in non-dir  -> skip + continua
errore I/O non classificato   -> ERR_IO
```

Nel codice questa decisione e' concentrata in
`child_open_error_is_recoverable()`. La funzione considera recuperabili
`ENOENT`, `ENOTDIR`, `EACCES` ed `EPERM` quando l'errore riguarda una directory
figlia aperta con `openat()`. La root resta gestita da `fs_scan_tree()` e quindi
continua a fallire con `ERR_IO` se non e' leggibile o non e' apribile.

### `fdopendir()`

`fdopendir()` trasforma un file descriptor aperto in uno stream `DIR *` usabile
con `readdir()`.

Se `fdopendir()` fallisce, la directory non e' realmente attraversabile con il
meccanismo scelto. Per la root e' quasi certamente un errore duro. Per una
directory figlia dobbiamo decidere se trattarlo come:

- errore duro dello scan
- errore parziale da saltare

La scelta dovrebbe essere coerente con `openat()`: se decidiamo che una
directory figlia non accessibile non deve fermare tutto il resync, allora anche
un fallimento di `fdopendir()` su una figlia dovrebbe diventare skip + continua.

### `path_join()`

`path_join()` costruisce il path testuale:

```text
parent = /tmp/root/a
name   = b
child  = /tmp/root/a/b
```

Questo path serve alla callback, ai test, ai log e alla futura indicizzazione.

Se `path_join()` fallisce, di solito significa che il path non entra in
`PATH_MAX`. Questo non e' un errore transitorio come una directory cancellata:
il chiamante non riceverebbe un path affidabile. Per ora e' corretto trattarlo
come `ERR_IO` o, in futuro, come errore specifico se aggiungeremo un codice piu'
preciso.

### `ERR_IO`

`ERR_IO` e' il codice generico usato quando lo scanner incontra un problema di
I/O o filesystem che non riesce a gestire localmente.

Il punto aperto non e' se `ERR_IO` serva: serve. Il punto e' decidere quando un
errore locale deve diventare errore dell'intero scan.

Principio proposto:

```text
errore sulla root       -> fallisce lo scan
errore su entry figlia  -> preferire skip + continua, se e' recuperabile
errore di path interno  -> fallisce lo scan
```

## Roadmap completa dello scanner

Questa roadmap divide lo scanner in passi piccoli. Alcuni sono necessari per il
resync, altri preparano usi futuri come indicizzazione e CLI.

### Fase 1 - Contratto base completato

Stato: implementato.

Risultato:

- API pubblica `fs_scan_tree()`
- opzioni `fs_scan_options_t`
- callback con `userdata`
- root emessa di default
- directory-only di default
- symlink non seguiti di default
- limiti `max_depth` e `max_entries`
- test `make test-scanner`
- documentazione tecnica e didattica iniziale

### Fase 2 - Errori parziali

Stato: primo passo implementato.

Obiettivo: decidere cosa succede quando una parte dell'albero non e' leggibile
o cambia durante lo scan.

Decisioni fissate:

- root non leggibile: hard failure
- entry sparita tra `readdir()` e `fstatat()`: skip
- directory figlia non apribile per `ENOENT`, `ENOTDIR`, `EACCES` o `EPERM`:
  skip
- path troppo lungo: hard failure

Il test scanner crea una directory `volatile`. La callback la rimuove dopo che
lo scanner l'ha osservata ma prima della discesa ricorsiva. Questo simula una
race reale: `fstatat()` vede una directory, poi `openat()` fallisce perche' la
directory non esiste piu'. Il contratto atteso e' che lo scan continui e ritorni
`ERR_OK`.

Decisioni ancora aperte:

- se aggiungere log diagnostici per directory figlie saltate
- se esporre statistiche di skip/errori al chiamante
- se distinguere errori I/O gravi da permessi o race transitorie con codici
  piu' specifici

Possibile implementazione futura:

```c
typedef struct {
    size_t emitted;
    size_t skipped;
    size_t io_errors;
    size_t permission_errors;
} fs_scan_stats_t;
```

Io non la introdurrei subito. Prima fissiamo la policy, poi aggiungiamo
statistiche solo se servono davvero al backend o alla CLI.

### Fase 3 - Symlink

Stato: in corso.

Obiettivo: distinguere due concetti diversi:

- `include_symlinks`: emetti il symlink come entry osservata
- `follow_symlinks`: attraversa il target del symlink

`include_symlinks` serve quando il chiamante vuole sapere che dentro l'albero
esiste un link simbolico, senza entrare nel suo target. Esempio:

```text
root/
    link_to_a -> a/
```

Con `include_symlinks = 1` e `follow_symlinks = 0`, lo scanner emette
`link_to_a` come `FS_SCAN_SYMLINK`. Non emette il contenuto del target come se
fosse una directory figlia.

Questa distinzione e' importante per due motivi:

- per il resync, sapere che un symlink esiste puo' essere utile come fatto
  osservato, ma seguirlo potrebbe far uscire Alfred dalla root osservata
- per una futura indicizzazione, un utente potrebbe voler vedere anche i
  symlink senza duplicare il contenuto raggiungibile tramite quei link

Decisione corrente:

- per resync: default conservativo, non seguire symlink
- `include_symlinks = 1` e' supportato e testato
- `follow_symlinks = 1` e' rimandato
- per indicizzazione: possibilita' futura di seguire symlink su richiesta, ma
  solo dopo avere una policy anti-cicli esplicita

Test aggiunto:

- symlink emesso come `FS_SCAN_SYMLINK` quando `include_symlinks = 1`
- symlink non seguito quando `follow_symlinks = 0`

Test rimandato:

- eventuale follow controllato quando `follow_symlinks = 1`

#### Policy anti-cicli per `follow_symlinks`

Seguire symlink e' delicato perche' un link puo' creare cicli o portare lo scan
fuori dall'albero scelto dall'utente.

Esempio di ciclo:

```text
root/
    a/
        back -> ../
```

Se lo scanner segue `back`, puo' tornare a `root`, poi rientrare in `a`, poi
seguire di nuovo `back`, e cosi' via.

Le policy comuni sono:

1. Non seguire symlink.

   E' la policy attuale e la piu' sicura per resync/watch. Il symlink puo'
   essere emesso come entry, ma il target non viene attraversato.

2. Tenere un set di directory gia' visitate usando `(st_dev, st_ino)`.

   `st_dev` identifica il device/filesystem, `st_ino` identifica l'inode. Se
   una directory target e' gia' stata visitata, lo scanner non rientra.

   ```text
   root visto
   a visto
   back punta a root, root gia' visto -> skip
   ```

3. Limitare la profondita' massima.

   `max_depth` e' una protezione utile, ma da sola non e' una vera policy
   anti-ciclo. Evita loop infiniti solo tagliando lo scan dopo una profondita'
   arbitraria.

4. Restare sotto la root iniziale.

   Prima di seguire un symlink, il target viene risolto e confrontato con la
   root reale dello scan. Se punta fuori root, viene saltato.

   ```text
   root/link -> /etc
   target fuori root -> skip
   ```

5. Non attraversare filesystem diversi.

   Lo scanner puo' salvare lo `st_dev` della root e saltare directory con
   device diverso. E' simile alla logica `find -xdev`.

6. Imporre un budget massimo di entry.

   `max_entries` limita il danno in caso di albero enorme o policy sbagliata,
   ma e' una rete di sicurezza, non una soluzione completa ai cicli.

Per Alfred, una futura implementazione di `follow_symlinks = 1` dovrebbe avere
almeno:

```text
visited set su (st_dev, st_ino)
max_depth come safety net
opzione per restare nello stesso device
opzione per restare sotto la root iniziale
```

Finche' queste policy non sono progettate e testate, `follow_symlinks = 1`
resta rimandato.

### Fase 4 - File e tipi speciali

Stato: implementato per il contratto iniziale.

`include_files = 1` e' gia' coperto dal test base. Ora e' coperto anche
`include_other = 1` usando una FIFO creata con `mkfifo`.

`include_other` serve a emettere entry che non sono:

- directory
- file regolari
- symlink

Esempi:

- FIFO / named pipe
- socket Unix
- device file
- altri tipi speciali del filesystem

Il test automatico usa una FIFO:

```bash
mkfifo "$TEST_ROOT/pipe.fifo"
```

Motivi:

- non richiede privilegi root
- e' stabile in un test locale
- viene classificata come `FS_SCAN_OTHER`
- non richiede di aprire o scrivere nella FIFO

Decisioni fissate:

- `include_other = 0` resta il default
- `include_other = 1` emette la FIFO come `FS_SCAN_OTHER`
- per il resync inotify, i tipi speciali non sono la parte piu' importante
- per una futura modalita' scan/index, sapere che esistono puo' essere utile

Restano da decidere:

- come trattare socket, fifo e device
- se questi tipi servono al resync o solo a una futura indicizzazione

Per ora non testiamo socket e device file. I socket richiedono piu' setup nel
test, mentre i device file possono richiedere privilegi o dipendere dalla
piattaforma. La FIFO basta per fissare il contratto pubblico:

```text
tipo non directory/file/symlink -> FS_SCAN_OTHER
```

Per il resync dei watch inotify, le directory restano la parte critica. File e
tipi speciali diventano importanti soprattutto se Alfred espone una modalita'
di scan/index.

### Fase 5 - Integrazione con watch manager

Stato: da progettare.

Obiettivo: valutare se sostituire o affiancare
`watch_manager_add_recursive_with_discovery()`.

Domande:

- lo scanner deve solo elencare directory e lasciare al watch manager
  `inotify_add_watch()`?
- il watch manager deve continuare a generare raw sintetici per directory
  scoperte?
- come evitiamo duplicazione tra discovery ricorsiva attuale e scanner?

Direzione probabile:

```text
scanner      -> trova directory
watch_manager -> aggiunge watch
backend      -> decide se generare raw sintetici
core         -> dedup/semantica
```

### Fase 6 - Resync dopo eventi critici

Stato: da progettare dopo la policy errori.

Trigger possibili:

- `IN_MOVE_SELF`
- `IN_DELETE_SELF`
- `IN_UNMOUNT`
- `IN_Q_OVERFLOW`

Uso possibile:

- marcare una subtree come stale
- fare scan da una root ancora affidabile
- ricostruire watch mancanti
- produrre diagnostica quando il path osservato non e' piu' affidabile

Per `IN_MOVE_SELF` resta il problema principale: senza un nuovo path non
possiamo inventare una relocation semantica. Lo scanner puo' aiutare solo se
abbiamo una root affidabile da cui ripartire.

### Fase 7 - CLI di indicizzazione

Stato: futura.

Esempi possibili:

```bash
alfred --scan /path
alfred --scan --dirs-only /path
alfred --scan --json /path
```

Questa fase non serve subito allo switch/resync. La teniamo in mente per non
disegnare un'API che la renda difficile.

### Fase 8 - Performance

Stato: futura, dopo test e policy.

Ottimizzazioni possibili:

- usare `dirent.d_type` come fast path quando affidabile
- chiamare `fstatat()` solo se servono metadati completi
- aggiungere statistiche di scan
- valutare batching o callback piu' specializzate

Non conviene ottimizzare prima di avere:

- contratto stabile
- test su errori/symlink
- primo uso reale nel backend

## Prossimi passi consigliati

1. decidere la policy sugli errori parziali di permesso/accesso
2. aggiungere test dedicati su `include_symlinks` e `follow_symlinks`
3. valutare se sostituire la discovery ricorsiva del watch manager
4. progettare l'uso per `IN_MOVE_SELF`
5. rimandare output CLI e JSON a un passo successivo

Questo approccio evita di mescolare subito tre problemi:

- implementare uno scanner robusto
- decidere la semantica di `IN_MOVE_SELF`
- aggiungere una feature utente di indicizzazione

Prima facciamo uno scanner piccolo, misurabile e testato. Poi decidiamo come
usarlo per resync.
