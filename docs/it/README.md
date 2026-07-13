# Documentazione didattica

Questa cartella contiene la documentazione pensata per studenti e nuovi
contributori. I commenti nel codice restano in inglese e sono sintetici; questi
documenti invece spiegano architettura, sintassi C, strumenti e ragionamenti con
piu' dettaglio.

## Indice ragionato

La tabella non sostituisce la
[Guida alla lettura della documentazione](27-guida-lettura-documentazione.md).
Serve come indice rapido: per ogni capitolo indica cosa contiene, chi dovrebbe
leggerlo e quando.

| Capitolo | Cosa contiene | Chi dovrebbe leggerlo | Quando leggerlo |
| --- | --- | --- | --- |
| [00 - Regole operative](00-regole-operative.md) | Metodo di lavoro, micro-step, commit, review, GitHub, documentazione e regole per agenti AI. | Maintainer, contributori, agenti AI. | Prima di modificare codice o documentazione. |
| [01 - Panoramica del progetto](01-panoramica-progetto.md) | Problema iniziale, differenza tra inotify raw ed eventi semantici, pipeline generale. | Tutti, soprattutto nuovi lettori. | Per capire perche' Alfred esiste. |
| [02 - Architettura generale](02-architettura-generale.md) | Livelli principali: applicazione, backend, core, log e responsabilita'. | Studenti e contributori. | Prima di leggere codice o fare refactor. |
| [03 - Struttura delle cartelle](03-struttura-cartelle.md) | Mappa delle directory e ruolo dei file principali. | Studenti e nuovi contributori. | Quando bisogna orientarsi nel repository. |
| [04 - Livello applicazione](04-livello-applicazione.md) | `main`, `app_init`, `app_run`, configurazione, shutdown e responsabilita' di `app`. | Chi lavora su startup, config o ciclo runtime. | Prima di toccare `app/` o la CLI. |
| [05 - Modulo inotify](05-modulo-inotify.md) | Backend Linux corrente, adapter, watch, maschere, eventi raw e discovery. | Chi lavora sul backend inotify. | Prima di modificare `modules/inotify/`. |
| [06 - Core engine](06-core-engine.md) | Correlazione raw -> semantica, rename/move, debounce e responsabilita' del core. | Chi lavora su eventi semantici. | Prima di cambiare semantica o test core. |
| [07 - Flusso eventi](07-flusso-eventi.md) | Percorso runtime dagli eventi kernel ai log e note storiche sullo shadow mode. | Studenti e contributori. | Quando bisogna capire cosa attraversa la pipeline. |
| [08 - C usato nel progetto](08-guida-c-usato-nel-progetto.md) | Concetti C usati in Alfred: puntatori, callback, `void *`, lifetime, ownership e leak. | Studenti e nuovi contributori C. | Prima di leggere API con callback o ownership memoria. |
| [09 - Makefile e build system](09-makefile-e-build-system.md) | Target `make`, variabili, `.PHONY`, build debug/release e quando aggiornare il Makefile. | Contributori. | Prima di cambiare build, test o file sorgenti compilati. |
| [10 - Debugging, test e strumenti](10-debugging-test-e-strumenti.md) | Procedura prima del commit, test suite, regex shell, log, browser del codice e strumenti. | Contributori e studenti. | Quando si deve verificare o debuggare una modifica. |
| [11 - Come contribuire](11-come-contribuire.md) | Fork, branch, PR, CI, review, aggiunta test e stile commenti. | Nuovi contributori. | Prima di aprire una PR. |
| [13 - Semantica degli eventi](13-semantica-eventi.md) | Decisioni semantiche su create, delete, modify, file-ready, rename, move, relocate, dedup e overflow. | Chi tocca eventi o core. | Prima di cambiare significato di un evento. |
| [14 - Scenari di test](14-scenari-test.md) | Scenari core, funzionali, backend, scanner, watcher e log attesi. | Chi aggiunge o interpreta test. | Quando un comportamento deve diventare verificabile. |
| [16 - Mappa codice e strutture](16-mappa-codice-e-strutture.md) | Lettura guidata della codebase, funzioni, strutture dati, stati watch e diagrammi. | Studenti, contributori, agenti AI. | Quando serve capire chi chiama chi e cosa modifica cosa. |
| [17 - Roadmap documentazione avanzata](17-roadmap-documentazione-avanzata.md) | Idee su documentazione navigabile, grafi, animazioni, Doxygen, Graphviz e Kythe. | Maintainer e doc tooling. | Quando si progetta documentazione interattiva o grafica. |
| [18 - Modello licenze](18-modello-licenze.md) | Ipotesi su core open source e moduli futuri piu' restrittivi o commerciali. | Maintainer. | Prima di decidere licenze o packaging commerciale. |
| [19 - Roadmap CLI e pagina man](19-roadmap-cli-e-man-page.md) | Stato CLI, configurazione, pagine man e direzione futura dell'interfaccia utente. | Chi lavora su CLI o documentazione utente. | Prima di cambiare opzioni o comandi. |
| [20 - Matrice eventi inotify](20-matrice-eventi-inotify.md) | Tutti gli eventi inotify, se Alfred li richiede, li osserva, li ignora o li rimanda. | Chi lavora sul backend o sulle maschere. | Prima di gestire un nuovo evento inotify. |
| [21 - Roadmap scanner e resync](21-roadmap-scanner-resync.md) | Scanner filesystem, resync, recovery, watch stale, identity tracking, CSV e benchmark. | Chi lavora su recovery o scanner. | Prima di toccare watcher table, scanner o lost-scope. |
| [22 - Contratto dei log](22-contratto-log.md) | Significato di `raw.log`, `events.log`, `output.jsonl` e record loggati. | Chi scrive test, audit o output. | Quando bisogna interpretare o cambiare log. |
| [23 - Roadmap plugin backend](23-roadmap-plugin-backend.md) | API futura per backend inotify, fanotify, audit, eBPF, Windows e macOS. | Chi progetta backend futuri. | Prima di introdurre nuove sorgenti eventi. |
| [24 - Roadmap AI agent guardrail](24-roadmap-ai-agent-guardrail.md) | Visione futura: Agent Action Ledger, workspace boundary, policy, deep runtime inspection. | Maintainer e architettura security. | Per orientare scelte future senza implementarle subito. |
| [25 - Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md) | Roadmap complessiva: Event Model, Backend API, JSONL, Lab, performance e backend complessi. | Maintainer e contributori esperti. | Quando si decide la prossima milestone. |
| [26 - Stato funzionalita'](26-stato-funzionalita.md) | Cosa Alfred supporta oggi, cosa e' diagnostico, cosa e' parziale e cosa e' rimandato. | Tutti. | Prima di progettare nuove feature o use case. |
| [27 - Guida lettura documentazione](27-guida-lettura-documentazione.md) | Percorsi di lettura per utente, contributore, studente, eventi, test e roadmap. | Tutti. | Quando non sai quali documenti leggere. |
| [28 - Audit documentazione e debiti](28-audit-documentazione-e-debiti.md) | Debiti documentali, diagrammi, link, decisioni rimandate e problemi noti della doc. | Maintainer. | Quando si ripulisce o riallinea la documentazione. |
| [29 - Event Model v0](29-event-model-v0.md) | `alfred_record_t`, layer/category/type, ownership, record raw/semantic/diagnostic e JSONL. | Chi lavora su record, adapter, writer o golden test. | Prima di cambiare schema dati o output strutturato. |
| [30 - Backend API v0](30-backend-api-v0.md) | Lifecycle backend, target, capabilities, errori e confine backend/core. | Chi lavora su backend API. | Prima del refactor backend-agnostic. |
| [31 - Milestone inotify reference backend](31-milestone-inotify-reference-backend.md) | Perimetro corrente: chiudere inotify come backend di riferimento senza allargare troppo lo scope. | Maintainer, agenti AI, contributori. | A inizio sessione di lavoro architetturale. |
| [32 - Writer API v0](32-writer-api-v0.md) | Sink, writer, queue, dispatcher, ownership, output strutturato e percorso caldo. | Chi lavora su output e performance. | Prima di toccare JSONL, text writer, socket o dispatcher. |
| [33 - Writer Runtime v0](33-writer-runtime-roadmap-v0.md) | Stato e roadmap del runtime output: queue bounded, drain single-threaded, dispatcher, JSONL/counter opt-in, statistiche e limiti futuri. | Chi lavora su runtime output. | Prima di toccare pipeline output, worker, code per sink o backpressure. |
| [34 - Report benchmark prestazioni](34-report-benchmark-prestazioni.md) | Misure eseguite, interpretazione, comandi benchmark e risultati osservati. | Chi valuta performance. | Dopo benchmark o prima di decisioni prestazionali. |
| [35 - Qualita' prodotto software](35-qualita-prodotto-software.md) | Robustezza, affidabilita', sicurezza, coerenza, performance, operabilita' e documentazione. | Studenti e maintainer. | Per capire come valutiamo la maturita' di Alfred. |
| [36 - Use case e integrazioni](36-use-cases-posizionamento-integrazioni.md) | Posizionamento, use case, competitor, integrazioni e matrice funzionalita' -> use case. | Maintainer, utenti, contributori. | Quando bisogna capire a cosa serve Alfred o comunicarlo. |
| [37 - Registro milestone del progetto](37-roadmap-milestone-progetto.md) | Milestone in ordine cronologico, durata prevista/reale, dipendenze, priorita', GitHub tracking e documenti collegati. | Maintainer, contributori, agenti AI. | Quando si decide o si ricostruisce l'evoluzione del progetto. |
| [38 - Visione Observation Runtime](38-visione-observation-runtime.md) | Visione lunga: Alfred come runtime di osservazioni, memoria, provenance, replay, feedback e sistemi intelligenti. | Maintainer e architettura. | Per orientare scelte future senza allargare lo scope corrente. |
| [39 - Principi architetturali futuri](39-principi-architetturali-futuri.md) | Principi pratici: provenance, osservazioni/inferenze separate, backend come sensori, log append-only e percorso caldo corto. | Maintainer, contributori esperti, agenti AI. | Prima di introdurre concetti comuni o nuove API architetturali. |
| [40 - Audit inotify vs Backend API v0](40-audit-inotify-backend-api-v0.md) | Gap list tra backend inotify corrente e Backend API v0 staged subset: lifecycle, target, capabilities, poll, ownership e test. | Chi lavora sulla milestone inotify Backend API v0. | Prima di aprire micro-step di codice sulla conformita' inotify. |
| [41 - Tracepoint e Alfred Lab MVP](41-tracepoint-lab-roadmap-mvp.md) | Roadmap della milestone Tracepoint/Lab: tracepoint logici, scenari MVP, mappa funzioni/test e limiti del primo Lab. | Maintainer, studenti, contributori e agenti AI. | Prima di definire tracepoint, scenari Lab, documentazione animabile o percorsi didattici del runtime. |
| [42 - Tracepoint Model v0](42-tracepoint-model-v0.md) | Contratto documentale dei tracepoint logici: naming, stabilita', metadati, vincoli hot-path, esempi e anti-pattern. | Chi definisce tracepoint, scenari Lab o futuro output trace. | Prima di promuovere un tracepoint a contratto stabile o implementare tracing runtime. |
| [43 - Agent workspace observe ledger v0](43-agent-workspace-observe-ledger-v0.md) | Primo contratto observe-mode per leggere i record correnti come ledger di effetti filesystem, separando evidenza attuale, campi futuri e claim vietati. | Maintainer, studenti, contributori security e agenti AI. | Prima di aggiungere campi agent/session/workspace, would-block o policy. |
| [Lab - Scenari concreti](lab/README.md) | Scenari Lab Markdown v0 che collegano comandi, tracepoint, funzioni, record, output e test. | Studenti, contributori e chi progetta il futuro Lab. | Dopo avere letto Tracepoint/Lab roadmap e Tracepoint Model v0. |
| [Glossario](glossario.md) | Definizioni dei termini ricorrenti del progetto. | Tutti. | Quando un termine non e' chiaro. |

## Audit esplorativi

| Capitolo | Cosa contiene | Chi dovrebbe leggerlo | Quando leggerlo |
| --- | --- | --- | --- |
| [Audit esplorativi notturni](audit/README.md) | Metodo generale degli audit, issue madre/figlie, artifact e rapporto con test ufficiali. | Maintainer e chi esegue audit. | Prima di una sessione esplorativa. |
| [Playbook test notturni](audit/nightly-playbook.md) | Procedura operativa autonoma: bootstrap, scenari, log, issue, upload artifact e report. | Agenti AI e maintainer. | Quando viene richiesto un audit notturno. |
| [Matrice maturita' audit](audit/maturity-matrix.md) | Dimensioni di maturita', freshness, confidence e come leggere i risultati degli audit. | Maintainer, studenti, QA. | Dopo audit o quando si valuta una funzionalita'. |

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
