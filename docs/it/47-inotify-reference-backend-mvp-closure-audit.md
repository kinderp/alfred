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
