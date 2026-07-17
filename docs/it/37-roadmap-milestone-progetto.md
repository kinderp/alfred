# Registro milestone del progetto

Questo documento e' il registro cronologico delle milestone di Alfred.
GitHub resta lo strumento operativo per issue, pull request, stato corrente e
avanzamento. Questo file invece conserva la memoria stabile del progetto:
ordine delle milestone, durata prevista, durata reale, dipendenze, motivazione
della priorita' e collegamenti alla documentazione consolidata.

## Perche' esiste

Alfred sta crescendo per micro-step, ma il progetto ha bisogno anche di una
vista piu' lunga. Senza un registro milestone diventa difficile capire:

- in quale ordine sono stati affrontati i problemi;
- perche' una milestone ha avuto precedenza rispetto a un'altra;
- quali milestone dipendono da contratti precedenti;
- quanto tempo era stato stimato e quanto tempo e' servito davvero;
- quali issue, PR e documenti dimostrano che una milestone e' stata chiusa;
- quali lezioni vanno ricordate quando si apre la milestone successiva.

Il registro non sostituisce le roadmap tecniche specifiche. Serve a collegarle
in una storia unica.

## Fonti di verita'

| Strumento | Ruolo | Cosa non deve diventare |
| --- | --- | --- |
| GitHub Milestone | Stato operativo, issue aperte/chiuse, data di fine orientativa, progresso visibile. | Documento narrativo completo. |
| GitHub Project | Board corrente: cosa e' da fare, in corso, bloccato o completato. | Archivio storico dettagliato. |
| Issue madre | Piano operativo della singola milestone, checklist, update, issue figlie e PR. | Specifica stabile da leggere tra mesi senza contesto. |
| Documentazione `docs/it` | Decisioni consolidate, razionale, dipendenze e memoria storica. | Board operativo minuto per minuto. |

La regola pratica e':

```text
GitHub racconta lo stato vivo.
Questo documento racconta l'evoluzione storica.
```

## Campi da usare per ogni milestone

Ogni milestone nuova deve avere almeno questi campi.

| Campo | Significato |
| --- | --- |
| Ordine | Posizione cronologica o logica nel percorso del progetto. |
| Milestone | Nome sintetico e stabile. |
| Stato | `planned`, `in progress`, `done`, `paused` o `superseded`. |
| Finestra prevista | Periodo orientativo di lavoro, con data di inizio e data di fine attesa. |
| Durata stimata | Quanti giorni o settimane pensiamo servano prima di iniziare. |
| Durata reale | Tempo effettivo, compilato a chiusura milestone. |
| Perche' ora | Motivo della priorita': cosa rischiamo o sblocchiamo facendo questa milestone adesso. |
| Dipendenze | Contratti, milestone o decisioni che devono esistere prima. |
| Sblocca | Lavori che diventano possibili dopo la chiusura. |
| GitHub | Link a milestone, issue madre, issue figlie principali e PR. |
| Documenti | MD che descrivono contratto, roadmap o decisioni finali. |

La data di fine e' orientativa, non una promessa rigida. Serve a ragionare su
velocita', priorita' e rischio. Se una milestone slitta, la data va aggiornata
insieme al motivo: scope cresciuto, bug emersi, review piu' lunga del previsto,
dipendenza non pronta, scelta architetturale rimandata.

Le milestone storiche possono avere date ricostruite dai commit, dalle issue o
dalle PR. Le milestone nuove invece devono partire sempre con una data di fine
orientativa esplicita.

## Regole di manutenzione

Quando si apre una milestone:

1. creare o aggiornare la GitHub Milestone;
2. assegnare una data di fine orientativa su GitHub;
3. creare una issue madre con piano, checklist e link alla roadmap primaria;
4. aggiungere una riga in questo registro;
5. indicare dipendenze, non-obiettivi e cosa la milestone deve sbloccare.

Se per un limite operativo la data non viene inserita subito nella GitHub
Milestone, deve comparire comunque nella issue madre e in questo registro.

Durante la milestone:

1. creare issue figlie per micro-step non banali;
2. collegare PR e issue figlie alla issue madre;
3. aggiornare la issue madre con gli update di review e i commit rilevanti;
4. aggiornare questo documento solo quando cambia stato, perimetro, data
   orientativa o dipendenza importante.

Quando la milestone viene chiusa:

1. compilare durata reale e data di chiusura;
2. linkare PR principali e issue chiuse;
3. scrivere una sintesi dell'esito;
4. indicare cosa e' stato rimandato;
5. chiudere o aggiornare la milestone GitHub.

## Registro cronologico

| Ordine | Milestone | Stato | Finestra prevista | Durata stimata | Durata reale | Perche' ora | Dipendenze | Sblocca | GitHub | Documenti |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | [Inotify reference backend](31-milestone-inotify-reference-backend.md) | done | Storica, prima del tracking GitHub formale -> chiusa per MVP 2026-07-16 UTC | Non stimata | Storica; chiusa per MVP dopo audit #176 | Serve un backend reale per scoprire problemi di eventi, watch, move, resync, record e test. La riga chiude come backend di riferimento MVP, non come runtime multi-backend completo. | Nessuna milestone Alfred precedente. | Event Model v0, Backend API v0, Writer Runtime, JSONL, test golden; ora sblocca future milestone fanotify/audit/eBPF o Agent Guard con debiti espliciti. | [Milestone #10](https://github.com/kinderp/alfred/milestone/10), [Issue #176](https://github.com/kinderp/alfred/issues/176), [Issue #185](https://github.com/kinderp/alfred/issues/185) | [05](05-modulo-inotify.md), [20](20-matrice-eventi-inotify.md), [26](26-stato-funzionalita.md), [47](47-inotify-reference-backend-mvp-closure-audit.md) |
| 1 | [Writer Runtime v0](33-writer-runtime-roadmap-v0.md) | done | 2026-06-26 -> 2026-06-29 | Circa 3 giorni | Circa 3 giorni | Il writer non deve restare nel percorso caldo; servivano coda bounded, dispatcher, sink, JSONL/counter runtime e test prima di procedere con altre API. | Event Model v0, Writer API v0, record ownership, JSONL v0. | Backend API v0 piu' pulita, output futuri, benchmark e integrazione Fluent Bit/OpenTelemetry. | [Milestone #1](https://github.com/kinderp/alfred/milestone/1), [Issue #32](https://github.com/kinderp/alfred/issues/32), [PR #38](https://github.com/kinderp/alfred/pull/38), [Issue #40](https://github.com/kinderp/alfred/issues/40), [PR #41](https://github.com/kinderp/alfred/pull/41), [Issue #42](https://github.com/kinderp/alfred/issues/42), [PR #43](https://github.com/kinderp/alfred/pull/43) | [32](32-writer-api-v0.md), [34](34-report-benchmark-prestazioni.md), [22](22-contratto-log.md) |
| 2 | [Backend API v0 implementation](30-backend-api-v0.md) | done | 2026-06-29 -> 2026-07-02 | Circa 1 settimana | Circa 3 giorni | Serve rendere reale il confine backend-agnostic: lifecycle, target, capabilities, error model e adapter senza far dipendere il core da inotify. La milestone chiude come staged subset: il main loop non migra ancora a `backend_ops->poll()` perche' il core consuma `alfred_raw_event_t`, mentre la poll staged emette `alfred_record_t`. | Inotify reference backend, Event Model v0, Writer Runtime v0. | Backend fanotify/audit/eBPF futuri, capabilities runtime, policy che degrada in base al backend, futura decisione sul core input model. | [Milestone #2](https://github.com/kinderp/alfred/milestone/2), [Issue #45](https://github.com/kinderp/alfred/issues/45), [Issue #67](https://github.com/kinderp/alfred/issues/67), [Issue #69](https://github.com/kinderp/alfred/issues/69), [Discussion #44](https://github.com/kinderp/alfred/discussions/44) | [23](23-roadmap-plugin-backend.md), [25](25-roadmap-unificata-dossier.md), [39](39-principi-architetturali-futuri.md) |
| 3 | [Inotify backend conforms to Backend API v0](31-milestone-inotify-reference-backend.md) | done | 2026-07-02 -> 2026-07-03 | Circa 1 settimana | Circa 1 giorno | Dopo aver definito l'API comune, inotify doveva diventare il primo backend conforme senza cambiare il comportamento utente. La milestone ha chiuso il subset staged: ops, capabilities, lifecycle, target management, poll staged, diagnostica e focused test sono documentati e tracciati. | Backend API v0 implementation. | Primo backend di riferimento per future implementazioni; base piu' chiara per registry backend, capabilities reali, fanotify/audit/eBPF futuri e decisione separata sul core input model. | [Milestone #3](https://github.com/kinderp/alfred/milestone/3), [Issue #71](https://github.com/kinderp/alfred/issues/71), [Issue #72](https://github.com/kinderp/alfred/issues/72), [Issue #76](https://github.com/kinderp/alfred/issues/76), [Issue #78](https://github.com/kinderp/alfred/issues/78), [Issue #80](https://github.com/kinderp/alfred/issues/80), [Issue #82](https://github.com/kinderp/alfred/issues/82), [Issue #84](https://github.com/kinderp/alfred/issues/84), [Issue #86](https://github.com/kinderp/alfred/issues/86), [Issue #88](https://github.com/kinderp/alfred/issues/88), [Issue #90](https://github.com/kinderp/alfred/issues/90) | [05](05-modulo-inotify.md), [30](30-backend-api-v0.md), [31](31-milestone-inotify-reference-backend.md), [40](40-audit-inotify-backend-api-v0.md) |
| 4 | [Tracepoint and Lab MVP](41-tracepoint-lab-roadmap-mvp.md) | done | 2026-07-03 -> 2026-07-13 UTC | Circa 1 settimana | Circa 10 giorni | Alfred doveva diventare spiegabile: test, studenti e demo avevano bisogno di tracepoint stabili, scenari leggibili e mappe fra funzioni, record e test. La milestone chiude come MVP documentale `stable-doc`: niente runtime trace pubblico, parser o UI. | Event Model v0, Writer Runtime v0, JSONL stabile, Backend API v0 staged, inotify backend reference staged. | Alfred Lab MVP, demo pubbliche, documentazione animabile, debugging piu' chiaro e primo formato per scenari didattici. Rimandati: `trace.jsonl`, parser, UI, output pipeline Lab, overflow/recursive mkdir e policy. | [Milestone #4](https://github.com/kinderp/alfred/milestone/4), [Issue #92](https://github.com/kinderp/alfred/issues/92), [Issue #93](https://github.com/kinderp/alfred/issues/93), [Issue #95](https://github.com/kinderp/alfred/issues/95), [Issue #97](https://github.com/kinderp/alfred/issues/97), [Issue #99](https://github.com/kinderp/alfred/issues/99), [Issue #101](https://github.com/kinderp/alfred/issues/101), [Issue #103](https://github.com/kinderp/alfred/issues/103), [Issue #105](https://github.com/kinderp/alfred/issues/105), [Issue #107](https://github.com/kinderp/alfred/issues/107), [Issue #109](https://github.com/kinderp/alfred/issues/109), [Issue #111](https://github.com/kinderp/alfred/issues/111), [Issue #113](https://github.com/kinderp/alfred/issues/113) | [41](41-tracepoint-lab-roadmap-mvp.md), [42](42-tracepoint-model-v0.md), [25](25-roadmap-unificata-dossier.md), [17](17-roadmap-documentazione-avanzata.md), [16](16-mappa-codice-e-strutture.md), [Lab](lab/README.md) |
| 5 | [Performance suite v0](34-report-benchmark-prestazioni.md) | done | 2026-07-13 -> 2026-07-13 UTC | Circa 1 settimana | Meno di 1 giorno | Le scelte di queue, writer, JSONL e futuri backend devono essere guidate da numeri, non da sensazioni. Dopo Writer Runtime v0 e Lab MVP serviva stabilizzare cosa misuriamo prima di riaprire thread, code per sink o formati futuri. La milestone chiude come contratto documentale iniziale: tassonomia, campi CSV, refresh policy, debiti e gate test/script sono espliciti. | Writer Runtime v0, benchmark iniziali, output counter/JSONL, runtime output opt-in. | Decisioni piu' disciplinate su worker, per-sink queues, buffering, binary writer, hot path e futuri benchmark backend. Rimandati: gate CI, percentili, ripetizioni statistiche, I/O reale, socket, thread, allocazioni, code per sink e backend non-inotify. | [Milestone #5](https://github.com/kinderp/alfred/milestone/5), [Issue #115](https://github.com/kinderp/alfred/issues/115), [Issue #116](https://github.com/kinderp/alfred/issues/116), [PR #117](https://github.com/kinderp/alfred/pull/117), [Issue #118](https://github.com/kinderp/alfred/issues/118), [PR #119](https://github.com/kinderp/alfred/pull/119), [Issue #120](https://github.com/kinderp/alfred/issues/120), [PR #121](https://github.com/kinderp/alfred/pull/121), [Issue #122](https://github.com/kinderp/alfred/issues/122), [PR #123](https://github.com/kinderp/alfred/pull/123), [Issue #124](https://github.com/kinderp/alfred/issues/124), [PR #125](https://github.com/kinderp/alfred/pull/125), [Issue #126](https://github.com/kinderp/alfred/issues/126), [Issue #127](https://github.com/kinderp/alfred/issues/127), [PR #128](https://github.com/kinderp/alfred/pull/128) | [33](33-writer-runtime-roadmap-v0.md), [34](34-report-benchmark-prestazioni.md), [35](35-qualita-prodotto-software.md) |
| 6 | [Agent workspace observe ledger](43-agent-workspace-observe-ledger-v0.md) | done | 2026-07-13 -> 2026-07-14 UTC | Circa 1 settimana | Circa 1 giorno | E' il primo ponte credibile verso Agent Guard: osservare effetti reali di una sessione agente senza promettere ancora enforcement completo. La milestone chiude come contratto documentale observe-mode: cosa Alfred puo' sapere oggi dagli effetti filesystem, quali claim sono vietati, quali campi restano futuri e quando test/benchmark diventano obbligatori. | Event Model v0, JSONL, Writer Runtime v0, Performance suite v0, process/session fields futuri, workspace boundary design. | Would-block mode, policy iniziali, Agent Action Ledger, integrazioni con agenti AI. Rimandati: schema runtime workspace/session, enforcement, fanotify/eBPF/audit, process tree, network, approval UI, policy engine completo e attribution agente/processo. | [Milestone #6](https://github.com/kinderp/alfred/milestone/6), [Issue #130](https://github.com/kinderp/alfred/issues/130), [Issue #131](https://github.com/kinderp/alfred/issues/131), [PR #132](https://github.com/kinderp/alfred/pull/132), [Issue #133](https://github.com/kinderp/alfred/issues/133), [PR #134](https://github.com/kinderp/alfred/pull/134), [76a53b8](https://github.com/kinderp/alfred/commit/76a53b8), [Issue #135](https://github.com/kinderp/alfred/issues/135), [PR #136](https://github.com/kinderp/alfred/pull/136), [c4051a7](https://github.com/kinderp/alfred/commit/c4051a7), [Issue #137](https://github.com/kinderp/alfred/issues/137), [PR #138](https://github.com/kinderp/alfred/pull/138), [c132457](https://github.com/kinderp/alfred/commit/c132457), [Issue #139](https://github.com/kinderp/alfred/issues/139), [PR #140](https://github.com/kinderp/alfred/pull/140), [2937d51](https://github.com/kinderp/alfred/commit/2937d51), [Issue #141](https://github.com/kinderp/alfred/issues/141), [PR #142](https://github.com/kinderp/alfred/pull/142), [6768209](https://github.com/kinderp/alfred/commit/6768209), [Issue #143](https://github.com/kinderp/alfred/issues/143) | [24](24-roadmap-ai-agent-guardrail.md), [36](36-use-cases-posizionamento-integrazioni.md), [29](29-event-model-v0.md), [33](33-writer-runtime-roadmap-v0.md), [34](34-report-benchmark-prestazioni.md), [43](43-agent-workspace-observe-ledger-v0.md) |
| 7 | [Workspace/session runtime schema v0](44-workspace-session-runtime-schema-v0.md) | done | 2026-07-14 -> 2026-07-14 UTC | Circa 1 settimana | Meno di 1 giorno | Dopo il ledger observe-mode serviva decidere i primi campi runtime di workspace/sessione senza saltare a policy, enforcement o attribuzione agente/processo non supportata. La milestone chiude come contratto documentale: decide significato, fonte di verita', runtime placement, ownership/lifetime, pubblicazione read-only futura, forma JSONL preferita, test/golden gate e benchmark gate senza implementare codice runtime. | Agent workspace observe ledger, Event Model v0, Writer Runtime v0, Performance suite v0. | Implementazione controllata futura di `workspace_root`, `workspace_id` e `ledger_session_id`, record metadata/sessione JSONL, golden dedicati e benchmark mirati. Rimandati: Agent Guard completo, policy, enforcement, agent attribution, process tree, network, file read affidabili, struct runtime, CLI/config e algoritmi id. | [Milestone #7](https://github.com/kinderp/alfred/milestone/7), [Issue #145](https://github.com/kinderp/alfred/issues/145), [Issue #146](https://github.com/kinderp/alfred/issues/146), [PR #147](https://github.com/kinderp/alfred/pull/147), [Issue #148](https://github.com/kinderp/alfred/issues/148), [PR #149](https://github.com/kinderp/alfred/pull/149), [Issue #150](https://github.com/kinderp/alfred/issues/150), [PR #151](https://github.com/kinderp/alfred/pull/151), [Issue #152](https://github.com/kinderp/alfred/issues/152), [PR #153](https://github.com/kinderp/alfred/pull/153), [Issue #154](https://github.com/kinderp/alfred/issues/154), [PR #155](https://github.com/kinderp/alfred/pull/155), [Issue #156](https://github.com/kinderp/alfred/issues/156), [PR #157](https://github.com/kinderp/alfred/pull/157), [79cafca](https://github.com/kinderp/alfred/commit/79cafca) | [44](44-workspace-session-runtime-schema-v0.md), [43](43-agent-workspace-observe-ledger-v0.md), [29](29-event-model-v0.md), [34](34-report-benchmark-prestazioni.md) |
| 8 | [Workspace/session runtime context v0](45-workspace-session-runtime-context-v0.md) | done | 2026-07-14 -> 2026-07-14 UTC | Circa 1 settimana | Meno di 1 giorno | Dopo lo schema documentale serviva il primo punto runtime reale per conservare contesto workspace/sessione senza toccare subito record, queue, JSONL o policy. La milestone chiude il sottoinsieme minimo: config opzionale, contesto app-owned immutabile, validazione valori vuoti/lunghi, test focused e doc/man page. | Workspace/session runtime schema v0, Agent workspace observe ledger, Event Model v0, Writer Runtime v0, Performance suite v0. | Futuro record metadata/sessione JSONL, CLI/config piu' esplicite, golden dedicati e successivo ragionamento su generazione id. Rimandati: enrichment per-record, policy, Agent Guard, process attribution, benchmark refresh. | [Milestone #8](https://github.com/kinderp/alfred/milestone/8), [Issue #158](https://github.com/kinderp/alfred/issues/158), [Issue #159](https://github.com/kinderp/alfred/issues/159), [Issue #161](https://github.com/kinderp/alfred/issues/161), [PR #160](https://github.com/kinderp/alfred/pull/160), [PR #162](https://github.com/kinderp/alfred/pull/162), [710bf15](https://github.com/kinderp/alfred/commit/710bf15) | [45](45-workspace-session-runtime-context-v0.md), [44](44-workspace-session-runtime-schema-v0.md), [43](43-agent-workspace-observe-ledger-v0.md), [34](34-report-benchmark-prestazioni.md) |
| 9 | [Metadata/session record JSONL v0](46-metadata-session-record-jsonl-v0.md) | done | 2026-07-14 -> 2026-07-15 UTC | Circa 1 settimana | Circa 1 giorno | Dopo il contesto runtime workspace/sessione serviva il primo output pubblico controllato. La milestone ha chiuso il record metadata/sessione separato `SESSION_CONTEXT`, evitando enrichment per-evento e ambiguita' di attribuzione. | Workspace/session runtime context v0, Workspace/session runtime schema v0, Event Model v0, Writer Runtime v0, Performance suite v0. | Golden JSONL per contesto sessione, ledger observe-mode piu' utile e futura integrazione Agent Action Ledger senza promettere policy o process attribution. Rimandati: agent session, process tree, policy, id generation e per-event enrichment. | [Milestone #9](https://github.com/kinderp/alfred/milestone/9), [Issue #163](https://github.com/kinderp/alfred/issues/163), [Issue #164](https://github.com/kinderp/alfred/issues/164), [Issue #170](https://github.com/kinderp/alfred/issues/170), [PR #171](https://github.com/kinderp/alfred/pull/171), [Issue #172](https://github.com/kinderp/alfred/issues/172), [PR #173](https://github.com/kinderp/alfred/pull/173), [Issue #174](https://github.com/kinderp/alfred/issues/174), [PR #175](https://github.com/kinderp/alfred/pull/175) | [46](46-metadata-session-record-jsonl-v0.md), [45](45-workspace-session-runtime-context-v0.md), [44](44-workspace-session-runtime-schema-v0.md), [29](29-event-model-v0.md), [34](34-report-benchmark-prestazioni.md) |
| 10 | [Inotify reference backend MVP closure audit](47-inotify-reference-backend-mvp-closure-audit.md) | done | 2026-07-15 -> 2026-07-16 UTC | Circa 1 settimana | Circa 1 giorno | La riga storica `Inotify reference backend` era rimasta aperta dopo milestone successive su writer, Backend API staged, JSONL, Lab, performance e contesto sessione. L'audit ha verificato documentazione, test, contratti e debiti prima della chiusura MVP. | Inotify reference backend, Backend API v0 staged, Event Model v0, Writer Runtime v0, JSONL/session metadata. | Chiusura storica del backend inotify di riferimento, debiti residui espliciti e base piu' pulita per future milestone fanotify/eBPF/audit o Agent Guard. Rimandati: main-loop migration, worker/backpressure, process/network/policy/enforcement e backend futuri. | [Milestone #10](https://github.com/kinderp/alfred/milestone/10), [Issue #176](https://github.com/kinderp/alfred/issues/176), [Issue #177](https://github.com/kinderp/alfred/issues/177), [PR #178](https://github.com/kinderp/alfred/pull/178), [Issue #179](https://github.com/kinderp/alfred/issues/179), [PR #180](https://github.com/kinderp/alfred/pull/180), [Issue #181](https://github.com/kinderp/alfred/issues/181), [PR #182](https://github.com/kinderp/alfred/pull/182), [Issue #183](https://github.com/kinderp/alfred/issues/183), [PR #184](https://github.com/kinderp/alfred/pull/184), [Issue #185](https://github.com/kinderp/alfred/issues/185) | [47](47-inotify-reference-backend-mvp-closure-audit.md), [31](31-milestone-inotify-reference-backend.md), [05](05-modulo-inotify.md), [20](20-matrice-eventi-inotify.md), [26](26-stato-funzionalita.md), [30](30-backend-api-v0.md) |
| 11 | [Core Input Model / Main Loop Migration v0](48-core-input-main-loop-migration-v0.md) | done | 2026-07-16 -> 2026-07-17 UTC | Circa 1 settimana | Circa 1 giorno | Dopo la chiusura MVP del backend inotify restava un debito centrale: il main loop usa ancora il ponte raw verso il core, mentre la Backend API v0 staged emette record normalizzati. La milestone ha deciso esplicitamente di non migrare ora: il runtime resta raw-first finche' bridge o record-first non soddisfano criteri di riapertura misurabili. | Inotify reference backend MVP closure audit, Backend API v0 staged, Event Model v0, Writer Runtime v0, Performance suite v0, JSONL/session metadata. | Decisione esplicita sul core input model, benchmark gate, test/contract gate e criteri di riapertura per bridge/record-first. Rimandati: migrazione runtime, bridge record->core, core record-first, nuovi backend, Agent Guard, policy/enforcement, registry multi-backend e worker/per-sink queues. | [Milestone #11](https://github.com/kinderp/alfred/milestone/11), [Issue #187](https://github.com/kinderp/alfred/issues/187), [Issue #188](https://github.com/kinderp/alfred/issues/188), [Issue #205](https://github.com/kinderp/alfred/issues/205), [PR #206](https://github.com/kinderp/alfred/pull/206), [Issue #207](https://github.com/kinderp/alfred/issues/207), [PR #208](https://github.com/kinderp/alfred/pull/208), [84ba7e9](https://github.com/kinderp/alfred/commit/84ba7e9) | [48](48-core-input-main-loop-migration-v0.md), [31](31-milestone-inotify-reference-backend.md), [30](30-backend-api-v0.md), [29](29-event-model-v0.md), [33](33-writer-runtime-roadmap-v0.md), [34](34-report-benchmark-prestazioni.md), [47](47-inotify-reference-backend-mvp-closure-audit.md) |
| 12 | [Universal Observation Runtime research](38-visione-observation-runtime.md) | future | Non pianificata | Non stimata | Non ancora nota | Serve evitare che il modello comune venga chiuso dentro filesystem/inotify se Alfred dovra' rappresentare osservazioni, inferenze, azioni e feedback. | Backend API v0 reale, almeno piu' domini/sensori, replay, projection layer minimo. | Possibile modello Observation, knowledge graph, world model digitale, agenti e LLM come adapter/interfaccia. | Da creare solo quando diventa lavoro operativo. | [39](39-principi-architetturali-futuri.md), [24](24-roadmap-ai-agent-guardrail.md) |

## Come leggere la durata

La durata stimata risponde alla domanda:

```text
Quanto pensiamo di metterci prima di iniziare?
```

La durata reale risponde alla domanda:

```text
Quanto ci abbiamo messo davvero, dopo review, CI, fix e documentazione?
```

La differenza tra le due non e' un errore da nascondere. E' informazione utile:
mostra quali parti del progetto sono mature e prevedibili e quali invece
richiedono ancora esplorazione.

## Dipendenze e priorita'

Una milestone non deve essere scelta solo perche' e' interessante. Deve
rispondere a una domanda di priorita':

- quale rischio riduce?
- quale contratto stabilizza?
- quale lavoro futuro sblocca?
- quale complessita' evita di anticipare?
- quali test o benchmark renderanno verificabile il risultato?

Esempio: `Writer Runtime v0` ha avuto precedenza su backend futuri perche'
prima di aggiungere nuove sorgenti bisogna evitare che output, serializzazione e
I/O rimangano nel percorso caldo.

## Relazione con le roadmap tecniche

Questo registro resta sintetico. I dettagli tecnici devono vivere nei documenti
specifici:

- [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)
- [Milestone inotify reference backend](31-milestone-inotify-reference-backend.md)
- [Writer Runtime Roadmap v0](33-writer-runtime-roadmap-v0.md)
- [Backend API v0](30-backend-api-v0.md)
- [Roadmap plugin backend](23-roadmap-plugin-backend.md)
- [Roadmap AI agent guardrail](24-roadmap-ai-agent-guardrail.md)
- [Visione Observation Runtime](38-visione-observation-runtime.md)
- [Principi architetturali futuri](39-principi-architetturali-futuri.md)

Quando una roadmap tecnica cambia l'ordine delle milestone, aggiornare anche
questo registro.
