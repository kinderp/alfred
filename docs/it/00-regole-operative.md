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

## Principi di ragionamento dell'agente

Queste regole valgono quando un agente AI aiuta a modificare o documentare il
progetto. Sono ispirate anche alle guideline "Karpathy-inspired" raccolte nel
repository `multica-ai/andrej-karpathy-skills`, ma adattate al nostro flusso di
lavoro e alla documentazione didattica di Alfred.

- Pensare prima di modificare: dichiarare assunzioni, dubbi e trade-off prima di
  toccare codice o documentazione quando il passo non e' banale.
- Non nascondere confusione: se una richiesta puo' essere interpretata in piu'
  modi, fermarsi e proporre le interpretazioni invece di sceglierne una in
  silenzio.
- Semplicita' prima: implementare il minimo necessario per il passo concordato,
  senza astrazioni speculative o opzioni configurabili non richieste.
- Modifiche chirurgiche: toccare solo i file necessari al passo corrente. Se si
  nota codice morto o migliorabile fuori contesto, segnalarlo o metterlo nel
  TODO, ma non cambiarlo senza accordo.
- Ogni riga modificata deve poter essere collegata alla richiesta corrente, a un
  test, a un bug o a un aggiornamento documentale necessario.
- Obiettivi verificabili: per ogni passo tecnico chiarire come si controlla il
  risultato, per esempio con `make`, `make test-core`, uno scenario shadow o una
  verifica mirata.
- Prima di fare refactor, distinguere sempre:
  - comportamento osservabile
  - responsabilita' dei moduli
  - pulizia interna
  - compatibilita' temporanea legacy/shadow
- Se una modifica serve solo a ridurre complessita' interna, documentare perche'
  non cambia la semantica osservabile.

Questi principi non devono rallentare correzioni ovvie o modifiche puramente
documentali. Servono soprattutto nei passaggi non banali: refactor, semantica
degli eventi, test, architettura, interfacce pubbliche e strumenti di sviluppo.

## Uso di indici semantici e grafi del codice

Alfred puo' essere esplorato con strumenti diversi, che hanno scopi diversi:

- `rg` e lettura diretta dei file: percorso normale per modifiche piccole e
  localizzate.
- Sourcebot/Elixir: browser umani per cercare e leggere il codice.
- Kythe: indice semantico e cross-reference C, utile per interrogazioni su
  simboli, definizioni, riferimenti e possibili mappe automatiche.
- Graphify: possibile knowledge graph piu' ampio, pensato per agenti AI e
  capace di collegare codice, documentazione, diagrammi e decisioni progettuali.

Regola pratica:

```text
Prima restringere il campo con indici/grafi quando il problema e' ampio;
poi aprire e verificare sempre i file sorgente reali prima di modificare.
```

Non usare Kythe o Graphify per ogni task. Per un cambio locale e chiaro, `rg` e
lettura diretta bastano. Usarli invece quando:

- bisogna capire l'impatto di un refactor
- bisogna sapere chi chiama o usa una funzione/struttura dati
- bisogna aggiornare la mappa del codice o la documentazione architetturale
- bisogna controllare se una spiegazione documentale e' ancora coerente con il
  codice
- un agente deve orientarsi nella codebase senza aprire molti file irrilevanti
- si vuole generare o validare una vista `funzione -> chiamanti -> chiamati`

Kythe va preferito per domande tecniche sui simboli C indicizzati, per esempio:

```text
dove e' definito questo simbolo?
quali file vede l'indice?
quali riferimenti conosce il grafo?
```

Graphify, se verra' integrato, andra' preferito per domande piu' larghe, per
esempio:

```text
quali documenti spiegano questa parte del codice?
quali moduli sono collegati a questa decisione architetturale?
quale cluster di file partecipa al flusso inotify -> raw -> core?
```

Limite fondamentale: un indice puo' essere vecchio, incompleto o impreciso.
Prima di usare una risposta di Kythe/Graphify per cambiare codice, controllare
sempre:

- che l'indice sia stato rigenerato dopo modifiche rilevanti
- che i file sorgente reali confermino la relazione trovata
- che i test o gli scenari documentati verifichino il comportamento atteso

Quando una scelta o una mappa viene ricavata da Kythe o Graphify, indicarlo
nella documentazione come supporto all'analisi, non come fonte unica di verita'.

## Commit

I commit devono seguire sempre queste regole:

- messaggio in inglese
- descrizione dettagliata in inglese
- lista finale `Modified files:`
- nessuna riga vuota tra gli elementi della lista dei file
- includere solo i file del passo corrente
- non committare file locali non tracciati, log generati o esperimenti fuori
  task

Le regole di stile del messaggio seguono le pratiche raccolte in
`Git Commit Best Practices`:

- fare commit puliti e monoscopo
- committare spesso su feature branch, senza aspettare un cambio enorme
- scrivere messaggi significativi, utili a reviewer e lettori futuri
- usare il modo imperativo e il tempo presente nel subject:
  `change`, non `changed` o `changes`
- usare il body per spiegare che cosa e' cambiato e perche'
- tenere il subject breve, idealmente intorno a 50 caratteri
- spezzare il body intorno a 72 caratteri quando e' pratico
- lasciare una riga vuota tra subject e body
- non chiudere il subject con un punto
- rimuovere punteggiatura non necessaria

Quando il tipo e' utile, usare il formato:

```text
<type>(<optional scope>): <subject>
```

Tipi ammessi:

- `feat`: nuova funzionalita' utente
- `fix`: correzione di bug utente
- `docs`: modifiche alla documentazione
- `style`: formattazione senza cambio di comportamento
- `refactor`: refactor del codice di produzione
- `test`: aggiunta o refactor di test
- `chore`: manutenzione ordinaria senza cambio di comportamento
- `build`: modifiche a build, tooling o dipendenze
- `perf`: miglioramento prestazionale

Nel nostro progetto il prefisso `<type>` e' consigliato ma non obbligatorio
per i commit gia' avviati con lo stile storico. Restano invece obbligatori il
testo in inglese, la descrizione dettagliata e la lista finale dei file senza
righe vuote tra gli elementi.

Esempio di formato:

```text
refactor: move legacy shadow lifecycle

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

Dopo aver discusso il passo e prima di modificare, consultare il codice come
farebbe un contributore umano:

- usare `rg` e lettura diretta dei file per cambi piccoli e localizzati
- usare Sourcebot quando serve cercare simboli, path o parole chiave dal browser
- usare Elixir quando serve navigare il codice C in modo piu' tradizionale
- usare Kythe solo come supporto semantico per restringere il campo, poi
  verificare sempre sui file reali

Guide operative:

- [Browser del codice](../../tools/code-browsing/README.md): script aggregati
  per controllare Docker, fare setup, avviare, fermare, riavviare e controllare
  Sourcebot, Elixir e Kythe insieme
- [Sourcebot](../sourcebot-browser/README.md): avvio, stop, log, reset e query
  utili; URL locale predefinito `http://127.0.0.1:3000`
- [Elixir](../code-browser/README.md): setup, reindex, avvio e stop; URL locale
  predefinito `http://127.0.0.1:8080/alfred/workspace/source`
- [Kythe](../kythe-browser/README.md): setup, reindex, server API e query CLI

Questi strumenti servono a capire il codice prima di modificarlo. Non
sostituiscono build, test e lettura del sorgente reale.
Per un esempio dettagliato di ricognizione con Kythe, `rg`, `sed` e lettura
della documentazione collegata, vedere
[Debugging, test e strumenti](10-debugging-test-e-strumenti.md).

Dopo modifiche al codice eseguire normalmente la procedura standard pre-commit:

```bash
git diff --check
make
make test
make test-backend-diagnostics
```

L'ordine e' intenzionale:

- `git diff --check` trova spazi finali, righe con whitespace problematico e
  difetti banali nel diff prima di perdere tempo con build e test.
- Il primo `make` verifica la build normale core-only, cioe' il percorso
  predefinito del progetto.
- `make test` verifica il percorso core end-to-end ufficiale.
- `make test-backend-diagnostics` verifica i log diagnostici del backend
  inotify, per esempio `WATCH_ADDED` e `WATCH_REMOVED`.

In queste regole, "core end-to-end ufficiale" significa che Alfred viene
eseguito davvero, legge eventi dal backend inotify reale, li converte in
`alfred_raw_event_t`, li passa al core e verifica lo stream semantico finale
prodotto dal core. Non e' un test unitario isolato: e' il percorso runtime
ufficiale `inotify -> raw -> core -> events.log`.

`ENABLE_LEGACY_SHADOW=1` era una variante di build temporanea usata durante la
migrazione: compilava anche il dispatcher legacy (`events.c`) e la
`move_cache`, cosi' il progetto poteva confrontare il vecchio comportamento
legacy con il core. La variante e' stata rimossa dal Makefile: la build
ottenuta con `make` e' l'unica build supportata e non include quei file.

Differenza minima da ricordare:

- core: stream semantico ufficiale e default del progetto
- shadow: modalita' diagnostica storica di confronto legacy/core, non piu'
  supportata dal runtime corrente

Per i dettagli, leggere:

- [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
- [Makefile e build system](09-makefile-e-build-system.md)
- [Confronto shadow mode](12-confronto-shadow-mode.md)

Questa procedura deve essere riproducibile anche senza strumenti di AI: ogni contributore
deve poterla eseguire manualmente prima di proporre o committare una modifica.
Se un comando fallisce, fermarsi, leggere l'errore e non proseguire con i passi
successivi finche' il problema non e' compreso.

Se il passo tocca move/rename, watch ricorsivi o raw event sintetici, eseguire
test core o backend mirati. Gli script in `tests/shadow/` sono storici e non
vanno usati come verifica ordinaria dopo la rimozione del dispatcher legacy.

## Stato semantico corrente

- Il runtime di default e' `event_engine=core`.
- `ALFRED_EVENT_ENGINE=shadow` e' un valore invalido; l'unico valore valido e'
  `core`.
- I test funzionali storici sono memoria della fase legacy/shadow, non verifica
  ordinaria del runtime corrente.
- I test core verificano lo stream semantico plain del core.
- `events.c`, `events.h`, `move_cache.c` e `move_cache.h` sono stati rimossi.
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
ufficiale. Lo shadow mode non e' piu' uno strumento di confronto corrente: resta
solo come storia della migrazione.
