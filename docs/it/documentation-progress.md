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
