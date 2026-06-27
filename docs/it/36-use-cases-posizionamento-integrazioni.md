# Use case, posizionamento e integrazioni

Questo documento spiega come raccontare Alfred senza confondere lo stato
corrente con la visione futura. Serve a rispondere a tre domande:

- cosa e' Alfred oggi?
- quali problemi risolve in modo credibile?
- con quali strumenti dovrebbe integrarsi invece di provare a sostituirli?

La risposta breve e':

```text
Alfred oggi:
motore leggero Linux per eventi filesystem semantici

Alfred a breve:
ledger strutturato di azioni filesystem e diagnostica runtime

Alfred in futuro:
runtime guardrail per agenti AI, basato su eventi, sessioni e policy
```

Questa gerarchia e' importante. Se Alfred viene presentato subito come EDR,
SIEM o guardrail completo per agenti AI, sembra incompleto. Se invece viene
presentato come motore semantico filesystem con output strutturato, il valore
attuale e' gia' concreto e la roadmap futura diventa credibile.

## Cosa e' Alfred oggi

Alfred parte da Linux `inotify`, osserva eventi filesystem e li trasforma in
fatti piu' stabili:

```text
inotify backend
-> alfred_raw_event_t
-> core semantico
-> alfred_event_t
-> alfred_record_t
-> sink/output testuale o JSONL
```

Il valore non e' leggere direttamente `IN_CREATE`, `IN_DELETE`,
`IN_MOVED_FROM` o `IN_MOVED_TO`. Quello lo fanno gia' Linux e strumenti come
`inotifywait`. Il valore e' trasformare eventi bassi e rumorosi in eventi
semantici piu' chiari:

- `FILE_CREATED`
- `FILE_MODIFIED`
- `FILE_READY`
- `FILE_RENAMED`
- `FILE_MOVED`
- `FILE_RELOCATED`
- `DIR_CREATED`
- `DIR_DELETED`
- `DIR_RENAMED`
- `DIR_MOVED`
- `DIR_RELOCATED`
- `OVERFLOW`

Alfred produce anche diagnostica sullo stato del monitoraggio:

- `WATCH_ADDED`
- `WATCH_REMOVED`
- `WATCH_STALE`
- `WATCH_RESYNC_BEGIN`
- `WATCH_RESYNC_OK`
- `WATCH_RESYNC_FAILED`
- `WATCH_LOST_*`

Questa diagnostica non e' un evento utente del filesystem. Serve a capire se il
monitoraggio e' ancora affidabile.

## Cosa Alfred non e' ancora

Alfred non e' ancora:

- un EDR completo;
- un SIEM;
- un sostituto di Fluent Bit;
- un sostituto di Wazuh;
- un sostituto di Falco, Tracee o Tetragon;
- un backend eBPF;
- un sistema di blocco preventivo completo;
- un Agent Guard completo.

Queste cose possono entrare nella roadmap, ma non devono essere promesse come
funzionalita' correnti. Il backend inotify vede molte modifiche filesystem, ma
non vede in modo affidabile chi ha letto un file, quale processo ha causato
l'azione, quale rete e' stata usata o se una policy puo' bloccare prima
dell'effetto. Per questi casi serviranno backend futuri come fanotify, audit,
eBPF, procfs o meccanismi di sandboxing.

## Use case supportati oggi

Questa sezione va letta insieme a
[Stato funzionalita' supportate](26-stato-funzionalita.md). Il capitolo sulle
funzionalita' risponde alla domanda tecnica "cosa fa Alfred oggi?". Questo
capitolo risponde alla domanda prodotto "in quali situazioni quella
funzionalita' aiuta qualcuno?".

### Motore semantico filesystem Linux

Questo e' il primo use case reale.

Problema: `inotify` espone eventi molto vicini al kernel. Uno spostamento puo'
arrivare come coppia `IN_MOVED_FROM` e `IN_MOVED_TO`; l'applicazione deve
correlare cookie, path e tipo oggetto.

Alfred risolve questo problema producendo eventi semantici:

```text
IN_MOVED_FROM path=/tmp/a.txt cookie=42
IN_MOVED_TO   path=/tmp/b.txt cookie=42

-> FILE_RENAMED old_path=/tmp/a.txt new_path=/tmp/b.txt
```

Utenti possibili:

- sviluppatori Linux;
- studenti di sistemi operativi;
- strumenti di automazione locale;
- tool di backup o indicizzazione;
- strumenti di debug filesystem.

### Trigger per backup, indicizzatori e scanner

`FILE_READY` e' uno degli eventi piu' pratici. Indica che Alfred ha osservato
la chiusura di un file dopo scrittura.

Esempio:

```text
FILE_CREATED /workspace/report.txt
FILE_MODIFIED /workspace/report.txt
FILE_READY /workspace/report.txt
```

Un indicizzatore, uno scanner o un sistema di backup possono aspettare
`FILE_READY` invece di leggere il file mentre e' ancora in scrittura.

### Monitoraggio ricorsivo di alberi dinamici

Alfred gestisce watch ricorsivi e casi in cui directory annidate vengono create
molto velocemente, per esempio:

```bash
mkdir -p root/one/two/three
```

Il problema e' che alcune sottodirectory possono nascere prima che Alfred abbia
aggiunto un watch sul loro padre. Alfred mitiga questo caso con discovery e
scan mirati, producendo eventi sintetici quando necessario.

Questo use case e' utile per:

- workspace di sviluppo;
- build directory;
- cartelle temporanee;
- directory generate da tool automatici;
- repository manipolati da agenti AI.

### Ledger strutturato degli eventi

Con `alfred_record_t` e il writer JSONL, Alfred puo' diventare un ledger locale
di azioni filesystem e diagnostica.

Esempio concettuale:

```json
{"layer":"semantic","category":"filesystem","type":"FILE_READY","path":"/repo/main.c"}
{"layer":"diagnostic","category":"watch","type":"WATCH_STALE","path":"/repo/tmp"}
```

JSONL non e' il contratto interno primario. Il contratto interno e'
`alfred_record_t`; JSONL e' un formato di output utile per test golden,
debugging, audit, integrazioni e strumenti esterni.

### Debug del backend e affidabilita' dei watch

Molti strumenti dicono solo "ho visto un evento". Alfred prova anche a spiegare
se il monitoraggio e' affidabile:

```text
WATCH_STALE
WATCH_RESYNC_BEGIN
WATCH_RESYNC_FAILED
WATCH_LOST_QUEUED
WATCH_LOST_FOUND
```

Questo e' importante per amministratori, sviluppatori e futuri moduli di
sicurezza. Se una directory osservata viene spostata o rinominata, il backend
puo' continuare a ricevere eventi per lo stesso watch descriptor, ma il path
conosciuto puo' non essere piu' affidabile. Alfred deve evitare di inventare
eventi semantici falsi e deve registrare il problema in modo esplicito.

## Use case parziali o futuri vicini

### Scanner filesystem e snapshot iniziale

Lo scanner puo' servire in due contesti:

- resync/recovery quando lo stato dei watch non e' piu' affidabile;
- indicizzazione esplicita di un albero filesystem.

Possibile CLI futura:

```bash
alfred --scan /path
alfred --scan --dirs-only /path
alfred --scan --json /path
```

Questo non e' ancora il prodotto principale, ma e' coerente con il lavoro su
scanner, identity tracking e lost-scope recovery.

### Tamper detection osservazionale

Alfred puo' osservare modifiche a file sensibili:

```text
/etc/ssh/sshd_config
~/.ssh/authorized_keys
.env
.git/hooks/*
systemd unit
cron file
shell rc file
```

Con il solo inotify questo e' monitoraggio osservazionale: Alfred puo' vedere
molte modifiche, ma non sempre puo' sapere chi le ha causate o bloccarle prima
che avvengano. Per enforcement servono backend futuri o integrazioni di
sandboxing.

### Workspace boundary per agenti AI

Questo e' il primo guardrail concreto da progettare in futuro.

Scenario:

```text
Task dichiarato:
modifica README.md nel repository corrente

Azione osservata:
write ./README.md
-> coerente

Azione osservata:
delete ~/.ssh/config
-> fuori workspace, WOULD_BLOCK in modalita' simulata
```

La prima versione credibile non deve bloccare tutto subito. Deve produrre un
Agent Action Ledger in modalita':

- observe;
- warn;
- would-block.

Il blocco reale verra' solo quando Alfred avra' backend e meccanismi capaci di
farlo in modo affidabile.

### Pattern security

Alfred potra' riconoscere sequenze, non solo singoli eventi:

```text
FILE_CREATE_SCRIPT
-> CHMOD_EXEC
-> PROCESS_EXEC
= GENERATED_CODE_EXECUTION
```

Oppure:

```text
molti FILE_MODIFIED
-> molti FILE_RENAMED
-> molti FILE_READY
= comportamento ransomware-like
```

Questa e' roadmap. Non va presentata come detection completa oggi.

## Matrice funzionalita' -> use case

Questa matrice collega le famiglie di funzionalita' correnti o pianificate ai
casi d'uso. Serve a evitare due errori:

- documentare funzionalita' senza spiegare a chi servono;
- inventare use case senza collegarli a capacita' reali o pianificate.

La colonna "Stato" indica se il use case e' gia' credibile oggi, se e'
parziale o se richiede roadmap futura.

| Famiglia funzionale | Esempi di funzionalita' Alfred | Use case collegati | Stato |
| --- | --- | --- | --- |
| Eventi semantici filesystem | `FILE_CREATED`, `FILE_DELETED`, `DIR_CREATED`, `DIR_DELETED` | Automazione locale, trigger per tool, didattica su inotify, audit filesystem semplice | Supportato |
| Modifica e file ready | `FILE_MODIFIED`, `FILE_READY`, debounce modify, close-write | Backup incrementale, indicizzazione, ingestion documentale, scanner che devono aspettare file completi | Supportato |
| Rename, move e relocate | `FILE_RENAMED`, `FILE_MOVED`, `FILE_RELOCATED`, equivalenti directory | Tool che non vogliono gestire cookie inotify, sincronizzatori, backup, tracking spostamenti in workspace | Supportato |
| Watch ricorsivi | watch su directory figlie, `WATCH_ADDED`, `WATCH_REMOVED`, discovery directory | Monitoraggio di workspace dinamici, directory di build, upload folder, repository modificati da agenti | Supportato |
| Creazione annidata veloce | scan mirato dopo create directory, raw sintetici per directory scoperte | Affidabilita' su `mkdir -p`, generatori di codice, estrazione archivi, tool che creano alberi velocemente | Supportato |
| Diagnostica watch | `WATCH_STALE`, `WATCH_RESYNC_*`, `WATCH_LOST_*` | Debug affidabilita', observability backend, spiegazione di perdita scope, audit tecnico | Supportato/parziale |
| Lost-scope recovery | identity tracking, recovery queue, fallback scan root, reinstall watch | Recupero di directory rinominate/spostate, resilienza su workspace vivi, riduzione perdita osservabilita' | Parziale |
| Scanner filesystem | scanner/resync, snapshot, scan directory | Indicizzazione iniziale, snapshot prima del monitoraggio, verifica copertura watch, recovery dopo perdita stato | Parziale/futuro vicino |
| Configurazione inotify | `inotify_watch_mask`, audit opt-in, `ALFRED_CONFIG` | Adattare rumore/costo agli scenari, debug backend, esperimenti controllati | Supportato |
| Audit raw opt-in | `IN_OPEN`, `IN_ACCESS`, `IN_CLOSE_NOWRITE` nel raw log backend | Debug didattico inotify, studio del rumore eventi, prototipo per futuri file-read semantics | Opt-in diagnostico |
| Attributi/metadati | `ALFRED_RAW_ATTRIB`, `IN_ATTRIB`, configurazione `-IN_ATTRIB` | Tamper detection su permessi/ownership, futuri eventi metadata changed | Raw supportato, semantica rimandata |
| Overflow | `ALFRED_RAW_OVERFLOW`, `OVERFLOW` core | Capire quando il monitoraggio non e' piu' completo, attivare rescan o alert operativo | Supportato minimo |
| JSONL ledger | `alfred_record_t`, writer JSONL, golden JSONL | Audit locale, test golden pubblici, integrazione Fluent Bit/OpenTelemetry/SIEM, replay futuro | Supportato/parziale |
| Queue, dispatcher e sink | record queue, dispatcher, text sink, JSONL sink | Separare percorso caldo da writer lenti, plugin writer futuri, output multipli | Parziale |
| Benchmark output | `make perf-record-sinks`, report benchmark | Misurare overhead, scegliere architettura con numeri, evitare ottimizzazioni a sensazione | Parziale |
| Policy e workspace boundary | `decision`, `agent_session_id`, policy future | Agent Action Ledger, observe/would-block, sicurezza per agenti AI | Futuro |
| Process/network context | `pid`, `ppid`, `uid`, `cmdline`, rete futura | Correlare chi ha causato un file event, session engine, security runtime | Futuro |
| Deep Runtime Inspection | stack sample, register sample, `mprotect`, `mmap`, crash futuri | Investigazione mirata su processi sospetti, exploit timeline, runtime security avanzata | Futuro lungo |

### Esempi di lettura della matrice

Se un utente chiede "Alfred mi aiuta con un indicizzatore?", la risposta non
parte dal nome tecnico `IN_CLOSE_WRITE`. Parte dal use case:

```text
Indicizzatore
-> ha bisogno di sapere quando un file e' completo
-> funzionalita' utile: FILE_READY
-> stato: supportato
```

Se un utente chiede "Alfred mi aiuta a fare inventory iniziale di una
directory?", la risposta e':

```text
Inventory/snapshot iniziale
-> funzionalita' utile: scanner filesystem
-> stato: parziale/futuro vicino
-> oggi lo scanner esiste come base tecnica per resync, ma la CLI pubblica di
   indicizzazione e' ancora roadmap
```

Se un utente chiede "Alfred blocca un agente AI che legge segreti?", la risposta
deve essere onesta:

```text
Agent Guard / secret protection
-> funzionalita' necessarie: file-read visibility, process context, policy,
   enforcement
-> stato: futuro
-> con il solo inotify Alfred puo' osservare molte modifiche, ma non puo'
   promettere blocco preventivo completo
```

## Brainstorming periodico sui use case

I use case non devono essere scritti una volta sola e poi dimenticati. Ogni
nuova famiglia di funzionalita' dovrebbe essere collegata almeno a un use case,
oppure dichiarata come infrastruttura interna.

Quando emerge una nuova idea, usare questa scheda:

```text
Nome use case:
Utente/persona:
Problema reale:
Funzionalita' Alfred coinvolte:
Stato: supportato / parziale / futuro
Output utile: text log / JSONL / SIEM / Lab / report / policy
Limiti attuali:
Test o demo possibile:
Documenti collegati:
```

Esempio:

```text
Nome use case:
Indicizzazione iniziale di un albero filesystem

Utente/persona:
sviluppatore di tool, backup/indexer, studente

Problema reale:
prima di osservare eventi live, il tool deve sapere cosa esiste gia'

Funzionalita' Alfred coinvolte:
scanner filesystem, identity tracking, JSONL futuro, CLI futura --scan

Stato:
parziale/futuro vicino

Output utile:
JSONL o report testuale

Limiti attuali:
lo scanner e' usato come base tecnica per resync; la CLI pubblica di scan non e'
ancora il prodotto principale

Test o demo possibile:
creare un albero con file e directory, lanciare scan, confrontare output atteso

Documenti collegati:
21-roadmap-scanner-resync.md, 26-stato-funzionalita.md, 36-use-cases-...
```

Regola pratica:

```text
Ogni volta che `26-stato-funzionalita.md` aggiunge una funzionalita' importante,
controllare se `36-use-cases-posizionamento-integrazioni.md` deve aggiungere o
aggiornare un use case collegato.
```

## Use case da non promettere ora

Per mantenere credibilita', Alfred non deve promettere nel breve periodo:

- detection completa di ransomware;
- blocco preventivo universale;
- protezione completa dei segreti;
- visibilita' affidabile su processo, rete e file read con solo inotify;
- equivalenza con Falco, Tracee, Tetragon o Wazuh;
- deep runtime inspection sempre attiva;
- enforcement Agent Guard completo.

La regola e':

```text
Promettere solo cio' che il backend corrente puo' osservare e il core puo'
spiegare.
```

## Competitor e riferimenti

Alfred non ha un solo competitor. Dipende dall'identita' scelta.

| Strumento | Vicinanza | Cosa fa meglio | Come Alfred si differenzia |
| --- | --- | --- | --- |
| `inotify-tools` / `inotifywait` | Alta tecnicamente | Semplice, stabile, gia' disponibile | Alfred aggiunge semantica, correlazione, diagnostica e record strutturati |
| Watchman | Molto alta oggi | File watching maturo, grandi codebase, trigger | Alfred punta su semantica esplicita, didattica, ledger e sicurezza futura |
| Fluent Bit | Alta architetturalmente | Pipeline input/filter/output, performance, integrazioni | Alfred produce eventi semantici; Fluent Bit li puo' trasportare |
| Vector / Filebeat | Media | Shipping e trasformazione log | Alfred deve esportare JSONL leggibile da questi tool, non sostituirli |
| OpenTelemetry Collector | Media-alta | Standard osservabilita' e vendor-neutral pipeline | Alfred puo' mappare i propri record verso OTel via Fluent Bit o exporter futuri |
| Wazuh / SIEM | Media come destinazione | Dashboard, regole, compliance, correlazione enterprise | Alfred puo' diventare sensore specializzato, non SIEM completo |
| Falco | Alta per security futura | Regole runtime, syscall, container/Kubernetes | Alfred puo' specializzarsi su filesystem semantics e agent action ledger |
| Tracee | Alta per security futura | eBPF runtime security e forensics | Alfred resta piu' piccolo, didattico e orientato a eventi/agent ledger |
| Tetragon | Molto alta per visione futura | eBPF, enforcement, processo/rete/Kubernetes | Alfred puo' differenziarsi su workspace, agent session e intent-to-action |
| Cilium | Bassa diretta | Networking e policy cloud-native eBPF | Rilevante come ecosistema, non come competitor diretto |
| osquery | Media filosoficamente | Query dello stato macchina via SQL | Alfred e' stream/event ledger; osquery puo' arricchire lo stato |

## Confronto con Fluent Bit

Fluent Bit e' il modello architetturale piu' importante da studiare, ma non e'
un concorrente funzionale diretto.

Fluent Bit:

```text
input
-> parser/filter
-> record
-> output
```

Alfred:

```text
backend
-> raw/semantic/diagnostic record
-> queue/dispatcher
-> sink/writer
```

La differenza e':

```text
Fluent Bit trasporta e trasforma telemetria.
Alfred interpreta eventi filesystem/OS e produce fatti semantici.
```

Quindi l'integrazione corretta e':

```text
Alfred -> JSONL -> Fluent Bit -> OpenTelemetry/Wazuh/Elastic/Splunk/Loki
```

Alfred non deve implementare output specifici per ogni vendor nel breve
periodo. Deve produrre un JSONL stabile, documentato e facile da spedire.

## Integrazioni consigliate

### Priorita' 1: JSONL stabile e Fluent Bit

Prima integrazione da rendere facile:

```text
Alfred JSONL
-> Fluent Bit tail input
-> destinazione esterna
```

Questo permette di inviare eventi Alfred verso molti sistemi senza aggiungere
complessita' nel core:

- stdout;
- file;
- HTTP endpoint;
- OpenTelemetry Collector;
- Elasticsearch/OpenSearch;
- Loki;
- Splunk;
- Kafka;
- Wazuh/SIEM.

### Priorita' 2: OpenTelemetry Collector

OpenTelemetry e' il linguaggio comune dell'osservabilita'. Per Alfred il primo
passo non deve essere un exporter OTLP nativo complesso, ma un mapping chiaro:

| Campo Alfred | Possibile mapping |
| --- | --- |
| `type` | `event.name` |
| `category` | `event.domain` |
| `layer` | `alfred.layer` |
| `path` | `file.path` |
| `old_path` | `file.old_path` |
| `new_path` | `file.new_path` |
| `backend` | `alfred.backend` |
| `decision` futuro | `alfred.policy.decision` |
| `agent_session_id` futuro | `alfred.agent.session_id` |

### Priorita' 3: syslog e SIEM

Syslog resta importante per integrazioni semplici:

```text
Alfred -> syslog/rsyslog -> Wazuh/SIEM
```

Questo non richiede che Alfred diventi una piattaforma enterprise. Alfred resta
una sorgente specializzata di eventi semantici.

### Priorita' 4: Vector, Filebeat e backend log

Vector e Filebeat sono utili per ambienti che non usano Fluent Bit.

Strategia:

```text
Alfred JSONL -> Vector/Filebeat -> backend log
```

Non conviene scrivere subito exporter nativi per Elastic, Splunk, Loki o
OpenSearch. JSONL stabile + shipper esterno e' piu' semplice e piu' robusto.

### Priorita' 5: osquery come enrichment futuro

osquery e' utile per interrogare lo stato della macchina.

Esempio:

```text
Alfred:
FILE_MODIFIED /etc/systemd/system/x.service

osquery:
quali servizi systemd esistono?
quale utente/processo e' attivo?
quali porte sono aperte?
```

Alfred racconta cosa e' successo. osquery puo' aiutare a capire lo stato
attuale dopo l'evento.

### Priorita' 6: Falco, Tracee e Tetragon

Questi strumenti sono riferimenti importanti per la security runtime.

Nel breve:

```text
Alfred -> Fluent Bit/SIEM
Falco/Tetragon/Tracee -> Fluent Bit/SIEM
```

Nel futuro:

```text
eventi Alfred filesystem
+ eventi Falco/Tetragon/eBPF processo/rete/syscall
+ sessione agente
= correlazione e policy
```

Per arrivarci Alfred deve prima stabilizzare:

- Event Model v0;
- Backend API v0;
- capabilities runtime;
- subject/process context;
- agent session;
- workspace boundary;
- policy decision record.

## Demo consigliate

### Demo 1: inotify e' raw, Alfred e' semantico

Comando:

```bash
mv a.txt b.txt
```

Vista kernel:

```text
IN_MOVED_FROM
IN_MOVED_TO
```

Vista Alfred:

```text
FILE_RENAMED old_path=a.txt new_path=b.txt
```

Messaggio: Alfred nasconde la complessita' del cookie move e produce un fatto
stabile.

### Demo 2: file ready

Comando:

```bash
printf 'hello\n' > report.txt
```

Vista Alfred:

```text
FILE_CREATED report.txt
FILE_MODIFIED report.txt
FILE_READY report.txt
```

Messaggio: un consumer puo' aspettare `FILE_READY` prima di leggere il file.

### Demo 3: workspace boundary per agenti AI

Scenario futuro o simulato:

```text
task: modifica README.md
azione: write ./README.md
decisione: ALLOW

azione: delete ~/.ssh/config
decisione: WOULD_BLOCK
```

Messaggio: la differenza futura di Alfred non e' solo vedere file events, ma
collegare intento, sessione agente ed effetto reale sul sistema.

## Persona target

| Persona | Cosa cerca | Messaggio per Alfred |
| --- | --- | --- |
| Studente | capire C, Linux, inotify, architettura | Alfred e' una codebase reale ma leggibile |
| Sviluppatore Linux | eventi filesystem gia' interpretati | meno logica custom sopra inotify |
| Tool builder | trigger affidabili | `FILE_READY`, rename/move/relocate e JSONL |
| Sysadmin | osservabilita' locale | diagnostica watch e ledger filesystem |
| Security engineer | base per detection | record strutturati e integrazione SIEM |
| AI tooling developer | audit agenti | futuro Agent Action Ledger e workspace boundary |

## Roadmap collegata ai use case

Ordine consigliato:

1. chiudere inotify come reference backend;
2. completare Event Model v0 e JSONL golden;
3. rendere facile l'integrazione JSONL -> Fluent Bit;
4. documentare mapping verso OpenTelemetry/SIEM;
5. stabilizzare Backend API v0 e capabilities;
6. aggiungere subject/process context quando ci sara' un backend adeguato;
7. introdurre Agent Action Ledger in observe/would-block mode;
8. progettare policy/enforcement solo dopo backend capaci di supportarlo.

La frase guida e':

```text
Prima Alfred deve produrre eventi migliori.
Poi puo' diventare un guardrail migliore.
```
