# Scenario: watch stale / recovery

id: lab.fs.watch-stale-recovery.v0
title: Explain stale watches, resync and lost-scope recovery
status: stable-doc
scenario kind: diagnostic

## Goal

Questo scenario spiega il primo percorso Lab MVP in cui Alfred non sta
raccontando una normale azione filesystem dell'utente, ma l'affidabilita' del
monitoraggio stesso.

Quando un watch inotify osserva una directory e quella directory viene spostata,
cancellata o resa non piu' raggiungibile, il kernel puo' continuare a parlare
del vecchio watch descriptor. Alfred pero' non puo' piu' fidarsi del path
testuale salvato nella watcher table. Il comportamento corretto non e'
inventare un `DIR_MOVED`, un `DIR_RELOCATED`, un `FILE_DELETED` o un
`FILE_CREATED` con un path forse falso. Il comportamento corretto e':

- marcare il watch come `STALE`;
- emettere diagnostica `WATCH_*`;
- tentare resync locale quando possibile;
- accodare recovery lost-scope se il vecchio path non e' piu' verificabile;
- droppare eventi successivi sul watch stale invece di mandarli al core con un
  path inaffidabile.

Il punto didattico e' distinguere:

- eventi semantici filesystem, come `FILE_CREATED` o `DIR_MOVED`;
- diagnostica backend, come `WATCH_STALE`, `WATCH_RESYNC_FAILED` e
  `WATCH_LOST_QUEUED`;
- recovery del monitoraggio, che serve a ripristinare la fiducia nei watch, non
  a descrivere direttamente cosa ha fatto l'utente.

## Trigger

Caso osservativo minimo: spostare la root osservata e poi creare un file nel
nuovo path reale.

```text
mv watched-root watched-root-moved
touch watched-root-moved/file-after-move.txt
```

Per inotify il fatto importante e' `IN_MOVE_SELF`: segnala che l'oggetto
osservato dal watch e' stato spostato, ma non contiene la destinazione. Questo
lo rende diverso da una coppia child-level `IN_MOVED_FROM` + `IN_MOVED_TO`,
che invece puo' essere correlata dal core come rename, move o relocate.

Caso delete-self: cancellare la root osservata.

```text
rm -rf watched-root
```

In questo caso il backend puo' vedere `IN_DELETE_SELF` e poi `IN_IGNORED`.
`IN_DELETE_SELF` marca la mapping come stale; `IN_IGNORED` permette la pulizia
finale del watch.

Caso recovery completa: osservare due root configurate, spostare una directory
watched da root A a root B e lasciare che la recovery ritrovi la stessa
identita' filesystem sotto la seconda root.

```text
mv root-a/lost root-b/lost
touch root-b/lost/proof.txt
```

Questo caso mostra la differenza tra:

- il `DIR_MOVED` semantico prodotto dalla coppia parent-level move;
- la riparazione del watch figlio che ha ricevuto `IN_MOVE_SELF`.

## Expected Evidence

Evidenza minima attesa nel caso `IN_MOVE_SELF`:

- il backend legge un evento raw `IN_MOVE_SELF` per il watch osservato;
- `backend_handle_move_self()` marca il watch come `WATCHER_STATE_STALE`;
- Alfred emette `WATCH_STALE` con `reason=IN_MOVE_SELF`;
- `backend_resync_watch()` prova il resync locale;
- `backend_probe_stale_watch_identity()` emette `WATCH_RESYNC_BEGIN`;
- se il vecchio path non e' raggiungibile o non ha la stessa identita',
  Alfred emette `WATCH_RESYNC_FAILED`;
- se il risultato richiede recovery ampia, `backend_enqueue_lost_scope()`
  accoda una entry e viene emesso `WATCH_LOST_QUEUED`;
- un evento successivo sullo stesso watch stale viene trasformato in
  `WATCH_STALE_EVENT_DROPPED`, non in raw/core event;
- i record diagnostici possono attraversare i sink/output strutturati senza
  dipendere dal formatter testuale umano.

Evidenza minima attesa nel caso recovery completa:

- la lost-scope queue contiene una entry matura;
- la recovery emette `WATCH_LOST_SCAN_BEGIN` per le root candidate;
- una scansione puo' non trovare la vecchia identita' e produrre
  `WATCH_LOST_NOT_FOUND`;
- una scansione successiva puo' ritrovare la stessa identita' e produrre
  `WATCH_LOST_FOUND`;
- Alfred aggiorna path/prefissi e completa con `WATCH_LOST_RECOVERY_END`;
- eventi successivi vengono osservati sotto il path recuperato, non sotto il
  vecchio path stale.

Questa evidenza non richiede runtime trace output. I tracepoint restano nomi
`stable-doc` che spiegano il percorso.

## Expected Records

Record diagnostico per watch stale:

```text
layer=diagnostic
category=watch
type=WATCH_STALE
backend=inotify
path=*/watched-root
watch.watch_id=<wd>
watch.state=stale
watch.reason=IN_MOVE_SELF
```

Record diagnostico per evento droppato su watch stale:

```text
layer=diagnostic
category=watch
type=WATCH_STALE_EVENT_DROPPED
backend=inotify
path=*/watched-root
watch.watch_id=<wd>
watch.event_mask includes IN_CREATE
watch.event_name=file-after-move.txt
```

Record diagnostici per resync locale:

```text
layer=diagnostic
category=recovery
type=WATCH_RESYNC_BEGIN
backend=inotify
path=*/watched-root
watch.watch_id=<wd>
watch.reason=IN_MOVE_SELF
```

```text
layer=diagnostic
category=recovery
type=WATCH_RESYNC_FAILED
backend=inotify
path=*/watched-root
watch.watch_id=<wd>
watch.reason=IN_MOVE_SELF
watch.error=path-unreachable
```

Record diagnostico per handoff alla lost-scope queue:

```text
layer=diagnostic
category=recovery
type=WATCH_LOST_QUEUED
backend=inotify
path=*/watched-root
watch.watch_id=<wd>
watch.reason=IN_MOVE_SELF
watch.error=path-unreachable
recovery.pending_count>=1
```

Record diagnostici rappresentativi per recovery completa:

```text
type=WATCH_LOST_SCAN_BEGIN
type=WATCH_LOST_NOT_FOUND
type=WATCH_LOST_FOUND
type=WATCH_LOST_PREFIX_UPDATED
type=WATCH_LOST_REINSTALLED
type=WATCH_LOST_RECOVERY_END
```

Questi record spiegano affidabilita' e recovery del monitoraggio. Non sono
sostituti di `FILE_*` o `DIR_*` semantic records.

## Logical Tracepoints

Tracepoint `stable-doc` attraversati:

```text
BACKEND_RAW_EVENT_READ
WATCH_DIAGNOSTIC_EMITTED
SINK_RECORD_EMITTED
```

Significato:

- `BACKEND_RAW_EVENT_READ`: il backend ha letto un self-event inotify, per
  esempio `IN_MOVE_SELF`, `IN_DELETE_SELF` o `IN_UNMOUNT`;
- `WATCH_DIAGNOSTIC_EMITTED`: Alfred ha prodotto diagnostica sullo stato o la
  recovery del watch;
- `SINK_RECORD_EMITTED`: un sink ha ricevuto il record diagnostico da emettere.

`RAW_EVENT_NORMALIZED` e `CORE_SEMANTIC_EVENT_EMITTED` non sono il centro di
questo scenario. Proprio questa e' la lezione: quando il path del watch non e'
piu' affidabile, Alfred deve fermare il percorso raw/core e produrre
diagnostica invece di semantica falsa.

## Function Path

Percorso principale per `IN_MOVE_SELF` oggi:

```text
inotify_backend_poll()
-> backend_poll()
-> backend_handle_move_self()
-> watcher_set_state(..., WATCHER_STATE_STALE)
-> backend_log_watch_stale()
-> backend_log_watch_diagnostic_record()
-> alfred_record_build_watch_diagnostic_with_os_error()
-> alfred_record_sink_emit()
-> backend_resync_watch()
-> backend_probe_stale_watch_identity()
-> backend_log_resync_record()
-> backend_log_resync_failure()
-> backend_enqueue_lost_scope()
-> backend_log_lost_scope_record()
```

Percorso per eventi successivi su watch stale:

```text
backend_poll()
-> watcher_get_state(..., wd)
-> backend_log_stale_event_dropped()
-> alfred_record_build_stale_event_dropped()
-> alfred_record_sink_emit()
```

Percorso per recovery lost-scope:

```text
inotify_backend_poll()
-> backend_lost_scope_process_due_runtime()
-> backend_lost_scope_process_due_with_ops()
-> backend_lost_scope_recover_entry_with_ops()
-> backend_log_lost_scope_record()
-> watcher_update_path_prefix()
-> backend_lost_scope_reinstall_missing_watches()
-> backend_log_lost_scope_record(... WATCH_LOST_RECOVERY_END ...)
```

Responsabilita' dei passaggi:

- `inotify_backend_poll()` e `backend_poll()` leggono e iterano gli eventi
  inotify disponibili;
- `backend_handle_move_self()` intercetta `IN_MOVE_SELF` prima che il core veda
  un path potenzialmente falso;
- `watcher_set_state()` cambia la watcher table da `VALID` a `STALE`;
- `backend_log_watch_stale()` emette `WATCH_STALE`;
- `backend_log_watch_diagnostic_record()` costruisce il record diagnostico,
  scrive il log compatibile e poi offre lo stesso record borrowed a
  `ctx->emit_record`;
- `backend_resync_watch()` orchestra il probe locale e l'eventuale handoff alla
  recovery ampia;
- `backend_probe_stale_watch_identity()` confronta raggiungibilita' e identita'
  `(st_dev, st_ino)` del vecchio path;
- `backend_log_resync_record()` emette diagnostica ricca `WATCH_RESYNC_*`;
- `backend_log_resync_failure()` normalizza le failure del probe in
  `WATCH_RESYNC_FAILED`;
- `backend_enqueue_lost_scope()` salva una entry di recovery se il watch non
  puo' essere validato localmente;
- `backend_log_lost_scope_record()` emette diagnostica `WATCH_LOST_*`;
- `backend_log_stale_event_dropped()` preserva evidenza di eventi arrivati su
  watch stale senza inoltrarli come raw/core event.

## State Changes

Stato rilevante:

- `watcher_table_t` mantiene mapping `wd -> path`, identita' filesystem e stato
  del watch;
- `WATCHER_STATE_VALID` significa che Alfred considera affidabile la mapping;
- `WATCHER_STATE_STALE` significa che il kernel puo' ancora parlare del watch,
  ma Alfred non si fida piu' del path salvato;
- `WATCHER_STATE_RESYNCING` puo' essere usato durante un tentativo di
  validazione/riparazione;
- una recovery riuscita riporta il watch a `WATCHER_STATE_VALID`;
- la lost-scope queue conserva path, root candidate, reason, retry e identita'
  necessarie a cercare la stessa directory in un secondo momento;
- i record diagnostici restano borrowed quando attraversano `emit_record`; un
  output queue deve clonarli se deve conservarli oltre il callback.

La regola di sicurezza del modello e': un path stale non deve diventare
semantica core. La diagnostica dice "il monitoraggio ha perso fiducia", non "il
filesystem ha sicuramente fatto questa azione utente".

## Expected Outputs

Output compatibili osservabili oggi per il caso move-self:

```text
raw.log:
IN_MOVE_SELF ... path=*/watched-root name=
```

```text
events.log:
WATCH_STALE wd=<wd> path=*/watched-root reason=IN_MOVE_SELF
WATCH_RESYNC_BEGIN wd=<wd> path=*/watched-root reason=IN_MOVE_SELF
WATCH_RESYNC_FAILED wd=<wd> path=*/watched-root reason=IN_MOVE_SELF error=path-unreachable
WATCH_LOST_QUEUED wd=<wd> path=*/watched-root reason=IN_MOVE_SELF error=path-unreachable pending=1
WATCH_STALE_EVENT_DROPPED wd=<wd> path=*/watched-root mask=*IN_CREATE* name=file-after-move.txt
```

Output semanticamente vietati nello stesso caso:

```text
DIR_RELOCATED
DIR_MOVED
DIR_RENAMED
FILE_CREATED path=*/watched-root/file-after-move.txt
core seq=
```

Con output JSONL abilitato, `output.jsonl` puo' contenere record strutturati
per `WATCH_STALE`, `WATCH_RESYNC_FAILED`, `WATCH_LOST_QUEUED`,
`WATCH_LOST_FOUND` e `WATCH_LOST_RECOVERY_END`, a seconda dello scenario.

Il Lab non deve dipendere dall'intera riga testuale. Deve usare questi output
come evidenza del comportamento, non come unico contratto fragile.

## Tests

existing:

- `tests/backend/test_self_events_root_watch.sh`
- `tests/backend/test_delete_self_nested_watch.sh`
- `tests/backend/test_watch_stale_output_failure.c`
- `tests/backend/test_resync_output_routing.c`
- `tests/backend/test_self_move_identity_match.sh`
- `tests/backend/test_self_move_identity_mismatch.sh`
- `tests/backend/test_lost_scope_queue.sh`
- `tests/backend/test_lost_scope_recovery.sh`
- `tests/backend/test_lost_scope_runtime_recovery.sh`
- `tests/jsonl/test_lost_scope_runtime_recovery_jsonl.sh`

missing:

- nessun test specifico sul file Markdown dello scenario;
- nessun test parser, perche' il formato scenario v0 non e' ancora parsato.

future:

- test focused se in futuro gli scenari Lab diventano input macchina;
- test focused se `WATCH_DIAGNOSTIC_EMITTED` diventa output trace pubblico;
- test Lab specifico che visualizzi la differenza tra diagnostica watch e
  semantica filesystem senza leggere direttamente i log compatibili.

## Common Failures

- Trasformare `IN_MOVE_SELF` in `DIR_MOVED` o `DIR_RELOCATED`: `IN_MOVE_SELF`
  non contiene il nuovo path.
- Trattare `IN_DELETE_SELF` come prova sufficiente per inventare delete
  semantici dei figli: i child delete devono arrivare da fatti child reali.
- Inoltrare eventi arrivati su un watch stale al core usando il vecchio path:
  quel path non e' piu' affidabile.
- Confondere `WATCH_RESYNC_FAILED` con un fallimento dell'utente: e' una
  failure del tentativo di recovery del monitoraggio.
- Pensare che `WATCH_LOST_QUEUED` significhi recovery gia' riuscita: significa
  solo che Alfred ha registrato lavoro di recovery differito.
- Aspettarsi sempre `WATCH_LOST_FOUND`: se la directory non e' piu' presente o
  non e' dentro una root candidata, il risultato corretto puo' essere
  `WATCH_LOST_NOT_FOUND` o un retry.
- Dipendere dall'intera riga di `events.log`: il formato umano puo' cambiare
  piu' facilmente del record strutturato.
- Pensare che `SINK_RECORD_EMITTED` significhi flush durable su disco: indica
  consegna al sink, non garanzia di persistenza finale.

## Non-goals

- Nessun runtime trace output.
- Nessun parser scenario.
- Nessuna UI Lab.
- Nessun cambio a `alfred_record_t`.
- Nessun cambio al percorso caldo.
- Nessuna nuova policy di recovery.
- Nessun nuovo comportamento inotify.
- Nessuna promessa di blocco o enforcement Agent Guard.
- Nessuna copertura completa di overflow, recursive mkdir o output pipeline
  `queue -> dispatcher -> sink`.

## Related Docs

- [Tracepoint e Alfred Lab MVP](../../41-tracepoint-lab-roadmap-mvp.md)
- [Tracepoint Model v0](../../42-tracepoint-model-v0.md)
- [Mappa codice e strutture dati](../../16-mappa-codice-e-strutture.md#mappa-tracepointlab-mvp)
- [Scenari di test](../../14-scenari-test.md)
- [Contratto dei log](../../22-contratto-log.md)
