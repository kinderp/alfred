# Stato documentazione didattica

Questo file traccia lo stato dei capitoli in `docs/it`.

Stati usati:

- `Completo`: capitolo leggibile e gia' utile agli studenti.
- `Parziale`: capitolo presente ma da espandere.
- `Da fare`: capitolo ancora vuoto o solo pianificato.

| Stato | Capitolo |
| --- | --- |
| Completo | `README.md` |
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
| Parziale | `11-come-contribuire.md` |
| Completo | `12-confronto-shadow-mode.md` |
| Completo | `13-semantica-eventi.md` |
| Completo | `14-scenari-test.md` |
| Parziale | `glossario.md` |

## Aggiornamenti recenti

- `10-debugging-test-e-strumenti.md`: aggiunta spiegazione dei test shadow e
  dell'uso prudente di `--strict`.
- `12-confronto-shadow-mode.md`: aggiunto lo scenario `move_rename_dir` e la
  differenza attesa tra legacy e core.
- `13-semantica-eventi.md`: chiarito che `RELOCATED` e' un evento unico sia per
  file sia per directory.
- `10-debugging-test-e-strumenti.md`: aggiunto riferimento allo scenario
  diagnostico `recursive_create_nested_dir`.
- `12-confronto-shadow-mode.md`: aggiunto lo scenario
  `recursive_create_nested_dir` per osservare il bug delle directory create
  prima dell'aggiunta dei watch ricorsivi.
- `12-confronto-shadow-mode.md`: documentato l'uso di `--keep-logs` per
  distinguere eventi semantici, raw log e diagnostica backend come
  `WATCH_ADDED`.
- `13-semantica-eventi.md`: documentata la semantica desiderata per creazioni
  ricorsive veloci come `mkdir -p`.
- `tests/shadow/compare_shadow_output.py`: ampliato l'help del comando con note
  su modalita' diagnostica, `--strict` e `--keep-logs`.
- `14-scenari-test.md`: aggiunto capitolo dedicato agli scenari funzionali e
  shadow, con operazioni filesystem, raw log atteso, event log atteso e note
  sulle differenze semantiche.
