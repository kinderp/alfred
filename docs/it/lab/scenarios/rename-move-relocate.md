# Scenario: rename / move / relocate

id: lab.fs.rename-move-relocate.v0
title: Correlate a move pair and classify rename, move or relocate
status: stable-doc
scenario kind: filesystem

## Goal

Questo scenario spiega il primo caso in cui Alfred non inoltra semplicemente un
raw event. Un rename o uno spostamento filesystem arriva da inotify come due
fatti separati:

- `MOVED_FROM`: vecchio path;
- `MOVED_TO`: nuovo path.

I due fatti sono collegati da un cookie. Il core salva la prima meta' in una
tabella di move pendenti, aspetta la seconda meta', recupera la coppia usando
il cookie e poi decide quale evento semantico emettere.

Il punto didattico e':

- stesso parent, nome diverso -> `FILE_RENAMED` o `DIR_RENAMED`;
- parent diverso, stesso nome -> `FILE_MOVED` o `DIR_MOVED`;
- parent diverso, nome diverso -> `FILE_RELOCATED` o `DIR_RELOCATED`.

La semantica core target e' un solo evento logico per una coppia matched. In
particolare, il caso relocated non deve produrre due eventi separati
`MOVED + RENAMED`.

## Trigger

Rename nello stesso parent:

```text
printf "rename\n" > old.txt
mv old.txt new.txt
```

Move verso un altro parent mantenendo lo stesso basename:

```text
mkdir src dst
printf "move\n" > src/item.txt
mv src/item.txt dst/item.txt
```

Relocate, cioe' cambio di parent e cambio di basename:

```text
mkdir src dst
printf "relocate\n" > src/before.txt
mv src/before.txt dst/after.txt
```

Gli stessi criteri valgono per le directory, con eventi `DIR_*`.

## Expected Evidence

Evidenza minima attesa:

- il backend vede un fatto `IN_MOVED_FROM` con path sorgente e cookie non zero;
- il backend vede un fatto `IN_MOVED_TO` con path destinazione e lo stesso
  cookie;
- l'adapter costruisce raw event distinti: `RAW_MOVED_FROM` e `RAW_MOVED_TO`;
- il core conserva `MOVED_FROM` in `moves[1024]`;
- il core recupera la voce quando arriva `MOVED_TO` con cookie coerente;
- `classify_move()` confronta parent directory e basename;
- il core produce una sola semantica finale;
- il logger/adapter costruisce un record semantico;
- un sink riceve il record per output umano o macchina.

Questa evidenza non richiede runtime trace output. I tracepoint restano nomi
`stable-doc` che spiegano il percorso.

## Expected Records

Record o evidenze strutturate attese per la parte raw:

```text
layer=normalized_raw
category=filesystem
type=RAW_MOVED_FROM
path=*/old.txt
raw_mask includes ALFRED_RAW_MOVED_FROM
cookie=<n>
```

```text
layer=normalized_raw
category=filesystem
type=RAW_MOVED_TO
path=*/new.txt
raw_mask includes ALFRED_RAW_MOVED_TO
cookie=<same n>
```

Record semantico atteso per rename file:

```text
layer=semantic
category=filesystem
type=FILE_RENAMED
old_path=*/old.txt
new_path=*/new.txt
```

Record semantico atteso per move file:

```text
layer=semantic
category=filesystem
type=FILE_MOVED
old_path=*/src/item.txt
new_path=*/dst/item.txt
```

Record semantico atteso per relocate file:

```text
layer=semantic
category=filesystem
type=FILE_RELOCATED
old_path=*/src/before.txt
new_path=*/dst/after.txt
```

Per le directory i tipi diventano `DIR_RENAMED`, `DIR_MOVED` e
`DIR_RELOCATED`. I raw directory includono anche `ALFRED_RAW_ISDIR`.

## Logical Tracepoints

Tracepoint `stable-doc` attraversati:

```text
BACKEND_RAW_EVENT_READ
RAW_EVENT_NORMALIZED
MOVE_FROM_STORED
MOVE_MATCH_FOUND
CORE_SEMANTIC_EVENT_EMITTED
SINK_RECORD_EMITTED
```

Significato:

- `BACKEND_RAW_EVENT_READ`: il backend ha letto `IN_MOVED_FROM` o
  `IN_MOVED_TO`;
- `RAW_EVENT_NORMALIZED`: l'adapter ha prodotto raw facts Alfred distinti;
- `MOVE_FROM_STORED`: il core ha conservato la prima meta' in `moves[]`;
- `MOVE_MATCH_FOUND`: il core ha trovato la voce pendente con lo stesso
  cookie;
- `CORE_SEMANTIC_EVENT_EMITTED`: il core ha emesso rename, move o relocate;
- `SINK_RECORD_EMITTED`: un sink ha ricevuto il record da emettere.

## Function Path

Percorso principale oggi:

```text
inotify_backend_poll()
-> backend_poll()
-> inotify_adapter_build_raw()
-> handle_backend_event()
-> alfred_process()
-> alfred_move_insert()
-> alfred_process()
-> alfred_move_take()
-> classify_move()
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
- `alfred_process()` vede `ALFRED_RAW_MOVED_FROM` e non emette subito una
  semantica utente;
- `alfred_move_insert()` copia il path sorgente in una entry owned della move
  table;
- `alfred_process()` vede `ALFRED_RAW_MOVED_TO` e cerca il cookie;
- `alfred_move_take()` rimuove la entry pendente e trasferisce ownership al
  caller;
- `classify_move()` confronta parent directory e basename;
- `emit()` produce il singolo evento semantico finale;
- `core_logger_on_event()` riceve l'evento semantico dal core;
- `alfred_record_from_event()` lo converte in `alfred_record_t`;
- `alfred_record_sink_emit()` consegna il record al sink configurato.

## State Changes

Stato rilevante:

- `watcher_table_t` risolve il watch descriptor nel path osservato;
- `alfred_raw_event_t.cookie` collega la sorgente e la destinazione;
- `alfred_raw_event_t.mask` contiene `ALFRED_RAW_MOVED_FROM` per la prima
  meta';
- `alfred_raw_event_t.mask` contiene `ALFRED_RAW_MOVED_TO` per la seconda
  meta';
- `alfred_engine_t.moves[1024]` conserva entry pendenti indicizzate dal cookie;
- `alfred_move_entry_t.src_path` e' una copia owned del path sorgente;
- `alfred_move_take()` rimuove la entry dalla tabella;
- dopo `emit()`, il core libera `src_path` e la entry;
- il record semantic porta `layer=semantic`, `category=filesystem` e un tipo
  tra rename, move o relocate.

Se `MOVED_TO` arriva senza un `MOVED_FROM` matched, il core non puo' dimostrare
rename/move/relocate. In quel caso produce un fallback create per segnalare che
un oggetto e' entrato nell'albero osservato.

## Classification Rule

La regola usata da `classify_move()` e' volutamente semplice:

| Parent directory | Basename | File semantic | Directory semantic |
| --- | --- | --- | --- |
| uguale | diverso | `FILE_RENAMED` | `DIR_RENAMED` |
| diverso | uguale | `FILE_MOVED` | `DIR_MOVED` |
| diverso | diverso | `FILE_RELOCATED` | `DIR_RELOCATED` |

Questa classificazione e' core semantics. I raw event restano sempre raw facts:
`RAW_MOVED_FROM` non e' gia' un rename e `RAW_MOVED_TO` non e' gia' un move.

## Expected Outputs

Output compatibili osservabili oggi per rename file:

```text
raw.log:
IN_MOVED_FROM ... name=old.txt cookie=<n>
RAW_MOVED_FROM path=*/old.txt mask=32 cookie=<n>
IN_MOVED_TO ... name=new.txt cookie=<n>
RAW_MOVED_TO path=*/new.txt mask=64 cookie=<n>
```

```text
events.log:
FILE_RENAMED from=*/old.txt to=*/new.txt
```

Con output JSONL abilitato, `output.jsonl` puo' contenere:

```text
{"layer":"normalized_raw","category":"filesystem","type":"RAW_MOVED_FROM",...}
{"layer":"normalized_raw","category":"filesystem","type":"RAW_MOVED_TO",...}
{"layer":"semantic","category":"filesystem","type":"FILE_RENAMED",...}
```

Per move e relocate il record semantico cambia in `FILE_MOVED` o
`FILE_RELOCATED`. I test JSONL controllano anche che non vengano emesse le
classificazioni sbagliate per la stessa operazione.

Il Lab non deve dipendere dall'intera riga testuale. Deve usare questi output
come evidenza del comportamento, non come unico contratto fragile.

## Tests

existing:

- `tests/core/test_rename_file.sh`
- `tests/core/test_rename_dir.sh`
- `tests/core/test_move_file.sh`
- `tests/core/test_move_dir.sh`
- `tests/core/test_move_rename_file.sh`
- `tests/core/test_move_rename_dir.sh`
- `tests/backend/test_raw_move_record_sink.sh`
- `tests/jsonl/test_rename_file_jsonl.sh`
- `tests/jsonl/test_file_moved_jsonl.sh`
- `tests/jsonl/test_file_relocated_jsonl.sh`
- `tests/jsonl/test_dir_renamed_jsonl.sh`
- `tests/jsonl/test_dir_moved_jsonl.sh`
- `tests/jsonl/test_dir_relocated_jsonl.sh`

missing:

- nessun test specifico sul file Markdown dello scenario;
- nessun test parser, perche' il formato scenario v0 non e' ancora parsato;
- nessun test diretto che ispeziona `moves[]`; per v0 usiamo evidenza
  indiretta attraverso eventi semantici e JSONL.

future:

- test focused se in futuro gli scenari Lab diventano input macchina;
- test focused se `MOVE_FROM_STORED` o `MOVE_MATCH_FOUND` diventano
  `public-output`;
- test focused per fallback `MOVED_TO` senza `MOVED_FROM`, se promosso a
  scenario Lab dedicato.

## Common Failures

- Confondere `RAW_MOVED_FROM` con un evento semantico: da solo e' solo la prima
  meta' della coppia.
- Confondere `RAW_MOVED_TO` con un move gia' classificato: serve prima trovare
  il `MOVED_FROM` matched.
- Aspettarsi due eventi per relocate: la semantica core target e' un solo
  evento `*_RELOCATED`.
- Perdere il cookie nei record strutturati: senza cookie non si puo' spiegare
  la correlazione.
- Non distinguere file e directory: i raw directory includono
  `ALFRED_RAW_ISDIR` e producono tipi `DIR_*`.
- Asserire l'intera riga di `events.log`: il formato umano puo' cambiare piu'
  facilmente del record strutturato.
- Pensare che esista `trace.jsonl`: per v0 il trace runtime e' esplicitamente
  rimandato.

## Non-goals

- Nessun runtime trace output.
- Nessun parser scenario.
- Nessuna UI Lab.
- Nessun cambio a `alfred_record_t`.
- Nessun cambio al percorso caldo.
- Nessun cambio alla move table o a `classify_move()`.
- Nessuna policy o decisione Agent Guard.
- Nessuna copertura di watch stale, lost-scope recovery o overflow.

## Related Docs

- [Tracepoint e Alfred Lab MVP](../../41-tracepoint-lab-roadmap-mvp.md)
- [Tracepoint Model v0](../../42-tracepoint-model-v0.md)
- [Scenario: create file](create-file.md)
- [Scenario: close-write / file-ready](file-ready.md)
- [Semantica degli eventi](../../13-semantica-eventi.md#rename-move-e-relocate)
- [Mappa codice e strutture dati](../../16-mappa-codice-e-strutture.md#mappa-tracepointlab-mvp)
- [Contratto dei log](../../22-contratto-log.md)
