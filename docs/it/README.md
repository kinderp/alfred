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
12. [Semantica degli eventi](13-semantica-eventi.md)
13. [Scenari di test](14-scenari-test.md)
14. [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
15. [Roadmap documentazione avanzata](17-roadmap-documentazione-avanzata.md)
16. [Modello licenze](18-modello-licenze.md)
17. [Roadmap CLI e pagina man](19-roadmap-cli-e-man-page.md)
18. [Matrice eventi inotify](20-matrice-eventi-inotify.md)
19. [Roadmap scanner e resync](21-roadmap-scanner-resync.md)
20. [Contratto dei log](22-contratto-log.md)
21. [Roadmap plugin backend](23-roadmap-plugin-backend.md)
22. [Roadmap AI agent guardrail](24-roadmap-ai-agent-guardrail.md)
23. [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)
24. [Stato funzionalita' supportate](26-stato-funzionalita.md)
25. [Guida alla lettura della documentazione](27-guida-lettura-documentazione.md)
26. [Audit documentazione e debiti dichiarati](28-audit-documentazione-e-debiti.md)
27. [Event Model v0](29-event-model-v0.md)
28. [Backend API v0](30-backend-api-v0.md)
29. [Milestone backend inotify di riferimento](31-milestone-inotify-reference-backend.md)
30. [Writer API v0](32-writer-api-v0.md)
31. [Roadmap Writer Runtime v0](33-writer-runtime-roadmap-v0.md)
32. [Report benchmark prestazioni](34-report-benchmark-prestazioni.md)
33. [Audit esplorativi notturni](audit/README.md)
34. [Glossario](glossario.md)

## Come usare questi documenti

Leggi i capitoli in ordine se parti da zero. Se invece stai lavorando su una
parte specifica della codebase, usa l'indice come riferimento rapido.

Per un percorso piu' guidato usa
[Guida alla lettura della documentazione](27-guida-lettura-documentazione.md).
Quel documento divide la lettura per obiettivi: capire il progetto da zero,
usare Alfred, contribuire al codice, studiare gli eventi, leggere i test,
seguire scanner/resync e orientarsi nella roadmap futura.

Per capire cosa e' stato provato durante gli audit esplorativi, leggere
[audit/README.md](audit/README.md). I report storici degli audit notturni
collegano scenari, comandi, artifact locali, issue GitHub e bug confermati.

Per una nuova sessione con un agente AI o per riprendere il lavoro dopo una
pausa lunga, partire da [AGENTS.md](../../AGENTS.md), dalle
[Regole operative](00-regole-operative.md) e dalla
[Milestone backend inotify di riferimento](31-milestone-inotify-reference-backend.md).
La milestone chiarisce che inotify e' il backend di riferimento corrente, ma non
il confine finale del prodotto.

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

## Pagine man

Le prime bozze delle pagine man sono in `docs/man/`:

- [alfred(1)](../man/man1/alfred.1)
- [alfred.conf(5)](../man/man5/alfred.conf.5)
- [alfred-events(7)](../man/man7/alfred-events.7)

Si possono leggere localmente con:

```bash
man -l docs/man/man1/alfred.1
man -l docs/man/man5/alfred.conf.5
man -l docs/man/man7/alfred-events.7
```
