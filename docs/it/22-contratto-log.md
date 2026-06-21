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

## Raw log audit inotify

Quando `inotify_audit_events` e' configurato, Alfred puo' chiedere al kernel
eventi audit come:

```text
IN_OPEN
IN_ACCESS
IN_CLOSE_NOWRITE
```

Questi eventi appaiono solo nel `raw.log` del backend inotify. Non sono ancora
`ALFRED_RAW_*`, non vengono inoltrati al core e non producono righe semantiche
in `events.log`.

Esempio di righe possibili nel `raw.log`:

```text
IN_OPEN wd=3 path=/tmp/alfred-audit-demo name=read-only.txt
IN_ACCESS wd=3 path=/tmp/alfred-audit-demo name=read-only.txt
IN_CLOSE_NOWRITE wd=3 path=/tmp/alfred-audit-demo name=read-only.txt
```

Significato:

| Raw inotify | Quando appare | Significato | Cosa non significa |
| --- | --- | --- | --- |
| `IN_OPEN` | un file o una directory viene aperto | il kernel ha osservato un'apertura | non indica creazione, modifica o intenzione dell'attore |
| `IN_ACCESS` | un file viene letto o eseguito | il kernel ha osservato accesso/lettura | non indica scrittura o modifica |
| `IN_CLOSE_NOWRITE` | un file o una directory viene chiuso senza scrittura | la sessione si e' chiusa senza write | non e' `FILE_READY` |

La regola piu' importante e':

```text
IN_CLOSE_NOWRITE != FILE_READY
```

`FILE_READY` e' prodotto da `IN_CLOSE_WRITE`, cioe' da una chiusura dopo
scrittura. Una chiusura senza scrittura puo' essere utile per audit/debug, ma
non significa che un nuovo contenuto sia pronto per essere consumato.

Contratto attuale:

- `inotify_audit_events=off` e' il default
- `inotify_audit_events=open,access,close-nowrite` rende visibili i fatti nel
  `raw.log`
- `events.log` non deve ricevere `FILE_READY` o `FILE_MODIFIED` per una lettura
  read-only
- `inotify_watch_mask=default,+IN_OPEN` resta invalido
- `inotify_audit_events=IN_OPEN` resta invalido, perche' la chiave audit usa
  nomi di policy in minuscolo

Test principali:

- `tests/backend/test_audit_kernel_events.sh`
- `tests/backend/test_audit_config_raw_log.sh`
- `tests/backend/test_audit_config_invalid_token.sh`

## Testo oggi, protocollo domani

Oggi questa reference descrive soprattutto righe testuali scritte nei file di
log. Questo non significa che il contratto di Alfred debba restare per sempre
testuale.

In futuro Alfred potrebbe inviare gli stessi fatti a un altro device di uscita:

- socket locale o di rete
- pipe binaria
- shared memory
- JSON Lines
- formato MessagePack
- formato Protocol Buffers
- scrittura diretta di strutture compatte definite da Alfred

Il punto architetturale e' che il contratto non deve essere "parse di stringhe".
Il contratto deve essere il fatto strutturato. La specifica comune di quel fatto
e' [Event Model v0](29-event-model-v0.md): un record identificato da
`layer + category + type`.

```text
layer      = diagnostic
category   = recovery
type       = WATCH_RESYNC_FAILED
watch_id   = 7
path       = /tmp/root
reason     = IN_MOVE_SELF
error      = identity-mismatch
```

La riga testuale:

```text
WATCH_RESYNC_FAILED wd=7 path=/tmp/root reason=IN_MOVE_SELF error=identity-mismatch
```

e' solo una serializzazione leggibile di quel fatto.

Il primo builder C per questa direzione e'
`alfred_record_build_watch_diagnostic()`, dichiarato in
`core/include/alfred_record_diagnostic.h`. Il builder costruisce record
`diagnostic + watch` o `diagnostic + recovery` per i principali `WATCH_*`
senza fare parsing della riga testuale. Le stringhe (`backend`, `path`,
`state`, `reason`, `error`) restano borrowed come nel resto di
`alfred_record_t`: chi vuole conservarle deve copiarle.

Il primo formatter testuale e' `alfred_record_format_text()`, dichiarato in
`core/include/alfred_record_text.h`. Questo formatter produce solo il payload:

```text
WATCH_RESYNC_FAILED wd=7 path=/tmp/root reason=IN_MOVE_SELF error=identity-mismatch
```

Quando il record diagnostico contiene anche un errore OS, il formatter mantiene
la compatibilita' con i log storici aggiungendo il suffisso testuale
`errno=N` oppure `errno=N (messaggio)`. Internamente pero' il record resta
strutturato: il codice numerico, il nome simbolico e il messaggio non sono
estratti facendo parsing della stringa.

Non produce timestamp, livello log o newline. Questi dettagli restano nel
logger, cosi' lo stesso payload puo' essere usato da test, file log, socket o
altri output device.

Questa distinzione sara' importante quando introdurremo output performanti. Il
backend e il core dovranno produrre eventi strutturati; poi un writer potra'
scegliere se serializzarli come testo, MessagePack, Protobuf o altro formato.
In quel modello:

- i test testuali continueranno a essere utili per didattica e debug
- il protocollo binario dovra' avere campi equivalenti ai log documentati qui
- non dovremo convertire una stringa testuale in binario
- dovremo invece serializzare direttamente lo stesso evento strutturato verso
  destinazioni diverse

Questa reference quindi va letta come contratto semantico dei campi testuali
oggi esistenti, non solo come formato definitivo dei file `.log`. Event Model v0
e' invece il riferimento per tradurre quei fatti in record strutturati.

### Strategia consigliata

La scelta consigliata e' progressiva:

1. definire prima un record strutturato interno, indipendente dal formato
2. mantenere il writer testuale attuale come serializzazione didattica/debug
3. aggiungere JSON Lines come primo output strutturato leggibile e testabile
4. aggiungere MessagePack quando serve un formato binario compatto e semplice
5. valutare Protocol Buffers se Alfred avra' client esterni stabili in piu'
   linguaggi e una API pubblica versionata
6. valutare socket binaria pura se le prestazioni estreme diventano un
   requisito competitivo del progetto

Questa progressione evita di bloccare l'architettura troppo presto. JSON Lines
aiuta a stabilizzare i campi; MessagePack puo' riusare quasi lo stesso modello;
Protobuf aggiunge schema e compatibilita' forte; la socket binaria pura massimizza
prestazioni e controllo, ma richiede disciplina molto maggiore su endianess,
allineamento, versioning, compatibilita' e gestione delle stringhe.

### Confronto dei formati

| Formato | Vantaggi | Svantaggi | Quando usarlo |
| --- | --- | --- | --- |
| Testo attuale | leggibile, didattico, facile da verificare con `grep` | parsing fragile, costoso, ambiguo per client esterni | debug, test shell, lezioni |
| JSON Lines | un record per riga, leggibile, facile da salvare e inviare su socket | piu' verboso, parsing piu' costoso di un binario | primo formato strutturato e testabile |
| MessagePack | compatto, veloce, conserva modello tipo dizionario | schema meno rigoroso di Protobuf se non lo imponiamo noi | output binario pragmatico |
| Protocol Buffers | schema IDL, campi numerati, compatibilita' tra versioni, molti linguaggi | toolchain e generazione codice, meno comodo in fase esplorativa | API pubblica stabile per client esterni |
| Socket binaria pura | massimo controllo, minime allocazioni, massime prestazioni potenziali | formato da progettare e mantenere, rischio bug di compatibilita' | obiettivo performance estreme o prodotto commerciale |

### Matching testo e record strutturato

Ogni riga testuale deve poter essere descritta da un record strutturato
equivalente. Il matching non deve avvenire facendo parsing della stringa: deve
avvenire perche' entrambi i writer ricevono lo stesso record.

Esempio:

```text
WATCH_RESYNC_FAILED wd=7 path=/tmp/root reason=IN_MOVE_SELF error=identity-mismatch
```

Record strutturato equivalente:

| Campo strutturato | Valore |
| --- | --- |
| `channel` | `events` |
| `category` | `backend-diagnostic` |
| `name` | `WATCH_RESYNC_FAILED` |
| `wd` | `7` |
| `path` | `/tmp/root` |
| `reason` | `IN_MOVE_SELF` |
| `error` | `identity-mismatch` |

JSON Lines possibile:

```json
{"channel":"events","category":"backend-diagnostic","name":"WATCH_RESYNC_FAILED","wd":7,"path":"/tmp/root","reason":"IN_MOVE_SELF","error":"identity-mismatch"}
```

MessagePack possibile:

```text
map {
  "channel": "events",
  "category": "backend-diagnostic",
  "name": "WATCH_RESYNC_FAILED",
  "wd": 7,
  "path": "/tmp/root",
  "reason": "IN_MOVE_SELF",
  "error": "identity-mismatch"
}
```

Socket binaria pura possibile:

```text
struct alfred_wire_record_header {
    uint16_t version;
    uint16_t type;
    uint32_t flags;
    uint64_t seq;
    uint64_t ts_ns;
    uint32_t payload_len;
};

payload:
    fixed numeric fields
    string table or length-prefixed UTF-8/byte strings
```

La socket binaria pura non dovrebbe copiare la forma testuale. Dovrebbe usare
campi numerici per `type`, `category`, `reason`, `error` quando possibile, e
stringhe length-prefixed per path o nomi non enumerabili. Questo riduce parsing,
allocazioni e dimensione del messaggio, ma richiede una specifica precisa.

### Regola architetturale

La direzione corretta e':

```text
codice backend/core
-> costruisce alfred_log_record_t
-> writer testuale serializza in riga leggibile
-> writer jsonl serializza in JSON Lines
-> writer msgpack/protobuf/binario serializza in formato efficiente
```

La direzione da evitare e':

```text
codice backend/core
-> costruisce stringa testuale
-> parser rilegge la stringa
-> convertitore produce messaggio binario
```

La seconda direzione spreca CPU, introduce ambiguita' e rende i test meno
affidabili. Se Alfred punta a prestazioni alte, il testo deve essere solo uno
dei writer, non la fonte primaria del dato.

## Diagnostica backend dei watch

Questi log descrivono lo stato del backend inotify e della watcher table. Non
sono eventi semantici del core.

| Log | Prodotto da | Quando appare | Significato | Cosa non significa | Test principali |
| --- | --- | --- | --- | --- | --- |
| `WATCH_ADDED wd=N path=P` | `watch_manager_add()` | dopo un `inotify_add_watch()` riuscito | Alfred sta osservando `P` con il watch descriptor `N` | non significa `DIR_CREATED`; una directory puo' esistere gia' e ricevere solo ora un watch | `test_watch_added_create_dir.sh`, `test_recursive_slow_watch_tree.sh` |
| `WATCH_REMOVED wd=N path=P` | `watch_manager_remove()` | dopo rimozione esplicita o cleanup da `IN_IGNORED` | Alfred non osserva piu' `P` con quel watch | non significa `DIR_DELETED`; la rimozione del watch e la cancellazione del path sono fatti diversi. Se esiste un delete semantico, deve arrivare da un raw delete reale, per esempio dal parent `IN_DELETE name=child` | `test_watch_removed_delete_dir.sh`, `test_self_events_root_watch.sh`, `test_delete_self_nested_watch.sh` |
| `WATCH_STALE wd=N path=P reason=R` | backend self-event/unmount handling | dopo `IN_MOVE_SELF`, `IN_DELETE_SELF` o `IN_UNMOUNT` sul path osservato direttamente | il mapping `wd -> path` non e' piu' affidabile e non va usato come se fosse valido | non significa delete, move o rename semantico; e' stato interno del backend. Nel caso nested, `IN_DELETE_SELF` del child non deve duplicare il `DIR_DELETED` prodotto dal parent `IN_DELETE | IN_ISDIR name=child`; nel caso `IN_UNMOUNT`, lo smontaggio non equivale a cancellazione del contenuto | `test_self_events_root_watch.sh`, `test_delete_self_nested_watch.sh`, `test_self_move_identity_match.sh`, `test_self_move_identity_mismatch.sh` |
| `WATCH_STALE_EVENT_DROPPED wd=N path=P mask=M name=Q` | `backend_poll()` | il kernel invia un evento per un `wd` ancora attivo ma marcato `STALE` | Alfred ha visto un fatto kernel, ma non lo inoltra al core perche' `P` non e' piu' affidabile | non significa che l'evento non sia accaduto; significa che Alfred non puo' costruire un raw/core event con path corretto | `test_self_events_root_watch.sh` |

`WATCH_ADDED` oggi implica anche che il path osservato e' una directory:
`watch_manager_add()` passa `IN_ONLYDIR` a `inotify_add_watch()`. Un file
passato come root non deve produrre `WATCH_ADDED`; deve fallire con diagnostica
di errore backend, come verificato da `test_onlydir_rejects_file_root.sh`.

Dal primo passo di Backend API v0, `WATCH_ADDED`, `WATCH_REMOVED` e
`WATCH_STALE` nascono come record diagnostici strutturati e attraversano gia' il
sink comune `emit(record)` prima di essere scritti come testo compatibile.

```text
watch_manager_add() / watch_manager_remove()
-> alfred_record_build_watch_diagnostic(WATCH_ADDED | WATCH_REMOVED)
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> alfred_record_format_text()
-> logger_event()

backend_handle_move_self/delete_self/unmount()
-> alfred_record_build_watch_diagnostic(WATCH_STALE, reason=R)
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> alfred_record_format_text()
-> logger_event()
```

Il payload testuale resta volutamente identico:

```text
WATCH_ADDED wd=N path=P
WATCH_REMOVED wd=N path=P
WATCH_STALE wd=N path=P reason=R
```

Questa compatibilita' e' importante per due motivi. Primo, i test e gli utenti
continuano a leggere lo stesso contratto. Secondo, il codice inizia a produrre
il dato come record Event Model v0 prima di scriverlo come testo. Quando
arriveranno JSONL, MessagePack, protobuf o socket binaria, non dovremo fare
parsing delle stringhe `WATCH_ADDED`, `WATCH_REMOVED` o `WATCH_STALE`: il
record strutturato sara' gia' il dato primario.

## Diagnostica backend del resync

Questi log descrivono il tentativo di recuperare fiducia dopo che un watch e'
diventato `STALE`.

Anche i diagnostici `WATCH_RESYNC_*` passano ora dal percorso strutturato:

```text
backend local resync
-> alfred_record_build_watch_diagnostic(...)
-> fill record.recovery.*
-> alfred_record_sink_emit()
-> alfred_record_text_sink_emit()
-> alfred_record_format_text()
-> logger_event() oppure logger_error()
```

`WATCH_RESYNC_SCAN_FAILED` conserva il canale `error.log`; gli altri record
`WATCH_RESYNC_*` restano diagnostici di `events.log`.

| Log | Quando appare | Significato | Cosa non significa |
| --- | --- | --- | --- |
| `WATCH_RESYNC_BEGIN wd=N path=P reason=R` | Alfred entra nella procedura di recovery per un watch stale | da questo punto il backend sta verificando se `P` puo' tornare affidabile | non significa che il resync riuscira' |
| `WATCH_RESYNC_SCAN_FAILED wd=N path=P reason=R rc=C` | lo scanner directory-only del resync non completa la scansione | `C` e' il codice di errore interno restituito dallo scanner | non e' evento filesystem; indica che il backend non puo' provare la copertura |
| `WATCH_RESYNC_SCAN_DONE wd=N path=P reason=R dirs=D watched=W missing=M` | il path e' stato provato affidabile e lo scanner directory-only ha finito | `D` directory viste, `W` gia' coperte da watch, `M` mancanti | non e' evento core e non indica ancora che i watch siano stati reinstallati |
| `WATCH_RESYNC_SCAN_CLASS wd=N path=P reason=R result=X` | dopo `WATCH_RESYNC_SCAN_DONE` | classificazione leggibile dei contatori: `scan-empty`, `scan-covered`, `needs-reinstall` | non modifica da sola la watcher table |
| `WATCH_RESYNC_SCAN_MISSING wd=N path=P reason=R missing_path=Q` | per ogni directory vista dallo scan ma non coperta da watch | `Q` e' candidata a ricevere un nuovo watch | non e' `DIR_CREATED`; descrive un buco di copertura watch |
| `WATCH_RESYNC_REINSTALLED wd=N path=P reason=R installed_path=Q` | dopo `watch_manager_add()` riuscito per un missing path | Alfred ha reinstallato un watch su `Q` durante il resync | non e' evento semantico; e' riparazione della copertura backend |
| `WATCH_RESYNC_REINSTALL_FAILED wd=N path=P reason=R missing_path=Q` | `watch_manager_add()` fallisce durante il resync | la riparazione completa non e' riuscita | non significa che `Q` sia stato cancellato; indica fallimento tecnico del watch |
| `WATCH_RESYNC_ROLLBACK wd=N path=P reason=R removed_wd=M` | dopo un fallimento di reinstallazione, prima di rimuovere un watch installato nello stesso tentativo | Alfred sta annullando una riparazione parziale per mantenere la policy all-or-stale | non significa che il path osservato da `M` sia stato cancellato; indica cleanup interno del tentativo di resync |
| `WATCH_RESYNC_FAILED wd=N path=P reason=R error=E` | recovery interrotta o non affidabile | il watch resta `STALE`; `E` spiega il ramo fallito | non e' evento utente; e' diagnostica backend |
| `WATCH_LOST_QUEUED wd=N path=P reason=R error=E pending=K` | il probe locale fallisce ma il backend ha ancora identita' salvata utile | Alfred ha accodato lo scope per una recovery ampia posticipata; `K` e' il numero di scope pending nella queue | non significa che il path sia stato ritrovato; non e' evento raw/core e non cambia semantica utente |
| `WATCH_LOST_QUEUE_SKIPPED wd=N path=P reason=R error=E` | Alfred vorrebbe accodare uno scope perso ma manca un dato necessario | la recovery ampia non parte per quello scope; `E` spiega il motivo, per esempio `missing-identity` | non significa che la directory sia stata cancellata; indica che manca la prova necessaria per cercarla |
| `WATCH_LOST_QUEUE_FAILED wd=N path=P reason=R error=E` | l'inserimento nella queue lost-scope fallisce | Alfred non ha registrato lavoro di recovery posticipata per quello scope | non e' evento utente; e' un errore tecnico del backend |
| `WATCH_LOST_SCAN_BEGIN root=R pending=K` | una recovery sincrona consuma una entry lost-scope e scansiona la `scan_root` salvata nella entry | Alfred sta cercando una identita' persa dentro `R`; `K` e' il numero di entry rimaste in coda | non significa che la directory sara' trovata; non installa watch |
| `WATCH_LOST_FOUND wd=N old_path=P new_path=Q reason=R` | lo scan trova una directory con la stessa identita' salvata | Alfred ha ritrovato l'oggetto a `Q`; il path trovato diventa il nuovo prefisso candidato | non significa ancora che watch mancanti o stato `VALID` siano stati aggiornati |
| `WATCH_LOST_PREFIX_UPDATED wd=N old_prefix=P new_prefix=Q children=C` | dopo `WATCH_LOST_FOUND`, la watcher table riscrive il vecchio prefisso con quello nuovo | Alfred ha riallineato il path del watch principale e di `C` watch figli gia' noti | non significa che la copertura della subtree sia completa; non installa watch mancanti e non torna a `VALID` |
| `WATCH_LOST_COVERAGE_DONE wd=N path=P reason=R dirs=D watched=W missing=M` | dopo l'aggiornamento prefissi, Alfred scansiona in modo strict la subtree ritrovata | `D` directory viste, `W` gia' presenti nella watcher table, `M` directory senza watch | non installa ancora watch; misura la copertura per la futura policy all-or-stale |
| `WATCH_LOST_COVERAGE_MISSING wd=N path=P reason=R missing_path=Q` | per ogni directory vista dallo scan lost-scope ma non coperta da watch | `Q` e' una directory reale nella subtree ritrovata che dovra' ricevere un watch in una futura reinstallazione | non e' `DIR_CREATED`; non installa ancora il watch |
| `WATCH_LOST_COVERAGE_CLASS wd=N path=P reason=R result=X` | dopo `WATCH_LOST_COVERAGE_DONE` | classificazione leggibile dei contatori: `scan-empty`, `scan-covered`, `needs-reinstall` | non cambia stato a `VALID` e non reinstalla watch |
| `WATCH_LOST_REINSTALLED wd=N path=P reason=R installed_path=Q` | dopo `watch_manager_add()` riuscito per un missing path nella lost-scope recovery | Alfred ha reinstallato un watch su `Q` durante la recovery ampia | non e' evento semantico; e' riparazione della copertura backend |
| `WATCH_LOST_REINSTALL_FAILED wd=N path=P reason=R missing_path=Q` | `watch_manager_add()` fallisce durante la lost-scope recovery | la recovery ampia non puo' completare la copertura | non significa che `Q` sia stato cancellato; indica fallimento tecnico del watch |
| `WATCH_LOST_ROLLBACK wd=N path=P reason=R removed_wd=M` | dopo un fallimento di reinstallazione lost-scope, prima di rimuovere un watch installato nello stesso tentativo | Alfred sta annullando una riparazione parziale per mantenere la policy all-or-stale | non significa che il path osservato da `M` sia stato cancellato |
| `WATCH_LOST_NOT_FOUND wd=N path=P reason=R retry=K` | lo scan non trova la stessa identita' nella root analizzata | Alfred non ha recuperato lo scope in quella root; il processore puo' ancora provare altre root configurate prima del retry | non significa che l'oggetto non esista altrove; potrebbe essere fuori dalla root scansionata |
| `WATCH_LOST_RETRY_SCHEDULED wd=N path=P reason=R result=X retry=K delay_ms=D pending=C` | una recovery lost-scope non riesce ma il budget non e' esaurito | Alfred ha rimesso la entry nella coda con un backoff dopo aver esaurito le root previste o dopo un errore tecnico; `K` e' il numero di fallimenti registrati, `D` il ritardo minimo e `C` la queue size dopo il reinserimento | non significa che esista un worker thread; oggi e' scheduling interno del processore sincrono |
| `WATCH_LOST_RECOVERY_GAVE_UP wd=N path=P reason=R result=X retries=K` | una recovery lost-scope fallisce quando il budget massimo e' stato raggiunto | Alfred smette di rischedulare quella entry e lascia il watch non affidabile | non e' un evento raw/core e non prova che la directory sia stata cancellata |
| `WATCH_LOST_RECOVERY_FAILED wd=N path=P reason=R error=scan-failed` | lo scanner fallisce durante la recovery ampia | la passata non e' affidabile e non produce recupero | non e' evento utente; indica fallimento tecnico della scansione |
| `WATCH_LOST_RECOVERY_END wd=N path=P reason=R result=valid watches=W` | identita', prefissi, scan strict e reinstallazione sono riusciti | la subtree recuperata torna affidabile e `W` watch sotto il prefisso sono stati marcati `VALID` | non implica create/move/delete; indica solo affidabilita' ripristinata |
| `WATCH_RESYNC_END wd=N path=P reason=R result=valid` | recovery riuscita | tutti i controlli necessari sono passati e il watch torna `VALID` | non implica create/move/delete; indica solo affidabilita' ripristinata |

I `WATCH_RESYNC_FAILED` logici, cioe' quelli nella forma:

```text
WATCH_RESYNC_FAILED wd=N path=P reason=R error=E
```

sono prodotti dal percorso strutturato Event Model v0:

```text
backend_log_resync_failure()
-> backend_log_watch_diagnostic_record(WATCH_RESYNC_FAILED)
-> alfred_record_build_watch_diagnostic_with_os_error(...)
-> alfred_record_format_text()
-> logger_event()
```

I fallimenti che includono anche `errno=N (...)` hanno ora un contratto dati
strutturato e un formatter compatibile. La policy Event Model v0 distingue
`error` Alfred dai campi OS `os_error_code`, `os_error_name` e
`os_error_message`. Questi campi sono presenti in `alfred_record_t`, il builder
diagnostico li popola nei `WATCH_RESYNC_FAILED` del runtime inotify e
`alfred_record_format_text()` li rende come `errno=N` o
`errno=N (messaggio)`.

`os_error_name` resta opzionale. Nel runtime corrente Alfred conserva il codice
numerico e il messaggio prodotto da `strerror(3)`; non inventa un nome simbolico
quando non ha una funzione affidabile per mapparlo.

Esempio di mapping:

```text
testo compatibile:
WATCH_RESYNC_FAILED wd=7 path=/tmp/root reason=IN_MOVE_SELF error=path-unreachable errno=2 (No such file or directory)

record strutturato:
type             = WATCH_RESYNC_FAILED
reason           = IN_MOVE_SELF
error            = path-unreachable
os_error_code    = 2
os_error_name    = ENOENT
os_error_message = No such file or directory
```

### Errori di `WATCH_RESYNC_FAILED`

| `error=` | Significato | Quando accade | Enqueue lost scope? |
| --- | --- | --- | --- |
| `path-unreachable` | il vecchio path non e' raggiungibile con `stat(2)` | la directory osservata e' stata spostata o rimossa dal path che Alfred conosceva | si', se il watch conserva identita' |
| `not-directory` | il vecchio path esiste ma non e' una directory | il path e' stato riusato da un file o da un altro tipo di oggetto non directory | si', se il watch conserva identita' |
| `identity-mismatch` | il path esiste ma `(st_dev, st_ino)` e' diverso | il vecchio nome e' stato riusato da una nuova directory diversa da quella osservata | si', se il watch conserva identita' |
| `missing-watch` | il `wd` non esiste piu' nella watcher table | il kernel/backend ha gia' rimosso il watch o la recovery arriva troppo tardi | no |
| `not-stale` | la recovery e' stata chiamata su un watch non stale | errore di flusso interno: non si dovrebbe recuperare un watch ancora affidabile | no |
| `set-resyncing-failed` | non e' stato possibile entrare nello stato `RESYNCING` | errore di bookkeeping della watcher table | no |
| `set-stale-failed` | Alfred non e' riuscito a lasciare il watch in `STALE` | errore di bookkeeping della watcher table dopo un fallimento | no |
| `set-valid-failed` | la recovery e' riuscita ma il ritorno a `VALID` e' fallito | errore di bookkeeping della watcher table dopo controlli positivi | no |
| `missing-identity` | il watch non ha identita' filesystem salvata | Alfred non ha `(st_dev, st_ino)` da usare come prova | no |
| `reinstall-failed` | almeno un missing watch non e' stato reinstallato | il path principale e' affidabile, ma la copertura watch della subtree non e' completa | no |

La colonna "Enqueue lost scope?" distingue due famiglie di fallimento.

La prima famiglia e' "path locale non affidabile, identita' salvata ancora
utile". Qui rientrano `path-unreachable`, `not-directory` e
`identity-mismatch`. In questi casi il probe locale non puo' tornare `VALID`,
ma Alfred ha ancora una prova forte dell'oggetto originale: la coppia
`(st_dev, st_ino)` salvata quando il watch era affidabile. Accodare lo scope
serve a dire: "non fidarti del vecchio path, ma prova piu' tardi a cercare
questa identita' dentro le root monitorate".

La seconda famiglia e' tecnica o non cercabile. `missing-watch`, `not-stale`,
`set-resyncing-failed`, `set-stale-failed` e `set-valid-failed` indicano
problemi di stato interno o di ordine delle chiamate. `missing-identity` dice
che manca proprio la prova con cui cercare l'oggetto. `reinstall-failed` e'
diverso: il path principale era stato gia' provato affidabile, ma Alfred non e'
riuscito a reinstallare tutti i watch figli; non e' quindi un caso di directory
persa da cercare tramite identita'.

Esempi pratici:

```bash
# path-unreachable: il path vecchio non esiste piu'
mv src source

# identity-mismatch: il path vecchio esiste, ma e' un altro inode
mv src source
mkdir src

# not-directory: il path vecchio e' stato riusato da un file
mv src source
touch src
```

In tutti e tre gli esempi Alfred non deve generare eventi semantici inventati.
Il comportamento corretto e':

```text
WATCH_STALE
WATCH_RESYNC_BEGIN
WATCH_RESYNC_FAILED ... error=<caso>
WATCH_LOST_QUEUED ... error=<caso>
```

`WATCH_LOST_QUEUED` non significa che Alfred abbia gia' ritrovato la directory.
Significa solo che ha salvato un lavoro di recovery ampia da eseguire piu'
tardi, possibilmente in batch.

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
| `ALFRED_RAW_OVERFLOW` | `IN_Q_OVERFLOW` con `wd=-1` | la coda ha perso eventi; il raw event e' globale e porta path vuoto |
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
| `OVERFLOW path=` | raw overflow | lo stream e' incompleto; serve rescan/recovery futura. Il path e' vuoto perche' l'overflow riguarda l'istanza inotify, non un singolo watch |

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
- [Roadmap plugin backend](23-roadmap-plugin-backend.md)
