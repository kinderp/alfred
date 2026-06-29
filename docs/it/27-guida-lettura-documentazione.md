# Guida alla lettura della documentazione

Questo documento spiega in quale ordine leggere la documentazione di Alfred e
come trovare rapidamente un argomento specifico. Serve per non perdersi quando
la documentazione cresce: il file `README.md` resta l'indice breve della
cartella, mentre questa guida e' il percorso ragionato per arrivare a una
comprensione completa del progetto.

La documentazione ha tre obiettivi diversi:

- aiutare un utente a compilare, configurare e provare Alfred;
- aiutare un contributore a modificare il codice senza rompere i contratti;
- aiutare uno studente a capire come e' costruita una codebase C reale.

## Percorso 1: capire il progetto da zero

Questo percorso e' pensato per chi non conosce Alfred e vuole capire prima il
problema, poi l'architettura e infine il codice.

1. [Panoramica del progetto](01-panoramica-progetto.md)
   spiega perche' Alfred esiste, cosa significa osservare il filesystem e
   perche' non basta esporre direttamente gli eventi del kernel.
2. [Glossario](glossario.md)
   chiarisce subito le parole ricorrenti: backend, core, raw event, evento
   semantico, dedup, relocate, sanitizer.
3. [Architettura generale](02-architettura-generale.md)
   introduce i livelli principali: applicazione, backend, core, log e modello
   degli eventi.
4. [Struttura delle cartelle](03-struttura-cartelle.md)
   collega i concetti ai percorsi reali del repository.
5. [Stato funzionalita' supportate](26-stato-funzionalita.md)
   riassume cosa funziona oggi, cosa e' diagnostico, cosa e' configurabile e
   cosa e' ancora fuori dallo scope corrente.
6. [Audit documentazione e debiti dichiarati](28-audit-documentazione-e-debiti.md)
   separa problemi documentali reali, debiti tecnici rimandati e idee future.

Dopo questi cinque documenti il lettore dovrebbe sapere che Alfred oggi usa il
core come stream ufficiale, che inotify e' solo il backend Linux corrente e che
gli eventi passano attraverso piu' livelli prima di diventare semantica stabile.

## Percorso 2: usare Alfred come utente

Questo percorso e' utile per compilare, lanciare e configurare Alfred senza
entrare subito nei dettagli interni.

1. [README principale](../../README.md)
   contiene la presentazione pubblica del progetto e i comandi rapidi.
2. [Makefile e build system](09-makefile-e-build-system.md)
   spiega `make`, i target principali, la build di debug e la build release.
3. [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
   mostra come eseguire test, conservare log, leggere fallimenti e usare gli
   strumenti di navigazione del codice.
4. [Roadmap CLI e pagina man](19-roadmap-cli-e-man-page.md)
   descrive lo stato attuale della CLI e le opzioni future.
5. Pagine man:
   [alfred(1)](../man/man1/alfred.1),
   [alfred.conf(5)](../man/man5/alfred.conf.5),
   [alfred-events(7)](../man/man7/alfred-events.7).

Le pagine man sono volutamente piu' secche e normative degli MD: servono come
riferimento operativo. Gli MD invece spiegano il ragionamento, le scelte e i
compromessi.

## Percorso 3: contribuire al codice

Questo percorso serve a chi vuole aprire una pull request.

1. [Come contribuire](11-come-contribuire.md)
   spiega fork, branch, sincronizzazione con upstream, struttura GitHub del
   progetto, pull request, CI e review.
2. [Regole operative](00-regole-operative.md)
   raccoglie il metodo di lavoro del progetto: micro-step, documentazione
   obbligatoria, commit message, test prima del commit, uso degli strumenti.
3. [Procedura manuale prima di un commit](10-debugging-test-e-strumenti.md#procedura-manuale-prima-di-un-commit)
   elenca i controlli da fare prima di proporre una modifica.
4. [Aggiungere un test](11-come-contribuire.md#aggiungere-un-test)
   spiega dove mettere i test e quale suite aggiornare.
5. [Stile dei commenti nel codice](11-come-contribuire.md#stile-dei-commenti-nel-codice)
   rimanda alle regole per commentare header, sorgenti e test.

La regola pratica e': prima si capisce il contratto che si sta toccando, poi si
modifica il codice, poi si aggiorna la documentazione e infine si eseguono i
test collegati.

Per capire anche il lavoro in corso su GitHub, leggere in questo ordine:

1. la milestone corrente, per esempio
   [`Writer Runtime v0`](https://github.com/kinderp/alfred/milestone/1);
2. la issue madre della milestone, per esempio
   [`Writer Runtime v0: implementation plan`](https://github.com/kinderp/alfred/issues/32);
3. il Project nella pagina
   [`Repository Projects`](https://github.com/kinderp/alfred/projects), dove
   [`Alfred Roadmap`](https://github.com/users/kinderp/projects/1) mostra issue
   e PR in una vista operativa;
4. le Discussions collegate, che conservano il ragionamento progettuale prima
   che diventi documentazione stabile.

Questa distinzione evita un errore comune: una Discussion puo' contenere idee
ancora aperte, mentre gli `.md` nel repository devono descrivere la decisione
consolidata.

## Percorso 4: capire il codice internamente

Questo percorso e' la lettura guidata per chi vuole sapere quale funzione chiama
quale altra funzione e quali strutture dati vengono modificate.

1. [Livello applicazione](04-livello-applicazione.md)
   descrive `main`, `app_init`, `app_run`, configurazione, log e shutdown.
2. [Modulo inotify](05-modulo-inotify.md)
   descrive il backend Linux corrente, la maschera di watch, l'adapter e la
   gestione dei watch ricorsivi.
3. [Core engine](06-core-engine.md)
   descrive il livello che trasforma raw event in eventi semantici.
4. [Flusso eventi](07-flusso-eventi.md)
   mostra il percorso runtime dagli eventi kernel fino ai log ufficiali.
5. [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
   e' il documento principale per seguire call graph, strutture dati, stati dei
   watch, code di recovery e scenari animabili.

Questo percorso va letto con il codice aperto a fianco. Per farlo in modo piu'
comodo si possono usare Sourcebot, Elixir o Kythe come spiegato in
[Debugging, test e strumenti](10-debugging-test-e-strumenti.md#browser-del-codice).

## Percorso 5: capire gli eventi

Questo e' il percorso piu' importante per non confondere eventi kernel, eventi
raw Alfred, eventi semantici e diagnostica.

1. [Semantica degli eventi](13-semantica-eventi.md)
   spiega le decisioni semantiche: create, delete, modify, file-ready, move,
   rename, relocate, dedup e overflow.
2. [Event Model v0](29-event-model-v0.md)
   definisce il record strutturato comune basato su
   `layer + category + type`.
3. [Matrice eventi inotify](20-matrice-eventi-inotify.md)
   elenca gli eventi inotify, indica se Alfred li richiede, li osserva, li
   converte in raw event o li lascia come diagnostica.
4. [Contratto dei log](22-contratto-log.md)
   spiega cosa significano le righe scritte nei log e in quale contesto vengono
   generate.
5. [Scenari di test](14-scenari-test.md)
   collega gli scenari reali ai log attesi.
6. [alfred-events(7)](../man/man7/alfred-events.7)
   riassume il modello in forma da manuale tecnico.

La distinzione principale e': il raw log del backend racconta cosa arriva dal
kernel o cosa diagnostica il backend; gli `ALFRED_RAW_*` sono il contratto
interno verso il core; gli eventi semantici sono il significato stabile che
Alfred vuole esporre verso l'esterno.

## Percorso 6: test, log e verifica

Questo percorso spiega come dimostrare che una modifica funziona.

1. [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
   contiene la procedura operativa e la spiegazione degli strumenti.
2. [Scenari di test](14-scenari-test.md)
   descrive scenari core, funzionali, backend diagnostics, scanner e watcher.
3. [Contratto dei log](22-contratto-log.md)
   serve per interpretare le righe assertate dai test.
4. [Stato funzionalita' supportate](26-stato-funzionalita.md)
   indica quali comportamenti sono gia' coperti e quali sono rimandati.
5. [Qualita' del prodotto software](35-qualita-prodotto-software.md)
   spiega robustezza, affidabilita', sicurezza, coerenza, performance,
   operabilita' e documentazione come concetti di ingegneria del software.
6. [Use case, posizionamento e integrazioni](36-use-cases-posizionamento-integrazioni.md)
   chiarisce cosa Alfred e' oggi, cosa non deve promettere ancora, quali demo
   usare e con quali strumenti integrarlo invece di sostituirli.

Per i test shell e' importante leggere anche la sezione sulle regex in
[Debugging, test e strumenti](10-debugging-test-e-strumenti.md#leggere-le-regex-nei-test-shell),
perche' molti assert confrontano pattern e non stringhe letterali.

Gli audit esplorativi notturni sono un livello diverso dai test ufficiali:
provano Alfred come farebbe un utente, lasciano una traccia su GitHub e
raccolgono scenari che potranno diventare test stabili. Per questo percorso
leggere anche:

1. [Audit esplorativi notturni](audit/README.md)
   descrive issue madre, issue figlie, report, artifact e promozione degli
   scenari.
2. [Playbook test notturni](audit/nightly-playbook.md)
   definisce la procedura operativa da seguire quando viene chiesto di iniziare
   una sessione di test notturni autonoma.
3. [Matrice di maturita' da audit](audit/maturity-matrix.md)
   spiega come stimare la maturita' osservata di una funzionalita' senza
   confonderla con la sola copertura della suite ufficiale.
4. [Scenari esplorativi nightly](../../tests/exploratory/nightly/README.md)
   mostra gli script rilanciabili e spiega quali log ispezionare.

## Percorso 7: resync, scanner e recovery

Questo percorso riguarda la parte piu' delicata: cosa succede quando un watch
diventa stale, una directory osservata viene rinominata o Alfred perde
temporaneamente lo scope osservato.

1. [Roadmap scanner e resync](21-roadmap-scanner-resync.md)
   e' il documento principale: contiene motivazioni, funzioni usate, stati,
   recovery, CSV di benchmark, esempi shell e debiti rimasti.
2. [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
   descrive `watcher_entry_t`, `watcher_state_t`, code di lost-scope recovery e
   transizioni di stato.
3. [Contratto dei log](22-contratto-log.md)
   spiega righe come `WATCH_STALE`, `WATCH_RESYNC_BEGIN`,
   `WATCH_RESYNC_FAILED` e messaggi di recovery.
4. [Matrice eventi inotify](20-matrice-eventi-inotify.md)
   chiarisce il ruolo di `IN_MOVE_SELF`, `IN_DELETE_SELF`, `IN_IGNORED` e
   `IN_UNMOUNT`.

Questi documenti vanno letti insieme: lo scanner e' un componente tecnico, ma
il motivo per cui esiste e' semantico e operativo.

## Percorso 8: roadmap futura

Questo percorso serve per capire dove sta andando il progetto.

1. [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)
   ordina Event Model v0, Backend API v0, JSONL, tracepoint logici, Alfred Lab,
   performance suite e backend complessi.
2. [Backend API v0](30-backend-api-v0.md)
   definisce lifecycle, target, capabilities e record emessi dai backend.
3. [Roadmap plugin backend](23-roadmap-plugin-backend.md)
   descrive l'idea di una API comune per backend inotify, fanotify, audit, eBPF,
   Windows e macOS.
4. [Writer API v0](32-writer-api-v0.md)
   definisce writer, sink, ownership, queue, dispatcher e output strutturato.
5. [Roadmap Writer Runtime v0](33-writer-runtime-roadmap-v0.md)
   ordina i passi per spostare text, JSONL e futuri writer fuori dal percorso
   caldo tramite coda, dispatcher e benchmark.
6. [Roadmap AI agent guardrail](24-roadmap-ai-agent-guardrail.md)
   collega Alfred all'obiettivo piu' ampio: runtime security per agenti AI.
7. [Roadmap documentazione avanzata](17-roadmap-documentazione-avanzata.md)
   raccoglie idee su documentazione navigabile, grafi, animazioni, Doxygen,
   Graphviz, Kythe e altri strumenti.
8. [Modello licenze](18-modello-licenze.md)
   documenta le ipotesi su core open source e moduli futuri piu' restrittivi o
   commerciali.
9. [Use case, posizionamento e integrazioni](36-use-cases-posizionamento-integrazioni.md)
   separa posizionamento attuale, competitor, integrazioni consigliate e use
   case futuri legati agli agenti AI.
10. [Registro milestone del progetto](37-roadmap-milestone-progetto.md)
   collega milestone GitHub, issue madre, durata orientativa, durata reale,
   dipendenze e documenti stabili in una vista cronologica.
11. [Visione Observation Runtime](38-visione-observation-runtime.md)
   descrive la direzione lunga: Alfred come runtime di osservazioni e non solo
   come motore di eventi filesystem.
12. [Principi architetturali futuri](39-principi-architetturali-futuri.md)
   traduce quella visione in regole pratiche: provenance, log append-only,
   backend come sensori e percorso caldo corto.

Questi documenti non sono tutti contratti gia' implementati: alcuni sono
decisioni attuali, altri sono roadmap. Prima di implementare codice nuovo bisogna
controllare sempre se il tema e' gia' marcato come futuro o rimandato.

## Percorso 9: sessione Codex o agente AI

Questo percorso e' pensato per una nuova sessione di lavoro con un agente AI.
L'obiettivo non e' leggere tutta la documentazione, ma ricostruire il contesto
minimo e poi scegliere i documenti pertinenti al task.

1. [AGENTS.md](../../AGENTS.md)
   e' l'entrypoint breve nella root: spiega priorita' corrente e regole
   architetturali da non violare.
2. [Regole operative](00-regole-operative.md)
   definisce metodo di lavoro, commit, test, documentazione e bootstrap.
3. [Questa guida](27-guida-lettura-documentazione.md)
   serve a scegliere il percorso corretto invece di leggere tutto.
4. [Stato documentazione](documentation-progress.md)
   indica quali documenti sono completi, parziali o superati.
5. [Milestone backend inotify di riferimento](31-milestone-inotify-reference-backend.md)
   definisce il perimetro corrente: chiudere inotify come backend di
   riferimento, senza trasformarlo nel confine finale del prodotto.
6. [Event Model v0](29-event-model-v0.md)
   va letto se il task tocca record, adapter, formatter, writer o log
   strutturati.
7. [Backend API v0](30-backend-api-v0.md)
   va letto se il task tocca backend, lifecycle, capabilities, target o sink.
8. [Writer API v0](32-writer-api-v0.md)
   va letto se il task tocca writer, JSONL, protobuf, MessagePack, socket,
   output strutturato, logger o percorso caldo.
9. [Roadmap Writer Runtime v0](33-writer-runtime-roadmap-v0.md)
   va letto se il task tocca queue, dispatcher, worker, backpressure, profili
   output, benchmark dei sink o collegamento runtime dei writer.
10. [Roadmap AI agent guardrail](24-roadmap-ai-agent-guardrail.md)
   e' visione futura: va usata per orientare le scelte, non come autorizzazione
   automatica a implementare Agent Guard completo.
11. [Principi architetturali futuri](39-principi-architetturali-futuri.md)
   va letto quando si introduce un campo comune, una nuova API o una astrazione
   che potrebbe vincolare il futuro modello di osservazioni.

Prima di modificare codice, una sessione agente dovrebbe rispondere almeno a
queste domande:

1. Quale modulo sto toccando?
2. Quale contratto puo' cambiare?
3. Quali documenti sono contrattuali per questo task?
4. Quali documenti sono solo roadmap?
5. Quali test vanno eseguiti o aggiornati?
6. Cosa non va implementato ora?

## Indice tematico rapido

| Argomento | Dove leggere |
| --- | --- |
| Architettura generale | [02](02-architettura-generale.md), [07](07-flusso-eventi.md), [16](16-mappa-codice-e-strutture.md) |
| Audit esplorativi notturni | [audit/README.md](audit/README.md), [audit/nightly-playbook.md](audit/nightly-playbook.md), [audit/maturity-matrix.md](audit/maturity-matrix.md), [tests/exploratory/nightly](../../tests/exploratory/nightly/README.md) |
| Backend inotify | [05](05-modulo-inotify.md), [20](20-matrice-eventi-inotify.md), [26](26-stato-funzionalita.md) |
| Backend API v0 | [30](30-backend-api-v0.md), [23](23-roadmap-plugin-backend.md), [29](29-event-model-v0.md) |
| Build e Makefile | [09](09-makefile-e-build-system.md) |
| Callback, puntatori a funzione e `void *userdata` | [08](08-guida-c-usato-nel-progetto.md), [16](16-mappa-codice-e-strutture.md), [32](32-writer-api-v0.md) |
| CLI e configurazione | [19](19-roadmap-cli-e-man-page.md), [alfred(1)](../man/man1/alfred.1), [alfred.conf(5)](../man/man5/alfred.conf.5) |
| Commenti nel codice | [11](11-come-contribuire.md#stile-dei-commenti-nel-codice), `docs/commenting-style.md`, `docs/commenting-progress.md` |
| Contribuire | [11](11-come-contribuire.md), [00](00-regole-operative.md) |
| Core engine | [06](06-core-engine.md), [13](13-semantica-eventi.md), [16](16-mappa-codice-e-strutture.md) |
| Dedup | [13](13-semantica-eventi.md#dedup), [glossario](glossario.md#dedup) |
| Diagnostica backend | [22](22-contratto-log.md), [14](14-scenari-test.md) |
| Event Model v0 | [29](29-event-model-v0.md), [22](22-contratto-log.md), [25](25-roadmap-unificata-dossier.md) |
| Eventi inotify | [20](20-matrice-eventi-inotify.md), [05](05-modulo-inotify.md), [alfred-events(7)](../man/man7/alfred-events.7) |
| Eventi semantici | [13](13-semantica-eventi.md), [06](06-core-engine.md), [22](22-contratto-log.md) |
| File ready / close-write | [13](13-semantica-eventi.md#scrittura-file-modify-e-file-ready), [14](14-scenari-test.md) |
| Fork e pull request | [11](11-come-contribuire.md) |
| GitHub Actions | [11](11-come-contribuire.md#github-actions-sulla-pr) |
| Milestone inotify corrente | [31](31-milestone-inotify-reference-backend.md), [30](30-backend-api-v0.md), [29](29-event-model-v0.md) |
| Log e copertura JSONL | [22](22-contratto-log.md#copertura-record-sink-e-output-jsonl), [14](14-scenari-test.md) |
| Lost-scope recovery | [21](21-roadmap-scanner-resync.md), [16](16-mappa-codice-e-strutture.md), [22](22-contratto-log.md) |
| Man page | [19](19-roadmap-cli-e-man-page.md), [docs/man](../man/) |
| Memoria C, lifetime, ownership e leak | [08](08-guida-c-usato-nel-progetto.md#lifetime-della-memoria), [29](29-event-model-v0.md#ownership-dei-campi), [32](32-writer-api-v0.md#ownership-e-record-accodati) |
| Milestone, dipendenze e durata prevista | [37](37-roadmap-milestone-progetto.md), [31](31-milestone-inotify-reference-backend.md), [25](25-roadmap-unificata-dossier.md) |
| Queue, dispatcher e sink record | [32](32-writer-api-v0.md), [33](33-writer-runtime-roadmap-v0.md), [16](16-mappa-codice-e-strutture.md), [29](29-event-model-v0.md) |
| Move, rename, relocate | [13](13-semantica-eventi.md#rename-move-e-relocate), [14](14-scenari-test.md) |
| Output strutturato futuro | [32](32-writer-api-v0.md), [33](33-writer-runtime-roadmap-v0.md), [22](22-contratto-log.md#testo-oggi-protocollo-domani), [25](25-roadmap-unificata-dossier.md) |
| Performance | [34](34-report-benchmark-prestazioni.md), [33](33-writer-runtime-roadmap-v0.md), [32](32-writer-api-v0.md#percorso-caldo), [21](21-roadmap-scanner-resync.md), [25](25-roadmap-unificata-dossier.md) |
| Posizionamento, competitor e integrazioni | [36](36-use-cases-posizionamento-integrazioni.md), [24](24-roadmap-ai-agent-guardrail.md), [32](32-writer-api-v0.md), [25](25-roadmap-unificata-dossier.md) |
| Observation Runtime e visione cognitiva | [38](38-visione-observation-runtime.md), [39](39-principi-architetturali-futuri.md), [24](24-roadmap-ai-agent-guardrail.md) |
| Qualita' prodotto | [35](35-qualita-prodotto-software.md), [audit/maturity-matrix.md](audit/maturity-matrix.md), [34](34-report-benchmark-prestazioni.md) |
| Plugin backend | [30](30-backend-api-v0.md), [23](23-roadmap-plugin-backend.md), [25](25-roadmap-unificata-dossier.md) |
| Plugin writer | [32](32-writer-api-v0.md#plugin-writer), [29](29-event-model-v0.md), [30](30-backend-api-v0.md) |
| Prossimi debiti da discutere | [28](28-audit-documentazione-e-debiti.md), [25](25-roadmap-unificata-dossier.md) |
| Regex nei test shell | [10](10-debugging-test-e-strumenti.md#leggere-le-regex-nei-test-shell) |
| Scanner filesystem | [21](21-roadmap-scanner-resync.md) |
| Self events | [20](20-matrice-eventi-inotify.md#eventi-sul-watch-stesso), [21](21-roadmap-scanner-resync.md), [22](22-contratto-log.md) |
| Shadow mode storico | [07](07-flusso-eventi.md#flusso-storico-shadow-mode), [00](00-regole-operative.md#stato-semantico-corrente) |
| Sourcebot, Elixir, Kythe | [10](10-debugging-test-e-strumenti.md#browser-del-codice), [docs/sourcebot-browser](../sourcebot-browser/README.md), [docs/code-browser](../code-browser/README.md), [docs/kythe-browser](../kythe-browser/README.md) |
| Test | [10](10-debugging-test-e-strumenti.md), [14](14-scenari-test.md), [11](11-come-contribuire.md#aggiungere-un-test) |
| Watch ricorsivi | [05](05-modulo-inotify.md#scan-ricorsivo-e-discovery), [21](21-roadmap-scanner-resync.md), [26](26-stato-funzionalita.md) |
| Watch state | [16](16-mappa-codice-e-strutture.md#watcher_state_t), [21](21-roadmap-scanner-resync.md), [22](22-contratto-log.md) |

## Come scegliere cosa leggere prima di modificare codice

Prima di toccare un file, conviene rispondere a queste domande:

1. Sto cambiando il comportamento utente?
   Leggi `README.md`, `alfred(1)`, `alfred.conf(5)` e la guida CLI.
2. Sto cambiando eventi o log?
   Leggi semantica eventi, matrice inotify, contratto log e scenari di test.
3. Sto cambiando watch, scanner o recovery?
   Leggi modulo inotify, roadmap scanner/resync, mappa strutture dati e log.
4. Sto cambiando build o test?
   Leggi Makefile, debugging/test/strumenti e come contribuire.
5. Sto introducendo un concetto nuovo?
   Aggiorna glossario, documento architetturale collegato, test e stato
   documentazione.

Questa regola evita modifiche isolate: in Alfred una modifica corretta deve
essere coerente almeno con codice, test, documentazione e contratto dei log.

## Documenti di riferimento rapido

Quando non serve leggere tutto, questi documenti sono i piu' utili come
riferimento veloce:

- [Stato funzionalita' supportate](26-stato-funzionalita.md):
  per sapere cosa Alfred supporta oggi.
- [Contratto dei log](22-contratto-log.md):
  per interpretare output e test.
- [Matrice eventi inotify](20-matrice-eventi-inotify.md):
  per decidere cosa fare con un evento kernel.
- [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md):
  per orientarsi nel codice.
- [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md):
  per capire la direzione futura.
- [Registro milestone del progetto](37-roadmap-milestone-progetto.md):
  per capire ordine cronologico, durata, dipendenze e priorita' delle
  milestone.
- [Principi architetturali futuri](39-principi-architetturali-futuri.md):
  per controllare che una nuova astrazione non leghi Alfred al solo filesystem
  o a un singolo backend.
- [Audit documentazione e debiti dichiarati](28-audit-documentazione-e-debiti.md):
  per distinguere problemi reali, scelte rimandate e punti da discutere.
- [documentation-progress.md](documentation-progress.md):
  per sapere quali capitoli sono completi, parziali o rimossi.

## Regola di manutenzione

Quando viene aggiunto un nuovo documento stabile in `docs/it`, bisogna
aggiornare anche:

- [README.md](README.md), se il documento deve essere visibile nell'indice breve;
- questo file, se il documento cambia il percorso di lettura o introduce un
  argomento nuovo;
- [documentation-progress.md](documentation-progress.md), per indicare lo stato
  del documento;
- eventuali pagine man, se il documento cambia un contratto utente o tecnico.
