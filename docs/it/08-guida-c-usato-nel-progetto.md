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
