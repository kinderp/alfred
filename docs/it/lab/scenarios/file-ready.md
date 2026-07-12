# Scenario: close-write / file-ready

id: lab.fs.file-ready.v0
title: Close a written file and observe when it is ready
status: stable-doc
scenario kind: filesystem

## Goal

Questo scenario spiega perche' `FILE_READY` e' diverso da `FILE_CREATED` e da
`FILE_MODIFIED`.

Il punto didattico e':

- `FILE_CREATED` dice che il file e' comparso;
- `FILE_MODIFIED` dice che sono stati scritti o cambiati byte;
- `FILE_READY` dice che il lato scrittura e' stato chiuso e quindi un consumer
  puo' ragionevolmente leggere il file senza inseguire una scrittura ancora in
  corso.

Questo e' importante per backup, indicizzatori, scanner, ingestion pipeline e
tool che non vogliono consumare un file mentre il writer lo sta ancora
aggiornando.

## Trigger

Comando rappresentativo:

```text
printf "hello\n" > a.txt
```

Scenario con due scritture, usato dai test esistenti:

```text
printf "initial\n" > editable.txt
printf "second\n" >> editable.txt
```

La seconda forma e' utile per dimostrare che Alfred puo' vedere piu' cicli di
modifica e chiusura sullo stesso path.

## Expected Evidence

Evidenza minima attesa:

- il backend vede almeno un fatto `IN_MODIFY` mentre il file viene scritto;
- il backend vede un fatto `IN_CLOSE_WRITE` quando il writer chiude il file;
- l'adapter costruisce raw event distinti: `RAW_MODIFY` e `RAW_CLOSE_WRITE`;
- il core produce semantiche distinte: `FILE_MODIFIED` e `FILE_READY`;
- il logger/adapter costruisce record semantici separati;
- un sink riceve i record per output umano o macchina.

Questa evidenza non richiede runtime trace output. I tracepoint restano nomi
`stable-doc` che spiegano il percorso.

## Expected Records

Record o evidenze strutturate attese per una singola scrittura:

```text
layer=normalized_raw
category=filesystem
type=RAW_MODIFY
path=*/a.txt
raw_mask includes ALFRED_RAW_MODIFY
```

```text
layer=normalized_raw
category=filesystem
type=RAW_CLOSE_WRITE
path=*/a.txt
raw_mask includes ALFRED_RAW_CLOSE_WRITE
```

```text
layer=semantic
category=filesystem
type=FILE_MODIFIED
path=*/a.txt
```

```text
layer=semantic
category=filesystem
type=FILE_READY
path=*/a.txt
```

Se il file non esisteva prima, puo' comparire anche `FILE_CREATED`. Quello e'
il risultato della creazione iniziale, non il segnale di readiness.

## Logical Tracepoints

Tracepoint `stable-doc` attraversati:

```text
BACKEND_RAW_EVENT_READ
RAW_EVENT_NORMALIZED
CORE_SEMANTIC_EVENT_EMITTED
SINK_RECORD_EMITTED
```

Significato:

- `BACKEND_RAW_EVENT_READ`: il backend ha letto o ricevuto fatti OS come
  `IN_MODIFY` e `IN_CLOSE_WRITE`;
- `RAW_EVENT_NORMALIZED`: l'adapter ha prodotto raw facts Alfred distinti;
- `CORE_SEMANTIC_EVENT_EMITTED`: il core ha emesso `FILE_MODIFIED` o
  `FILE_READY`;
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
- `alfred_process()` interpreta `ALFRED_RAW_MODIFY` e
  `ALFRED_RAW_CLOSE_WRITE` come fatti diversi;
- `emit()` produce eventi semantici distinti;
- `core_logger_on_event()` riceve l'evento semantico dal core;
- `alfred_record_from_event()` lo converte in `alfred_record_t`;
- `alfred_record_sink_emit()` consegna il record al sink configurato.

## State Changes

Stato rilevante:

- `watcher_table_t` risolve il watch descriptor nel path osservato;
- `alfred_raw_event_t.mask` contiene `ALFRED_RAW_MODIFY` per la modifica;
- `alfred_raw_event_t.mask` contiene `ALFRED_RAW_CLOSE_WRITE` per la chiusura
  del writer;
- il core incrementa la sequenza degli eventi semantici emessi;
- il record semantic porta `layer=semantic`, `category=filesystem` e
  `type=FILE_MODIFIED` oppure `type=FILE_READY`.

Il percorso file-ready non usa la cache move `moves[]`. Non decide nemmeno che
il file sia semanticamente "buono" o "sicuro": dice solo che il writer ha
chiuso il lato scrittura osservato dal backend.

## Expected Outputs

Output compatibili osservabili oggi:

```text
raw.log:
IN_MODIFY ... name=a.txt
RAW_MODIFY path=*/a.txt mask=4
IN_CLOSE_WRITE ... name=a.txt
RAW_CLOSE_WRITE path=*/a.txt mask=16
```

```text
events.log:
FILE_MODIFIED path=*/a.txt
FILE_READY path=*/a.txt
```

Con output JSONL abilitato, `output.jsonl` puo' contenere:

```text
{"layer":"normalized_raw","category":"filesystem","type":"RAW_MODIFY",...}
{"layer":"normalized_raw","category":"filesystem","type":"RAW_CLOSE_WRITE",...}
{"layer":"semantic","category":"filesystem","type":"FILE_MODIFIED",...}
{"layer":"semantic","category":"filesystem","type":"FILE_READY",...}
```

Il Lab non deve dipendere dall'intera riga testuale. Deve usare questi output
come evidenza del comportamento, non come unico contratto fragile.

## Tests

existing:

- `tests/core/test_modify_file.sh`
- `tests/backend/test_raw_close_write_record_sink.sh`
- `tests/jsonl/test_modify_file_jsonl.sh`

missing:

- nessun test specifico sul file Markdown dello scenario;
- nessun test parser, perche' il formato scenario v0 non e' ancora parsato.

future:

- test focused se in futuro gli scenari Lab diventano input macchina;
- test focused se un tracepoint viene promosso da `stable-doc` a
  `public-output`;
- test focused se Alfred introdurra' un concetto piu' forte di file
  consumabile, per esempio verifica di size stabile, checksum o policy.

## Common Failures

- Confondere `FILE_MODIFIED` con `FILE_READY`: il primo riguarda byte cambiati,
  il secondo riguarda la chiusura del writer.
- Confondere `FILE_READY` con sicurezza o validita' del contenuto: Alfred non
  sta dicendo che il file sia affidabile, solo che il writer ha chiuso.
- Aspettarsi un solo evento per una scrittura: un file nuovo puo' produrre
  create, modify e close-write.
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
- Nessuna garanzia che il contenuto del file sia valido, sicuro o completo dal
  punto di vista applicativo.
- Nessuna copertura di rename, move, relocate o watch stale.

## Related Docs

- [Tracepoint e Alfred Lab MVP](../../41-tracepoint-lab-roadmap-mvp.md)
- [Tracepoint Model v0](../../42-tracepoint-model-v0.md)
- [Scenario: create file](create-file.md)
- [Mappa codice e strutture dati](../../16-mappa-codice-e-strutture.md#mappa-tracepointlab-mvp)
- [Contratto dei log](../../22-contratto-log.md)
