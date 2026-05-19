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
int inotify_fd;
```

e' il file descriptor restituito da `inotify_init1()`.

Si legge con:

```c
read(app->inotify_fd, buffer, sizeof(buffer));
```

e si chiude con:

```c
close(app->inotify_fd);
```

## Ownership

Ownership significa: chi e' responsabile di una risorsa?

Esempi nel progetto:

- `logger_t` possiede i suoi `FILE *`
- `app_t` possiede il file descriptor `inotify_fd`
- `watcher_table_t` possiede le copie dei path associati ai watch

Capire ownership e' fondamentale in C per evitare:

- memory leak
- doppie `free()`
- uso di risorse gia' chiuse

## Regola pratica

Quando leggi una funzione C, chiediti:

1. quali parametri riceve?
2. puo' ricevere `NULL`?
3. cosa modifica?
4. cosa possiede?
5. cosa restituisce?
6. chi deve fare cleanup?
