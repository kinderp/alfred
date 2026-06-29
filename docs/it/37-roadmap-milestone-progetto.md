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
| 0 | Inotify reference backend | in progress | Storica, prima del tracking GitHub formale | Non stimata | In corso | Serve un backend reale per scoprire problemi di eventi, watch, move, resync, record e test. | Nessuna milestone Alfred precedente. | Event Model v0, Backend API v0, Writer Runtime, JSONL, test golden. | Non nata come milestone GitHub unica. | [31](31-milestone-inotify-reference-backend.md), [05](05-modulo-inotify.md), [20](20-matrice-eventi-inotify.md), [26](26-stato-funzionalita.md) |
| 1 | Writer Runtime v0 | done | 2026-06-26 -> 2026-06-29 | Circa 3 giorni | Circa 3 giorni | Il writer non deve restare nel percorso caldo; servivano coda bounded, dispatcher, sink, JSONL/counter runtime e test prima di procedere con altre API. | Event Model v0, Writer API v0, record ownership, JSONL v0. | Backend API v0 piu' pulita, output futuri, benchmark e integrazione Fluent Bit/OpenTelemetry. | [Milestone #1](https://github.com/kinderp/alfred/milestone/1), [Issue #32](https://github.com/kinderp/alfred/issues/32), [PR #38](https://github.com/kinderp/alfred/pull/38), [Issue #40](https://github.com/kinderp/alfred/issues/40), [PR #41](https://github.com/kinderp/alfred/pull/41), [Issue #42](https://github.com/kinderp/alfred/issues/42), [PR #43](https://github.com/kinderp/alfred/pull/43) | [32](32-writer-api-v0.md), [33](33-writer-runtime-roadmap-v0.md), [34](34-report-benchmark-prestazioni.md), [22](22-contratto-log.md) |
| 2 | Backend API v0 implementation | planned | Da definire alla creazione della milestone GitHub | Da stimare | Non ancora nota | Serve rendere reale il confine backend-agnostic: lifecycle, target, capabilities, error model e adapter senza far dipendere il core da inotify. | Inotify reference backend, Event Model v0, Writer Runtime v0. | Backend fanotify/audit/eBPF futuri, capabilities runtime, policy che degrada in base al backend. | Da creare. | [30](30-backend-api-v0.md), [23](23-roadmap-plugin-backend.md), [25](25-roadmap-unificata-dossier.md) |
| 3 | Inotify backend conforms to Backend API v0 | planned | Da definire dopo Backend API v0 | Da stimare | Non ancora nota | Dopo aver definito l'API comune, inotify deve diventare il primo backend conforme senza cambiare il comportamento utente. | Backend API v0 implementation. | Primo backend sostituibile, registry backend, capabilities reali, test di conformita'. | Da creare. | [30](30-backend-api-v0.md), [05](05-modulo-inotify.md), [31](31-milestone-inotify-reference-backend.md) |
| 4 | Tracepoint and Lab MVP | planned | Da definire | Da stimare | Non ancora nota | Alfred deve diventare spiegabile: test, studenti e demo hanno bisogno di tracepoint stabili e scenari leggibili. | Event Model v0, Writer Runtime v0, JSONL stabile, Backend API v0 almeno avviata. | Alfred Lab, demo pubbliche, documentazione animabile, debugging piu' chiaro. | Da creare. | [25](25-roadmap-unificata-dossier.md), [17](17-roadmap-documentazione-avanzata.md), [16](16-mappa-codice-e-strutture.md) |
| 5 | Performance suite v0 | planned | Da definire | Da stimare | Non ancora nota | Le scelte di queue, writer, JSONL e futuri backend devono essere guidate da numeri, non da sensazioni. | Writer Runtime v0, benchmark iniziali, output counter/JSONL. | Decisioni su worker, per-sink queues, buffering, binary writer e hot path. | Da creare. | [34](34-report-benchmark-prestazioni.md), [33](33-writer-runtime-roadmap-v0.md), [35](35-qualita-prodotto-software.md) |
| 6 | Agent workspace observe ledger | planned | Da definire | Da stimare | Non ancora nota | E' il primo ponte credibile verso Agent Guard: osservare effetti reali di una sessione agente senza promettere ancora enforcement completo. | Event Model v0, JSONL, process/session fields futuri, workspace boundary design. | Would-block mode, policy iniziali, Agent Action Ledger, integrazioni con agenti AI. | Da creare. | [24](24-roadmap-ai-agent-guardrail.md), [36](36-use-cases-posizionamento-integrazioni.md), [29](29-event-model-v0.md) |

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

Quando una roadmap tecnica cambia l'ordine delle milestone, aggiornare anche
questo registro.
