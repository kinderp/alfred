# Stato documentazione didattica

Questo file traccia lo stato corrente dei capitoli in `docs/it`.

Non e' piu' un diario completo della migrazione storica. La cronologia
dettagliata dei vecchi passaggi, incluso lo shadow mode, resta recuperabile da
Git. Qui devono restare solo informazioni utili per capire quali documenti sono
attivi, quali sono incompleti e quali sono stati rimossi perche' superati.

## Stati usati

- `Completo`: capitolo leggibile e gia' utile agli studenti.
- `Parziale`: capitolo presente ma da espandere.
- `Rimosso`: capitolo eliminato perche' storico o superato.

## Capitoli attivi

| Stato | Capitolo |
| --- | --- |
| Completo | `README.md` |
| Completo | `00-regole-operative.md` |
| Completo | `01-panoramica-progetto.md` |
| Completo | `02-architettura-generale.md` |
| Parziale | `03-struttura-cartelle.md` |
| Completo | `04-livello-applicazione.md` |
| Completo | `05-modulo-inotify.md` |
| Completo | `06-core-engine.md` |
| Completo | `07-flusso-eventi.md` |
| Completo | `08-guida-c-usato-nel-progetto.md` |
| Completo | `09-makefile-e-build-system.md` |
| Completo | `10-debugging-test-e-strumenti.md` |
| Completo | `11-come-contribuire.md` |
| Completo | `13-semantica-eventi.md` |
| Completo | `14-scenari-test.md` |
| Parziale | `16-mappa-codice-e-strutture.md` |
| Parziale | `17-roadmap-documentazione-avanzata.md` |
| Parziale | `18-modello-licenze.md` |
| Parziale | `19-roadmap-cli-e-man-page.md` |
| Completo | `20-matrice-eventi-inotify.md` |
| Parziale | `21-roadmap-scanner-resync.md` |
| Parziale | `22-contratto-log.md` |
| Parziale | `23-roadmap-plugin-backend.md` |
| Parziale | `24-roadmap-ai-agent-guardrail.md` |
| Completo | `25-roadmap-unificata-dossier.md` |
| Completo | `26-stato-funzionalita.md` |
| Completo | `27-guida-lettura-documentazione.md` |
| Completo | `28-audit-documentazione-e-debiti.md` |
| Parziale | `29-event-model-v0.md` |
| Parziale | `30-backend-api-v0.md` |
| Parziale | `glossario.md` |

## Capitoli rimossi

| Capitolo | Motivo |
| --- | --- |
| `12-confronto-shadow-mode.md` | Documento storico della migrazione legacy/core. Lo shadow mode non e' piu' una modalita' runtime e non deve restare nel percorso di lettura corrente. |
| `15-todo-switch-core.md` | TODO storico dello switch al core. I debiti ancora utili sono stati migrati nella roadmap unificata, nella matrice inotify, nella roadmap scanner/resync e nei capitoli architetturali correnti. |

## Documenti tecnici fuori da `docs/it`

| Stato | Capitolo |
| --- | --- |
| Completo | `docs/code-browser/README.md` |
| Completo | `docs/sourcebot-browser/README.md` |
| Parziale | `docs/kythe-browser/README.md` |
| Parziale | `docs/man/man1/alfred.1` |
| Parziale | `docs/man/man5/alfred.conf.5` |
| Parziale | `docs/man/man7/alfred-events.7` |

## Aggiornamento recente

La pulizia corrente ha:

- rimosso i due capitoli storici sullo shadow mode e sul TODO dello switch
- aggiornato `README.md` e `docs/it/README.md`
- sostituito i riferimenti operativi a `15-todo-switch-core.md` con
  `25-roadmap-unificata-dossier.md`
- aggiornato i capitoli attivi che descrivevano ancora l'archiviazione dei
  documenti storici come prossimo passo
- mantenuto solo le note storiche necessarie a capire perche' `shadow` e'
  invalido e perche' il core e' l'unico riferimento semantico corrente

## Aggiornamento feature matrix

`26-stato-funzionalita.md` raccoglie in un unico posto le funzionalita'
supportate dal backend inotify, dal raw log, dagli `ALFRED_RAW_*`, dal core
semantico, dalla diagnostica backend e dalla lost-scope recovery. Il documento
rimanda ai test principali ed e' stato usato come base per Event Model v0. Deve
restare il controllo da leggere prima di progettare Backend API v0, JSONL o
nuovi record.

## Aggiornamento pagine man

Sono state aggiunte tre bozze di pagine man:

- `alfred(1)`: uso utente corrente, ambiente, log, limiti e esempi
- `alfred.conf(5)`: formato key-value, chiavi supportate e default
- `alfred-events(7)`: modello eventi corrente, self events, diagnostica e
  lost-scope recovery

Le pagine sono marcate come parziali perche' dovranno essere riallineate dopo
Event Model v0, Backend API v0, JSONL e futura CLI con opzioni esplicite.

## Aggiornamento guida di lettura

`27-guida-lettura-documentazione.md` e' stato aggiunto come documento d'insieme.
Non sostituisce gli altri capitoli: spiega in quale ordine leggerli, separa i
percorsi per utente, contributore, studente e sviluppatore, e fornisce un indice
tematico rapido per ritrovare argomenti come eventi, test, log, scanner,
resync, plugin backend, configurazione e roadmap.

## Aggiornamento audit documentazione

`28-audit-documentazione-e-debiti.md` raccoglie il controllo corrente su link
locali, diagrammi Mermaid, animazioni rimandate e debiti dichiarati nella
documentazione. Serve come punto di discussione per distinguere cosa e' rotto,
cosa e' volutamente rimandato e quali decisioni architetturali conviene
affrontare per prime.

## Aggiornamento Event Model v0

`29-event-model-v0.md` definisce il modello eventi approvato per il prossimo
filone architetturale: un record comune identificato da
`layer + category + type`, con `layer` e `category` controllati e `type`
controllato per ogni coppia layer/category. Il documento include campi comuni,
campi filesystem, campi watch/recovery, campi security futuri opzionali, un
diagramma Mermaid dei record implementati oggi e una tabella riassuntiva con
rimandi a matrice inotify, contratto log, semantica eventi e scenari di test.
Resta parziale perche' `alfred_record_t` esiste come contratto dati in
`core/include/alfred_record.h` e il primo adapter raw esiste in
`core/src/alfred_record_adapter.c`. Lo stesso file contiene anche l'adapter
semantico `alfred_event_t -> alfred_record_t`, usato da `core_logger_on_event()`
prima del formatter testuale. Esiste anche il builder diagnostico `WATCH_*` in
`core/src/alfred_record_diagnostic.c` e il formatter testuale in
`core/src/alfred_record_text.c`, ma mancano ancora writer JSONL e uso runtime
nel backend inotify.

## Aggiornamento Backend API v0

`30-backend-api-v0.md` definisce la proposta documentale della Backend API v0.
Il documento stabilisce lifecycle, target management, emit sink basato su Event
Model v0, capabilities, error model, ownership, mapping del backend inotify
corrente e roadmap implementativa. Il primo tipo C comune,
`core/include/alfred_record.h`, e' stato aggiunto come contratto dati. Esiste
anche l'adapter `alfred_raw_event_t -> alfred_record_t` per i record
`normalized_raw + filesystem + RAW_*`, l'adapter semantico
`alfred_event_t -> alfred_record_t` e il builder diagnostico per i principali
record `WATCH_*`. Esiste anche `alfred_record_format_text()` per produrre il
payload testuale da record. `WATCH_ADDED`, `WATCH_REMOVED`, `WATCH_STALE` e la
famiglia locale `WATCH_RESYNC_*` sono i primi log diagnostici backend che il
runtime costruisce come record e formatta poi come testo compatibile. Il documento
include uno schema Mermaid della pipeline C introdotta finora. La policy Event
Model v0 per errori OS ora distingue `error`, `os_error_code`,
`os_error_name` e `os_error_message`, e la struttura C `alfred_record_t`
contiene il payload `os_error`. Il builder diagnostico puo' gia' popolare
questi campi tramite `alfred_record_build_watch_diagnostic_with_os_error()`.
Il formatter testuale puo' gia' renderli nella forma compatibile
`errno=N (...)`. Il runtime inotify usa gia' questo percorso per
`WATCH_RESYNC_FAILED` con `errno`, conservando codice OS e messaggio nel record.
Resta parziale perche' manca ancora un vero `emit(record)` comune e il raw path
runtime non e' ancora migrato. I diagnostici runtime `WATCH_RESYNC_*` e
`WATCH_LOST_*` usano pero' gia' record Event Model v0 e formatter testuale
compatibile.
