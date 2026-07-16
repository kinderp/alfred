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
