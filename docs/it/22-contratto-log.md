# Contratto dei log di Alfred

Questo capitolo e' la reference centrale per capire cosa significano le righe
che Alfred scrive nei file di log.

Il punto importante e' che non tutte le righe di log hanno lo stesso livello.
Alcune descrivono fatti del kernel, altre stato interno del backend, altre
eventi raw consegnati al core e altre ancora eventi semantici per l'utente.
Confondere questi livelli porta a bug semantici: per esempio `WATCH_ADDED` non
significa `DIR_CREATED`, e `WATCH_STALE` non significa `DIR_MOVED`.

## File di log

Alfred usa tre destinazioni principali:

| File | Contenuto | Pubblico principale |
| --- | --- | --- |
| `raw.log` | eventi tecnici osservati dal backend inotify | sviluppatori backend, test diagnostici |
| `events.log` | diagnostica backend e eventi semantici core | test, sviluppatori, futuri utenti |
| `errors.log` | errori di configurazione, I/O o runtime | debug e manutenzione |

`events.log` oggi contiene due famiglie diverse:

- log diagnostici del backend, come `WATCH_ADDED` o `WATCH_RESYNC_BEGIN`
- eventi semantici del core, come `FILE_CREATED` o `DIR_RELOCATED`

Questa convivenza e' intenzionale durante la fase didattica e di integrazione,
ma deve essere letta con attenzione. La prima parola della riga e' parte del
contratto: dice a quale famiglia appartiene il messaggio.

## Livelli del contratto

```text
Linux inotify event
-> backend raw log
-> ALFRED_RAW_* event
-> core semantic event
-> events.log
```

Non tutti gli eventi attraversano tutti i livelli.

Esempi:

- `IN_CREATE` puo' diventare `ALFRED_RAW_CREATE` e poi `FILE_CREATED`
- `IN_CREATE | IN_ISDIR` puo' diventare `ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR`
  e poi `DIR_CREATED`
- `IN_MOVE_SELF` oggi resta diagnostica backend: produce `WATCH_STALE`, non
  produce `ALFRED_RAW_MOVED_*` e non produce `DIR_MOVED`
- `WATCH_ADDED` descrive un nuovo watch inotify, non una directory creata

## Testo oggi, protocollo domani

Oggi questa reference descrive soprattutto righe testuali scritte nei file di
log. Questo non significa che il contratto di Alfred debba restare per sempre
testuale.

In futuro Alfred potrebbe inviare gli stessi fatti a un altro device di uscita:

- socket locale o di rete
- pipe binaria
- shared memory
- formato MessagePack
- formato Protocol Buffers
- scrittura diretta di strutture compatte definite da Alfred

Il punto architetturale e' che il contratto non deve essere "parse di stringhe".
Il contratto deve essere il fatto strutturato:

```text
kind       = WATCH_RESYNC_FAILED
wd         = 7
path       = /tmp/root
reason     = IN_MOVE_SELF
error      = identity-mismatch
```

La riga testuale:

```text
WATCH_RESYNC_FAILED wd=7 path=/tmp/root reason=IN_MOVE_SELF error=identity-mismatch
```

e' solo una serializzazione leggibile di quel fatto.

Questa distinzione sara' importante quando introdurremo output performanti. Il
backend e il core dovranno produrre eventi strutturati; poi un writer potra'
scegliere se serializzarli come testo, MessagePack, Protobuf o altro formato.
In quel modello:

- i test testuali continueranno a essere utili per didattica e debug
- il protocollo binario dovra' avere campi equivalenti ai log documentati qui
- non dovremo convertire una stringa testuale in binario
- dovremo invece serializzare direttamente lo stesso evento strutturato verso
  destinazioni diverse

Questa reference quindi va letta come contratto semantico dei campi, non solo
come formato definitivo dei file `.log`.

## Diagnostica backend dei watch

Questi log descrivono lo stato del backend inotify e della watcher table. Non
sono eventi semantici del core.

| Log | Prodotto da | Quando appare | Significato | Cosa non significa | Test principali |
| --- | --- | --- | --- | --- | --- |
| `WATCH_ADDED wd=N path=P` | `watch_manager_add()` | dopo un `inotify_add_watch()` riuscito | Alfred sta osservando `P` con il watch descriptor `N` | non significa `DIR_CREATED`; una directory puo' esistere gia' e ricevere solo ora un watch | `test_watch_added_create_dir.sh`, `test_recursive_slow_watch_tree.sh` |
| `WATCH_REMOVED wd=N path=P` | `watch_manager_remove()` | dopo rimozione esplicita o cleanup da `IN_IGNORED` | Alfred non osserva piu' `P` con quel watch | non significa `DIR_DELETED`; la rimozione del watch e la cancellazione del path sono fatti diversi | `test_watch_removed_delete_dir.sh`, `test_self_events_root_watch.sh` |
| `WATCH_STALE wd=N path=P reason=R` | backend self-event handling | dopo `IN_MOVE_SELF` o `IN_DELETE_SELF` sul path osservato direttamente | il mapping `wd -> path` non e' piu' affidabile e non va usato come se fosse valido | non significa delete, move o rename semantico; e' stato interno del backend | `test_self_events_root_watch.sh`, `test_self_move_identity_match.sh`, `test_self_move_identity_mismatch.sh` |

## Diagnostica backend del resync

Questi log descrivono il tentativo di recuperare fiducia dopo che un watch e'
diventato `STALE`.

| Log | Quando appare | Significato | Cosa non significa |
| --- | --- | --- | --- |
| `WATCH_RESYNC_BEGIN wd=N path=P reason=R` | Alfred entra nella procedura di recovery per un watch stale | da questo punto il backend sta verificando se `P` puo' tornare affidabile | non significa che il resync riuscira' |
| `WATCH_RESYNC_SCAN_DONE wd=N path=P reason=R dirs=D watched=W missing=M` | il path e' stato provato affidabile e lo scanner directory-only ha finito | `D` directory viste, `W` gia' coperte da watch, `M` mancanti | non e' evento core e non indica ancora che i watch siano stati reinstallati |
| `WATCH_RESYNC_SCAN_CLASS wd=N path=P reason=R result=X` | dopo `WATCH_RESYNC_SCAN_DONE` | classificazione leggibile dei contatori: `scan-empty`, `scan-covered`, `needs-reinstall` | non modifica da sola la watcher table |
| `WATCH_RESYNC_SCAN_MISSING wd=N path=P reason=R missing_path=Q` | per ogni directory vista dallo scan ma non coperta da watch | `Q` e' candidata a ricevere un nuovo watch | non e' `DIR_CREATED`; descrive un buco di copertura watch |
| `WATCH_RESYNC_REINSTALLED wd=N path=P reason=R installed_path=Q` | dopo `watch_manager_add()` riuscito per un missing path | Alfred ha reinstallato un watch su `Q` durante il resync | non e' evento semantico; e' riparazione della copertura backend |
| `WATCH_RESYNC_REINSTALL_FAILED wd=N path=P reason=R missing_path=Q` | `watch_manager_add()` fallisce durante il resync | la riparazione completa non e' riuscita | non significa che `Q` sia stato cancellato; indica fallimento tecnico del watch |
| `WATCH_RESYNC_FAILED wd=N path=P reason=R error=E` | recovery interrotta o non affidabile | il watch resta `STALE`; `E` spiega il ramo fallito | non e' evento utente; e' diagnostica backend |
| `WATCH_RESYNC_END wd=N path=P reason=R result=valid` | recovery riuscita | tutti i controlli necessari sono passati e il watch torna `VALID` | non implica create/move/delete; indica solo affidabilita' ripristinata |

### Errori di `WATCH_RESYNC_FAILED`

| `error=` | Significato |
| --- | --- |
| `missing-watch` | il `wd` non esiste piu' nella watcher table |
| `not-stale` | la recovery e' stata chiamata su un watch non stale |
| `set-resyncing-failed` | non e' stato possibile entrare nello stato `RESYNCING` |
| `path-unreachable` | il vecchio path non e' raggiungibile con `stat(2)` |
| `not-directory` | il vecchio path esiste ma non e' una directory |
| `set-stale-failed` | Alfred non e' riuscito a lasciare il watch in `STALE` |
| `set-valid-failed` | la recovery e' riuscita ma il ritorno a `VALID` e' fallito |
| `missing-identity` | il watch non ha identita' filesystem salvata |
| `identity-mismatch` | il path esiste ma `(st_dev, st_ino)` e' diverso |
| `reinstall-failed` | almeno un missing watch non e' stato reinstallato |

## Raw Alfred

I raw Alfred sono `ALFRED_RAW_*`. Sono input del core, non eventi finali per
l'utente. Possono essere combinati con `ALFRED_RAW_ISDIR`.

| Raw | Origine tipica inotify | Significato per il core |
| --- | --- | --- |
| `ALFRED_RAW_CREATE` | `IN_CREATE` | un path e' comparso |
| `ALFRED_RAW_DELETE` | `IN_DELETE` | un path figlio e' stato cancellato |
| `ALFRED_RAW_MODIFY` | `IN_MODIFY` | contenuto file modificato |
| `ALFRED_RAW_ATTRIB` | `IN_ATTRIB` | metadati cambiati; semantica rimandata |
| `ALFRED_RAW_CLOSE_WRITE` | `IN_CLOSE_WRITE` | uno scrittore ha chiuso il file |
| `ALFRED_RAW_MOVED_FROM` | `IN_MOVED_FROM` | vecchio path di una correlazione move/rename |
| `ALFRED_RAW_MOVED_TO` | `IN_MOVED_TO` | nuovo path di una correlazione move/rename |
| `ALFRED_RAW_OVERFLOW` | `IN_Q_OVERFLOW` | la coda ha perso eventi |
| `ALFRED_RAW_ISDIR` | `IN_ISDIR` | flag: il soggetto e' una directory |

## Eventi semantici core

Questi sono gli eventi destinati all'utente o a una futura API pubblica. Sono
emessi dal core e formattati in `events.log`.

| Evento | Quando viene emesso | Significato |
| --- | --- | --- |
| `FILE_CREATED path=P` | raw create file | un nuovo file e' apparso |
| `DIR_CREATED path=P` | raw create directory | una nuova directory e' apparsa |
| `FILE_DELETED path=P` | raw delete file | un file figlio osservato e' stato cancellato |
| `DIR_DELETED path=P` | raw delete directory | una directory figlia osservata e' stata cancellata |
| `FILE_MODIFIED path=P` | raw modify file dopo debounce | contenuto file modificato |
| `FILE_READY path=P` | raw close-write file | uno scrittore ha chiuso il file dopo scrittura |
| `FILE_RENAMED from=A to=B` | moved-from/to stesso parent e basename diverso | stesso file, nuovo nome nella stessa directory |
| `DIR_RENAMED from=A to=B` | come sopra per directory | stessa directory, nuovo nome nello stesso parent |
| `FILE_MOVED from=A to=B` | parent diverso, basename uguale | stesso file spostato in altra directory |
| `DIR_MOVED from=A to=B` | come sopra per directory | stessa directory spostata in altro parent |
| `FILE_RELOCATED from=A to=B` | parent e basename diversi | file spostato e rinominato insieme |
| `DIR_RELOCATED from=A to=B` | come sopra per directory | directory spostata e rinominata insieme |
| `OVERFLOW path=` | raw overflow | lo stream e' incompleto; serve rescan/recovery |

## Regole da ricordare

- `WATCH_*` e' diagnostica backend, non semantica core.
- `ALFRED_RAW_*` e' input del core, non output utente finale.
- `FILE_*`, `DIR_*` e `OVERFLOW` sono semantica core.
- `IN_MOVE_SELF` non contiene il nuovo path: non basta per emettere
  `DIR_MOVED`, `DIR_RENAMED` o `DIR_RELOCATED`.
- `WATCH_REMOVED` non implica `DIR_DELETED`.
- `WATCH_STALE` conserva informazione per recovery; non cancella il watch dalla
  tabella.

## Rimandi

- [Semantica degli eventi](13-semantica-eventi.md)
- [Scenari di test](14-scenari-test.md)
- [Matrice eventi inotify](20-matrice-eventi-inotify.md)
- [Roadmap scanner e resync](21-roadmap-scanner-resync.md)
