# Documentazione didattica

Questa cartella contiene la documentazione pensata per studenti e nuovi
contributori. I commenti nel codice restano in inglese e sono sintetici; questi
documenti invece spiegano architettura, sintassi C, strumenti e ragionamenti con
piu' dettaglio.

## Percorso consigliato

1. [Panoramica del progetto](01-panoramica-progetto.md)
2. [Architettura generale](02-architettura-generale.md)
3. [Struttura delle cartelle](03-struttura-cartelle.md)
4. [Livello applicazione](04-livello-applicazione.md)
5. [Modulo inotify](05-modulo-inotify.md)
6. [Core engine](06-core-engine.md)
7. [Flusso eventi](07-flusso-eventi.md)
8. [C usato nel progetto](08-guida-c-usato-nel-progetto.md)
9. [Makefile e build system](09-makefile-e-build-system.md)
10. [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
11. [Come contribuire](11-come-contribuire.md)
12. [Confronto shadow mode](12-confronto-shadow-mode.md)
13. [Semantica degli eventi](13-semantica-eventi.md)
14. [Scenari di test](14-scenari-test.md)
15. [TODO switch verso il core](15-todo-switch-core.md)
16. [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
17. [Roadmap documentazione avanzata](17-roadmap-documentazione-avanzata.md)
18. [Glossario](glossario.md)

## Come usare questi documenti

Leggi i capitoli in ordine se parti da zero. Se invece stai lavorando su una
parte specifica della codebase, usa l'indice come riferimento rapido.

Per capire il codice in modo guidato, dopo aver letto architettura, app, modulo
inotify e core, passa a [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md).
Quel capitolo collega funzioni, strutture dati, campi modificati, eventi trigger
e diagrammi. Va usato come percorso di lettura della codebase, non solo come
appendice.

Per navigare il codice direttamente dal browser, usa anche la cartella
[code-browser](../code-browser/README.md). Contiene gli script per avviare
Bootlin Elixir su Alfred e cercare funzioni, strutture dati, definizioni e
riferimenti dal browser.

Per preparare e gestire tutti i browser del codice insieme, usa invece
[tools/code-browsing](../../tools/code-browsing/README.md). Contiene gli script
aggregati per controllare Docker, fare setup, avviare, fermare, riavviare e
controllare Sourcebot, Elixir e Kythe.

Sono disponibili anche due esperimenti aggiuntivi:

- [sourcebot-browser](../sourcebot-browser/README.md): Sourcebot, una UI web
  moderna per ricerca indicizzata, file explorer e lettura del codice.
- [kythe-browser](../kythe-browser/README.md): Kythe, utile come backend
  semantico sperimentale, ma non come browser grafico principale.

Per gli studenti, il percorso pratico consigliato e' partire da Sourcebot o
Elixir e usare Kythe solo dopo aver capito perche' un grafo semantico puo'
servire a costruire strumenti piu' avanzati.

Ogni capitolo dovrebbe contenere:

- contesto generale
- responsabilita' del componente
- esempi di codice o comandi
- diagrammi quando aiutano
- punti importanti da ricordare
- domande o esercizi di ripasso quando utile

## Stato della documentazione

Il file [documentation-progress.md](documentation-progress.md) tiene traccia dei
capitoli scritti, incompleti o ancora da iniziare.
