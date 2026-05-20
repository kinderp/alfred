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

Nel codice attuale il legacy logga gia':

```text
WATCH_ADDED
WATCH_REMOVED
```

Lo fa in `modules/inotify/src/watch_manager.c` con `logger_event()`. Inoltre
`modules/inotify/src/events.c` trasforma `IN_IGNORED` in `WATCH_REMOVED`.

Decisione architetturale:

```text
WATCH_ADDED e WATCH_REMOVED devono restare visibili come diagnostica del
backend, ma non devono diventare eventi semantici del core.
```

In futuro potremo spostare o duplicare questi messaggi nel raw/backend log. La
cosa importante e' che non siano trattati come eventi di dominio allo stesso
livello di `FILE_CREATED`, `DIR_DELETED` o `FILE_RENAMED`.

## Eventi semantici

Un evento semantico descrive il significato dell'operazione vista da Alfred.

Esempi:

```text
FILE_CREATED
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

Durante lo shadow mode confrontiamo il legacy dispatcher e il core. Questo non
significa che il legacy abbia sempre ragione.

Decisione:

```text
il core definisce la semantica target.
```

Il legacy serve come riferimento storico per non perdere casi importanti, ma se
una differenza e' dovuta a una semantica migliore del core, preferiamo il core.

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
