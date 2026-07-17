# MVP operational usability v0

Questa milestone serve a rendere l'MVP corrente di Alfred piu' semplice da
usare, verificare e spiegare prima di allargare il progetto con nuovi backend,
Agent Guard o visioni piu' lunghe come Observation Runtime.

Il punto di partenza e' importante: Alfred ha gia' un backend inotify di
riferimento, Backend API v0 staged, Event Model v0, record strutturati, JSONL
opt-in, benchmark iniziali, Lab documentale e una decisione esplicita sul core
input model. Pero' un utente o contributore nuovo deve ancora leggere molti
documenti per capire come avviare Alfred, quali opzioni sono reali, quali log
guardare e quali comandi dimostrano che l'MVP funziona.

Questa milestone chiude quel gap operativo.

## Obiettivo

L'obiettivo e':

```text
rendere l'MVP attuale facile da lanciare, controllare e spiegare
senza cambiare il percorso caldo e senza anticipare backend futuri.
```

In pratica significa:

- chiarire la CLI corrente e decidere quali opzioni minime implementare ora;
- distinguere bene opzioni implementate da opzioni roadmap;
- allineare README, documentazione italiana e pagine man;
- definire un percorso di smoke test riproducibile per utenti e contributori;
- mantenere chiaro cosa Alfred supporta oggi e cosa resta rimandato;
- non riaprire decisioni appena chiuse sul core input model.

## Perche' ora

Dopo `Core Input Model / Main Loop Migration v0`, la scelta architetturale e'
deliberata:

```text
runtime raw-first per v0
nessun bridge record->core
nessun core record-first
```

Quindi non serve forzare una migrazione del percorso caldo. Il lavoro piu'
utile e' tornare all'MVP osservabile: rendere cio' che esiste davvero piu'
accessibile e verificabile.

Questa milestone prepara anche il terreno per futuri utenti reali. Prima di
aggiungere fanotify, eBPF, audit o policy, Alfred deve saper rispondere bene a
domande semplici:

- come si lancia?
- come si configura?
- come si controlla che la configurazione sia valida?
- quali file produce?
- come capisco se JSONL e' abilitato?
- quali eventi sono supportati?
- quale comando di test dimostra che l'MVP non e' rotto?

## Non obiettivi

Questa milestone non implementa:

- fanotify;
- eBPF;
- audit;
- backend Windows o macOS;
- Agent Guard;
- policy engine;
- enforcement o approval UI;
- process attribution;
- network events;
- migrazione del main loop a `backend_ops->poll()`;
- core record-first;
- worker thread;
- code per sink;
- nuovo formato binario;
- Universal Observation Runtime.

Questi temi possono influenzare la chiarezza dei contratti, ma non devono
allargare il lavoro corrente.

## Confini architetturali da preservare

La milestone deve rispettare i confini gia' decisi:

| Confine | Regola |
| --- | --- |
| Backend | Non scrive JSONL e non decide policy finali. |
| Core | Continua a consumare `alfred_raw_event_t` nel runtime v0. |
| Record | Resta il contratto strutturato per output, adapter e writer. |
| JSONL | Resta output opt-in, non Backend API. |
| Writer runtime | Resta single-threaded/opt-in; niente worker thread in questa milestone. |
| CLI | Deve orchestrare configurazione e uso, non conoscere semantica inotify interna. |

## Domande della milestone

Le domande da chiudere sono:

1. Qual e' la CLI minima utile per un MVP pubblico?
2. `--help` e `--version` vanno implementati subito?
3. `--check-config` e' abbastanza piccolo da introdurre ora o va rimandato?
4. La precedenza `ALFRED_CONFIG` vs futura `--config` va solo documentata o
   implementata?
5. Quale comando di smoke test deve poter eseguire un nuovo contributore?
6. README e man page descrivono il comportamento reale o contengono roadmap
   non chiaramente marcata?
7. I log `raw.log`, `events.log`, `errors.log` e `output.jsonl` sono spiegati
   abbastanza bene per un utente?

## Checklist proposta

| Item | Stato | Note |
| --- | --- | --- |
| Setup milestone, issue madre e roadmap | In progress | Questo documento e la issue madre #209 aprono la milestone. |
| Audit CLI/user workflow corrente | Todo | Confrontare `main.c`, `app`, README, man page e test. |
| Decidere CLI minima v0 | Todo | Candidati: `--help`, `--version`, forse `--check-config`. |
| Implementare comportamento selezionato | Todo | Solo dopo contratto e test focused. |
| Aggiungere test CLI/config | Todo | Exit status, stdout/stderr, nessun backend avviato per comandi informativi. |
| Allineare README e man page | Todo | Devono distinguere implementato vs roadmap. |
| Definire smoke test MVP | Todo | Un percorso breve: build, run su tmpdir, evento, log, JSONL opt-in. |
| Chiusura readiness | Todo | Sintesi di cosa e' affidabile, cosa resta rimandato e cosa si puo' aprire dopo. |

## Criteri di chiusura

La milestone puo' chiudersi quando:

- esiste una CLI minima documentata e coperta da test, oppure e' documentata
  esplicitamente la decisione di non ampliarla ancora;
- README e man page non contraddicono il comportamento reale;
- esiste un percorso di smoke test MVP riproducibile;
- la matrice funzionalita' resta coerente con il README pubblico;
- eventuali opzioni CLI nuove hanno exit status, stdout/stderr e failure mode
  espliciti;
- la documentazione spiega cosa controllare nei log compatibili e in
  `output.jsonl`;
- i non-goal restano rispettati.

## Collegamenti

- GitHub Milestone: [MVP operational usability v0](https://github.com/kinderp/alfred/milestone/12)
- Issue madre: [#209](https://github.com/kinderp/alfred/issues/209)
- Roadmap CLI e pagina man: [19-roadmap-cli-e-man-page.md](19-roadmap-cli-e-man-page.md)
- Stato funzionalita': [26-stato-funzionalita.md](26-stato-funzionalita.md)
- Registro milestone: [37-roadmap-milestone-progetto.md](37-roadmap-milestone-progetto.md)
- Core input decision: [48-core-input-main-loop-migration-v0.md](48-core-input-main-loop-migration-v0.md)
