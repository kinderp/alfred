# Glossario

Stato: parziale.

## Backend

Modulo che parla con un meccanismo specifico del sistema operativo, per esempio
Linux `inotify`.

## Core

Motore centrale che interpreta eventi raw e produce eventi semantici.

## Evento raw

Evento vicino al sistema operativo, non ancora interpretato completamente.

## Evento semantico

Evento gia' interpretato dal core, per esempio `FILE_RENAMED`.

## File descriptor

Numero intero usato da Unix/Linux per rappresentare una risorsa aperta, per
esempio un file o un descrittore inotify.

## Linking

Fase in cui i file oggetto `.o` vengono uniti per produrre il binario finale.

## Sanitizer

Controllo runtime aggiunto dal compilatore per trovare errori di memoria o
comportamenti indefiniti.
