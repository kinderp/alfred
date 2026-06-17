# Glossario

Stato: parziale.

## Backend

Modulo che parla con un meccanismo specifico del sistema operativo, per esempio
Linux `inotify`.

## Core

Motore centrale che interpreta eventi raw e produce eventi semantici.

## Evento raw

Evento vicino al sistema operativo, non ancora interpretato completamente.

## Evento raw sintetico

Evento `alfred_raw_event_t` creato da Alfred invece che ricevuto direttamente
dal backend. Serve quando il programma scopre un fatto reale tramite scan, per
esempio una directory creata prima che fosse aggiunto il watch inotify.

## Evento semantico

Evento gia' interpretato dal core, per esempio `FILE_RENAMED`.

## Event Model v0

Specifiche del record strutturato comune di Alfred. In Event Model v0 un record
e' identificato da `layer + category + type`, per esempio
`semantic + filesystem + FILE_CREATED` oppure
`diagnostic + watch + WATCH_STALE`. Serve come base per JSONL, Backend API v0,
plugin, tracepoint e futuri guardrail.

## Directory contenitore

Directory che contiene direttamente un file o un'altra directory. Per il path
`/tmp/progetto/a.txt`, la directory contenitore e' `/tmp/progetto` e il nome
dell'oggetto e' `a.txt`.

## File descriptor

Numero intero usato da Unix/Linux per rappresentare una risorsa aperta, per
esempio un file o un descrittore inotify.

## Relocate

Operazione in cui un oggetto cambia sia directory contenitore sia nome. Per
esempio `/tmp/a.txt -> /var/b.txt` diventa `FILE_RELOCATED`.

## Shadow mode

Modalita' diagnostica storica in cui il vecchio dispatcher legacy e il core
venivano eseguiti sugli stessi eventi per confrontare gli output. Il runtime
corrente usa `event_engine=core`; `shadow` non e' piu' un valore valido.

## Dedup

Abbreviazione di deduplicazione. Indica la logica che evita di produrre due
volte lo stesso evento logico, per esempio due `DIR_CREATED` per lo stesso path
arrivati a causa di un evento sintetico e di un evento reale molto ravvicinati.

## Linking

Fase in cui i file oggetto `.o` vengono uniti per produrre il binario finale.

## Sanitizer

Controllo runtime aggiunto dal compilatore per trovare errori di memoria o
comportamenti indefiniti.

## Frame animabile

Singolo passo di uno scenario descritto in forma testuale. Un frame indica cosa
succede, quali funzioni vengono chiamate, quali campi delle strutture dati
cambiano e quale output viene prodotto. Piu' frame ordinati possono diventare
una GIF, un video o una vista HTML interattiva.

## Documentazione dinamica

Documentazione che mostra l'evoluzione di un processo nel tempo, per esempio
l'inserimento di un watch nella tabella o la correlazione `MOVED_FROM` /
`MOVED_TO` nel core. Nel progetto la base resta il Markdown; eventuali
animazioni saranno derivate dagli scenari descritti nei documenti.

## Artefatto generato

File prodotto automaticamente a partire da una sorgente, per esempio un SVG o
una GIF generati dagli scenari Markdown. Gli artefatti generati non dovrebbero
diventare la fonte principale delle spiegazioni: se cambia il comportamento,
prima si aggiorna il Markdown e poi si rigenera l'output.

## Mermaid

Linguaggio testuale per scrivere diagrammi dentro Markdown. E' utile per
flowchart e sequence diagram statici, ma non produce direttamente GIF animate.

## SVG

Formato vettoriale per immagini. E' adatto a diagrammi generati perche' resta
leggibile anche quando viene ingrandito e puo' essere prodotto da script.
