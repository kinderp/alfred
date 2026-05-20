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

Modalita' temporanea in cui il vecchio dispatcher e il nuovo core ricevono gli
stessi eventi raw. Il vecchio dispatcher resta il comportamento ufficiale,
mentre il core viene osservato e confrontato.

## Dedup

Abbreviazione di deduplicazione. Indica la logica che evita di produrre due
volte lo stesso evento logico, per esempio due `DIR_CREATED` per lo stesso path
arrivati a causa di un evento sintetico e di un evento reale molto ravvicinati.

## Linking

Fase in cui i file oggetto `.o` vengono uniti per produrre il binario finale.

## Sanitizer

Controllo runtime aggiunto dal compilatore per trovare errori di memoria o
comportamenti indefiniti.
