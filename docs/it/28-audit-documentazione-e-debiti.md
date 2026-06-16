# Audit documentazione e debiti dichiarati

Questo documento raccoglie l'audit corrente della documentazione. Serve a
distinguere tre cose diverse:

- problemi reali della documentazione da sistemare;
- debiti tecnici o documentali gia' dichiarati e volutamente rimandati;
- idee future che non devono essere scambiate per mancanze immediate.

L'audit non sostituisce [documentation-progress.md](documentation-progress.md):
quel file dice lo stato dei capitoli. Questo file spiega invece cosa resta da
discutere dopo aver letto la documentazione.

## Controlli eseguiti

Sono stati eseguiti questi controlli:

```bash
find docs -type f -name '*.md' | sort
rg -n 'TODO|todo|da fare|rimand|futuro|prossim|debit|animaz' docs README.md
rg -n 'Mermaid|mermaid' docs/it README.md
rg -n '12-confronto-shadow-mode|15-todo-switch-core|ENABLE_LEGACY_SHADOW|EVENT_ENGINE_SHADOW|shadow mode' README.md docs/it docs/man
git diff --check
```

E' stato fatto anche un controllo locale dei link Markdown. Il risultato e':

```text
missing=0 checked=38
```

Questo significa che i link locali controllati puntano a file esistenti. Il
controllo non valida i link HTTP esterni e non garantisce che ogni anchor sia
semanticamente quella desiderata; verifica pero' che non ci siano riferimenti a
file rimossi o non presenti nel repository.

## Esito sintetico

| Area | Esito |
| --- | --- |
| Link locali Markdown | Nessun file mancante trovato |
| Indice italiano | Aggiornato con la guida di lettura |
| README principale | Contiene il link alla guida di lettura |
| Pagine man | Presenti come bozze in `docs/man/` |
| Shadow mode | Resta citato solo come contesto storico o errore intenzionale |
| Diagrammi Mermaid | Nessun blocco con linee sorgente oltre la soglia critica controllata |
| Animazioni | Desiderate ma non implementate; sono documentate come roadmap futura |

## Audit dei diagrammi Mermaid

La documentazione contiene diagrammi Mermaid nei capitoli principali di
architettura, flusso eventi, core, modulo inotify, mappa del codice, matrice
inotify, Makefile e scanner/resync.

Il controllo statico sui blocchi Mermaid ha misurato la lunghezza massima delle
righe interne ai diagrammi. Nessun blocco ha linee oltre 95 caratteri. Il
diagramma piu' vicino al limite e' quello dei tre livelli evento in
[Architettura generale](02-architettura-generale.md#tre-livelli-di-evento), che
arriva a 93 caratteri in una riga sorgente ma spezza gia' i nodi con `<br/>`.

Questa scelta corregge il problema gia' incontrato in passato: nodi come
`ALFRED_RAW_CREATE | ALFRED_RAW_ISDIR path=$ROOT/one/two` diventano illeggibili
se vengono scritti su una sola riga dentro il rettangolo. La regola corrente e':
quando un nodo contiene flag lunghi, path o piu' campi, il testo deve essere
spezzato in piu' righe con `<br/>`.

### Diagrammi principali controllati

| Documento | Tipo di diagramma | Stato |
| --- | --- | --- |
| `01-panoramica-progetto.md` | vista generale | leggibile |
| `02-architettura-generale.md` | livelli architetturali e tre livelli evento | leggibile; e' il diagramma piu' denso |
| `04-livello-applicazione.md` | flusso applicativo e `app_run()` | leggibile |
| `05-modulo-inotify.md` | flusso backend | leggibile |
| `06-core-engine.md` | logger e core flow | leggibile |
| `07-flusso-eventi.md` | runtime corrente e storico shadow | leggibile |
| `09-makefile-e-build-system.md` | pipeline build | leggibile |
| `16-mappa-codice-e-strutture.md` | call graph, strutture dati, scenari | leggibile ma molto esteso |
| `20-matrice-eventi-inotify.md` | pipeline concettuale eventi | leggibile |
| `21-roadmap-scanner-resync.md` | watch state, resync e recovery | leggibile |

### Limite dell'audit Mermaid

Il comando `mmdc` non risulta disponibile nell'ambiente corrente, quindi questo
audit e' statico: controlla sorgenti, lunghezze e spiegazioni vicine ai
diagrammi, ma non produce immagini renderizzate.

Per una verifica visuale completa servira' in futuro una delle due strade:

- aggiungere Mermaid CLI o Kroki al tooling documentale;
- generare immagini in CI e fallire se un diagramma non viene renderizzato.

## Stato delle animazioni

Le animazioni vere delle transizioni di stato dei watch non sono implementate.
Sono pero' gia' previste nella documentazione.

I riferimenti principali sono:

- [Roadmap documentazione avanzata](17-roadmap-documentazione-avanzata.md),
  che cita VHS, asciinema, D2, SVG/HTML generato e walkthrough;
- [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md),
  che contiene scenari animabili e un formato logico a frame;
- [Regole operative](00-regole-operative.md),
  che richiedono viste grafiche quando aiutano e indicano che le animazioni
  reali vanno progettate separatamente da Mermaid.

La parte statica del diagramma di stato dei watch e del resync e' gia'
documentata in [Roadmap scanner e resync](21-roadmap-scanner-resync.md). In
particolare ci sono:

- una tabella degli stati `VALID`, `STALE`, `RESYNCING`, `REMOVED`;
- un flow chart del resync v0;
- un `stateDiagram-v2` con trigger `T0` ... `T12`;
- note sui trigger che spiegano quale evento o condizione causa ogni
  transizione.

Il prossimo passo sulle animazioni non dovrebbe essere aggiungere subito un
tool pesante. Prima conviene scegliere uno scenario pilota:

1. `IN_MOVE_SELF -> WATCH_STALE -> WATCH_LOST_QUEUED`;
2. lost-scope recovery che ritrova la directory per `(st_dev, st_ino)`;
3. reinstallazione dei watch mancanti;
4. ritorno a `VALID` oppure rollback e permanenza in `STALE`.

Solo dopo ha senso scegliere il formato: GIF, SVG/HTML interattivo, asciinema,
VHS o D2.

## Debiti dichiarati da discutere

Questa sezione elenca i principali elementi che la documentazione indica come
futuri, rimandati o da discutere. Non tutti hanno la stessa priorita'.

### Event Model v0

Stato: da progettare.

Documenti collegati:

- [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)
- [Stato funzionalita' supportate](26-stato-funzionalita.md)
- [Contratto dei log](22-contratto-log.md)
- [alfred-events(7)](../man/man7/alfred-events.7)

Motivo: prima di stabilizzare JSONL, plugin backend e policy guardrail serve un
modello eventi piu' esplicito. Oggi esistono raw event e semantic event, ma il
progetto futuro avra' bisogno di campi strutturati come sorgente, timestamp,
identita', categoria, severita', sessione e contesto.

Priorita' consigliata: alta.

### Backend API v0

Stato: da progettare.

Documenti collegati:

- [Roadmap plugin backend](23-roadmap-plugin-backend.md)
- [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)
- [Modulo inotify](05-modulo-inotify.md)

Motivo: Alfred oggi ha il backend inotify integrato nel codice. Prima di
aggiungere fanotify, audit, eBPF, Windows o macOS serve una API comune per
inizializzare backend, produrre raw event normalizzati, gestire configurazione,
diagnostica, recovery e ownership memoria.

Priorita' consigliata: alta, ma dopo Event Model v0.

### JSONL e output strutturato

Stato: rimandato.

Documenti collegati:

- [Contratto dei log](22-contratto-log.md#testo-oggi-protocollo-domani)
- [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)

Motivo: il testo attuale e' leggibile e comodo per test e didattica. Per uso
professionale serviranno output strutturati, probabilmente JSONL all'inizio e
poi eventualmente MessagePack, protobuf o protocollo binario ad alte
prestazioni. JSONL non dovrebbe pero' nascere prima del modello eventi.

Priorita' consigliata: media-alta, dopo Event Model v0.

### Performance suite

Stato: rimandata.

Documenti collegati:

- [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)
- [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
- [Roadmap scanner e resync](21-roadmap-scanner-resync.md)

Motivo: Alfred punta a prestazioni elevate. I test funzionali non bastano per
scegliere tra recovery sincrona, batch, worker thread, scanner piu' aggressivo o
output binario. Servono benchmark ripetibili, warmup, percentili, dataset
controllati e confronto tra soluzioni.

Priorita' consigliata: media. Diventa alta prima di ottimizzare pesantemente lo
scanner o introdurre output binario.

### Overflow inotify completo

Stato: parzialmente supportato.

Documenti collegati:

- [Matrice eventi inotify](20-matrice-eventi-inotify.md)
- [Roadmap scanner e resync](21-roadmap-scanner-resync.md)
- [Stato funzionalita' supportate](26-stato-funzionalita.md)

Motivo: `IN_Q_OVERFLOW` viene tradotto in `ALFRED_RAW_OVERFLOW` e poi
`OVERFLOW`, ma la recovery completa dopo perdita eventi e' rimandata. Un
overflow non e' locale: indica che lo stream non e' piu' affidabile e richiede
una policy di resync piu' ampia.

Priorita' consigliata: media. Va progettato insieme a resync globale e
performance.

### Worker thread per recovery

Stato: rimandato.

Documenti collegati:

- [Roadmap scanner e resync](21-roadmap-scanner-resync.md)
- [Stato funzionalita' supportate](26-stato-funzionalita.md)

Motivo: oggi la recovery lost-scope puo' essere processata in modo sincrono e a
batch limitato. Un worker thread ridurrebbe il blocco del percorso eventi, ma
aggiunge sincronizzazione, ownership, shutdown ordinato, backpressure e race
condition. Non conviene introdurlo prima di misurare.

Priorita' consigliata: media-bassa fino alla performance suite.

### Metadata semantic events

Stato: raw supportato, semantica rimandata.

Documenti collegati:

- [Semantica degli eventi](13-semantica-eventi.md#attributi-e-metadati)
- [Matrice eventi inotify](20-matrice-eventi-inotify.md)
- [Stato funzionalita' supportate](26-stato-funzionalita.md)

Motivo: `IN_ATTRIB` e' osservabile e puo' produrre `ALFRED_RAW_ATTRIB`, ma non
e' ancora deciso se generare `FILE_METADATA_CHANGED` o `DIR_METADATA_CHANGED`.
Bisogna evitare semantica troppo rumorosa.

Priorita' consigliata: bassa-media.

### Eventi audit inotify

Stato: opt-in raw log, non stream semantico.

Documenti collegati:

- [Matrice eventi inotify](20-matrice-eventi-inotify.md)
- [Contratto dei log](22-contratto-log.md)
- [Stato funzionalita' supportate](26-stato-funzionalita.md)

Motivo: `IN_OPEN`, `IN_ACCESS` e `IN_CLOSE_NOWRITE` sono utili per osservazione
e guardrail futuri, ma non devono inquinare il contratto filesystem core. Sono
oggi visibili solo nel raw backend log quando configurati.

Priorita' consigliata: bassa finche' non esiste Event Model v0 o uno stream
audit separato.

### IN_MASK_CREATE e IN_MASK_ADD

Stato: rimandati.

Documenti collegati:

- [Matrice eventi inotify](20-matrice-eventi-inotify.md)
- [Stato funzionalita' supportate](26-stato-funzionalita.md)

Motivo: sono flag di policy del watch manager, non eventi da convertire. Possono
servire per hardening o aggiornamenti dinamici delle maschere, ma introdurli ora
complicherebbe la policy senza un beneficio immediato.

Priorita' consigliata: bassa.

### CLI futura

Stato: documentata ma non implementata.

Documenti collegati:

- [Roadmap CLI e pagina man](19-roadmap-cli-e-man-page.md)
- [alfred(1)](../man/man1/alfred.1)
- [alfred.conf(5)](../man/man5/alfred.conf.5)

Motivo: la CLI corrente e' minimale. La documentazione propone opzioni come
`--help`, `--version`, `-c`, `--config`, `--print-config`, `--check-config` e
un futuro modello a sottocomandi. Va progettata prima di modificare il
comportamento utente.

Priorita' consigliata: media, soprattutto prima di ampliare l'uso pubblico.

### Man page

Stato: bozze presenti.

Documenti collegati:

- [Roadmap CLI e pagina man](19-roadmap-cli-e-man-page.md)
- [alfred(1)](../man/man1/alfred.1)
- [alfred.conf(5)](../man/man5/alfred.conf.5)
- [alfred-events(7)](../man/man7/alfred-events.7)

Motivo: le pagine man sono piu' precise degli MD per uso operativo, ma sono
bozze perche' cambieranno con Event Model v0, Backend API v0, JSONL e CLI
futura.

Priorita' consigliata: aggiornarle solo quando cambia un contratto.

### Documentazione dinamica e animazioni

Stato: roadmap.

Documenti collegati:

- [Roadmap documentazione avanzata](17-roadmap-documentazione-avanzata.md)
- [Mappa del codice e strutture dati](16-mappa-codice-e-strutture.md)
- [Regole operative](00-regole-operative.md)

Motivo: Mermaid e' utile per diagrammi statici, ma non basta per mostrare
transizioni nel tempo. Per gli studenti sarebbe utile una sequenza animata che
mostri watch table, lost-scope queue, raw event e semantic event mentre cambiano.

Priorita' consigliata: media-bassa. Prima scegliere uno scenario pilota.

## Punti da discutere

Alla luce dell'audit, i prossimi temi da discutere non sono altri cleanup
documentali generici. I punti veramente decisionali sono:

1. partire da Event Model v0;
2. progettare Backend API v0;
3. decidere se anticipare una CLI stabile;
4. scegliere uno scenario pilota per animazioni/watch state;
5. definire quando introdurre una performance suite ripetibile.

La mia raccomandazione e': prima Event Model v0. Senza quello, CLI, JSONL,
plugin backend, guardrail e output binario rischiano di nascere su un modello
ancora troppo implicito.
