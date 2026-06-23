# Writer API v0

Questo documento definisce la proposta Writer API v0 di Alfred.

La decisione principale e':

```text
I writer sono consumer di alfred_record_t.
Non sono il modello dati primario.
Non devono vivere nel percorso caldo dell'evento.
```

Il documento e' una roadmap architetturale. Il codice corrente ha gia' un primo
confine `record -> sink -> text sink`, ma non ha ancora una vera Writer API con
code, worker, backpressure e plugin writer configurabili.

## Perche' serve

Alfred non deve essere solo un logger sincrono. Il suo obiettivo e' osservare
eventi di sistema con overhead basso e trasformarli in record utili per log,
audit, policy, Agent Guard, Lab e integrazioni esterne.

Se il backend o il core chiamano direttamente funzioni come:

```c
fprintf(...);
fflush(...);
jsonl_writer_write(...);
protobuf_encode(...);
socket_send(...);
```

nel percorso caldo dell'evento, Alfred rischia di bloccarsi sul componente piu'
lento: filesystem, socket, dashboard, encoder, plugin o consumer esterno.

Writer API v0 serve a evitare questo errore progettuale.

## Percorso caldo

Il percorso caldo di Alfred deve essere il piu' corto possibile:

```text
evento OS
-> collector/backend
-> normalizzazione minima
-> alfred_record_t
-> enqueue su coda/ring buffer
```

Nel percorso caldo non devono vivere:

- writer testuale;
- JSONL;
- protobuf;
- MessagePack;
- socket TCP;
- Unix socket;
- `fprintf()`;
- `fflush()`;
- encode binari costosi;
- dashboard;
- Alfred Lab;
- report;
- policy pesante;
- plugin lenti;
- allocazioni non necessarie;
- `malloc()`/`free()` per evento;
- `strdup()` per evento;
- `snprintf()` per generare output;
- escaping JSON;
- regex;
- lookup pesanti non bounded;
- lock pesanti o non bounded.

Questa regola e' piu' importante della scelta del formato. Un writer JSONL
ottimo, ma chiamato sincronicamente dal backend per ogni evento, sarebbe comunque
nel posto sbagliato.

Il costo di una chiamata indiretta tramite function pointer e' normalmente molto
piccolo rispetto al costo di un writer reale. Il problema non e':

```text
plugin = lento
```

Il problema e':

```text
plugin o writer lento chiamato sincronicamente nel percorso caldo = lento
```

Quindi una interfaccia plugin-like statica puo' essere accettabile anche in un
progetto orientato alle prestazioni, se il lavoro costoso resta fuori hot path.

## Percorso fuori hot path

Fuori dal percorso caldo il flusso target diventa:

```text
coda/ring buffer
-> dispatcher/worker
-> writer API
-> writer text/jsonl/protobuf/messagepack/socket/lab
```

I writer possono fare lavoro costoso solo perche' sono fuori dal percorso caldo.
Possono serializzare, raggruppare, fare flush, spedire su socket o aggiornare UI,
ma non devono bloccare il collector.

Il fan-out sincrono dal backend e' vietato. Questo schema e' sbagliato:

```c
void backend_emit(record)
{
    text_write(record);
    jsonl_write(record);
    protobuf_write(record);
    socket_send(record);
}
```

Ogni evento pagherebbe il costo di tutti i writer abilitati. Lo schema target e':

```c
void backend_emit(record)
{
    ring_buffer_push(record);
}
```

Il dispatcher lavora dopo:

```c
void dispatcher_loop(void)
{
    while (ring_buffer_pop(&record)) {
        for each enabled sink:
            sink_write(record);
    }
}
```

Il primo passo implementato verso questo schema e' `alfred_record_queue_t`.
Questa coda non e' ancora il dispatcher definitivo e non e' ancora collegata al
runtime. Serve a validare il confine minimo:

```text
record borrowed in ingresso
-> record owned dentro la coda
-> record owned consegnato al consumatore
```

La coda e' bounded e single-threaded nella versione v0. Questo e' un vincolo
voluto, non una mancanza casuale:

- bounded significa che l'overflow viene visto subito e non nascosto da crescita
  illimitata della memoria;
- single-threaded significa che possiamo testare prima ownership, FIFO,
  wraparound e cleanup senza confondere il contratto con mutex e scheduling;
- non collegata al runtime significa che il path caldo non cambia finche' non
  abbiamo deciso dispatcher, backpressure e benchmark.

La stessa idea vale per il primo dispatcher v0. Anche il dispatcher e'
bounded, ma il limite non e' il numero di record: e' il numero massimo di sink
registrabili. Il chiamante fornisce un array di sink e una capacita'. Se la
capacita' e' esaurita, l'aggiunta di un altro sink fallisce. Questo rende
esplicito il fan-out massimo:

```text
queue bounded      = massimo numero di record in attesa
dispatcher bounded = massimo numero di sink destinatari
```

In una fase successiva conviene valutare una coda per sink:

```text
record queue
-> dispatcher
-> text queue
-> jsonl queue
-> msgpack queue
-> lab queue
```

In questo modello un writer JSONL lento non rallenta il writer MessagePack, il
writer testuale o il backend. Ogni coda puo' avere una policy diversa:
lossless, best-effort, debug, drop controllato o disabilitazione del sink.

## Responsabilita'

| Modulo | Puo' fare | Non deve fare |
| --- | --- | --- |
| Backend/collector | osservare eventi OS, normalizzare il minimo, produrre record | aspettare writer, fare I/O di output, serializzare JSONL/protobuf |
| Core | correlare e produrre record semantici | scrivere su file/socket o dipendere dal formato di output |
| Dispatcher | leggere dalla coda, scegliere consumer, gestire policy di delivery | cambiare il significato del record |
| Writer | serializzare o trasportare record | applicare semantica core, leggere stato backend, bloccare il backend |
| Policy pesante | analizzare record e produrre decisioni | stare nel path sincrono del backend senza budget |
| Lab/dashboard | visualizzare record e pipeline | diventare dipendenza del runtime hot path |

## Writer previsti

Writer API v0 deve permettere almeno questi writer:

| Writer | Uso principale | Note |
| --- | --- | --- |
| `text` | compatibilita' log attuali e didattica | primo writer compatibile, leggibile dagli studenti |
| `jsonl` | test golden, integrazioni semplici, debugging strutturato | formato testuale strutturato, non API primaria |
| `protobuf` | integrazioni con schema forte | utile quando il record model e' piu' stabile |
| `messagepack` | binario compatto e flessibile | meno rigido di protobuf, utile per prototipi performanti |
| `unix_socket` | integrazione locale con daemon, Lab o agent runtime | ideale per processi locali e controllo permessi Unix |
| `tcp_socket` | integrazioni remote future | richiede modello sicurezza, autenticazione e cifratura |
| `binary_native` | prestazioni estreme | da rimandare finche' schema e benchmark non sono chiari |

Il writer testuale e' utile, ma puo' diventare il writer piu' pericoloso per le
prestazioni: usa formattazione, stringhe lunghe, newline, file I/O e spesso
flush frequenti. Per questo va distinto tra:

- `text debug`: leggibile, verboso, anche con flush frequente;
- `jsonl buffered`: output pubblico strutturato, ma con batching;
- `binary/messagepack/protobuf`: output futuri per performance e integrazioni;
- `noop/counter`: sink di benchmark per misurare il costo senza I/O.

In produzione il flush per evento deve essere evitato salvo configurazione
esplicita di debug o modalita' lossless molto costosa.

## Regola sul formato

JSONL, protobuf, MessagePack e formati binari sono serializzazioni di
`alfred_record_t`. Non devono diventare il contratto interno.

La direzione corretta e':

```text
alfred_record_t
-> writer text/jsonl/protobuf/messagepack/socket
```

La direzione sbagliata e':

```text
backend
-> stringa JSONL
-> parsing
-> core/policy/writer
```

Il testo puo' restare molto utile per debug e didattica, ma non deve essere
usato come sorgente di verita' strutturata.

## Backpressure

Se un writer e' lento, Alfred deve avere una policy esplicita. Non deve
bloccare indefinitamente il backend.

Le policy da progettare sono:

- `lossless`: il sistema accetta backpressure e puo' rallentare o fermarsi;
- `drop_diagnostics`: scarta diagnostica meno importante prima degli eventi
  principali;
- `drop_oldest`: mantiene record recenti e dichiara perdita;
- `drop_newest`: conserva backlog esistente e dichiara perdita;
- `overflow_record`: emette un record strutturato che segnala perdita o coda
  satura;
- `metrics`: espone contatori di record accodati, scritti, scartati e falliti.

La scelta non va nascosta dentro un writer. Deve essere configurabile o almeno
documentata come default.

Una classificazione pratica dei sink puo' essere:

```c
typedef enum {
    ALFRED_SINK_CRITICAL,
    ALFRED_SINK_BEST_EFFORT,
    ALFRED_SINK_DEBUG
} alfred_sink_class_t;
```

Esempi:

| Classe | Esempio | Policy possibile |
| --- | --- | --- |
| `critical` | ledger JSONL audit | errore serio o modalita' lossless controllata |
| `best_effort` | Lab, UI, sink diagnostici | drop controllato con metriche |
| `debug` | text writer verboso | disabilitabile o flush configurabile |

Questa distinzione evita che un sink di debug lento rallenti Alfred in
produzione.

## Fase corrente

Il codice corrente usa ancora bridge sincroni:

```text
record
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> logger_raw/logger_event/logger_error
```

Questi bridge sono accettabili come migrazione incrementale per:

- togliere stringhe sparse dal codice;
- far passare i log compatibili da `alfred_record_t`;
- mantenere test esistenti e payload testuali;
- preparare JSONL e writer futuri.

Non sono pero' il modello finale ad alte prestazioni. Il modello finale deve
spostare serializzazione e I/O fuori dal percorso caldo con code e worker.

## Roadmap implementativa

1. Continuare a migrare i record correnti verso `alfred_record_t`.
2. Introdurre un dispatcher applicativo che riceve record.
3. Definire una coda/ring buffer tra produttori e writer.
4. Definire classi sink: critical, best-effort, debug.
5. Implementare un writer no-op/counter per benchmark senza I/O.
6. Implementare un writer text compatibile sopra Writer API v0.
7. Implementare un writer JSONL minimo buffered con test golden.
8. Aggiungere metriche di coda, drop e backpressure.
9. Solo dopo valutare protobuf, MessagePack e socket.
10. Solo dopo valutare writer plugin dinamici.

## Profili operativi

Alfred deve distinguere debug, produzione, benchmark e audit. Senza profili,
si rischia di misurare una configurazione di debug e scambiarla per prestazione
di produzione.

| Profilo | Writer | Flush | Obiettivo |
| --- | --- | --- | --- |
| `debug` | text verboso, JSONL opzionale | anche immediato | leggibilita' e didattica |
| `production` | JSONL buffered, text minimo o spento | batch/periodico | throughput e stabilita' |
| `benchmark` | no-op/counter, poi writer singoli | controllato | misurare overhead puro |
| `audit` | ledger JSONL critical, hash chain futura | controllato | integrita' e ricostruzione |

Il profilo `benchmark` e' essenziale: se il sink no-op e' veloce ma JSONL e'
lento, il collo di bottiglia e' il writer, non il backend o l'architettura
plugin-like.

## Plugin writer

I writer devono seguire lo stesso principio dei backend: prima API statica
semplice e testabile, poi eventuali plugin dinamici.

Fase iniziale:

```text
writer compilati staticamente con interfaccia plugin-like
-> noop/counter
-> text
-> jsonl
```

Fase successiva:

```text
writer dinamici opzionali
-> protobuf
-> messagepack
-> unix_socket
-> tcp_socket
-> binary_native
```

I plugin writer dinamici richiederanno un contratto ABI, versioning, ownership
della memoria, error model e limiti sul tempo di esecuzione. Non vanno
introdotti prima che il record model e la coda siano stabili.

Per i backend la cautela deve essere ancora maggiore. I backend sono vicini alla
sorgente eventi e quindi piu' sensibili a chiamate indirette, lock, allocazioni
e conversioni nel posto sbagliato. La strategia consigliata e':

- backend plugin-like compilati staticamente nella prima fase;
- writer plugin-like compilati staticamente nella prima fase;
- plugin `.so` rimandati;
- eventuali plugin out-of-process per sink non critici o non fidati.

I plugin out-of-process possono essere piu' lenti, ma sono piu' sicuri: se il
processo esterno crasha, Alfred non deve cadere insieme a lui.

## Benchmark da progettare

Le prestazioni devono essere misurate, non dedotte. I benchmark minimi per
Writer API v0 sono:

1. backend -> no-op sink;
2. backend -> text sink;
3. backend -> JSONL sink;
4. backend -> JSONL + text;
5. backend -> JSONL buffered;
6. backend -> JSONL con flush immediato;
7. backend -> MessagePack/protobuf futuri.

Metriche da raccogliere:

- eventi al secondo;
- CPU;
- memoria;
- latenza media;
- p95/p99 latency;
- profondita' code;
- record droppati;
- errori dei sink;
- tempo speso in serializzazione;
- tempo speso in I/O.

Il benchmark piu' importante e' il no-op sink: misura il costo del cuore di
Alfred senza I/O. Solo dopo ha senso confrontare text, JSONL e formati binari.

## Ownership e record accodati

La Writer API v0 non puo' assumere che un `alfred_record_t` borrowed resti
valido dopo la chiamata che lo ha prodotto. Nel runtime corrente questo e'
accettabile perche' i bridge sono sincroni e immediati. Nel modello target,
invece, il record entra in una coda e viene letto piu' tardi da un dispatcher o
da un worker del writer.

Regola:

```text
Ogni record che supera il confine della coda deve essere owned oppure deve
puntare a memoria con lifetime esplicitamente garantito dal runtime.
```

Per v0 la soluzione piu' chiara e' una copia owned prima dell'enqueue. Anche se
ha un costo, elimina ambiguita' su stringhe borrowed come path, messaggi di
errore, nome backend, futuro `agent_session_id`, workspace, command line o
policy rule. Nel codice questa scelta preparatoria e' rappresentata da:

```c
int alfred_record_clone_owned(const alfred_record_t *src,
                              alfred_record_t *dst);
void alfred_record_destroy_owned(alfred_record_t *record);
```

Queste funzioni non sono ancora collegate al runtime hot path. Servono a
verificare il contratto di ownership prima di introdurre queue, dispatcher e
writer asincroni.

### Confronto fra strategie di ownership

| Strategia | Idea | Pro | Contro | Stato |
| --- | --- | --- | --- | --- |
| Deep copy per record | duplicare tutte le stringhe usate dal record | semplice, sicura, testabile | `malloc()` e copie nel path caldo se usata prima dell'enqueue | implementata come API preparatoria |
| Storage inline fisso | buffer dentro lo slot della coda | niente allocazioni per evento, lifetime semplice | record grandi, copie potenzialmente pesanti, limiti fissi | candidata per ring buffer performante |
| Pool/arena per batch | molte stringhe dentro una regione liberata insieme | meno allocazioni, migliore localita' | lifetime piu' complesso, difficile se i sink hanno tempi diversi | futura, dopo benchmark |
| String/path table | condividere path e prefissi con lifetime controllato | riduce duplicati, utile su alberi ricorsivi | reference counting, lock, debugging piu' difficile | futura, non per v0 |

La regola pratica e':

```text
prima sicurezza del lifetime, poi ottimizzazione misurata
```

Se la deep copy viene messa direttamente nel backend poll loop, puo' rallentare
Alfred perche' porta nel path caldo `strlen()`, `malloc()`, `memcpy()` e
gestione degli errori. Per questo la API owned nasce ora, ma il collegamento al
dispatcher verra' deciso insieme ai benchmark no-op, text e JSONL.

## Code per sink

Il primo dispatcher potra' usare una coda comune, ma l'architettura deve
restare compatibile con una coda per sink:

```text
record queue
-> dispatcher
-> text queue
-> jsonl queue
-> msgpack queue
-> lab queue
```

Il motivo e' semplice: un writer lento non deve rallentare tutto Alfred. Un
writer JSONL con disco lento, un socket bloccato o una UI di debug non devono
fermare il backend che osserva eventi OS.

Le classi di sink previste sono:

| Classe | Uso | Policy possibile |
| --- | --- | --- |
| critical | ledger/audit che non deve perdere eventi senza segnalarlo | errore serio, backpressure o shutdown controllato |
| best-effort | integrazioni utili ma non essenziali | drop controllato con contatore/diagnostica |
| debug | output umano, Lab, trace verbose | disabilitabile o campionabile |

Questa classificazione non deve vivere nascosta dentro un writer. Deve essere
visibile nella configurazione o nel registry, cosi' i test e i benchmark
possono verificare cosa succede quando un sink e' lento.

## Record Queue v0

La prima API di coda e':

```c
int alfred_record_queue_init(alfred_record_queue_t *queue, size_t capacity);
void alfred_record_queue_destroy(alfred_record_queue_t *queue);
void alfred_record_queue_clear(alfred_record_queue_t *queue);
int alfred_record_queue_push(alfred_record_queue_t *queue,
                             const alfred_record_t *record);
int alfred_record_queue_pop(alfred_record_queue_t *queue,
                            alfred_record_t *record);
```

`push()` riceve un record borrowed e clona le stringhe con
`alfred_record_clone_owned()`. Questo permette al produttore di riusare il suo
buffer subito dopo la chiamata. `pop()` trasferisce al chiamante il record owned:
da quel momento il chiamante deve chiamare `alfred_record_destroy_owned()`.

Questo modello e' didatticamente importante perche' separa tre concetti che
spesso vengono confusi:

| Concetto | Significato |
| --- | --- |
| record borrowed | vista valida solo durante la chiamata sincrona |
| record owned | record che possiede le stringhe e puo' vivere piu' a lungo |
| queue boundary | punto in cui un record puo' sopravvivere al produttore |

In termini di prestazioni questa non e' ancora la soluzione finale. La coda v0
usa deep copy per rendere il contratto facile da verificare. Prima di usarla nel
path caldo bisognera' misurare:

- costo di `malloc()` e `free()` per evento;
- costo della copia dei path;
- latenza media e p95/p99;
- throughput con sink no-op, text e JSONL;
- comportamento quando la coda e' piena.

Solo dopo questi benchmark decideremo se mantenere deep copy, passare a storage
inline, introdurre pool/arena o usare tabelle path condivise.

## Record Dispatcher v0

### Che cosa sono dispatcher e sink

Il dispatcher e' il componente che riceve un `alfred_record_t` e lo consegna a
uno o piu' destinatari registrati. Non decide il significato del record, non lo
trasforma in JSONL, non scrive su file e non apre socket. Il suo compito e'
solo fare routing:

```text
alfred_record_t
-> dispatcher
-> sink 1
-> sink 2
-> sink 3
```

Un sink e' un consumatore di record. In pratica e' un piccolo oggetto che dice:
"quando ricevi un record, chiama questa funzione con questo contesto privato".
Nel codice il sink base e':

```c
typedef int (*alfred_record_sink_emit_fn)(void *userdata,
                                          const alfred_record_t *record);

typedef struct {
    alfred_record_sink_emit_fn emit;
    void *userdata;
} alfred_record_sink_t;
```

I due campi vanno letti cosi':

| Campo | Significato |
| --- | --- |
| `emit` | funzione che il dispatcher deve chiamare per consegnare il record |
| `userdata` | puntatore opaco al contesto privato del sink |

`userdata` permette di usare la stessa interfaccia per sink molto diversi. Per
esempio:

- un text sink puo' mettere in `userdata` il proprio buffer e la callback che
  scrive il payload testuale;
- un futuro JSONL sink potra' mettere in `userdata` il proprio writer JSONL;
- un sink di test puo' mettere in `userdata` una struttura che conta quante volte
  e' stato chiamato;
- un futuro socket sink potra' mettere in `userdata` il file descriptor o il
  contesto del protocollo.

Questa separazione e' importante perche' il dispatcher non deve conoscere il tipo
reale del sink. Il dispatcher vede solo:

```text
emit(record)
```

Il resto resta privato del sink.

### Come lavorano insieme

Il percorso concettuale completo e':

```text
producer/backend/core
-> alfred_record_t
-> queue
-> dispatcher
-> sink
-> writer/bridge
```

Ogni pezzo ha una responsabilita' diversa:

| Pezzo | Responsabilita' |
| --- | --- |
| producer/backend/core | produce un record strutturato |
| queue | conserva record owned in attesa |
| dispatcher | sceglie quali sink devono ricevere il record |
| sink | riceve il record attraverso `emit()` |
| writer/bridge | serializza, scrive, invia o adatta il record |

Una regola utile da ricordare e':

```text
la queue conserva, il dispatcher consegna, il sink consuma
```

Per esempio, con tre sink registrati:

```text
text sink
jsonl sink
lab sink
```

il dispatcher fara':

```text
record -> text sink
record -> jsonl sink
record -> lab sink
```

Il record passato ai sink e' una vista borrowed valida durante la chiamata. Se un
sink vuole conservarlo per dopo, deve copiarlo o usare un'altra strategia di
ownership sicura. Questo e' lo stesso principio usato per le code: appena un
record supera un confine asincrono, non puo' piu' dipendere da memoria borrowed
del produttore.

### Come il dispatcher contatta i sink

Il dispatcher contiene un array di sink registrati. Quando si chiama:

```c
alfred_record_dispatcher_dispatch_one(&dispatcher, &record);
```

il comportamento v0 e' equivalente a:

```c
for (i = 0; i < dispatcher->count; ++i) {
    alfred_record_sink_emit(&dispatcher->sinks[i].sink, record);
}
```

`alfred_record_sink_emit()` e' un wrapper difensivo: controlla che il sink esista,
che la funzione `emit` non sia `NULL` e poi chiama davvero:

```c
sink->emit(sink->userdata, record);
```

Quindi il dispatcher non chiama direttamente `jsonl_write()`, `fprintf()` o
`socket_send()`. Chiama sempre la stessa interfaccia astratta:

```text
alfred_record_sink_emit()
```

Sara' poi il sink concreto a decidere cosa fare.

### Ordine di chiamata

Nel dispatcher v0 i sink vengono chiamati in ordine di registrazione. Se il
codice registra i sink cosi':

```text
1. text
2. jsonl
3. lab
```

l'ordine di dispatch sara':

```text
record -> text
record -> jsonl
record -> lab
```

Questo ordine e' testato in `tests/backend/test_record_dispatcher.c`. Il test
usa sink finti che salvano l'ordine di chiamata e verifica che il dispatcher non
lo cambi.

### Cosa succede se un sink fallisce

La policy v0 e' semplice: se un sink fallisce, il dispatcher si ferma e ritorna
errore.

Esempio:

```text
text  -> ok
jsonl -> errore
lab   -> non chiamato
```

Questa non e' necessariamente la policy finale. In futuro useremo le classi sink
per decidere comportamenti diversi:

- un sink `critical` potrebbe richiedere backpressure, errore serio o shutdown
  controllato;
- un sink `best-effort` potrebbe perdere record in modo controllato e aumentare
  un contatore diagnostico;
- un sink `debug` potrebbe essere disabilitato, campionato o droppato quando e'
  lento.

Per v0 scegliamo la regola piu' semplice perche' vogliamo fissare il contratto
base prima di introdurre policy di backpressure piu' complesse.

### Perche' il dispatcher non e' un writer

Il dispatcher non e' un writer. Questa distinzione e' fondamentale.

Il dispatcher sa:

- quanti sink sono registrati;
- in quale ordine chiamarli;
- se un sink ha fallito;
- quale classe futura ha un sink.

Il dispatcher non deve sapere:

- come si formatta JSONL;
- come si formatta MessagePack;
- come si scrive su `raw.log`;
- come si fa `fflush()`;
- come si invia un record su socket;
- come si disegna Alfred Lab.

Questa separazione protegge il percorso caldo di Alfred. Il backend deve produrre
record e accodarli rapidamente; serializzazione, I/O, flush e integrazioni lente
devono stare dietro sink/writer fuori dal path caldo.

La prima API dispatcher e':

```c
int alfred_record_dispatcher_init(alfred_record_dispatcher_t *dispatcher,
                                  alfred_record_dispatcher_sink_t *sinks,
                                  size_t capacity);
void alfred_record_dispatcher_clear(alfred_record_dispatcher_t *dispatcher);
int alfred_record_dispatcher_add_sink(
    alfred_record_dispatcher_t *dispatcher,
    const char *name,
    alfred_record_dispatcher_sink_class_t sink_class,
    const alfred_record_sink_t *sink);
int alfred_record_dispatcher_dispatch_one(
    const alfred_record_dispatcher_t *dispatcher,
    const alfred_record_t *record);
```

Il dispatcher non possiede writer, file descriptor, socket o buffer di
serializzazione. Possiede solo la registrazione dei sink dentro lo storage
fornito dal chiamante. Questo e' intenzionale:

- mantiene il dispatcher piccolo;
- rende il limite di fan-out esplicito;
- evita allocazioni interne nella API v0;
- prepara test e benchmark senza introdurre thread;
- tiene writer e policy fuori dal contratto di routing.

`dispatch_one()` chiama i sink in ordine di registrazione usando
`alfred_record_sink_emit()`. Se un sink fallisce, la versione v0 si ferma subito
e ritorna errore. Non esistono ancora retry, drop controllato, isolamento per
sink o coda per sink: queste sono decisioni di backpressure da prendere dopo i
benchmark.

La differenza fra i tre concetti e':

| Pezzo | Responsabilita' |
| --- | --- |
| queue | conserva record owned in attesa |
| dispatcher | sceglie e chiama i sink registrati |
| sink | riceve un record e lo consegna a un writer/bridge |

Questa distinzione evita una confusione pericolosa: il dispatcher non e' il
writer. Un dispatcher non deve sapere come si formatta JSONL, come si scrive su
file o come si manda un messaggio su socket. Deve solo consegnare record.

### Che cosa significa drain della queue

"Drain della queue" significa svuotare una coda consumando i record che contiene.
Nel nostro caso la queue contiene record owned:

```text
queue:
[record 1][record 2][record 3]
```

Fare drain significa eseguire un ciclo completo:

```text
pop record 1
-> dispatch record 1 ai sink
-> destroy record 1

pop record 2
-> dispatch record 2 ai sink
-> destroy record 2

pop record 3
-> dispatch record 3 ai sink
-> destroy record 3
```

Alla fine la coda e' vuota:

```text
queue:
vuota
```

Quindi "drain" non significa solo leggere la coda. Significa:

```text
estrai
consegna
rilascia memoria
```

Nel codice il percorso e':

```text
alfred_record_queue_t
-> alfred_record_queue_pop()
-> alfred_record_dispatcher_dispatch_one()
-> sink emit()
-> alfred_record_destroy_owned()
```

Questa funzione serve per collegare i pezzi che abbiamo separato:

- la queue conserva record owned;
- il dispatcher consegna record ai sink;
- i sink ricevono record;
- il destroy chiude il ciclo di ownership.

Il parametro `max_records` rende il drain bounded anche nel tempo di lavoro. Se
la queue contiene 1000 record ma `max_records = 64`, una chiamata processa al
massimo 64 record e poi ritorna:

```text
queue iniziale: 1000 record
max_records: 64
drain corrente: 64 record processati
queue finale: 936 record ancora in attesa
```

Questo sara' utile nel runtime futuro per evitare che il dispatcher monopolizzi
troppo a lungo il programma. In altre parole, `max_records` aiuta batch,
latenza e fairness.

La policy v0 in caso di errore e' volutamente semplice. Se un sink fallisce dopo
il `pop`, il record gia' estratto viene distrutto e la funzione ritorna errore.
Nel runtime finale dovremo decidere una policy piu' completa:

- retry;
- requeue;
- dead-letter queue;
- drop diagnostico;
- shutdown controllato per sink critical.

Queste decisioni appartengono alla backpressure policy futura. Non le nascondiamo
dentro il primo helper di drain.

## Regola contrattuale

La regola da non violare e':

```text
Il backend non aspetta il writer.
```

Il backend produce record e li consegna alla coda. Tutto il resto e' lavoro di
dispatcher, worker, writer o policy fuori hot path.

La frase guida e':

```text
Interoperabilita' al bordo, prestazioni nel cuore.
```
