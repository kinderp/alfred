# Audit notturno 2026-06-25

Questo report documenta la prima tornata di audit esplorativo notturno fatta su
Alfred. L'obiettivo era usare il programma come un utente, confrontare i log con
il contratto documentato e lasciare traccia su GitHub di eventuali bug.

## Riferimenti GitHub

| Tipo | Link |
| --- | --- |
| Issue madre | <https://github.com/kinderp/alfred/issues/29> |
| Bug confermato | <https://github.com/kinderp/alfred/issues/30> |

La issue madre contiene il diario pubblico dell'audit. La issue figlia `#30`
contiene il bug specifico trovato durante la sessione.

## Commit e branch testati

| Campo | Valore |
| --- | --- |
| Branch locale | `main` |
| Commit locale osservato a inizio audit | `d85f9f0` |
| Build | `make all` |
| Modalita' runtime | `ALFRED_EVENT_ENGINE=core` |

## Artifact locali

Gli artifact locali sono stati creati sotto `/tmp`. Ogni run ha una cartella
separata, cosi' i log di scenari diversi non si sovrascrivono.

| Cartella | Scopo | Stato |
| --- | --- | --- |
| `/tmp/alfred-nightly-audit-2026-06-25` | Primo tentativo del runner | Scartato: bug nello script di audit, non in Alfred |
| `/tmp/alfred-nightly-audit-2026-06-25-r2` | Run principale corretto | Tutti gli scenari passati |
| `/tmp/alfred-nightly-audit-2026-06-25-extra` | Scenari aggiuntivi di configurazione e casi limite | Ha individuato il bug root-file |
| `/tmp/alfred-nightly-audit-2026-06-25-recursive-fast` | Test manuale dedicato a `mkdir -p` ricorsivo veloce | Passato |
| `/tmp/alfred-nightly-audit-2026-06-25-confirm-root-file` | Riproduzione minimale del bug root path file regolare | Bug confermato |

### File tipici dentro una cartella artifact

| File o directory | Significato |
| --- | --- |
| `alfred.conf` | Configurazione usata dallo scenario. |
| `root/` | Directory osservata da Alfred durante il test. |
| `raw.log` | Log raw compatibile/testuale degli eventi normalizzati. |
| `events.log` | Log diagnostico e semantico testuale. |
| `output.jsonl` | Output strutturato JSONL, quando `output_enabled=true`. |
| `errors.log` | Errori applicativi prodotti da Alfred. |
| `alfred.stdout` | Standard output del processo Alfred. |
| `alfred.stderr` | Standard error del processo Alfred. |
| `alfred.pid` | PID del processo Alfred avviato dallo scenario. |
| `exit.status` | Codice di uscita del comando, utile per distinguere timeout e failure. |
| `assertions.log` | Errori prodotti dallo script di audit quando un assert fallisce. |
| `summary.md` | Riepilogo automatico del run. |
| `findings.tsv` | Finding grezzi del runner, se presenti. |

La distinzione importante e':

- `raw.log`, `events.log` e `output.jsonl` servono a confrontare Alfred con il
  contratto atteso;
- `stdout`, `stderr`, `exit.status` e `assertions.log` servono a capire come si
  e' comportato il processo durante il test.

## Scenari principali

| Scenario | Cosa verifica | Esito |
| --- | --- | --- |
| `base-jsonl` | Ciclo base file e directory con raw, semantica e JSONL | PASS |
| `moves-jsonl` | Rename, move e relocate di file e directory | PASS |
| `lost-scope-jsonl` | Recovery lost-scope e routing JSONL dei record `WATCH_*` | PASS |
| `audit-raw` | Eventi raw opt-in `IN_OPEN`, `IN_ACCESS`, `IN_CLOSE_NOWRITE` | PASS |
| `invalid-config` | Configurazione output invalida deve fallire | PASS |
| `output-disabled` | Con output disabilitato non deve essere scritto JSONL | PASS |
| `disable-attrib` | `inotify_watch_mask=default,-IN_ATTRIB` disabilita `IN_ATTRIB` | PASS |
| `invalid-watch-mask` | Token invalido nella maschera deve fallire chiaramente | PASS |
| `recursive-fast` | `mkdir -p one/two/three` produce create sintetiche e watch | PASS |
| `root-file-onlydir` | Root file regolare deve fallire senza event loop | FAIL, issue `#30` |

## Bug confermato

### Root path file regolare

Scenario:

```sh
DIR=/tmp/alfred-nightly-audit-2026-06-25-confirm-root-file
rm -rf "$DIR"
mkdir -p "$DIR"
printf 'x\n' > "$DIR/not-a-dir.txt"
printf 'output_enabled=false\n' > "$DIR/alfred.conf"

(
  cd "$DIR" || exit 1
  timeout 3s env \
    ALFRED_CONFIG="$DIR/alfred.conf" \
    ALFRED_EVENT_ENGINE=core \
    /lab/progetto_3a_inf/alfred "$DIR/not-a-dir.txt" \
    >alfred.stdout 2>alfred.stderr
  echo $? > exit.status
)
```

Comportamento atteso:

- Alfred deve rifiutare una root che e' un file regolare;
- non deve entrare nell'event loop;
- non deve produrre un falso `WATCH_ADDED`;
- deve spiegare l'errore in modo controllato.

Comportamento reale:

- il processo entra nell'event loop;
- il comando termina solo per timeout;
- `events.log` contiene startup ed event loop;
- non c'e' `WATCH_ADDED`, ma nemmeno un errore immediato.

Estratto significativo da `events.log`:

```text
logger initialized
alfred core initialized event_engine=core
watcher table initialized
inotify backend initialized fd=6
signal handlers installed
application startup complete
event loop started
event loop terminated
shutdown started
resources released
```

Questo e' stato aperto come:

```text
https://github.com/kinderp/alfred/issues/30
```

## Nota sul primo tentativo scartato

Il primo runner dell'audit aveva un problema di gestione del PID/subshell. Alcuni
scenari JSONL sembravano fallire per assenza o mancato flush di `output.jsonl`,
ma l'errore era nello script di audit, non in Alfred. Per questo motivo il run
`/tmp/alfred-nightly-audit-2026-06-25` e' stato considerato storico ma scartato
come evidenza tecnica.

Il run corretto e' `r2`.

## Cosa e' stato trasformato in materiale riproducibile

Gli scenari principali sono stati trascritti in:

```text
tests/exploratory/nightly/
```

Questi script non sono ancora test ufficiali. Servono a:

- ripetere l'audit;
- confrontare nuovi run con quelli precedenti;
- decidere quali scenari promuovere a golden test;
- riprodurre velocemente il bug `#30`.

## Prossimi audit suggeriti

- Ripetere `root-file-should-fail.sh` dopo il fix di `#30`.
- Aggiungere scenari su `IN_DELETE_SELF` e `IN_UNMOUNT`.
- Provare sequenze lunghe di rename/move su directory con sottoalberi.
- Confrontare `events.log` e `output.jsonl` per ogni scenario JSONL golden.
- Raccogliere benchmark separati solo dopo aver stabilizzato gli audit
  funzionali esplorativi.
