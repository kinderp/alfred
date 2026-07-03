# Milestone: backend inotify di riferimento

Questo documento definisce il perimetro corrente del lavoro su Alfred.

La visione lunga del progetto e' piu' ampia: Alfred vuole diventare un runtime
di controllo per agenti AI, capace di osservare azioni reali, collegarle a una
sessione agente e applicare policy. La milestone corrente pero' resta piu'
concreta e verificabile: chiudere il backend Linux inotify come primo backend
di riferimento.

## Obiettivo

L'obiettivo e' usare `inotify` per validare l'architettura comune di Alfred:

- Backend API v0;
- Event Model v0;
- record strutturati comuni;
- adapter da eventi backend/core a record;
- dispatcher o sink di output;
- Writer API v0 come confine per output e serializzazioni;
- Writer Runtime v0 come roadmap per code, dispatcher, sink e benchmark fuori
  dal percorso caldo;
- writer testuale compatibile con i log correnti;
- primo writer JSONL;
- test golden e test diagnostici;
- documentazione didattica e contrattuale;
- prime basi per benchmark e misurazioni di performance.

`inotify` e' quindi un banco di prova architetturale. Deve mostrare che Alfred
sa ricevere eventi da una sorgente concreta, normalizzarli, conservarne i
dettagli utili, produrre record comuni e serializzarli senza mescolare
responsabilita' tra backend, core, policy e output.

## Non obiettivi

Questa milestone non implementa:

- fanotify;
- eBPF;
- audit;
- backend Windows;
- backend macOS;
- dashboard;
- policy engine completo;
- blocco runtime completo;
- Agent Guard completo;
- analisi AI locale;
- correlazione completa tra prompt, processo, rete e filesystem.

Questi temi restano nella roadmap. Possono influenzare le scelte
architetturali, ma non devono allargare il passo corrente se non vengono
richiesti esplicitamente.

## Regola chiave

```text
inotify deve validare il contratto di Alfred,
non definire il confine finale del prodotto.
```

Questa regola evita due errori opposti:

- buttare via il lavoro su inotify solo perche' la visione futura e' piu'
  ampia;
- progettare il modello comune come se tutti i backend futuri avessero `wd`,
  `mask`, `cookie`, watch ricorsivi e self events.

I dettagli come `wd`, `mask`, `cookie`, `IN_MOVE_SELF`, `IN_DELETE_SELF` e
`WATCH_STALE` sono importanti, ma appartengono al backend Linux inotify o alla
diagnostica collegata. Il modello comune deve poter rappresentare anche eventi
di processo, rete, policy, agent session e altri sistemi operativi.

## Confine tra backend e modello comune

Il backend inotify deve:

- osservare eventi Linux reali;
- gestire watch, stale state, resync e lost-scope recovery;
- preservare dettagli sorgente quando servono per debug o diagnosi;
- produrre record Alfred comuni attraverso adapter e helper;
- documentare limiti, eventi non gestiti e scelte rimandate.

Il backend inotify non deve:

- decidere policy di sicurezza finali;
- generare direttamente JSONL come formato primario;
- conoscere dashboard, UI o orchestratori agente;
- introdurre nel record comune campi obbligatori validi solo per inotify;
- trasformarsi nel centro concettuale del prodotto.

## Linguaggio del record comune

Quando si aggiungono campi o record nuovi, bisogna chiedersi se il concetto e'
generale o specifico del backend.

Esempi di concetti generali:

| Concetto | Significato |
| --- | --- |
| `domain` | area osservata: filesystem, process, network, policy |
| `action` | azione normalizzata: create, write, delete, rename, exec |
| `subject` | processo, utente, agente o sessione che causa l'azione |
| `object` | risorsa osservata: path, directory, socket, secret |
| `source` | backend o collector che ha osservato il fatto |
| `decision` | allow, deny, would-block, approval, warning |
| `evidence` | eventi o dettagli che giustificano il record |

Esempi di concetti specifici di inotify:

| Concetto | Dove deve vivere |
| --- | --- |
| `wd` | payload sorgente o diagnostica backend |
| `mask` | payload sorgente o mappatura raw |
| `cookie` | payload sorgente per correlazione move |
| `watch_state` | diagnostica backend/watch manager |
| `IN_*` | matrice eventi inotify e dettagli sorgente |

La distinzione non significa perdere informazioni. Significa conservarle nel
livello corretto.

## Endpoint della milestone corrente

La milestone `Inotify backend conforms to Backend API v0` puo' chiudersi quando
`inotify` e' documentato e testato come backend di riferimento per il
sottoinsieme staged di Backend API v0.

Questo significa:

- `inotify_backend_ops()` espone una tabella statica `alfred_backend_ops_t`
  valida;
- `inotify_backend_capabilities()` dichiara solo capability realmente
  supportate dal backend inotify;
- `app.c` usa la tabella ops per init, target startup, start, stop e destroy;
- `add_target()` e `remove_target()` coprono il target filesystem-path v0;
- `backend_ops->poll(timeout_ms = 0)` esiste, delega al poll inotify esistente
  e converte `alfred_raw_event_t` in `alfred_record_t`;
- ownership di target path, emit callback, `userdata` e record borrowed e'
  documentata e coperta dai test focused;
- i test focused coprono descriptor, capabilities, lifecycle, target
  management e poll staged;
- i limiti e i ponti intenzionali sono documentati.

Non significa ancora:

- runtime Alfred backend-agnostic end-to-end;
- main loop migrato a `backend_ops->poll()`;
- core semantico capace di consumare direttamente `alfred_record_t`;
- supporto multi-backend;
- fanotify, audit, eBPF, Windows o macOS;
- enforcement/policy Agent Guard.

Il main loop raw bridge resta quindi un ponte intenzionale:

```text
app_run()
-> app_poll_legacy_raw_backend_once()
-> inotify_backend_poll()
-> handle_backend_event(alfred_raw_event_t)
-> alfred_process(core, raw)
```

Non e' una non-conformita' nascosta. E' il confine scelto finche' non viene
deciso e misurato se il core dovra' consumare record, oppure se servira' un
ponte misurato tra record e raw/core.

L'audit di riferimento e'
[Audit inotify vs Backend API v0](40-audit-inotify-backend-api-v0.md).

## Deliverable gia' consolidati

Questa milestone eredita lavoro gia' chiuso da milestone precedenti:

- Event Model v0 e record strutturati comuni;
- Writer API v0;
- Writer Runtime v0 con queue bounded, dispatcher e output opt-in;
- writer JSONL minimo e test collegati;
- log testuali compatibili prodotti attraverso record/sink per i percorsi gia'
  migrati;
- documentazione del percorso caldo e del debito benchmark.

Questi deliverable restano prerequisiti e contesto, ma non vanno confusi con
l'endpoint specifico della conformita' inotify Backend API v0.

## Documenti da leggere per questa milestone

Per lavorare su questa milestone leggere almeno:

1. [Regole operative](00-regole-operative.md)
2. [Guida alla lettura della documentazione](27-guida-lettura-documentazione.md)
3. [Event Model v0](29-event-model-v0.md)
4. [Backend API v0](30-backend-api-v0.md)
5. [Writer API v0](32-writer-api-v0.md)
6. [Roadmap Writer Runtime v0](33-writer-runtime-roadmap-v0.md)
7. [Modulo inotify](05-modulo-inotify.md)
8. [Flusso eventi](07-flusso-eventi.md)
9. [Matrice eventi inotify](20-matrice-eventi-inotify.md)
10. [Contratto dei log](22-contratto-log.md)
11. [Stato funzionalita' supportate](26-stato-funzionalita.md)
12. [Registro milestone del progetto](37-roadmap-milestone-progetto.md)
13. [Principi architetturali futuri](39-principi-architetturali-futuri.md)

La [Roadmap AI agent guardrail](24-roadmap-ai-agent-guardrail.md) va letta per
capire la direzione, ma non autorizza da sola a implementare policy, blocco o
backend futuri durante questa milestone.

La [Visione Observation Runtime](38-visione-observation-runtime.md) e i
[Principi architetturali futuri](39-principi-architetturali-futuri.md) valgono
allo stesso modo: orientano nomi, confini e responsabilita', ma non trasformano
la milestone corrente in un progetto di knowledge graph, world model o AI
generale.
