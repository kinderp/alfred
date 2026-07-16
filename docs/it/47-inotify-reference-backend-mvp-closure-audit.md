# Inotify reference backend MVP closure audit

Questo documento definisce la milestone
`Inotify reference backend MVP closure audit`.

La milestone non aggiunge un nuovo backend e non promette Agent Guard. Serve a
chiudere in modo verificabile il lavoro storico sul backend Linux inotify come
primo backend di riferimento per l'MVP.

## Perche' ora

Il registro cronologico ha ancora la riga storica `Inotify reference backend`
in stato `in progress`. Nel frattempo Alfred ha chiuso milestone successive:

- Writer Runtime v0;
- Backend API v0 staged;
- inotify conforme al sottoinsieme staged di Backend API v0;
- Tracepoint and Lab MVP;
- Performance suite v0;
- Agent workspace observe ledger v0;
- workspace/session runtime schema e context;
- Metadata/session record JSONL v0.

Questo significa che il backend inotify non e' piu' solo un prototipo iniziale:
e' il riferimento concreto che ha validato record, adapter, writer, JSONL, test
e documentazione. Prima di aprire lavori nuovi conviene verificare se puo'
essere dichiarato chiuso per l'MVP e quali debiti restano consapevolmente fuori.

## Obiettivo

L'obiettivo e' produrre una risposta chiara a queste domande:

1. cosa supporta oggi il backend Linux inotify;
2. quali comportamenti sono coperti da test;
3. quali limiti sono accettabili per MVP;
4. quali documenti sono ancora stale o troppo ambigui;
5. quali debiti restano futuri e non devono bloccare la chiusura;
6. se la riga storica `Inotify reference backend` puo' passare da `in progress`
   a `done`.

## Non obiettivi

Questa milestone non implementa:

- fanotify;
- eBPF;
- audit;
- backend Windows o macOS;
- multi-backend runtime;
- policy engine;
- Agent Guard;
- process attribution;
- network telemetry;
- enforcement;
- nuova semantica filesystem non richiesta dall'audit.

Se l'audit scopre un bug piccolo e concreto, puo' aprire una issue figlia
dedicata. Se invece scopre un tema architetturale piu' grande, quel tema deve
restare come debito o roadmap futura.

## Documenti da verificare

| Documento | Perche' va letto |
| --- | --- |
| [31 - Milestone backend inotify di riferimento](31-milestone-inotify-reference-backend.md) | Definisce il confine storico e il significato di backend di riferimento |
| [05 - Modulo inotify](05-modulo-inotify.md) | Spiega backend, adapter, watch, raw event e diagnostica |
| [20 - Matrice eventi inotify](20-matrice-eventi-inotify.md) | Dice quali eventi inotify sono richiesti, osservati, convertiti o ignorati |
| [26 - Stato funzionalita' supportate](26-stato-funzionalita.md) | Riassume cosa Alfred supporta oggi e cosa resta fuori |
| [30 - Backend API v0](30-backend-api-v0.md) | Spiega il sottoinsieme staged e i limiti del main loop attuale |
| [29 - Event Model v0](29-event-model-v0.md) | Protegge il modello comune da dettagli troppo Linux/inotify-specifici |
| [46 - Metadata/session record JSONL v0](46-metadata-session-record-jsonl-v0.md) | Conferma lo stato dell'output strutturato usabile nel ledger observe-mode |

## Checklist operativa

| Step | Esito atteso |
| --- | --- |
| Setup milestone | GitHub Milestone, issue madre, Project e documento roadmap creati |
| Audit documentazione | Stale statement, promesse e ambiguita' corretti o registrati |
| Audit test | Comportamenti supportati collegati a test focused, JSONL golden o diagnostici |
| Audit contratti | Backend API v0, Event Model v0 e output pipeline coerenti con inotify |
| Debiti residui | Debiti futuri separati da bug bloccanti MVP |
| Closure | Registro milestone aggiornato solo dopo review e CI |

## Audit test e contratti

Chiudere inotify per MVP non significa solo vedere la CI verde. Significa sapere
quale suite protegge quale contratto e sapere anche cosa una suite non sta
provando.

| Suite o target | Contratto protetto | Cosa non prova |
| --- | --- | --- |
| `make test-core` | Core semantico filesystem: conversione da eventi raw Alfred a eventi applicativi come create, delete, modify, ready, rename, move, relocate e overflow. | Runtime inotify reale, watch table, diagnostica backend, output JSONL pubblico. |
| `make test-backend-diagnostics` | Runtime inotify, configurazione, watch diagnostics, self-events, audit/debug stream, lost-scope/recovery e confini operativi del backend. | Completezza del contratto JSONL e tutte le combinazioni semantiche del core. |
| `make test-jsonl` | Contratto pubblico strutturato JSONL per famiglie rappresentative di record raw, semantici, diagnostici, recovery, overflow, metadata e session/context. | Tutte le righe del log testuale legacy e ogni dettaglio interno del backend. |
| Test testuali e contratto log | Compatibilita' dei log `events.log`, `raw.log` ed `errors.log` dove restano parte del comportamento osservabile. | Evoluzione del record model interno e stabilita' del JSONL come formato strutturato. |
| Benchmark e test performance | Regressioni grossolane sul costo dei percorsi misurati e confronto tra pipeline, queue, dispatcher e writer. | Garanzia assoluta di performance in produzione, workload kernel reali, thread futuri o backpressure completa. |

La suite JSONL non deve duplicare ogni test core o backend. Deve coprire il
contratto esterno strutturato con casi rappresentativi e stabili. Se un test
core prova una regola interna molto specifica, non serve sempre un golden JSONL
equivalente. Serve invece un golden quando quella regola diventa parte del
contratto pubblico che un consumer esterno puo' leggere.

## Mappa di chiusura per famiglie

| Famiglia | Evidenza di chiusura MVP | Decisione |
| --- | --- | --- |
| Semantica filesystem core | `make test-core`, matrice in [26 - Stato funzionalita'](26-stato-funzionalita.md), golden JSONL rappresentativi in [22 - Contratto log](22-contratto-log.md). | Coperta per MVP. |
| Record raw normalizzati | Test backend, sink-capability table e JSONL raw rappresentativi. | Coperta per MVP. |
| Watch, stale state, resync e lost-scope | Test backend diagnostici, documentazione del modulo inotify e golden recovery JSONL. | Coperta per i percorsi principali. |
| Backend API v0 staged | [30 - Backend API v0](30-backend-api-v0.md), [31 - milestone inotify](31-milestone-inotify-reference-backend.md), test sugli helper e documentazione del sottoinsieme staged. | Coperta come staged subset; la migrazione completa del main loop resta futura. |
| Output text e JSONL | Contratto log, writer/sink docs, `make test-jsonl` e test testuali esistenti. | Coperta per MVP, con output testuale e JSONL in parallelo. |
| Audit/debug inotify opt-in | Test backend e documentazione; il flusso resta di supporto diagnostico. | Accettabile per MVP; non e' un contratto JSONL completo. |
| Eventi kernel osservati ma non promossi | Matrice eventi inotify e note di non-goal. | Accettabile se documentati come raw/debug o ignorati deliberatamente. |
| Performance | Report e benchmark manuali esistenti. | Non blocca la chiusura se non introduce regressioni note nel percorso caldo. |

## Debiti accettabili

Questi punti non bloccano la chiusura MVP se restano documentati e tracciati:

- un test dedicato per `MOVED_TO` senza `MOVED_FROM` puo' rafforzare la
  copertura, ma oggi il comportamento e' coperto indirettamente dai casi di
  move/relocate;
- un test specifico per audit directory con `IN_ISDIR` sarebbe utile, ma il
  flusso audit/debug non e' il contratto pubblico principale;
- scenari kernel reali difficili da rendere deterministici in CI, come overflow
  reale o unmount/race particolari, possono restare coperti da test sintetici,
  diagnostici e documentazione;
- worker thread, code per sink, backpressure completa e dispatcher asincrono
  sono parte del Writer Runtime futuro, non del backend inotify MVP;
- process attribution, rete, policy, agent guard e enforcement richiedono
  backend futuri e non devono rientrare nella chiusura inotify.

## Registro debiti residui

Questa tabella trasforma i debiti accettabili in un registro di chiusura. La
colonna `Perche' non blocca` e' importante: evita di confondere un limite noto
con un bug nascosto. La colonna `Quando riaprirlo` indica invece il momento in
cui quel debito deve tornare attivo.

| Debito | Stato corrente | Perche' non blocca l'MVP | Quando riaprirlo |
| --- | --- | --- | --- |
| Main loop ancora su raw bridge | `app_run()` usa ancora il ponte raw isolato verso il core semantico; Backend API v0 staged e' implementata e testata, ma non e' il loop end-to-end unico. | Il ponte e' esplicito in [30 - Backend API v0](30-backend-api-v0.md) e in [31 - Milestone backend inotify](31-milestone-inotify-reference-backend.md). Cambiarlo ora toccherebbe il percorso caldo senza benchmark comparativi. | Quando si decide il core input model definitivo: `alfred_raw_event_t`, `alfred_record_t` o ponte misurato record -> raw/core. |
| Benchmark del main-loop migration | I benchmark correnti coprono record, queue, dispatcher, output pipeline e runtime output, ma non confrontano ancora il raw bridge con un loop backend-agnostic completo. | La chiusura inotify MVP non cambia il percorso caldo. Non serve inventare un numero prima di modificare il loop. | Prima di qualunque PR che sposti il loop principale su `backend_ops->poll()` o introduca un ponte record/core nuovo. |
| Worker thread, per-sink queues e backpressure pubblica | Writer Runtime v0 ha coda bounded, drain sincrono, dispatcher e output opt-in; worker e code per sink restano futuri. | Il backend inotify puo' essere chiuso come reference backend anche se i writer restano single-threaded e opt-in. Il percorso caldo target e' documentato. | Quando una milestone Writer Runtime successiva introduce worker reali, code per sink, drop policy pubblica o backpressure osservabile. |
| Test dedicato `MOVED_TO` senza `MOVED_FROM` | Il fallback e' documentato come supportato e coperto indirettamente dagli scenari move/recursive. | Non cambia la semantica principale di rename/move/relocate e non e' un bug noto del runtime corrente. | Quando si rafforza la suite core con edge case di ingresso nell'albero osservato senza sorgente nota. |
| Test audit directory con `IN_ISDIR` | Il flusso audit/debug resta diagnostico e non entra nel contratto JSONL v0. | L'audit opt-in non e' il contratto pubblico principale dell'MVP; la semantica core directory e' coperta dai test create/delete/move directory. | Quando si promuove l'audit inotify opt-in a contratto strutturato o a famiglia JSONL dedicata. |
| Overflow e unmount reali deterministici in CI | Overflow e unmount reali dipendono da kernel, timing e ambiente. Esistono test sintetici/diagnostici e documentazione dei limiti. | Un test non deterministico peggiorerebbe affidabilita' della CI. Per MVP basta distinguere contratto supportato, simulazione e limiti operativi. | Quando si introduce un harness controllato per stress kernel, namespace/mount dedicati o test privilegiati opzionali. |
| Raw audit e raw self-event dedicati in JSONL | I raw filesystem principali e diagnostica watch/recovery sono runtime-routed; audit opt-in e self-event raw dedicati non sono ancora famiglie JSONL pubbliche. | JSONL v0 deve restare rappresentativo e stabile, non un dump di ogni fatto backend/debug. | Quando una famiglia audit/self-event diventa requisito di consumer esterni o di un ledger piu' ampio. |
| Process attribution, rete e policy | Inotify non fornisce processo affidabile, rete, permission events o enforcement. | Promettere questi comportamenti con solo inotify sarebbe falso. Servono backend futuri e un policy/risk model. | In milestone fanotify, audit/eBPF, process context, Agent Guard observe/enforce o policy engine. |
| Metadata semantics | `IN_ATTRIB` e `RAW_ATTRIB` sono disponibili come raw fact; `FILE_METADATA_CHANGED`/`DIR_METADATA_CHANGED` restano rimandati. | L'MVP ha gia' raw evidence e non deve inventare una semantica metadata incompleta. | Quando si progetta una semantica metadata comune, indipendente da inotify e compatibile con backend futuri. |
| Lifecycle/error JSONL generico | Startup, shutdown, config e molti errori applicativi restano log umani; JSONL v0 copre raw, semantica e diagnostica watch/recovery rappresentativa. | Il modello lifecycle/error pubblico richiede schema proprio e non deve essere aggiunto solo per chiudere inotify. | Quando lifecycle, config error o errori applicativi diventano record pubblici versionati. |

## Cosa sarebbe un blocker

Un debito diventa blocker della chiusura inotify MVP solo se una di queste
condizioni e' vera:

- la documentazione promette un comportamento che il codice non implementa;
- un comportamento dichiarato supportato non ha test focused, golden o
  diagnostico ragionevole;
- il comportamento corrente viola Backend API v0, Event Model v0 o il contratto
  log/JSONL gia' stabilito;
- un limite noto produce dati falsi invece di dati incompleti o diagnostici;
- un nuovo claim di sicurezza promette processo, rete, policy o enforcement
  senza backend e test adeguati.

Se nessuna di queste condizioni e' vera, il limite deve restare nel registro
debiti e non deve allargare la chiusura MVP.

## Regola di verifica finale

Prima di chiudere la milestone, una PR di closure dovrebbe almeno verificare:

- `git diff --check`;
- `make test-core`;
- `make test-backend-diagnostics`;
- `make test-jsonl`;
- CI GitHub `Build and test` verde.

Se una PR e' solo documentale, puo' validare localmente con `git diff --check`,
ma la chiusura della milestone deve comunque appoggiarsi a una CI completa
recente.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- la documentazione non presenta contraddizioni importanti sul backend inotify;
- la matrice eventi e lo stato funzionalita' sono coerenti con il comportamento
  implementato;
- i test rilevanti sono identificati e, se necessario, aggiornati;
- i limiti del sottoinsieme staged di Backend API v0 sono ancora espliciti;
- i debiti futuri sono dichiarati senza bloccare l'MVP;
- la riga storica `Inotify reference backend` nel registro cronologico puo'
  essere chiusa con una sintesi dell'esito.

## Regola guida

```text
Chiudere inotify per MVP non significa completare Alfred.
Significa avere un primo backend Linux documentato, testato e abbastanza stabile
da reggere i prossimi strati.
```
