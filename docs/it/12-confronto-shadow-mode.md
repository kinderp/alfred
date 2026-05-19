# Confronto shadow mode

Questo capitolo spiega come confrontare l'output del vecchio dispatcher inotify
con l'output del nuovo core mentre il progetto e' in shadow mode.

## Perche' confrontare i due output

In shadow mode lo stesso evento percorre due strade:

```text
legacy path:
struct inotify_event -> events.c -> logger_event()

core path:
struct inotify_event -> inotify_adapter -> alfred_process()
                    -> core_logger_on_event() -> logger_event()
```

Il vecchio path resta il comportamento ufficiale. Il nuovo path serve a capire
se il core interpreta gli eventi nello stesso modo, meglio o peggio.

## Tool diagnostico

Il tool si trova in:

```text
tests/shadow/compare_shadow_output.py
```

Esegue uno scenario piccolo, legge `events.log`, separa righe legacy e righe
core, normalizza i path temporanei e stampa le differenze.

## Prerequisito

Prima compila il progetto:

```bash
make
```

Il tool cerca il binario:

```text
./alfred
```

Se hai modificato header importanti, per esempio `app/include/app.h`, puoi fare
una rebuild completa:

```bash
make re
```

Il Makefile ora traccia le dipendenze dagli header con file `.d`, quindi `make`
dovrebbe ricompilare automaticamente i file toccati. `make re` resta utile se
vuoi eliminare ogni dubbio durante una sessione di debug.

## Scenari disponibili

Per vedere gli scenari:

```bash
python3 tests/shadow/compare_shadow_output.py --help
```

Scenari iniziali:

```text
create_file
delete_file
rename_file
```

## Esempio di uso

```bash
python3 tests/shadow/compare_shadow_output.py create_file
```

Output tipico:

```text
Scenario: create_file

Legacy:
  FILE_CREATED path=$ROOT/a.txt

Core:
  FILE_CREATED path=$ROOT/a.txt

Only in legacy:
  <none>

Only in core:
  <none>
```

In questo esempio legacy e core coincidono.

## Modalita' diagnostica e modalita' strict

Per default il tool non fallisce se trova differenze. Serve a osservare.

Se vuoi usarlo come test bloccante:

```bash
python3 tests/shadow/compare_shadow_output.py create_file --strict
```

Con `--strict`, il comando restituisce errore se legacy e core differiscono.

In questa fase iniziale e' meglio non usare `--strict` nei test ufficiali,
perche' sappiamo gia' che alcune differenze sono attese.

## Conservare i log

Per copiare i log dell'ultima esecuzione:

```bash
python3 tests/shadow/compare_shadow_output.py create_file --keep-logs
```

I file vengono copiati in:

```text
tests/shadow/last-run/
```

Questa cartella e' utile per ispezionare manualmente `events.log`, `raw.log` ed
eventuali errori.

## Come il tool normalizza gli eventi

Il tool trasforma path temporanei in `$ROOT`.

Esempio:

```text
/tmp/alfred_shadow_abc123/watched/a.txt
```

diventa:

```text
$ROOT/a.txt
```

Questo rende confrontabili run diversi.

Il tool riconosce due formati.

Legacy:

```text
FILE_CREATED path=/tmp/a.txt
FILE_RENAMED from=/tmp/a.txt to=/tmp/b.txt
```

Core:

```text
core seq=1 type=FILE_CREATED path=/tmp/a.txt pid=0
core seq=2 type=FILE_RENAMED from=/tmp/a.txt to=/tmp/b.txt pid=0
```

Entrambi vengono convertiti in una forma comune.

## Come leggere le differenze

`Only in legacy` significa:

```text
il vecchio dispatcher ha prodotto un evento che il core non ha prodotto
```

`Only in core` significa:

```text
il core ha prodotto un evento che il vecchio dispatcher non ha prodotto
```

Una differenza puo' indicare:

- bug nel core
- bug nel vecchio dispatcher
- comportamento intenzionalmente diverso
- evento non ancora supportato
- scenario troppo rumoroso

## Strategia consigliata

1. Parti da scenari piccoli.
2. Leggi legacy e core.
3. Guarda `Only in legacy`.
4. Guarda `Only in core`.
5. Decidi se la differenza e' attesa.
6. Documenta le differenze ricorrenti.
7. Solo dopo rendi un confronto `--strict`.

## Risultati iniziali osservati

I primi scenari minimi coincidono:

| Scenario | Stato |
| --- | --- |
| `create_file` | legacy e core coincidono |
| `delete_file` | legacy e core coincidono |
| `rename_file` | legacy e core coincidono |

Questo non significa che il core sia gia' pronto a sostituire il vecchio
dispatcher. Significa solo che i casi base sono allineati.

## Prossimi scenari da aggiungere

Scenari utili:

- create directory
- delete directory
- move file
- move and rename file
- rename directory
- recursive create
- queue overflow, se riproducibile

Ogni scenario dovrebbe essere piccolo e leggibile.
