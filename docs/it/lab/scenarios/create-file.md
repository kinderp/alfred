# Scenario: create file

id: lab.fs.create-file.v0
title: Create a file and observe the semantic event
status: stable-doc
scenario kind: filesystem

## Goal

Questo scenario spiega il percorso piu' corto del Lab MVP: Alfred osserva la
creazione di un file, normalizza il fatto backend, il core produce semantica
filesystem e un sink riceve il record corrispondente.

Il punto didattico e' distinguere tre livelli:

- il backend vede un fatto tecnico Linux, per esempio `IN_CREATE`;
- Alfred normalizza quel fatto in un raw event, per esempio `RAW_CREATE`;
- il core produce semantica stabile, per esempio `FILE_CREATED`.

## Trigger

Comando minimo:

```text
touch a.txt
```

Nei test esistenti si usa spesso una scrittura:

```text
printf "hello\n" > a.txt
```

La scrittura e' utile per i test end-to-end perche' puo' mostrare anche
`FILE_MODIFIED` e `FILE_READY`. Questo scenario pero' si concentra su
`FILE_CREATED`; `FILE_READY` appartiene allo scenario close-write/file-ready.

## Expected Evidence

Evidenza minima attesa:

- il backend legge un evento `IN_CREATE` per `a.txt`;
- l'adapter costruisce un `alfred_raw_event_t` con mask create e path borrowed;
- il raw sink puo' emettere una riga compatibile `RAW_CREATE`;
- il core riceve il raw event e produce un evento semantico `FILE_CREATED`;
- il logger/adapter costruisce un `alfred_record_t` semantic;
- un sink riceve il record per output umano o macchina.

Questa evidenza non richiede runtime trace output. I tracepoint restano nomi
`stable-doc` che spiegano il percorso.

## Expected Records

Record o evidenze strutturate attese:

```text
layer=normalized_raw
category=filesystem
type=RAW_CREATE
path=*/a.txt
raw_mask includes ALFRED_RAW_CREATE
```

```text
layer=semantic
category=filesystem
type=FILE_CREATED
path=*/a.txt
```

Se il trigger crea una directory invece di un file, l'evento semantico atteso
diventa `DIR_CREATED`. Se il trigger scrive contenuto, possono comparire anche
`FILE_MODIFIED` e `FILE_READY`, ma non sono il focus di questo scenario.

## Logical Tracepoints

Tracepoint `stable-doc` attraversati:

```text
BACKEND_RAW_EVENT_READ
RAW_EVENT_NORMALIZED
CORE_SEMANTIC_EVENT_EMITTED
SINK_RECORD_EMITTED
```

Significato:

- `BACKEND_RAW_EVENT_READ`: il backend ha letto o ricevuto il fatto OS;
- `RAW_EVENT_NORMALIZED`: l'adapter ha prodotto la forma raw Alfred;
- `CORE_SEMANTIC_EVENT_EMITTED`: il core ha emesso `FILE_CREATED`;
- `SINK_RECORD_EMITTED`: un sink ha ricevuto il record da emettere.

## Function Path

Percorso principale oggi:

```text
inotify_backend_poll()
-> backend_poll()
-> inotify_adapter_build_raw()
-> handle_backend_event()
-> alfred_process()
-> emit()
-> core_logger_on_event()
-> alfred_record_from_event()
-> alfred_record_sink_emit()
```

Responsabilita' dei passaggi:

- `inotify_backend_poll()` legge eventi dal backend inotify corrente;
- `backend_poll()` itera gli eventi letti dal file descriptor inotify;
- `inotify_adapter_build_raw()` traduce `struct inotify_event` in
  `alfred_raw_event_t`;
- `handle_backend_event()` e' il ponte runtime corrente verso il core
  semantico;
- `alfred_process()` interpreta il raw event nel core;
- `emit()` produce l'evento semantico;
- `core_logger_on_event()` riceve l'evento semantico dal core;
- `alfred_record_from_event()` lo converte in `alfred_record_t`;
- `alfred_record_sink_emit()` consegna il record al sink configurato.

## State Changes

Stato rilevante:

- `watcher_table_t` risolve il watch descriptor nel path osservato;
- `alfred_raw_event_t.path` punta al path normalizzato per il file creato;
- `alfred_raw_event_t.mask` contiene `ALFRED_RAW_CREATE`;
- il core incrementa la sequenza degli eventi semantici emessi;
- il record semantic porta `layer=semantic`, `category=filesystem` e
  `type=FILE_CREATED`.

Il percorso create non usa la cache move `moves[]`. Quella compare negli
scenari rename/move/relocate.

## Expected Outputs

Output compatibili osservabili oggi:

```text
raw.log:
IN_CREATE ... name=a.txt
RAW_CREATE path=*/a.txt mask=1
```

```text
events.log:
FILE_CREATED path=*/a.txt
```

Con output JSONL abilitato, `output.jsonl` puo' contenere:

```text
{"layer":"normalized_raw","category":"filesystem","type":"RAW_CREATE",...}
{"layer":"semantic","category":"filesystem","type":"FILE_CREATED",...}
```

Il Lab non deve dipendere dall'intera riga testuale. Deve usare questi output
come evidenza del comportamento, non come unico contratto fragile.

## Tests

existing:

- `tests/core/test_create_file.sh`
- `tests/backend/test_raw_create_record_sink.sh`
- `tests/jsonl/test_create_file_and_dir_jsonl.sh`

missing:

- nessun test specifico sul file Markdown dello scenario;
- nessun test parser, perche' il formato scenario v0 non e' ancora parsato.

future:

- test focused se in futuro gli scenari Lab diventano input macchina;
- test focused se un tracepoint viene promosso da `stable-doc` a
  `public-output`.

## Common Failures

- Confondere `IN_CREATE` con `FILE_CREATED`: il primo e' backend evidence, il
  secondo e' semantica core.
- Aspettarsi `FILE_READY` da questo scenario: `FILE_READY` richiede close-write
  e appartiene a un altro scenario.
- Asserire l'intera riga di `events.log`: il formato umano puo' cambiare piu'
  facilmente del record strutturato.
- Pensare che `SINK_RECORD_EMITTED` significhi flush durable su disco: indica
  consegna al sink, non garanzia di persistenza finale.
- Pensare che esista `trace.jsonl`: per v0 il trace runtime e' esplicitamente
  rimandato.

## Non-goals

- Nessun runtime trace output.
- Nessun parser scenario.
- Nessuna UI Lab.
- Nessun cambio a `alfred_record_t`.
- Nessun cambio al percorso caldo.
- Nessuna policy o decisione Agent Guard.
- Nessuna copertura di rename, move, relocate, watch stale o file-ready.

## Related Docs

- [Tracepoint e Alfred Lab MVP](../../41-tracepoint-lab-roadmap-mvp.md)
- [Tracepoint Model v0](../../42-tracepoint-model-v0.md)
- [Mappa codice e strutture dati](../../16-mappa-codice-e-strutture.md#mappa-tracepointlab-mvp)
- [Contratto dei log](../../22-contratto-log.md)
