# Guida al C usato nel progetto

Questo capitolo spiega i costrutti C che compaiono nella codebase. Non e' una
guida completa al linguaggio C: e' una guida pratica per capire questo progetto.

## File `.c` e file `.h`

In C si separano spesso dichiarazioni e implementazioni.

I file `.h` dichiarano:

- tipi
- costanti
- funzioni disponibili ad altri file

I file `.c` implementano:

- il corpo delle funzioni
- funzioni private del file
- logica operativa

Esempio:

```text
app/include/logger.h   dichiara logger_init()
app/src/logger.c       implementa logger_init()
```

## Include guard

Gli header usano una protezione:

```c
#ifndef LOGGER_H
#define LOGGER_H

...

#endif
```

Serve a evitare che lo stesso header venga incluso due volte nello stesso file
di compilazione.

## `#include`

`#include` copia logicamente il contenuto di un header nel file corrente.

```c
#include "logger.h"
#include <stdio.h>
```

Differenza pratica:

- `"logger.h"`: header del progetto
- `<stdio.h>`: header della libreria standard o di sistema

## `struct`

Una `struct` raggruppa piu' campi in un unico tipo.

Esempio:

```c
typedef struct {
    FILE *raw;
    FILE *event;
    FILE *error;
} logger_t;
```

Qui `logger_t` rappresenta un logger con tre stream.

## `typedef`

`typedef` crea un nome piu' comodo per un tipo.

Senza `typedef` dovresti scrivere:

```c
struct app app;
```

Con `typedef` puoi scrivere:

```c
app_t app;
```

## Puntatori

Un puntatore contiene l'indirizzo di un oggetto.

Esempio:

```c
int app_init(app_t *app, int argc, char **argv);
```

`app_t *app` significa: la funzione riceve l'indirizzo di una struct `app_t`.

Questo permette alla funzione di modificare l'oggetto originale.

Esempio:

```c
app_t app;
app_init(&app, argc, argv);
```

`&app` significa "indirizzo di app".

## Puntatori a `void`

Un puntatore a `void` si scrive:

```c
void *userdata;
```

`void *` significa: "puntatore a qualcosa, ma non dico ancora a quale tipo".

E' un puntatore generico. Puo' contenere l'indirizzo di oggetti di tipi diversi:

```c
logger_t logger;
config_t config;

void *a = &logger;
void *b = &config;
```

Pero' c'e' una regola fondamentale: prima di usare il valore, bisogna sapere che
tipo reale contiene e convertirlo.

Esempio:

```c
void callback(void *userdata)
{
    logger_t *logger = userdata;

    logger_event(logger, "message");
}
```

Qui stiamo dicendo:

```text
userdata e' generico per chi chiama,
ma dentro questa callback io so che contiene un logger_t *.
```

### Perche' usare `void *`

`void *` serve quando vuoi scrivere codice generico.

Nel nostro progetto il core non deve conoscere `logger_t`, perche' `logger_t`
appartiene al livello `app`. Il core deve poter dire:

```text
quando produco un evento, chiamo questa funzione e le passo questo contesto
generico.
```

Il contesto generico e' `userdata`.

Oggi `userdata` sara' un `logger_t *`.

Domani potrebbe essere:

- una connessione socket
- un handle a database SQLite
- una struttura per una UI
- una coda di messaggi
- una struttura di test

Il core non cambia, perche' per lui e' sempre solo:

```c
void *userdata;
```

### Rischio dei `void *`

`void *` e' potente, ma non e' automaticamente sicuro.

Se passi un tipo e poi lo interpreti come un altro, il programma puo' rompersi.

Esempio sbagliato:

```c
config_t config;

core_logger_on_event(ev, &config); /* sbagliato */
```

Dentro `core_logger_on_event()` facciamo:

```c
logger_t *logger = userdata;
```

Ma `userdata` in realta' puntava a `config_t`. Il compilatore non puo'
proteggerti completamente da questo errore, perche' `void *` nasconde il tipo.

Regola pratica:

> Chi passa `userdata` e chi lo usa devono essere d'accordo sul tipo reale.

Nel nostro caso:

```c
alfred_create(&cfg, core_logger_on_event, &app->logger);
```

significa:

```text
core_logger_on_event ricevera' un logger_t * dentro userdata.
```

## Puntatori a funzione

In C anche una funzione ha un indirizzo. Un puntatore a funzione e' una
variabile che contiene l'indirizzo di una funzione.

Esempio semplice:

```c
int add(int a, int b)
{
    return a + b;
}

int (*operation)(int, int);

operation = add;
```

Ora puoi chiamare:

```c
int result = operation(2, 3);
```

e il risultato sara':

```text
5
```

La sintassi:

```c
int (*operation)(int, int);
```

significa:

```text
operation e' un puntatore a funzione
che prende due int
e restituisce int
```

Le parentesi intorno a `*operation` sono importanti.

Senza parentesi:

```c
int *operation(int, int);
```

significherebbe un'altra cosa: una funzione che restituisce `int *`.

## `typedef` per puntatori a funzione

La sintassi dei puntatori a funzione puo' essere difficile da leggere. Per
questo spesso si usa `typedef`.

Esempio:

```c
typedef int (*operation_fn)(int a, int b);
```

Ora puoi scrivere:

```c
operation_fn operation = add;
```

Nel progetto usiamo lo stesso principio:

```c
typedef void (*alfred_emit_fn)(
    const alfred_event_t *ev,
    void *userdata
);
```

Questo crea un nome per il tipo:

```text
alfred_emit_fn
```

che significa:

```text
puntatore a funzione che riceve:
- const alfred_event_t *ev
- void *userdata

e non restituisce nulla.
```

## Callback

Una callback e' una funzione che passi a un altro componente affinche' venga
chiamata piu' tardi.

Invece di fare:

```text
io chiamo direttamente il logger
```

il core fa:

```text
io chiamo la funzione che mi e' stata data
```

Questo rende il core piu' generico.

Nel nostro progetto:

```c
void core_logger_on_event(const alfred_event_t *ev, void *userdata);
```

questa funzione ha la forma richiesta da:

```c
alfred_emit_fn
```

Quindi puo' essere passata al core.

Uso previsto:

```c
alfred_create(&cfg, core_logger_on_event, &app->logger);
```

Significato:

```text
crea il core;
quando il core produce un evento semantico,
chiama core_logger_on_event();
passa &app->logger come userdata.
```

Dentro la callback:

```c
void core_logger_on_event(const alfred_event_t *ev, void *userdata)
{
    logger_t *logger = userdata;

    logger_event(logger, "...", ...);
}
```

Il core non sa che `userdata` e' un `logger_t *`. Lo sa solo la callback.

## Perche' le callback sono utili

Le callback separano chi produce un evento da chi decide cosa farne.

Nel nostro caso:

```text
core produce eventi semantici
app decide dove mandarli
```

Oggi possiamo loggare su file:

```text
alfred_event_t -> core_logger_on_event -> logger_event -> events.log
```

Domani possiamo cambiare destinazione senza modificare il core:

```text
alfred_event_t -> socket_callback -> rete
alfred_event_t -> sqlite_callback -> database
alfred_event_t -> ui_callback     -> interfaccia grafica
alfred_event_t -> test_callback   -> array in memoria per test
```

Questo e' un vantaggio architetturale importante:

- il core resta riusabile
- il core resta testabile
- il core non dipende dal logger
- l'applicazione puo' cambiare output
- i test possono intercettare eventi senza leggere file di log

## Callback e ownership

Quando il core chiama:

```c
core_logger_on_event(ev, userdata);
```

l'evento `ev` e' valido solo durante la callback.

Questo significa:

- puoi leggerlo
- puoi copiarne i campi
- non devi salvarne il puntatore per usarlo dopo

Se una callback vuole conservare l'evento, deve copiarlo in una memoria propria.

Questa regola e' scritta anche nell'header del core:

```text
event memory expires after callback returns
```

## Callback usate davvero in Alfred

Nel progetto il pattern piu' comune e':

```c
typedef tipo_ritorno (*nome_callback_t)(argomenti..., void *userdata);
```

oppure:

```c
typedef tipo_ritorno (*nome_callback_t)(void *userdata, argomenti...);
```

La posizione di `userdata` puo' cambiare da API ad API, ma il significato resta
lo stesso: e' il contesto opaco che il chiamante vuole ricevere quando la
callback verra' invocata.

La parola "opaco" significa: il componente che richiama la callback non sa che
tipo reale c'e' dietro `userdata`. Per lui e' solo un indirizzo generico. La
funzione callback invece conosce il tipo reale e lo converte.

Esempio concettuale:

```c
static int my_callback(const alfred_raw_event_t *raw, void *userdata)
{
    app_t *app = userdata;

    return alfred_process(app->core, raw);
}
```

La funzione che chiama `my_callback()` non deve conoscere `app_t`. Deve solo
sapere che deve passare lo stesso `userdata` ricevuto dal suo chiamante.

### Tabella dei callback principali

| Tipo callback | Dove vive | Chi lo chiama | A cosa serve |
| --- | --- | --- | --- |
| `alfred_emit_fn` | `core/include/alfred_correlator.h` | core semantico | consegnare un `alfred_event_t` prodotto dal core |
| `inotify_backend_event_fn` | `modules/inotify/include/inotify_backend.h` | backend inotify | consegnare un `alfred_raw_event_t` all'app/core boundary |
| `fs_scan_fn` | `app/include/fs_scanner.h` | scanner filesystem | notificare ogni entry trovata durante una scansione |
| `watcher_iter_fn` | `modules/inotify/include/watcher.h` | watcher table | visitare watch in uno stato specifico |
| `alfred_record_sink_emit_fn` | `core/include/alfred_record_sink.h` | dispatcher o bridge record | consegnare un `alfred_record_t` a un sink generico |
| `alfred_record_text_sink_write_fn` | `core/include/alfred_record_text_sink.h` | text sink | consegnare un payload testuale gia' formattato |

Questa tabella mostra una cosa importante: Alfred usa callback a livelli
diversi, non solo nel core. Il concetto e' sempre lo stesso, ma cambia il tipo di
dato consegnato.

### Esempio: callback del backend inotify

Il backend inotify espone:

```c
typedef int (*inotify_backend_event_fn)(
    const alfred_raw_event_t *raw,
    void *userdata
);
```

La lettura della firma si fa da destra verso sinistra:

```text
inotify_backend_event_fn e' un puntatore a funzione
che riceve:
- const alfred_raw_event_t *raw
- void *userdata
e restituisce int
```

Quando `inotify_backend_poll()` riceve un evento dal kernel, costruisce un raw
event Alfred e poi chiama:

```text
on_event(raw, userdata)
```

Nel runtime attuale `userdata` e' un `app_t *`, perche' la callback applicativa
deve poter raggiungere il core:

```text
inotify_backend_poll(&ctx, handle_backend_event, app)

handle_backend_event(raw, userdata)
  app = userdata
  alfred_process(app->core, raw)
```

Il backend non sa che `userdata` contiene `app_t *`. Questa e' la separazione
voluta: il backend produce raw event, ma non conosce l'applicazione completa.

### Esempio: callback del core

Il core usa:

```c
typedef void (*alfred_emit_fn)(
    const alfred_event_t *ev,
    void *userdata
);
```

Qui il core produce un evento semantico e chiama la callback registrata:

```text
emit(ev, userdata)
```

Nel runtime corrente la callback e' `core_logger_on_event()` e `userdata` punta
al logger o a un contesto che permette di scrivere `events.log`. In futuro la
stessa callback potrebbe inviare record verso un dispatcher, una coda, un socket
o un test in memoria.

La regola di lifetime resta:

```text
ev e' valido solo durante la callback
```

Se la callback vuole conservare informazioni, deve copiarle.

### Esempio: sink generico

Il sink generico usa:

```c
typedef int (*alfred_record_sink_emit_fn)(
    void *userdata,
    const alfred_record_t *record
);

typedef struct {
    alfred_record_sink_emit_fn emit;
    void *userdata;
} alfred_record_sink_t;
```

Qui la callback non e' passata come argomento singolo a una funzione: e' un campo
dentro una struttura. Questo e' molto comune in C quando si vuole costruire una
"piccola interfaccia".

La struttura significa:

```text
quando vuoi consegnare un record a questo sink:
1. prendi sink.emit
2. passa sink.userdata
3. passa record
```

La funzione wrapper:

```c
alfred_record_sink_emit(&sink, &record);
```

fa internamente il controllo difensivo e poi chiama:

```c
sink->emit(sink->userdata, record);
```

Questo e' il punto in cui il puntatore a funzione viene richiamato davvero.

### Esempio: text sink

Il text sink e' un sink concreto. Espone:

```c
typedef int (*alfred_record_text_sink_write_fn)(
    void *userdata,
    const char *payload
);

typedef struct {
    alfred_record_text_sink_write_fn write;
    void *userdata;
    char *buffer;
    size_t buffer_size;
} alfred_record_text_sink_t;
```

Qui ci sono due livelli:

```text
dispatcher
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> text_sink.write(userdata, payload)
```

Il generic sink riceve un record. Il text sink lo formatta in una stringa. Poi
chiama un'altra callback `write()` per consegnare il payload testuale a chi sa
dove scriverlo.

Questa catena puo' sembrare lunga, ma separa responsabilita' diverse:

- il dispatcher decide quali sink chiamare;
- il generic sink uniforma l'interfaccia;
- il text sink formatta;
- la callback `write` scrive o cattura il payload.

Nei test `write` puo' salvare il payload in un array. Nel runtime puo' adattarsi
al logger. In futuro potrebbe scrivere su un buffer, una pipe o un socket.

### Esempio: callback dello scanner

Lo scanner filesystem usa:

```c
typedef int (*fs_scan_fn)(const fs_scan_entry_t *entry, void *userdata);
```

Lo scanner attraversa l'albero e, per ogni entry trovata, chiama la callback.
Questo evita di imporre allo scanner una singola politica. Lo stesso scanner puo'
essere usato per:

- installare watch startup;
- scoprire directory create troppo velocemente;
- fare resync;
- fare benchmark;
- costruire report futuri.

La callback decide cosa fare con ogni `entry`.

### Esempio: callback della watcher table

La watcher table usa:

```c
typedef int (*watcher_iter_fn)(
    const watcher_entry_t *entry,
    void *userdata
);
```

Questa callback serve a visitare piu' watch senza esporre direttamente l'array
interno della tabella. La watcher table mantiene il controllo del proprio stato;
il chiamante riceve una `const watcher_entry_t *`, quindi puo' leggere l'entry ma
non deve modificarla.

Il ritorno `int` permette alla callback di fermare l'iterazione:

```text
0     -> continua
!= 0  -> fermati e propaga questo valore
```

Questo pattern e' utile quando si cerca il primo elemento interessante o quando
un errore deve interrompere il giro.

### Come leggere una callback in una struct

Quando vedi una struct come:

```c
typedef struct {
    alfred_record_sink_emit_fn emit;
    void *userdata;
} alfred_record_sink_t;
```

leggila cosi':

```text
questa struct rappresenta un oggetto richiamabile;
emit e' la funzione da chiamare;
userdata e' il contesto da passare alla funzione.
```

Quando poi vedi:

```c
alfred_record_sink_emit(&sink, &record);
```

devi immaginare:

```text
sink.emit(sink.userdata, record)
```

Il wrapper esiste per centralizzare i controlli:

- `sink != NULL`
- `sink->emit != NULL`
- `record != NULL`

### Regole pratiche per non sbagliare

1. Leggi sempre il `typedef` della callback prima di usarla.
2. Controlla ordine e tipo degli argomenti.
3. Chiediti chi possiede `userdata`.
4. Chiediti quanto vive il record passato alla callback.
5. Non salvare puntatori borrowed se la documentazione dice che valgono solo
   durante la chiamata.
6. Se devi conservare un record oltre la callback, usa una copia owned o una
   strategia di ownership equivalente.

## `NULL`

`NULL` indica un puntatore che non punta a un oggetto valido.

Molte funzioni controllano:

```c
if (app == NULL)
    return ERR_INVALID_ARG;
```

Questo evita accessi a memoria non valida.

## Array fissi

Nel progetto compaiono buffer fissi:

```c
char raw_log[PATH_MAX];
```

`PATH_MAX` e' una dimensione massima per path filesystem.

Vantaggio:

- cleanup semplice
- niente `malloc()` per quei campi

Svantaggio:

- bisogna controllare la dimensione quando si copia testo

## `snprintf`

`snprintf()` scrive testo in un buffer con limite di dimensione:

```c
snprintf(cfg->raw_log, sizeof(cfg->raw_log), "raw.log");
```

E' preferibile a funzioni non sicure come `strcpy()` perche' conosce la
dimensione del buffer.

## `FILE *`

`FILE *` rappresenta uno stream della libreria standard C.

Nel logger:

```c
FILE *raw;
FILE *event;
FILE *error;
```

Questi stream vengono aperti con:

```c
fopen(path, "a");
```

e chiusi con:

```c
fclose(fp);
```

Chi apre una risorsa deve essere anche responsabile di chiuderla. Questo e' un
concetto di ownership.

## Funzioni variadic

Una funzione variadic accetta un numero variabile di argomenti.

Esempio:

```c
void logger_error(logger_t *lg, const char *fmt, ...);
```

I tre puntini `...` indicano argomenti variabili, come in `printf()`.

Dentro `logger.c` questi argomenti vengono gestiti con:

```c
va_list ap;
va_start(ap, fmt);
vfprintf(fp, fmt, ap);
va_end(ap);
```

Questo permette di scrivere:

```c
logger_error(&app->logger, "errno=%d", errno);
```

## `static`

Una funzione `static` definita in un file `.c` e' visibile solo in quel file.

Esempio:

```c
static void trim_newline(char *s);
```

Questo e' utile per helper interni che non devono diventare API pubblica.

## Codici di ritorno

In C e' comune segnalare successo o errore con il valore di ritorno.

Esempio:

```c
int logger_init(...);
```

Nel progetto spesso:

- `0` significa successo
- valore negativo significa errore

Oppure si usano valori dell'enum `error_t`:

```c
ERR_OK
ERR_ALLOC
ERR_IO
```

## `errno`

Molte funzioni di sistema impostano `errno` quando falliscono.

Esempio:

```c
logger_error(&app->logger,
             "read failed errno=%d (%s)",
             errno,
             strerror(errno));
```

`strerror(errno)` converte il numero dell'errore in testo leggibile.

## File descriptor

Un file descriptor e' un intero usato da Unix/Linux per rappresentare una
risorsa aperta.

Nel progetto:

```c
app->inotify.fd
```

e' il file descriptor restituito da `inotify_init1()`. Il campo vive dentro
`inotify_backend_t`, non direttamente dentro `app_t`.

Si legge con:

```c
read(ctx->runtime->fd, buffer, sizeof(buffer));
```

e si chiude con:

```c
close(ctx->runtime->fd);
```

## Lifetime della memoria

Il lifetime e' il tempo durante il quale un oggetto in memoria esiste davvero ed
e' lecito usarlo. In C questo concetto e' fondamentale, perche' il linguaggio non
controlla automaticamente se un puntatore punta ancora a memoria valida.

I casi principali sono:

| Tipo di memoria | Dove nasce | Quando muore | Chi la libera |
| --- | --- | --- | --- |
| automatica, o stack | variabili locali di funzione | quando la funzione ritorna | nessuno, la libera il runtime C |
| statica | variabili globali o `static` | alla fine del programma | nessuno durante il runtime normale |
| dinamica, o heap | `malloc()`, `calloc()`, `realloc()` | quando viene chiamata `free()` | il codice che possiede il puntatore |

### Memoria automatica

Una variabile locale normale vive sullo stack:

```c
static const char *make_bad_path(void)
{
    char path[64];

    snprintf(path, sizeof(path), "/tmp/file");
    return path; /* sbagliato */
}
```

`path` esiste solo dentro `make_bad_path()`. Quando la funzione ritorna, quel
buffer non e' piu' valido. Il puntatore restituito punta a memoria scaduta.

Questo errore si chiama spesso dangling pointer: il puntatore esiste ancora, ma
l'oggetto a cui puntava non esiste piu'.

### Memoria statica

Una string literal come:

```c
const char *path = "/tmp/file";
```

ha lifetime statico: il testo `"/tmp/file"` vive per tutta la durata del
programma. Pero' non e' memoria posseduta dal chiamante e non deve essere
liberata con `free()`.

Questo e' sbagliato:

```c
const char *path = "/tmp/file";

free((void *)path); /* sbagliato */
```

### Memoria dinamica

La memoria dinamica nasce con `malloc()` o funzioni simili:

```c
char *copy = malloc(strlen(path) + 1);
strcpy(copy, path);
```

Questa memoria resta valida finche' qualcuno chiama:

```c
free(copy);
```

Qui nasce il problema dell'ownership: chi deve chiamare `free()`?

## Ownership

Ownership significa: chi e' responsabile di una risorsa?

Se un componente possiede una risorsa, deve anche sapere quando liberarla.
Se invece riceve solo un puntatore borrowed, puo' leggerlo temporaneamente, ma
non deve liberarlo e non deve conservarlo oltre il lifetime promesso.

Esempi nel progetto:

- `logger_t` possiede i suoi `FILE *`
- `app_t` possiede `inotify_backend_t`, che a sua volta possiede il file
  descriptor inotify
- `watcher_table_t` possiede le copie dei path associati ai watch
- un `alfred_record_t` borrowed non possiede i path a cui punta
- un `alfred_record_t` owned creato con `alfred_record_clone_owned()` possiede
  le copie delle stringhe e deve essere distrutto con
  `alfred_record_destroy_owned()`

In molte API di Alfred useremo tre parole:

| Termine | Significato pratico |
| --- | --- |
| zeroed | la struct e' stata azzerata: numeri a `0`, puntatori a `NULL` |
| non-owned | la struct non possiede memoria dinamica che deve liberare |
| owned | la struct possiede memoria dinamica e deve liberarla con la cleanup corretta |

`zeroed` significa che la struct e' stata inizializzata a zero:

```c
alfred_record_t dst;

memset(&dst, 0, sizeof(dst));
```

Dopo questa operazione:

```c
dst.path == NULL;
dst.old_path == NULL;
dst.new_path == NULL;
```

Quindi `dst` non contiene puntatori da liberare.

`non-owned` significa che la struct non possiede memoria dinamica. Puo' essere
vuota, oppure puo' contenere puntatori borrowed che non deve liberare:

```c
alfred_record_t record;

memset(&record, 0, sizeof(record));
record.path = "/tmp/file"; /* borrowed/static string */
```

Qui `record.path` punta a una stringa statica. `record` non la possiede, quindi
non deve fare `free(record.path)`.

`owned` significa invece che la struct contiene puntatori a memoria dinamica che
ha la responsabilita' di liberare:

```c
alfred_record_t dst;

memset(&dst, 0, sizeof(dst));
alfred_record_clone_owned(&src, &dst);

/* dst ora possiede le copie delle stringhe clonate */

alfred_record_destroy_owned(&dst);
```

Nel caso di `alfred_record_clone_owned(&src, &dst)`, la scelta piu' sicura e':

```c
alfred_record_t dst;

memset(&dst, 0, sizeof(dst));
alfred_record_clone_owned(&src, &dst);

/* uso dst */

alfred_record_destroy_owned(&dst);
```

Detto in modo semplice:

- `zeroed`: tutti i campi sono a zero o `NULL`;
- `non-owned`: la struct non contiene memoria che lei deve liberare;
- `owned`: la struct contiene memoria allocata che dovra' essere liberata.

Il problema nasce se `dst` e' gia' owned e ci fai un altro clone sopra senza
destroy. In quel caso perdi i vecchi puntatori e crei un memory leak.

Capire ownership e' fondamentale in C per evitare:

- memory leak
- doppie `free()`
- `free()` su memoria borrowed o statica
- use-after-free
- uso di puntatori a variabili locali ormai scadute

### Borrowed e owned

Un puntatore borrowed e' un prestito: puoi usarlo, ma non lo possiedi.

Esempio:

```c
void print_path(const char *path)
{
    printf("%s\n", path);
}
```

Qui `print_path()` riceve un `const char *`, lo legge e basta. Non deve fare
`free(path)`, perche' non sa se `path` punta a:

- una string literal
- un buffer sullo stack del chiamante
- una stringa dentro una struct
- memoria allocata dinamicamente

Un puntatore owned invece e' responsabilita' del codice che lo possiede:

```c
char *copy_path(const char *path)
{
    char *copy;

    copy = malloc(strlen(path) + 1);
    if (copy == NULL) {
        return NULL;
    }

    strcpy(copy, path);
    return copy; /* il chiamante ora possiede copy */
}
```

Chi chiama deve liberare:

```c
char *path = copy_path("/tmp/file");

/* uso path */

free(path);
```

### Memory leak

Un memory leak accade quando il programma perde l'ultimo puntatore a memoria
allocata dinamicamente senza aver chiamato `free()`.

Esempio:

```c
char *path;

path = malloc(16);
strcpy(path, "/tmp/one");

path = malloc(16);
strcpy(path, "/tmp/two");

free(path);
```

La seconda assegnazione sovrascrive il puntatore alla prima allocazione. Dopo:

```text
path -> malloc("/tmp/two")
        malloc("/tmp/one")  <-- perso, non liberabile
```

La memoria che conteneva `"/tmp/one"` e' ancora allocata, ma il programma non ha
piu' nessun puntatore per raggiungerla e liberarla.

Questo e' esattamente il rischio discusso per
`alfred_record_clone_owned(src, dst)`: se `dst` contiene gia' stringhe owned e
la funzione scrive un nuovo clone sopra `dst` senza prima distruggere il vecchio
contenuto, i vecchi puntatori vengono persi.

### Double free

Una double free accade quando chiami `free()` due volte sulla stessa
allocazione.

```c
char *path = malloc(16);

free(path);
free(path); /* sbagliato */
```

Dopo la prima `free()`, quel puntatore non rappresenta piu' una risorsa valida
da liberare. Per questo molte funzioni di cleanup nel progetto azzerano la
struttura dopo aver liberato le risorse:

```c
alfred_record_destroy_owned(&record);
```

`alfred_record_destroy_owned()` libera i campi owned e poi fa `memset()` della
struct, cosi' i vecchi puntatori non restano visibili.

### Use-after-free

Un use-after-free accade quando usi memoria dopo averla liberata:

```c
char *path = malloc(16);
strcpy(path, "/tmp/file");

free(path);

printf("%s\n", path); /* sbagliato */
```

Il programma potrebbe sembrare funzionare, oppure stampare spazzatura, oppure
crashare. In C il risultato e' comportamento indefinito.

### Free su memoria borrowed

Non tutto quello che e' un puntatore va liberato.

Questo e' sbagliato:

```c
alfred_record_t record;

memset(&record, 0, sizeof(record));
record.path = "/tmp/file"; /* borrowed/static string */

alfred_record_destroy_owned(&record); /* sbagliato */
```

`alfred_record_destroy_owned()` va chiamata solo su record che sono stati
inizializzati da `alfred_record_clone_owned()` o da una funzione che documenta
lo stesso contratto owned. Se la struct contiene puntatori borrowed, il destroy
provera' a liberarli e il programma puo' rompersi.

### Contratti di ownership nelle funzioni

Ogni funzione che riceve o restituisce puntatori dovrebbe chiarire il contratto:

| Contratto | Significato |
| --- | --- |
| borrowed input | la funzione puo' leggere il puntatore solo durante la chiamata |
| owned input | la funzione riceve anche la responsabilita' di liberare |
| borrowed output | il chiamante puo' leggere ma non deve liberare |
| owned output | il chiamante deve liberare con la funzione indicata |
| transfer ownership | la proprieta' passa da un oggetto a un altro |

Esempio reale:

```c
int alfred_record_clone_owned(const alfred_record_t *src,
                              alfred_record_t *dst);
void alfred_record_destroy_owned(alfred_record_t *record);
```

Il contratto e':

- `src` e' borrowed: viene letto, ma non viene modificato e non viene liberato;
- `dst` deve essere vuoto o non-owned;
- dopo il successo, `dst` possiede le copie delle stringhe;
- il chiamante deve chiudere il ciclo con `alfred_record_destroy_owned(&dst)`.

Pattern corretto:

```c
alfred_record_t dst;

memset(&dst, 0, sizeof(dst));

alfred_record_clone_owned(&src1, &dst);
/* uso dst */
alfred_record_destroy_owned(&dst);

alfred_record_clone_owned(&src2, &dst);
/* uso dst */
alfred_record_destroy_owned(&dst);
```

Pattern sbagliato:

```c
alfred_record_clone_owned(&src1, &dst);
alfred_record_clone_owned(&src2, &dst); /* leak del primo clone */
alfred_record_destroy_owned(&dst);
```

Il secondo clone sovrascrive i puntatori owned del primo clone. Il destroy finale
libera solo il secondo clone.

### Ownership e code

Le code rendono il problema piu' importante. Se un backend produce un record con
puntatori borrowed verso buffer temporanei, quel record e' sicuro solo finche'
il backend non riusa quei buffer.

Per questo la regola per una coda e':

```text
se un record deve essere letto piu' tardi, deve possedere i dati necessari
oppure deve puntare a memoria con lifetime garantito
```

Nel branch corrente la `alfred_record_queue_t` usa una scelta semplice:

```text
push(record borrowed) -> clone owned dentro la coda
pop(record)           -> trasferisce ownership al chiamante
destroy(record)       -> libera le stringhe owned
```

Questo non e' ancora il modello piu' performante possibile, ma e' il modello
piu' chiaro per fissare il contratto e scrivere test.

Quando leggi una API di questo tipo, prova sempre a seguire la ownership come
una freccia:

```text
record borrowed del producer
    |
    | push() clona
    v
record owned dentro la queue
    |
    | pop() trasferisce
    v
record owned nel chiamante
    |
    | destroy_owned()
    v
memoria liberata
```

Se la freccia si perde, probabilmente c'e' un memory leak. Se due componenti
pensano entrambi di possedere la stessa memoria, probabilmente c'e' il rischio
di double free. Se un componente usa memoria dopo che la freccia e' arrivata al
destroy, probabilmente c'e' un use-after-free.

## Regola pratica

Quando leggi una funzione C, chiediti:

1. quali parametri riceve?
2. puo' ricevere `NULL`?
3. cosa modifica?
4. cosa possiede?
5. cosa restituisce?
6. chi deve fare cleanup?
7. i puntatori ricevuti sono borrowed o owned?
8. il chiamante puo' conservare quei puntatori dopo il ritorno?
9. se la funzione fallisce, chi libera le risorse gia' allocate?
