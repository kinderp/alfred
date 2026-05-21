# Regole operative per continuare l'integrazione

Questo file raccoglie le regole pratiche emerse durante l'integrazione del core.
Serve come memoria di lavoro: se la sessione viene interrotta o ripresa in un
altro momento, leggere questo file prima di continuare.

## Metodo di lavoro

- Procedere per passi piccoli e verificabili.
- Discutere prima strategia, semantica e trade-off; modificare il codice solo
  dopo l'accordo esplicito.
- Non anticipare refactor grandi se il passo corrente non e' stato approvato.
- Dopo ogni passo, fermarsi e chiarire il prossimo punto prima di proseguire.
- Se una decisione e' delicata, documentare anche il ragionamento, non solo il
  risultato.
- Se una spiegazione data in chat riguarda una scelta reale del codice, riportare
  quella spiegazione anche negli `.md` o nei commenti del codice.
- Quando utile, citare il commit che introduce o spiega una scelta, cosi' gli
  studenti possono risalire alla modifica concreta.

## Commit

I commit devono seguire sempre queste regole:

- messaggio in inglese
- descrizione dettagliata in inglese
- lista finale `Modified files:`
- nessuna riga vuota tra gli elementi della lista dei file
- includere solo i file del passo corrente
- non committare file locali non tracciati, log generati o esperimenti fuori
  task

Esempio di formato:

```text
Short English title

Detailed English explanation of what changed and why.

Additional technical context if useful.

Modified files:
- path/one.c
- path/two.md
```

## Documentazione

- Tenere sempre aggiornata la documentazione in `docs/it`.
- La documentazione deve essere in italiano, dettagliata e didattica.
- Spiegare sempre:
  - cosa fa il codice
  - dove si trova
  - perche' e' stato scritto cosi'
  - quali alternative sono state scartate o rimandate
  - quali effetti ha sui test e sul comportamento osservabile
- Scrivere pensando a studenti con poca esperienza: evitare salti logici e
  spiegare i concetti teorici quando servono.
- La guida sul codice deve essere impostata come una lettura guidata della
  codebase: non solo elenco di API, ma percorso ragionato in cui un lettore meno
  esperto viene accompagnato da flusso runtime, funzioni, responsabilita',
  strutture dati, campi modificati e motivazioni. Quando emergono concetti C,
  Linux, inotify, callback, puntatori a funzione o strutture dati, rimandare ai
  capitoli dedicati come glossario, guida C o documenti architetturali.
- Aggiornare `docs/it/documentation-progress.md` quando cambiano codice, test o
  documentazione rilevante.
- Aggiornare i file di scenario quando cambiano eventi attesi o semantica.
- Aggiornare `docs/commenting-style.md` e `docs/commenting-progress.md` se il
  passo riguarda commenti nel codice o stile dei commenti.
- Quando si documentano flussi complessi, strutture dati o responsabilita' tra
  moduli, aggiungere anche una vista grafica quando aiuta: diagrammi Mermaid,
  tabelle `campo -> scritto da -> letto da`, sequence diagram e schemi
  statici. Se il processo e' naturalmente dinamico, descrivere anche i frame
  logici che in futuro potrebbero generare GIF, video o viste interattive.
  Mermaid resta la scelta semplice per gli `.md`; animazioni reali vanno
  progettate come passo successivo con strumenti dedicati.

## Verifiche

Dopo modifiche al codice eseguire normalmente:

```bash
make
make test-core
make test
```

Se il passo tocca shadow mode, legacy, move/rename, watch ricorsivi o raw event
sintetici, eseguire anche scenari mirati con:

```bash
python3 tests/shadow/compare_shadow_output.py <scenario>
```

Esempi utili:

```bash
python3 tests/shadow/compare_shadow_output.py move_rename_dir
python3 tests/shadow/compare_shadow_output.py move_file
python3 tests/shadow/compare_shadow_output.py recursive_create_nested_dir
```

## Stato semantico corrente

- Il runtime di default e' `event_engine=core`.
- `ALFRED_EVENT_ENGINE=shadow` serve ancora per confronto legacy/core.
- I test funzionali storici forzano shadow esplicito.
- I test core verificano lo stream semantico plain del core.
- `events.c` e' legacy shadow, non sorgente ufficiale degli eventi.
- `move_cache` e' confinata al dispatcher legacy.
- `WATCH_ADDED` e `WATCH_REMOVED` sono diagnostica backend, non semantica core.
- `FILE_MODIFIED` e `FILE_READY` non sono duplicati: sono fasi diverse della
  vita del file.
- Nel core, move+rename e' un solo evento `RELOCATED`, non una coppia
  `MOVED` + `RENAMED`.
- Lo scan ricorsivo delle directory e' stato considerato stato backend, non
  semantica core.
- L'overflow inotify e' rimandato: e' una policy di recovery/resync, non uno
  scenario semantico base stabile.

## Commit chiave recenti

- `60323c6 Introduce explicit inotify backend boundary`
- `533f5a1 Encapsulate inotify descriptor and watchers`
- `e3d1234 Confine legacy move cache to events dispatcher`
- `37d68f4 Move legacy shadow dispatch behind backend`
- `93d210a Make core event engine the default`

Questi commit descrivono il passaggio dal legacy dispatcher al core come stream
ufficiale, mantenendo lo shadow mode come strumento di confronto.
