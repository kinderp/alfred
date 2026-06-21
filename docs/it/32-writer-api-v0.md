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
- lock pesanti o non bounded.

Questa regola e' piu' importante della scelta del formato. Un writer JSONL
ottimo, ma chiamato sincronicamente dal backend per ogni evento, sarebbe comunque
nel posto sbagliato.

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
4. Implementare un writer text compatibile sopra Writer API v0.
5. Implementare un writer JSONL minimo con test golden.
6. Aggiungere metriche di coda, drop e backpressure.
7. Solo dopo valutare protobuf, MessagePack e socket.
8. Solo dopo valutare writer plugin dinamici.

## Plugin writer

I writer devono seguire lo stesso principio dei backend: prima API statica
semplice e testabile, poi eventuali plugin dinamici.

Fase iniziale:

```text
writer compilati staticamente
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

## Regola contrattuale

La regola da non violare e':

```text
Il backend non aspetta il writer.
```

Il backend produce record e li consegna alla coda. Tutto il resto e' lavoro di
dispatcher, worker, writer o policy fuori hot path.
