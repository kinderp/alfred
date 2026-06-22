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
policy rule. Ottimizzazioni come pool, arena, string table o buffer reference
counted dovranno arrivare dopo benchmark e non prima del contratto.

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
