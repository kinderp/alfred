# Audit notturno 2026-07-01

Questo report documenta una sessione di audit esplorativo notturno eseguita su
`main` dopo il lavoro recente su Backend API v0, lifecycle wiring e Writer
Runtime. L'obiettivo non era fare benchmark, ma usare Alfred come utente e
verificare che gli scenari gia' esplorati e alcuni casi mirati non mostrassero
regressioni visibili.

## Traccia GitHub

| Elemento | Link |
| --- | --- |
| Issue madre | [#68 Nightly exploratory audit: 2026-07-01 Backend API v0 regression](https://github.com/kinderp/alfred/issues/68) |
| Artifact completi | [Google Drive archive](https://drive.google.com/open?id=1CK9nHEJM8zwh-X_80FWjR-97_3VDDH3b) |
| Known failure ancora aperto | [#30 Alfred starts event loop when root path is a regular file](https://github.com/kinderp/alfred/issues/30) |

## Ambiente

| Campo | Valore |
| --- | --- |
| Branch | `main` |
| Commit validato | `09409b7` |
| Build | `make all` passato |
| Tipo audit | scenario-based exploratory audit |

## Artifact locali

Gli artifact completi sono stati compressi in:

```text
/tmp/alfred-nightly-audit-2026-07-01.zip
```

La directory sorgente locale era:

```text
/tmp/alfred-nightly-audit-2026-07-01/
```

Conteneva le cartelle dei run principali:

| Cartella | Scenario |
| --- | --- |
| `alfred-nightly-base-jsonl-lifecycle.1prHJb` | lifecycle base con JSONL |
| `alfred-nightly-moves-jsonl.O5atno` | rename/move/relocate con JSONL |
| `alfred-nightly-lost-scope-jsonl.2JMhmh` | lost-scope recovery con JSONL |
| `alfred-nightly-audit-raw-events.Nxytr2` | eventi raw opt-in |
| `alfred-nightly-recursive-fast-mkdir-p.Bo8AZR` | `mkdir -p` ricorsivo veloce |
| `alfred-nightly-root-file-should-fail.T9yYUU` | known failure #30 |
| `alfred-nightly-audit-2026-07-01-output-disabled` | output strutturato disabilitato |
| `alfred-nightly-audit-2026-07-01-invalid-output-format` | formato output invalido |
| `alfred-nightly-audit-2026-07-01-light-burst-jsonl` | burst leggero di 80 file con JSONL |

## Scenari eseguiti

La suite esplorativa base ha prodotto:

```text
PASS base-jsonl-lifecycle
PASS moves-jsonl
PASS lost-scope-jsonl
PASS audit-raw-events
PASS recursive-fast-mkdir-p
SKIP root-file-should-fail.sh (known issue #30)
```

Il known failure e' stato poi lanciato esplicitamente:

```text
FAIL root-file-should-fail
```

Il fallimento e' lo stesso gia' tracciato nella issue `#30`: una root che e' un
file regolare non fallisce subito in startup, ma avvia l'event loop e termina
per timeout nello scenario di audit.

## Scenari mirati aggiunti

### Output disabilitato

Scenario: `output_enabled=false`.

Esito:

```text
PASS output-disabled
```

Osservazioni:

- `events.log` compatibile resta attivo;
- `output.jsonl` non viene creato;
- `FILE_CREATED` e `FILE_DELETED` compaiono nel log eventi.

Questo conferma che il percorso compatibile testuale resta disponibile anche
quando la pipeline di output strutturato e' disabilitata.

### Formato output invalido

Scenario: `output_enabled=true` con `output_format=text`, che oggi non e' un
formato valido per la pipeline strutturata.

Esito:

```text
PASS invalid-output-format
```

Osservazioni:

- Alfred termina con exit status `1`;
- non entra nell'event loop;
- il failure mode e' coerente con la validazione della configurazione.

### Burst leggero JSONL

Scenario: creazione di 80 file con JSONL abilitato.

Esito:

```text
PASS light-burst-jsonl
```

Valori osservati:

```text
FILE_CREATED=80
JSONL_LINES=721
```

`output.jsonl` e' stato validato riga per riga come JSONL. Il primo controllo
manuale aveva usato `python3 -m json.tool output.jsonl`, ma quel comando e'
adatto a un singolo documento JSON e non a un file JSONL composto da un oggetto
JSON per riga. L'artifact contiene `audit-note.txt` con questa correzione.

## Risultato

Non sono stati trovati nuovi bug confermati.

La regressione gia' nota sulla root file resta aperta in `#30`. Gli scenari
eseguiti indicano che il lavoro recente su Backend API v0 e Writer Runtime non
ha rotto in modo visibile:

- log testuali compatibili;
- output JSONL;
- semantica rename/move/relocate;
- lost-scope recovery;
- raw audit opt-in;
- configurazione `output_enabled`;
- validazione di `output_format`.

## Impatto sulla matrice di maturita'

Questo audit aggiorna la freschezza della validazione per gli scenari provati
sul commit `09409b7`. Non basta pero' per promuovere le funzionalita' ad alta
maturita': manca ancora varieta' piu' ampia, ripetizione su piu' notti,
misurazione di performance reale e copertura ufficiale per tutti gli scenari.

La matrice aggiornata e i dati manuali sono in:

- [maturity-matrix.md](maturity-matrix.md)
- [maturity-data-template.csv](maturity-data-template.csv)

## Prossimi audit suggeriti

- Riprovare `root-file-should-fail.sh` dopo il fix di `#30`.
- Aggiungere scenari negativi su configurazioni output invalide diverse.
- Separare un audit dedicato a JSONL burst piu' ampio da un vero benchmark.
- Ripetere lost-scope recovery dopo modifiche a watcher table, scanner o
  Backend API v0.
