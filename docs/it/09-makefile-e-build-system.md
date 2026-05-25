# Makefile e build system

Il Makefile e' il file che dice al computer come compilare il progetto.
In un progetto C non basta avere file `.c` e `.h`: bisogna trasformare i file
sorgente in file oggetto `.o` e poi collegarli in un eseguibile finale.

## Comando base

Per compilare:

```bash
make
```

Il risultato atteso e':

```text
alfred
```

cioe' il binario eseguibile del progetto.

## Concetti fondamentali

### Sorgenti `.c`

I file `.c` contengono implementazioni:

```text
app/src/app.c
core/src/alfred_correlator.c
modules/inotify/src/watcher.c
```

### Header `.h`

I file `.h` contengono dichiarazioni, tipi e interfacce:

```text
app/include/app.h
core/include/alfred_correlator.h
modules/inotify/include/watcher.h
```

### Object file `.o`

Ogni file `.c` viene compilato in un file oggetto `.o`.

Esempio:

```text
app/src/app.c
    |
    v
build/obj/app/src/app.o
```

### Linking

Il linker prende tutti i `.o` e li unisce nel binario finale:

```text
app.o + core.o + inotify.o
        |
        v
      alfred
```

## Diagramma della build

```mermaid
flowchart LR
    C1[app/src/*.c] --> O1[build/obj/app/*.o]
    C2[core/src/*.c] --> O2[build/obj/core/*.o]
    C3[modules/inotify/src/*.c] --> O3[build/obj/modules/inotify/*.o]

    O1 --> L[linker]
    O2 --> L
    O3 --> L
    L --> B[alfred]
```

## Target futuri per la documentazione

Al momento il Makefile non genera ancora diagrammi o animazioni della
documentazione. Nei capitoli didattici si cita un possibile target futuro:

```bash
make docs-animations
```

Questo comando non e' ancora implementato. L'idea e' usarlo in futuro per
trasformare gli scenari animabili descritti in
`docs/it/16-mappa-codice-e-strutture.md` in file generati, per esempio SVG,
GIF, video o pagine HTML.

Una possibile struttura di output sara':

```text
docs/generated/animations/
```

Regola importante: il Markdown in `docs/it` deve restare la sorgente didattica
principale. I file in `docs/generated/` dovrebbero essere derivati e
rigenerabili, non scritti a mano come documentazione primaria.

## Variabili principali

Nel Makefile una variabile si definisce cosi':

```make
TARGET := alfred
```

e si usa cosi':

```make
$(TARGET)
```

### TARGET

```make
TARGET := alfred
```

Nome del binario finale.

### MODULES

```make
MODULES ?= inotify
```

Lista dei moduli da compilare.

`?=` significa: usa questo valore solo se l'utente non ne ha passato uno da
riga di comando.

Esempio:

```bash
make MODULES=inotify
```

In futuro potremo avere:

```bash
make MODULES=fanotify
make MODULES="inotify replay"
```

### ENABLE_LEGACY_SHADOW

```make
ENABLE_LEGACY_SHADOW ?= 0
```

Controlla se il dispatcher legacy di shadow mode viene compilato.

Default:

```bash
make
```

compila il binario normale core-only. In questa build `events.c` e
`move_cache.c` non vengono inclusi.

Per costruire un binario che supporta il confronto legacy/core:

```bash
make ENABLE_LEGACY_SHADOW=1
```

In questa variante il Makefile:

- aggiunge `-DALFRED_ENABLE_LEGACY_SHADOW`
- compila `modules/inotify/src/events.c`
- compila `modules/inotify/src/move_cache.c`

Se un binario core-only viene avviato con:

```bash
ALFRED_EVENT_ENGINE=shadow ./alfred /path
```

Alfred deve fallire in modo esplicito, perche' lo shadow mode richiede il
dispatcher legacy compilato. Non sarebbe corretto fare fallback silenzioso al
core: chi chiede shadow vuole un confronto reale.

Il Makefile usa directory oggetto separate per le varianti di build:

```text
build/obj/core/
build/obj/legacy-shadow/
```

Questo evita di riusare per errore `.o` compilati con flag diversi.

Le macro `-D...` sono raccolte nella variabile `DEFINES`, usata sia dalla build
di sviluppo sia dalla build `release`. Questo evita che una variante release
perda per errore macro importanti come `ALFRED_ENABLE_LEGACY_SHADOW`.

### Directory

```make
APP_DIR     := app
CORE_DIR    := core
MODULE_DIR  := modules
BUILD_DIR   := build
OBJ_DIR     := $(BUILD_DIR)/obj
```

Queste variabili evitano di ripetere stringhe in tutto il Makefile.

## Compilatore

```make
CC := gcc
```

`gcc` e' il compilatore C usato dal progetto.

## Flag di compilazione

I flag modificano il comportamento del compilatore.

Nel Makefile i flag sono separati per responsabilita':

```make
C_STANDARD
WARNINGS
DEBUG_FLAGS
RELEASE_FLAGS
SANITIZERS
DEPFLAGS
INCLUDES
CFLAGS
LDFLAGS
```

Questa separazione rende piu' facile capire perche' un flag esiste e quando
viene usato.

### Standard C

```make
C_STANDARD := -std=gnu99
```

Il progetto usa C99 con estensioni GNU.

### Warning

```make
WARNINGS := \
    -Wall \
    -Wextra \
    -Wpedantic \
    ...
```

I warning aiutano a trovare codice sospetto. Non sono sempre errori, ma spesso
indicano problemi reali.

Esempi:

- conversioni pericolose
- variabili inutilizzate
- formati `printf()` sbagliati
- codice non portabile

### Debug flags

```make
DEBUG_FLAGS := \
    -g3 \
    -O0 \
    -DDEBUG
```

Significato:

- `-g3`: include informazioni di debug per GDB
- `-O0`: disabilita ottimizzazioni, codice piu' facile da debuggare
- `-DDEBUG`: definisce la macro `DEBUG`

### Release flags

```make
RELEASE_FLAGS := \
    -O2 \
    -DNDEBUG
```

Significato:

- `-O2`: abilita ottimizzazioni
- `-DNDEBUG`: disabilita codice condizionato da debug, se presente

## Sanitizer

```make
SANITIZERS := \
    -fsanitize=address \
    -fsanitize=undefined
```

I sanitizer sono controlli runtime aggiunti dal compilatore.

### AddressSanitizer

`-fsanitize=address` aiuta a trovare errori di memoria:

- accesso fuori dai limiti di un array
- uso di memoria dopo `free()`
- doppia `free()`
- stack buffer overflow

### UndefinedBehaviorSanitizer

`-fsanitize=undefined` aiuta a trovare comportamenti indefiniti del C:

- overflow aritmetico signed
- shift illegali
- accessi non validi
- uso scorretto di certi tipi

## Include path

```make
INCLUDES := \
    -I$(APP_INC_DIR) \
    -I$(CORE_INC_DIR) \
    -I$(CORE_PRIVATE_INC_DIR)
```

`-I` dice al compilatore dove cercare gli header inclusi con:

```c
#include "app.h"
```

Quando abilitiamo il modulo `inotify`, il Makefile aggiunge anche:

```make
-Imodules/inotify/include
```

## CFLAGS e LDFLAGS

```make
CFLAGS = ...
LDFLAGS := ...
```

`CFLAGS` vengono usati durante la compilazione dei `.c` in `.o`.

`LDFLAGS` vengono usati durante il linking finale.

I sanitizer devono comparire sia in compilazione sia in linking.

### CFLAGS

`CFLAGS` contiene flag usati quando il compilatore trasforma un `.c` in un
file `.o`.

Esempio:

```text
app/src/main.c -> build/obj/app/src/main.o
```

In questa fase servono:

- standard C
- warning
- flag di debug o release
- sanitizer
- dependency tracking
- include path

Nel Makefile:

```make
CFLAGS = \
    $(C_STANDARD) \
    $(WARNINGS) \
    $(DEBUG_FLAGS) \
    $(SANITIZERS) \
    $(DEPFLAGS) \
    $(INCLUDES)
```

### LDFLAGS

`LDFLAGS` contiene flag usati durante il linking, cioe' quando tutti i `.o`
vengono uniti nel binario finale.

Esempio:

```text
main.o + app.o + core.o -> alfred
```

Nel Makefile:

```make
LDFLAGS := \
    -fsanitize=address \
    -fsanitize=undefined
```

I sanitizer devono comparire anche in `LDFLAGS`, perche' il linker deve
collegare il runtime dei sanitizer al binario finale.

### Perche' `CFLAGS =` e non sempre `CFLAGS :=`

Nel Makefile `CFLAGS` usa `=`:

```make
CFLAGS = ...
```

Questo permette di aggiungere include o macro dopo la definizione iniziale.

Esempio:

```make
INCLUDES += -I$(MODULE_DIR)/inotify/include
CFLAGS += -DALFRED_ENABLE_INOTIFY
```

Se `CFLAGS` fosse espanso troppo presto, alcune aggiunte successive potrebbero
non essere incluse nel comando finale.

## Dipendenze dagli header

Quando un file `.c` include un header `.h`, una modifica all'header deve far
ricompilare anche il `.c`.

Esempio:

```text
app/src/main.c include app/include/app.h
```

Se cambia `app.h`, anche `main.c` deve essere ricompilato.

Per questo il Makefile usa:

```make
DEPFLAGS := -MMD -MP
```

Questi flag chiedono a `gcc` di generare file `.d` accanto agli object file.

Esempio:

```text
build/obj/app/src/main.o
build/obj/app/src/main.d
```

Il file `.d` contiene dipendenze come:

```make
build/obj/app/src/main.o: app/src/main.c app/include/app.h ...
```

Poi il Makefile include questi file:

```make
-include $(DEPS)
```

Il trattino davanti a `include` significa: non fallire se i file `.d` non
esistono ancora. Alla prima build infatti non esistono; vengono creati durante
la compilazione.

Questa parte e' importante. Senza dependency tracking, potresti modificare una
struct in un header e lasciare compilati vecchi `.o` non aggiornati. In C questo
puo' causare bug difficili da capire.

### Cosa significa `-MMD`

`-MMD` dice a `gcc`:

```text
mentre compili il file .c, genera anche un file .d con le dipendenze dagli
header del progetto.
```

Gli header di sistema, come `<stdio.h>`, normalmente non vengono inclusi nel
file `.d`. Questo mantiene i file di dipendenza piu' piccoli e leggibili.

### Cosa significa `-MP`

`-MP` aggiunge regole vuote per gli header.

Serve a evitare errori se un header viene rimosso o rinominato. Senza `-MP`,
Make potrebbe leggere un vecchio `.d` che nomina un header non piu' esistente e
fermarsi con un errore.

### Cosa contiene un file `.d`

Un file `.d` e' un file Make generato automaticamente.

Esempio semplificato:

```make
build/obj/app/src/app.o: app/src/app.c app/include/app.h \
 app/include/config.h app/include/logger.h

app/include/app.h:
app/include/config.h:
app/include/logger.h:
```

La prima riga dice:

```text
app.o dipende da app.c e dagli header elencati.
```

Le righe vuote sugli header sono generate da `-MP`.

### Perche' usiamo `-include $(DEPS)`

Alla prima build i file `.d` non esistono ancora. Se scrivessimo:

```make
include $(DEPS)
```

Make fallirebbe perche' non trova quei file.

Scrivendo:

```make
-include $(DEPS)
```

Make prova a includerli, ma non fallisce se mancano.

Dopo la prima compilazione, i file `.d` esistono e Make li usa per capire cosa
ricompilare.

### Perche' questo e' importante per gli studenti

Se modifichi una struct in un header:

```c
typedef struct app {
    ...
} app_t;
```

tutti i `.c` che usano quella struct devono essere ricompilati.

Se non succede, puoi ottenere bug strani: il codice nuovo e il codice vecchio
possono avere idee diverse sulla dimensione della struct.

Durante lo sviluppo abbiamo visto proprio un caso di questo tipo: dopo aver
modificato `app_t`, un object file non ricompilato poteva usare una dimensione
vecchia della struct. I file `.d` evitano questo problema.

### `.o` e `.d` insieme

Per ogni sorgente:

```text
app/src/app.c
```

la build produce:

```text
build/obj/app/src/app.o
build/obj/app/src/app.d
```

Il `.o` serve al linker.

Il `.d` serve a Make.

## Liste dei sorgenti

Il Makefile separa i sorgenti per livello:

```make
APP_SRCS := ...
CORE_SRCS := ...
MODULE_SRCS := ...
```

Poi li unisce:

```make
SRCS := $(APP_SRCS) $(CORE_SRCS) $(MODULE_SRCS)
```

Questo rende visibile da dove arriva ogni parte del programma.

## Aggiungere una nuova coppia `.h` e `.c`

Quando aggiungi una nuova funzionalita' in C, spesso crei due file:

```text
nome_modulo.h
nome_modulo.c
```

Il file `.h` contiene l'interfaccia pubblica del modulo: tipi, costanti e
prototipi delle funzioni che altri file possono chiamare.

Il file `.c` contiene l'implementazione.

### Passo 1: scegliere il livello giusto

Prima domanda:

> Questa funzionalita' appartiene ad `app`, `core` o `modules/inotify`?

Esempi:

| Funzionalita' | Posizione corretta |
| --- | --- |
| logging applicativo | `app/` |
| correlazione eventi raw | `core/` |
| conversione `struct inotify_event` | `modules/inotify/` |
| parsing configurazione | `app/` |

### Passo 2: creare l'header

Esempio:

```text
modules/inotify/include/inotify_adapter.h
```

L'header deve avere:

- commento iniziale del file
- include guard
- include necessari
- prototipi documentati

Schema:

```c
#ifndef INOTIFY_ADAPTER_H
#define INOTIFY_ADAPTER_H

int funzione_pubblica(void);

#endif /* INOTIFY_ADAPTER_H */
```

### Passo 3: creare il file `.c`

Esempio:

```text
modules/inotify/src/inotify_adapter.c
```

Il file `.c` deve includere il suo header:

```c
#include "inotify_adapter.h"
```

Questo e' importante: se la dichiarazione nell'header e l'implementazione nel
`.c` non coincidono, il compilatore puo' segnalarlo.

### Passo 4: aggiungere il `.c` al Makefile

Creare il file non basta. Il Makefile deve sapere che quel `.c` va compilato.

Se il file appartiene ad `app/`, aggiungilo a:

```make
APP_SRCS := \
    ...
    $(APP_DIR)/src/nuovo_file.c
```

Se appartiene al core:

```make
CORE_SRCS := \
    ...
    $(CORE_DIR)/src/nuovo_file.c
```

Se appartiene al modulo inotify:

```make
MODULE_SRCS += \
    $(MODULE_DIR)/inotify/src/nuovo_file.c
```

Esempio reale:

```make
MODULE_SRCS += \
    $(MODULE_DIR)/inotify/src/inotify_adapter.c \
    $(MODULE_DIR)/inotify/src/inotify_backend.c \
    ...
```

Nota: `events.c` e `move_cache.c` non vanno aggiunti al percorso normale. Sono
compilati solo dentro il blocco `ENABLE_LEGACY_SHADOW=1`, perche' servono al
confronto legacy e non al runtime core-only.

### Passo 5: controllare gli include path

Il Makefile deve anche sapere dove cercare gli header.

Per `app/include`:

```make
-I$(APP_INC_DIR)
```

Per `core/include`:

```make
-I$(CORE_INC_DIR)
```

Per `modules/inotify/include`:

```make
-I$(MODULE_DIR)/inotify/include
```

Se metti l'header in una directory gia' inclusa, non devi modificare gli include
path.

### Passo 6: compilare subito

Dopo aver aggiunto i file:

```bash
make
```

Se il Makefile e' corretto, dovresti vedere una riga simile:

```text
[CC] modules/inotify/src/inotify_adapter.c
```

Se non la vedi, probabilmente hai dimenticato di aggiungere il `.c` alla lista
giusta nel Makefile.

### Passo 7: aggiornare la documentazione

Ogni nuova coppia `.h/.c` deve aggiornare:

```text
docs/commenting-progress.md
```

Se il file introduce un concetto importante per gli studenti, aggiorna anche la
documentazione italiana in:

```text
docs/it/
```

Nel caso dell'adapter inotify abbiamo aggiornato:

```text
docs/it/05-modulo-inotify.md
```

### Checklist rapida

Quando aggiungi una coppia `.h/.c`, controlla:

- il file `.h` ha include guard?
- il file `.c` include il proprio `.h`?
- le funzioni pubbliche sono dichiarate nell'header?
- le funzioni private sono `static` nel `.c`?
- il `.c` e' stato aggiunto al Makefile?
- `make` compila il nuovo file?
- hai aggiornato `docs/commenting-progress.md`?
- serve aggiornare anche `docs/it/`?

## Regola per gli object file

```make
OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
```

Questa trasformazione dice:

```text
per ogni file .c in SRCS,
crea il nome corrispondente .o dentro build/obj
```

Esempio:

```text
app/src/main.c
    -> build/obj/app/src/main.o
```

## Target principali

### all

```make
all: banner directories $(TARGET)
```

Target predefinito. Quando scrivi `make`, Make esegue `all`.

Dipende da:

- `banner`
- `directories`
- `alfred`

### directories

Crea le directory di build necessarie:

```make
mkdir -p build
mkdir -p build/obj/...
```

### $(TARGET)

Collega tutti gli object file:

```make
$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
```

### Regola generica di compilazione

```make
$(OBJ_DIR)/%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@
```

Significato delle variabili automatiche:

- `$<`: il primo prerequisito, cioe' il file `.c`
- `$@`: il target da produrre, cioe' il file `.o`

Esempio pratico:

```text
$< = app/src/main.c
$@ = build/obj/app/src/main.o
```

## Pulizia

### clean

```bash
make clean
```

Rimuove la directory `build/`.

### fclean

```bash
make fclean
```

Esegue `clean` e rimuove anche il binario `alfred`.

### re

```bash
make re
```

Ricompila tutto da zero:

```text
fclean -> all
```

## Build release

```bash
make release
```

Compila con flag ottimizzati e senza sanitizer.

Serve quando vuoi un binario piu' vicino all'uso finale, non quando stai
sviluppando o cercando bug.

## Target di utilita'

### run

```bash
make run
```

Compila e poi esegue:

```bash
./alfred
```

Nota: il programma richiede percorsi da monitorare, quindi `make run` potrebbe
non essere sufficiente per un'esecuzione reale.

### test-core

```bash
make test-core
```

Il target `make test-core` ricostruisce esplicitamente il binario nella variante
core-only prima di lanciare:

```text
tests/core/
```

In questo modo non dipende da un eventuale binario legacy-shadow prodotto da un
test precedente.

Nonostante il nome, `test-core` non salta il backend: gli script creano file e
directory reali, Alfred riceve eventi inotify reali e il core produce
`events.log`. Questa e' la suite end-to-end ufficiale del percorso core.

### test-backend-diagnostics

```bash
make test-backend-diagnostics
```

Il target ricostruisce il binario core-only e lancia:

```text
tests/backend/
```

Questa suite non controlla la semantica utente finale. Controlla log e stato
diagnostico del backend inotify, per esempio `WATCH_ADDED` e `WATCH_REMOVED`.
Sono righe utili per capire se la tabella dei watch viene aggiornata
correttamente, ma non devono essere confuse con eventi semantici del core.

### test-legacy-shadow

```bash
make test-legacy-shadow
```

Esegue gli script in:

```text
tests/functional/
```

Questi test funzionali storici usano ancora lo shadow legacy. Per questo il
target costruisce prima il binario con:

```bash
ENABLE_LEGACY_SHADOW=1
```

### test

```bash
make test
```

`make test` e' ora l'alias ufficiale di `make test-core`. Il nome storico del
target punta quindi al contratto end-to-end del percorso core. Chi deve eseguire
i controlli storici legacy/shadow deve usare esplicitamente:

```bash
make test-legacy-shadow
```

### valgrind

```bash
make valgrind
```

Esegue il programma sotto Valgrind per cercare problemi di memoria.

### gdb

```bash
make gdb
```

Avvia il debugger GDB sul binario.

### format

```bash
make format
```

Formatta il codice con `clang-format`.

### scan

```bash
make scan
```

Esegue `cppcheck`, uno strumento di analisi statica.

### tidy

```bash
make tidy
```

Esegue `clang-tidy`, un altro strumento di analisi statica.

## Comandi piu' usati

| Obiettivo | Comando |
| --- | --- |
| Compilare | `make` |
| Pulire object file | `make clean` |
| Pulire tutto | `make fclean` |
| Ricompilare da zero | `make re` |
| Build ottimizzata | `make release` |
| Eseguire test ufficiali core | `make test` |
| Eseguire test end-to-end core espliciti | `make test-core` |
| Eseguire diagnostica backend inotify | `make test-backend-diagnostics` |
| Eseguire test funzionali legacy/shadow | `make test-legacy-shadow` |
| Build con confronto legacy/core | `make ENABLE_LEGACY_SHADOW=1` |
| Cercare problemi memoria | `make valgrind` |
| Debuggare | `make gdb` |
| Formattare codice | `make format` |
| Analisi statica base | `make scan` |
| Analisi statica avanzata | `make tidy` |

## Errori comuni

### Errore di compilazione

Succede mentre un `.c` viene trasformato in `.o`.

Cause comuni:

- header mancante
- tipo non dichiarato
- funzione chiamata con parametri sbagliati
- errore di sintassi C

### Errore di linking

Succede dopo la compilazione, quando il linker crea `alfred`.

Cause comuni:

- funzione dichiarata ma non implementata
- file `.c` non aggiunto a `SRCS`
- libreria mancante

### Warning

Un warning non blocca sempre la build, ma non va ignorato. In C molti warning
segnalano bug reali o comportamenti pericolosi.
