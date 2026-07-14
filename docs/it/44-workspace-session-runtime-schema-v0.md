# Workspace/session runtime schema v0

Questo documento definisce la milestone `Workspace/session runtime schema v0`.

La milestone precedente,
[Agent workspace observe ledger v0](43-agent-workspace-observe-ledger-v0.md),
ha chiuso un contratto observe-mode: Alfred puo' leggere come ledger i record
strutturati gia' prodotti e runtime-routed, ma non ha ancora campi runtime per
descrivere workspace e sessione osservazionale.

Questa milestone prende solo quel debito minimo:

```text
workspace_root
workspace_id
ledger_session_id
```

Non implementa Agent Guard completo, policy, enforcement, process tree,
network, fanotify, eBPF, audit o attribuzione reale agente/processo.

## Obiettivo

L'obiettivo e' decidere come questi tre campi devono entrare nel contratto di
Alfred prima di scrivere codice runtime.

Le domande da chiudere sono:

1. qual e' il significato preciso di ogni campo?
2. qual e' la fonte di verita'?
3. dove vive il valore nel runtime?
4. chi possiede la memoria delle stringhe?
5. quando il valore viene copiato dentro un record?
6. come appare in JSONL?
7. quali test golden proteggono il contratto pubblico?
8. quale benchmark serve se aumentiamo dimensione record, copie stringa o
   volume JSONL?

La regola pratica e':

```text
prima source of truth, ownership, test e benchmark gate;
poi eventualmente schema C e JSONL.
```

## Non obiettivi

Questa milestone non introduce:

- `agent_session_id`;
- `agent_name`;
- `task_id`;
- `declared_intent`;
- process tree;
- rete;
- file read affidabili;
- classificazione `inside_workspace` / `outside_workspace`;
- decisioni `would_block`, `blocked`, `allowed` o `requires_approval`;
- policy engine;
- enforcement;
- integrazione LLM.

Questi campi e comportamenti restano futuri perche' richiedono una fonte di
verita' diversa dal solo inotify e, in molti casi, backend o motori non ancora
implementati.

## Campi candidati

| Campo | Significato | Fonte di verita' v0 | Stato milestone corrente |
| --- | --- | --- | --- |
| `workspace_root` | Root filesystem dichiarata come perimetro osservazionale | Valore esplicito da CLI/config/orchestratore fidato | Semantica, fonte, runtime placement e JSONL gate definiti; codice futuro |
| `workspace_id` | Identificatore opaco del workspace dichiarato o generato da Alfred | Valore esplicito o generazione Alfred documentata, mai inferenza silenziosa | Semantica, fonte, runtime placement e JSONL gate definiti; algoritmo futuro |
| `ledger_session_id` | Identificatore della run/sessione osservazionale Alfred | Runtime Alfred locale all'avvio della run | Semantica, fonte, runtime placement e JSONL gate definiti; algoritmo futuro |

## Regole generali di source of truth

Questi campi descrivono contesto operativo. Non sono fatti osservati dal
backend inotify e non sono prove di causalita' agente/processo.

La fonte di verita' v0 deve rispettare queste regole:

1. un valore deve essere esplicito oppure generato da Alfred con una regola
   documentata;
2. Alfred non deve dedurre automaticamente il workspace da path osservati, PID,
   timestamp, nome processo o contenuto dei file;
3. un campo assente significa "contesto non disponibile", non "fuori
   workspace" e non "policy violata";
4. un valore presente descrive il contesto dichiarato della run, non dimostra
   che un agente abbia causato un record;
5. ogni algoritmo futuro di generazione deve essere deterministicamente
   documentato o dichiarato esplicitamente come opaco/non stabile.

Questa distinzione e' importante per la sicurezza: confondere un contesto
dichiarato con una prova osservata trasformerebbe Alfred da ledger affidabile a
sistema che attribuisce responsabilita' senza evidenza.

## Fonti vietate in v0

Per v0 questi campi non possono essere derivati da:

| Fonte vietata | Perche' e' vietata |
| --- | --- |
| Primo path osservato | Il primo evento puo' essere fuori scope o arrivare dopo altri fatti persi |
| Prefisso comune dei path | I path osservati sono effetti, non dichiarazione di perimetro |
| Directory corrente di Alfred | Alfred puo' essere lanciato da una directory diversa dal workspace |
| PID, PPID o nome processo | Inotify non fornisce attribuzione processo affidabile nel modello corrente |
| Timestamp di evento filesystem | Identifica un fatto osservato, non una sessione o un workspace |
| Nome agente o prompt | Non esiste ancora adapter agente fidato in questa milestone |
| Contenuto dei file | Sarebbe invasivo, costoso e fuori dallo scope observe-ledger v0 |

Se in futuro una fonte vietata diventa supportabile, deve passare da una
milestone esplicita con backend, test, privacy review e benchmark adeguati.

## Relazione fra `workspace_root` e `workspace_id`

`workspace_root` e `workspace_id` sono collegati, ma non sono equivalenti.

Regole v0:

- `workspace_root` e' il solo campo che puo' descrivere un perimetro
  filesystem dichiarato;
- `workspace_id` e' solo una label opaca di correlazione e non prova che un
  path sia dentro o fuori dal workspace;
- `workspace_root` puo' essere presente senza `workspace_id`: in quel caso
  Alfred conosce il perimetro dichiarato ma non ha ancora una label opaca;
- `workspace_id` puo' essere presente senza `workspace_root`: in quel caso
  Alfred puo' raggruppare record per workspace dichiarato, ma non puo'
  interpretare quel valore come boundary filesystem;
- quando entrambi sono presenti, devono provenire dalla stessa configurazione o
  dallo stesso contesto runtime dichiarato;
- se una PR futura genera `workspace_id` da `workspace_root`, deve dichiarare
  algoritmo, stabilita', normalizzazione, rischi privacy e casi di collisione.

Queste regole evitano un errore importante: un consumatore JSONL non deve
trattare un record con solo `workspace_id` come prova che il path osservato sia
contenuto in un workspace specifico.

### `workspace_root`

`workspace_root` e' un path dichiarato. Non deve essere dedotto dal primo evento
osservato o dal prefisso comune dei path.

Rischio principale:

```text
se Alfred deduce il workspace dai path osservati, trasforma una supposizione in
contratto di sicurezza.
```

Per questo la fonte corretta deve essere esplicita: opzione CLI, file di config
o contesto runtime creato dall'utente o da un orchestratore fidato.

Contratto v0:

- rappresenta una root locale dichiarata per interpretare una run;
- non implica che Alfred possa bloccare accessi fuori root;
- non implica che tutti i record siano dentro quella root;
- non deve essere normalizzato usando eventi osservati;
- se configurato come path relativo, l'eventuale risoluzione deve essere
  definita in una PR di implementazione, non lasciata al writer;
- se assente, Alfred deve omettere il contesto invece di inventarlo.

Esempio corretto:

```text
utente/orchestratore avvia Alfred con workspace_root=/home/user/progetto
```

Esempio non corretto:

```text
Alfred osserva /home/user/progetto/src/main.c e decide che /home/user/progetto
e' il workspace.
```

### `workspace_id`

`workspace_id` serve a riferirsi al workspace senza esporre sempre il path
testuale. Deve essere stabile abbastanza da collegare record della stessa
sessione, ma non deve promettere identita' globale se non viene progettata.

Contratto v0:

- identifica il workspace dichiarato, non il filesystem object osservato;
- deve essere opaco per i consumatori: non devono parsarlo come path o hash
  semantico;
- puo' essere fornito esplicitamente oppure generato da Alfred;
- se generato da Alfred, la PR di implementazione deve dichiarare se e' stabile
  solo per la run, per quella macchina o per quel path normalizzato;
- non deve essere derivato silenziosamente da `workspace_root` senza dichiarare
  algoritmo, stabilita' e rischi privacy;
- se assente, i record restano validi ma non correlabili per workspace id.

Scelte ancora future:

- formato (`ws_...`, UUID, hash, contatore locale);
- stabilita' richiesta;
- algoritmo di generazione;
- eventuale configurazione utente;
- relazione con privacy e minimizzazione del path.

Esempio corretto:

```text
workspace_id=ws_7f3a... e' un identificatore opaco assegnato alla run o alla
configurazione.
```

Esempio non corretto:

```text
un consumatore assume che workspace_id sia sempre SHA-256(workspace_root).
```

### `ledger_session_id`

`ledger_session_id` identifica una sessione osservazionale Alfred. Non significa
ancora sessione agente.

Esempio corretto:

```text
questa run di Alfred ha prodotto questi record osservazionali.
```

Esempio non corretto:

```text
l'agente Codex ha causato questi record.
```

Il secondo richiede `agent_session_id`, process context e un adapter agente o
orchestratore. Non appartiene a questa milestone.

Contratto v0:

- identifica la run o finestra osservazionale di Alfred;
- e' prodotto dal runtime Alfred, non dal backend;
- non e' un `agent_session_id`;
- non attribuisce causalita' a processi o agenti;
- puo' essere usato per raggruppare record emessi dalla stessa run;
- deve essere opaco per i consumatori;
- se Alfred non lo emette ancora, il ledger observe-mode resta valido ma non ha
  identificatore di sessione.

Scelte ancora future:

- algoritmo di generazione;
- quando viene creato;
- se cambia dopo reload di configurazione;
- se viene scritto una volta come record sessione o ripetuto su ogni record;
- come viene preservato in replay.

## Posizionamento runtime v0

La direzione v0 e' conservativa:

```text
config/CLI/orchestratore
        |
        v
contesto runtime workspace/sessione immutabile
        |
        +--> futuro writer/session metadata
        +--> futuro JSONL enrichment misurato
```

I campi `workspace_root`, `workspace_id` e `ledger_session_id` non devono essere
aggiunti subito a ogni `alfred_record_t`.

Il motivo e' prestazionale e architetturale:

- il percorso caldo deve restare `backend -> record -> queue`;
- la queue oggi clona record owned al confine;
- aggiungere tre stringhe per record aumenterebbe dimensione, copie, cleanup e
  volume JSONL prima di avere test e benchmark dedicati;
- il contesto workspace/sessione e' contesto della run, non un fatto osservato
  diverso per ogni evento filesystem.

La decisione v0 e':

| Scelta | Decisione |
| --- | --- |
| Owner primario | Runtime/app di Alfred, dopo parsing configurazione e prima dell'osservazione |
| Mutabilita' | Immutabile per la durata della run osservazionale |
| Record v0 | Non aggiungere per ora campi stringa workspace/session a `alfred_record_t` |
| Queue v0 | Non aumentare il clone owned per record |
| Backend | Non vede e non possiede questi campi |
| Core filesystem | Non usa questi campi per semantica create/delete/rename/move |
| Writer futuri | Possono leggere un contesto stabile solo dopo una decisione JSONL/test/benchmark |
| Session metadata futuro | Preferibile per descrivere una run una volta, se basta ai consumatori |

Questa scelta non vieta di aggiungere campi ai record in futuro. Dice solo che
la prima implementazione non deve farlo senza una PR dedicata, golden JSONL e
benchmark refresh.

## Alternative considerate

| Opzione | Vantaggi | Rischi |
| --- | --- | --- |
| Campi dentro `alfred_record_t` | Ogni record e' autosufficiente, JSONL semplice | Piu' memoria, piu' copie stringa, piu' costo al confine queue |
| Contesto runtime separato | Evita copie per record, piu' leggero | Writer e sink devono accedere al contesto in modo chiaro |
| Enrichment nel writer JSONL | Non tocca hot path se il contesto e' stabile | JSONL contiene campi non presenti nel record interno |
| Record diagnostico/sessione separato | Pochi byte per record, sessione descritta una volta | Consumatori devono correlare record e sessione |

La scelta corrente privilegia `Contesto runtime separato`, con possibilita'
futura di `Record diagnostico/sessione separato`.

`Enrichment nel writer JSONL` resta futuro perche' trasformerebbe il JSONL in
un output piu' ricco del record interno. Puo' essere accettabile, ma solo dopo
aver deciso contratto pubblico, test golden e costo per-event.

`Campi dentro alfred_record_t` resta futuro perche' sarebbe la soluzione piu'
autosufficiente per replay e writer binari, ma e' anche quella che tocca di piu'
hot path, ownership e benchmark.

La scelta definitiva di una futura implementazione deve dipendere da:

- costo sul percorso caldo;
- costo di clone owned;
- leggibilita' del JSONL;
- facilita' dei golden test;
- compatibilita' con futuri writer binari;
- privacy e minimizzazione dei dati.

## Ownership e lifetime

Il contesto runtime workspace/sessione deve avere un solo owner chiaro:

```text
app/runtime Alfred
```

Regole v0:

- le stringhe del contesto sono owned dal runtime;
- il runtime crea o riceve i valori prima di iniziare a osservare eventi;
- il contesto deve essere completamente inizializzato prima di essere pubblicato
  a writer, sink, worker o altri reader runtime;
- dopo la pubblicazione il contesto e' read-only: nessun campo deve essere
  modificato in place;
- il runtime distrugge i valori durante shutdown, dopo writer/sink/pipeline che
  possono leggerli;
- i backend non conservano puntatori a questi valori;
- i record accodati non conservano puntatori borrowed a questi valori;
- nessun writer deve salvare un puntatore borrowed oltre la chiamata corrente;
- se un writer deve conservare un valore, deve copiarlo nella propria memoria
  owned con contratto esplicito.

Queste regole evitano un bug classico: mettere in coda un record che contiene
un puntatore borrowed a un contesto poi distrutto o ricaricato.

### Pubblicazione e reader asincroni futuri

Per v0 questa sezione e' una regola di progetto, non l'introduzione di un worker
thread. Serve a evitare che una futura implementazione asincrona renda insicuro
il contesto separato.

Se un futuro writer, sink o worker legge il contesto workspace/sessione:

- il puntatore o handle al contesto deve essere pubblicato solo dopo
  inizializzazione completa;
- il contesto pubblicato deve essere trattato come immutabile da tutti i thread;
- reload, shutdown o cambio sessione non devono mutare o liberare in place un
  contesto ancora leggibile da worker o sink;
- lo shutdown deve fermare, sincronizzare, joinare o drenare tutti i reader che
  possono osservare il contesto prima di distruggerlo;
- se un componente deve conservare un valore oltre la vita del contesto, deve
  farne una copia owned con ownership documentata.

Questa regola protegge un rischio diverso dalla queue: anche se i record
accodati non contengono puntatori borrowed, un writer asincrono potrebbe comunque
leggere un contesto laterale. Quel contesto deve quindi avere una pubblicazione
read-only e una distruzione sincronizzata.

## Reload e cambio configurazione

Per v0 il contesto workspace/sessione e' immutabile. Un eventuale reload di
configurazione non deve modificare in place stringhe lette da writer o pipeline.

Scelte ammesse per una futura implementazione:

1. non supportare reload per questi campi e richiedere una nuova run;
2. chiudere la sessione corrente e aprire una nuova sessione con nuovo
   `ledger_session_id`;
3. usare uno swap esplicito solo dopo drain/sincronizzazione documentata, con un
   nuovo contesto gia' inizializzato e senza mutare quello vecchio.

Scelta vietata:

```text
modificare o liberare in place le stringhe del contesto mentre record o writer
possono ancora osservarle.
```

## Impatto su record e queue

Il confine queue resta invariato in questa decisione documentale.

In particolare:

- `alfred_record_clone_owned()` non deve iniziare a copiare
  `workspace_root`, `workspace_id` o `ledger_session_id` in questo step;
- `alfred_record_queue_push()` non deve ricevere record con puntatori borrowed
  al contesto runtime workspace/sessione;
- `alfred_record_queue_pop()` continua a trasferire ownership dei soli campi
  owned gia' definiti dal record model corrente;
- ogni cambiamento futuro a queste funzioni richiede test focused e benchmark
  refresh.

Se in futuro decidiamo di mettere uno di questi campi dentro
`alfred_record_t`, la PR dovra' aggiornare header, clone/destroy/pop/push,
formatter, JSONL golden e benchmark.

Domande da chiudere prima del codice:

- nome e forma della futura struct di contesto;
- punto preciso di creazione nel lifecycle app/config;
- punto preciso di distruzione nello shutdown;
- API con cui writer o session metadata potranno leggere il contesto;
- comportamento se la configurazione non fornisce valori;
- algoritmo futuro per generare `workspace_id` e `ledger_session_id`.

Regola iniziale:

```text
nessun puntatore borrowed deve sopravvivere al contesto che lo possiede.
```

Per v0 la conseguenza pratica e':

```text
nessun record in queue deve dipendere dalla vita del contesto
workspace/sessione.
```

## JSONL e test

I campi diventano contratto pubblico solo quando entrano in JSONL.

Decisione v0:

```text
questo step non cambia lo schema JSONL corrente
```

I record JSONL correnti non devono iniziare a emettere `workspace_root`,
`workspace_id` o `ledger_session_id` senza una PR di implementazione dedicata.
Quella PR dovra' scegliere tipo di record, routing, golden, validazione dei
valori e benchmark. Questo evita di rendere pubblico un campo prima che siano
chiari significato, ordine nello stream, costo e comportamento dei consumer.

### Forma pubblica preferita

La prima forma pubblica preferita e' un record metadata/sessione separato,
emesso una volta per sessione osservazionale o quando una futura sessione cambia.

Il motivo e':

- `workspace_root`, `workspace_id` e `ledger_session_id` descrivono la run, non
  un fatto filesystem diverso per ogni record;
- ripeterli su ogni riga aumenta volume JSONL, escaping e parsing;
- una riga metadata rende esplicito che il contesto e' dichiarato, non osservato
  dal backend;
- i record evento restano leggeri e continuano a rappresentare il fatto
  osservato.

Esempio concettuale non ancora implementato:

```json
{
  "schema_version": 0,
  "layer": "diagnostic",
  "category": "backend",
  "type": "LEDGER_SESSION_STARTED",
  "ledger_session": {
    "ledger_session_id": "ls_01...",
    "workspace_id": "ws_01...",
    "workspace_root": "/home/user/project"
  }
}
```

Questo esempio non crea ancora un tipo C e non autorizza a usare esattamente
`diagnostic/backend/LEDGER_SESSION_STARTED`. La PR di implementazione dovra'
definire una coppia `layer/category/type` controllata nel modello eventi prima
di scrivere golden JSONL.

### Per-record enrichment

L'arricchimento per-record resta rimandato.

Puo' diventare corretto se un consumatore deve leggere ogni riga JSONL in modo
completamente indipendente, senza mantenere stato di sessione. In quel caso la
PR dovra' documentare perche' il vantaggio supera:

- maggiore volume per evento;
- costo di escaping e formattazione;
- possibile ripetizione di path sensibili;
- compatibilita' con writer binari e replay;
- rischio di confondere contesto dichiarato con evidenza osservata.

Finche' questa giustificazione non esiste, la scelta default resta:

```text
contesto sessione separato prima, enrichment per-record solo se misurato e
motivato
```

### Assente, vuoto e invalido

Il JSONL pubblico deve distinguere tre casi:

| Caso | Significato |
| --- | --- |
| Campo assente | Alfred non ha quel contesto oppure la feature non e' abilitata |
| Campo presente e non vuoto | Valore dichiarato o generato secondo il contratto documentato |
| Campo presente ma vuoto | Non valido nel contratto v0, salvo futura regola esplicita |

Per v0, `workspace_root`, `workspace_id` e `ledger_session_id` non devono essere
emessi come stringhe vuote in JSONL valido. Se una configurazione futura fornisce
un valore vuoto, l'implementazione deve scegliere esplicitamente fra:

1. rifiutare la configurazione prima di avviare la sessione;
2. trattare il valore come assente e, se utile, produrre una diagnostica
   strutturata;
3. accettare il vuoto solo con una motivazione documentata e golden dedicato.

La scelta 3 non e' il default perche' una stringa vuota rende ambigua la
differenza fra "non configurato", "configurato male" e "valore intenzionalmente
vuoto".

### Test richiesti prima dell'output pubblico

Quando questi campi diventano output JSONL pubblico, servono almeno:

- test formatter/sink per forma, escaping e omissione dei campi assenti;
- golden JSONL per almeno uno scenario filesystem reale;
- test per campo assente, per non fingere contesto non disponibile;
- test per rifiutare o gestire esplicitamente i valori vuoti;
- test che dimostrino che i record evento non ricevono per-record enrichment se
  la scelta resta session metadata separato;
- aggiornamento del contratto log e di questo documento;
- aggiornamento della pagina man interessata quando l'opzione/config diventa
  utente.

Se la prima implementazione usa un record metadata/sessione separato, i golden
devono coprire:

- emissione del metadata record quando i valori sono configurati/generati;
- assenza del metadata record quando il contesto non e' disponibile o la feature
  non e' abilitata;
- ordine del metadata record rispetto ai record osservativi;
- escaping di `workspace_root`;
- omissione dei campi assenti dentro l'oggetto sessione;
- nessuna attribuzione agente o policy implicita.

Se invece una PR futura sceglie per-record enrichment, i golden devono coprire
almeno:

- record semantico filesystem con contesto presente;
- record raw normalizzato con contesto presente;
- record diagnostico watch/recovery con contesto presente;
- record senza contesto, dove i campi vengono omessi;
- valori con caratteri che richiedono escaping;
- prova che il campo presente non significa `inside_workspace`,
  `allowed` o `agent-caused`.

Regola:

```text
campo assente e campo vuoto non devono significare automaticamente la stessa
cosa.
```

Un campo assente puo' voler dire "Alfred non ha quel contesto". Un campo vuoto
potrebbe essere configurazione invalida o valore esplicitamente vuoto. Prima di
implementare bisogna scegliere e documentare.

## Benchmark gate

La milestone eredita il gate definito in
[Report benchmark prestazioni](34-report-benchmark-prestazioni.md) e nel ledger
observe-mode.

Decisione v0:

```text
questo step non richiede benchmark refresh perche' non cambia runtime,
record, queue, writer, JSONL o workload.
```

Il benchmark diventa necessario solo quando una PR futura cambia il percorso
misurato o rende pubblici questi campi in output. La regola e' sempre la stessa:
non usare numeri vecchi per giustificare un percorso che non e' piu' quello
misurato.

### Casi che richiedono benchmark

Serve benchmark refresh o benchmark mirato se:

- i campi entrano in `alfred_record_t`;
- aumenta la dimensione del record;
- aumenta il numero di stringhe clonate per record;
- aumenta il volume JSONL per ogni evento;
- cambia queue, clone owned, dispatcher, sink o writer;
- il writer arricchisce ogni record con lavoro per-event.

Mappa pratica:

| Scelta futura | Benchmark richiesto | Perche' |
| --- | --- | --- |
| Aggiungere i campi a `alfred_record_t` | `queue-*`, `queue-dispatcher-*`, `output-pipeline-*`, runtime output se collegato | Cambia dimensione record, clone owned, destroy e memoria attraversata dalla queue |
| Clonare `workspace_root`, `workspace_id`, `ledger_session_id` per ogni record | benchmark queue/owned clone e runtime output | Aumenta allocazioni, copie stringa e cleanup per evento |
| Per-record JSONL enrichment | JSONL formatter/sink/writer e runtime output | Aumenta byte per record, escaping, buffering, I/O e parsing consumer |
| Metadata/session record separato una volta per run | smoke runtime output o benchmark mirato leggero solo se il record entra nel runtime reale | Costo per-run, non per-evento; serve verificare ordine e flush piu' che throughput |
| Nuova validazione CLI/config prima dell'avvio | di norma nessun benchmark, salvo parsing costoso o path normalization pesante | Non attraversa il percorso caldo degli eventi |
| Algoritmo costoso per generare `workspace_id` | benchmark mirato se eseguito per evento; non necessario se eseguito una volta per run | Il costo dipende dal punto di esecuzione |
| Nuovo writer/session metadata sink | benchmark writer/output se partecipa alla pipeline | Cambia fan-out, buffering o I/O a valle |

### Casi che non richiedono benchmark

Non serve benchmark refresh per:

- setup documentale;
- decisione di naming;
- issue/roadmap;
- test che assertano comportamento gia' esistente senza cambiare runtime.

Non serve benchmark refresh nemmeno per un golden JSONL che dimostra assenza dei
campi, se il codice runtime e il formatter non cambiano. In quel caso il test
protegge il contratto pubblico, ma non modifica il costo misurato.

### Benchmark minimi per una PR di implementazione

Una PR futura che implementa davvero questi campi deve dichiarare nella sua
descrizione quale riga della tabella sopra sta attraversando e quali benchmark
ha eseguito o escluso.

Regole minime:

- se cambia solo un record metadata/sessione emesso una volta, spiegare perche'
  il costo e' per-run e non per-evento;
- se cambia `alfred_record_t` o il clone owned, eseguire benchmark queue e
  output pipeline collegati;
- se cambia JSONL per ogni evento, eseguire benchmark formatter/sink/writer e
  almeno un runtime output;
- se cambia la pipeline, aggiornare anche i contatori runtime o spiegare perche'
  quelli esistenti restano sufficienti;
- se una differenza piccola viene usata per decidere architettura, ripetere i
  run o dichiarare che i numeri sono solo orientativi.

Claim vietati senza benchmark:

- "il contesto per-record e' gratis";
- "ripetere tre stringhe in JSONL non pesa";
- "il metadata record separato e' sempre migliore";
- "questa scelta non cambia backpressure";
- "la queue resta equivalente" se clone, dimensione record o drain sono cambiati.

## Endpoint della milestone

La milestone puo' chiudersi quando esiste una decisione documentata su:

- significato dei tre campi;
- fonte di verita';
- posizionamento runtime;
- ownership e lifetime;
- forma JSONL;
- test/golden necessari;
- benchmark richiesti o esplicitamente non richiesti;
- debiti rimandati.

Solo dopo questo endpoint ha senso aprire una milestone o PR di implementazione
C/JSONL.

## Stato di chiusura v0

Workspace/session runtime schema v0 e' chiusa come milestone documentale. Questo
non significa che Alfred emetta gia' `workspace_root`, `workspace_id` o
`ledger_session_id` in runtime o in JSONL. Significa che il contratto minimo per
implementarli in modo sicuro e' stato deciso.

Decisioni consolidate:

- `workspace_root` e' contesto filesystem dichiarato esplicitamente, non dedotto
  da eventi osservati;
- `workspace_id` e' un identificatore opaco di correlazione, non una prova di
  containment filesystem;
- `ledger_session_id` identifica una run osservazionale Alfred, non una sessione
  agente;
- i tre campi non devono essere derivati da path, PID, timestamp, nome agente,
  prompt o contenuto dei file;
- il posizionamento v0 preferito e' un contesto runtime immutabile owned da
  app/runtime;
- i campi non entrano per ora in ogni `alfred_record_t`;
- la queue non deve aumentare il clone owned per questi campi in questa
  milestone;
- i backend non vedono e non possiedono questi valori;
- un futuro writer/session metadata potra' leggere il contesto solo tramite API
  esplicita, con pubblicazione read-only e teardown sincronizzato;
- lo schema JSONL corrente non cambia;
- la forma pubblica futura preferita e' un record metadata/sessione separato,
  non per-record enrichment;
- campi presenti ma vuoti non sono JSONL valido v0 salvo futura regola
  esplicita;
- questa milestone documentale non richiede benchmark refresh;
- ogni implementazione futura deve dichiarare quale benchmark gate attraversa.

Debiti rimandati intenzionalmente:

- nome e forma della futura struct runtime;
- punto preciso di creazione nel lifecycle app/config;
- punto preciso di distruzione nello shutdown;
- API read-only per writer/session metadata;
- opzioni CLI/config e validazione valori vuoti;
- algoritmo e stabilita' di `workspace_id`;
- algoritmo e stabilita' di `ledger_session_id`;
- tipo controllato `layer/category/type` del futuro metadata/session record;
- golden JSONL per metadata record o per-record enrichment, secondo la scelta
  implementativa;
- benchmark mirati quando i campi entreranno in record, queue, writer, JSONL o
  pipeline;
- replay e preservazione della sessione;
- relazione futura con `agent_session_id`, process tree, policy e would-block.

Questi debiti non sono difetti nascosti della milestone. Sono il confine scelto
per non trasformare un contratto di workspace/sessione in Agent Guard completo o
in una modifica non misurata del percorso caldo.
