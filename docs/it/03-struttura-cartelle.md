# Struttura delle cartelle

Il progetto e' organizzato come un piccolo monorepo C modulare.

```text
progetto_3a_inf/
├── app/
├── core/
├── modules/
├── tests/
├── docs/
├── Makefile
└── README.md
```

## app/

Contiene il livello applicazione.

```text
app/
├── include/
└── src/
```

Responsabilita':

- avviare il programma
- inizializzare configurazione e logger
- gestire il ciclo principale
- coordinare shutdown e cleanup

File principali:

```text
app/include/app.h
app/src/main.c
app/src/app.c
```

## core/

Contiene il motore semantico.

```text
core/
├── include/
└── src/
```

Responsabilita':

- ricevere eventi raw
- correlare eventi collegati
- produrre eventi semantici

Il core non dovrebbe dipendere da `inotify`.

## modules/

Contiene i backend specifici.

Oggi:

```text
modules/inotify/
├── include/
└── src/
```

Responsabilita':

- parlare con Linux tramite `inotify`
- gestire watch descriptor
- osservare directory e file
- trasformare eventi Linux in eventi raw comuni

## tests/

Contiene test funzionali, stress test e helper di test.

```text
tests/
├── functional/
├── stress/
└── lib/
```

## docs/

Contiene documentazione per sviluppatori e studenti.

```text
docs/
├── commenting-style.md
├── commenting-progress.md
└── it/
```

`docs/it/` contiene la documentazione didattica in italiano.

## Makefile

Il Makefile principale compila l'intero progetto.

Gestisce:

- compilazione debug
- compilazione release
- sanitizer
- test
- Valgrind
- GDB
- formattazione
- analisi statica

Per iniziare:

```bash
make
```
