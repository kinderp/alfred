# Qualita' del prodotto software

Questo capitolo spiega perche' Alfred misura dimensioni come robustezza,
affidabilita', performance, sicurezza, coerenza e documentazione. Non e' solo
una lista di parole: e' il modo in cui un progetto software diventa un prodotto
usabile, mantenibile e credibile.

Alfred ha un obiettivo ambizioso: diventare un runtime di osservazione e
controllo per agenti intelligenti. Per questo non basta che "funzioni sul mio
computer". Deve essere piccolo, leggero, performante, veloce, stabile, robusto,
affidabile, sicuro, semplice e chiaro da capire.

## Correttezza non basta

Un programma e' corretto quando produce il risultato atteso per un certo input.
Ma la correttezza in isolamento non basta per costruire un buon prodotto.

Esempio:

```text
Scenario: creo file.txt
Risultato atteso: FILE_CREATED
Risultato reale:   FILE_CREATED
```

Questo scenario dice che una parte funziona. Non dice pero':

- se Alfred regge migliaia di eventi;
- se consuma troppa memoria;
- se il comportamento resta uguale dopo un refactor;
- se un path strano lo inganna;
- se il log e' comprensibile;
- se uno studente capisce il codice;
- se una futura policy di sicurezza puo' fidarsi di quel record.

Per questo separiamo piu' dimensioni di qualita'.

## Dimensioni principali

| Dimensione | Domanda semplice |
| --- | --- |
| Correttezza funzionale | Fa quello che deve fare? |
| Robustezza | Regge input difficili, casi limite e condizioni sporche? |
| Affidabilita' | Continua a comportarsi bene nel tempo e in run ripetuti? |
| Stabilita' | Non crasha e non degrada dopo uso prolungato o cambiamenti? |
| Performance | E' veloce e introduce poco overhead? |
| Leggerezza | Usa poche risorse quando lavora e quando e' a riposo? |
| Sicurezza | Non introduce rischi e non si lascia aggirare facilmente? |
| Coerenza | Si comporta in modo uniforme e spiegabile? |
| Semplicita' | Il design evita complessita' non necessaria? |
| Manutenibilita' | Si puo' modificare senza rompere tutto? |
| Operabilita' | Quando qualcosa va storto, si capisce cosa e' successo? |
| Documentazione | Utenti, contributori e studenti possono capirlo? |

Queste dimensioni sono collegate, ma non sono la stessa cosa. Una funzionalita'
puo' essere corretta ma lenta, sicura ma difficile da usare, veloce ma
incoerente, documentata ma non robusta.

## Robustezza

La robustezza misura la capacita' di un sistema di reggere condizioni non
ideali.

Esempi in Alfred:

- configurazione invalida;
- root path che non e' una directory;
- directory create molto velocemente;
- directory osservata che viene spostata;
- eventi inotify ravvicinati;
- output writer lento;
- path lunghi o con caratteri particolari.

Un sistema robusto non deve necessariamente riuscire a fare tutto. Deve pero'
fallire in modo controllato, spiegabile e recuperabile quando possibile.

Esempio:

```text
Buono:
Alfred rifiuta una root non directory e spiega l'errore.

Cattivo:
Alfred entra nell'event loop senza watch validi e resta appeso.
```

## Affidabilita'

L'affidabilita' misura se il comportamento resta corretto nel tempo.

Un test passato una volta e' utile, ma non basta. Per parlare di affidabilita'
servono run ripetuti:

- sulla stessa macchina;
- dopo modifiche al codice;
- con timing leggermente diversi;
- con scenari piu' lunghi;
- con piu' eventi.

Nel nostro processo, gli audit notturni aiutano proprio a costruire
affidabilita': ripetono scenari realistici e lasciano traccia storica di cosa e'
stato provato.

## Stabilita'

La stabilita' e' vicina all'affidabilita', ma guarda soprattutto la capacita' di
Alfred di restare vivo e sano durante l'esecuzione.

Domande:

- il processo crasha?
- la memoria cresce senza motivo?
- le code si riempiono e non si svuotano?
- un errore locale rompe tutto il runtime?
- dopo ore il comportamento e' ancora lo stesso?

I sanitizer, i test lunghi e i futuri soak test servono a misurare questa
dimensione.

## Performance e leggerezza

Performance significa fare lavoro velocemente. Leggerezza significa consumare
poche risorse.

Per Alfred entrambe contano:

- deve gestire molti eventi filesystem;
- deve avere overhead basso;
- non deve bloccare il backend per colpa di writer lenti;
- deve tenere il percorso caldo corto;
- deve evitare allocazioni e I/O non necessari dove contano le prestazioni.

Esempio di percorso caldo desiderato:

```text
evento OS
-> backend/collector
-> normalizzazione minima
-> alfred_record_t
-> enqueue su coda
```

Tutto il resto, come JSONL, text writer, socket, dashboard o report, deve stare
fuori dal percorso caldo quando Alfred dovra' puntare a prestazioni alte.

### Le astrazioni devono pagare il proprio costo

Una astrazione architetturale puo' essere utile, ma non e' gratuita. Porte,
adapter, dispatcher, code, callback, plugin-like API, projection o writer
aggiungono concetti da capire e, a volte, costo runtime.

Per Alfred la domanda non e':

```text
questa architettura e' elegante?
```

La domanda e':

```text
questa architettura rende Alfred piu' piccolo, chiaro, testabile,
robusto, sicuro o estendibile senza danneggiare prestazioni e semplicita'?
```

Se una astrazione tocca il percorso caldo, deve essere misurata. Il confronto
minimo dovrebbe indicare:

- baseline prima della modifica;
- nuova versione;
- scenario no-op o counter quando possibile;
- scenario JSONL o text quando rilevante;
- `records/sec` o `ns/record`;
- eventuali allocazioni, copie, lock o I/O aggiunti;
- interpretazione del risultato.

Se il costo e' piccolo rispetto al beneficio, va scritto. Se il costo e'
incerto, la modifica resta sperimentale o va rimandata.

Regola sintetica:

```text
Architecture must pay rent.
```

Una astrazione che non compra chiarezza, testabilita', sicurezza,
affidabilita', estensibilita' necessaria o performance misurabile e' debito,
non qualita'.

La discussione architetturale collegata e':
[Architecture direction: lightweight ports-and-adapters for Alfred](https://github.com/kinderp/alfred/discussions/44).

## Sicurezza

La sicurezza e' centrale per Alfred. Se Alfred deve proteggere sistemi da agenti
intelligenti, non puo' essere fragile.

Sicurezza non significa solo "non crashare". Significa anche:

- non perdere segreti nei log;
- non farsi ingannare da path strani o symlink;
- non produrre decisioni ambigue;
- fallire in modo conservativo quando non sa cosa fare;
- non caricare plugin non fidati senza modello di sicurezza;
- non richiedere privilegi eccessivi senza motivo;
- non permettere bypass evidenti del monitoraggio.

Regola:

```text
Security gates maturity.
```

Se una funzionalita' e' fragile dal punto di vista sicurezza, non puo' essere
considerata matura anche se passa i test funzionali.

## Coerenza

La coerenza misura se Alfred si comporta in modo uniforme e spiegabile.

Esempi:

- file e directory dovrebbero seguire regole parallele quando possibile;
- errori simili dovrebbero avere messaggi simili;
- `raw.log`, `events.log` e `output.jsonl` dovrebbero raccontare la stessa
  azione senza contraddirsi;
- le configurazioni dovrebbero avere nomi e default coerenti;
- se una differenza e' necessaria, la documentazione deve spiegare perche'.

Regola:

```text
Incoherence blocks clarity.
```

Un comportamento incoerente puo' anche funzionare tecnicamente, ma rende il
sistema difficile da capire e da mantenere.

## Semplicita' e manutenibilita'

Semplice non significa banale. Un sistema puo' essere complesso perche' risolve
problemi reali, ma non deve essere complicato senza motivo.

Per Alfred significa:

- funzioni con responsabilita' chiare;
- moduli separati;
- API piccole;
- ownership della memoria esplicita;
- poche eccezioni;
- configurazioni comprensibili;
- test mirati;
- documentazione aggiornata.

La manutenibilita' misura quanto e' facile modificare il progetto senza causare
regressioni. Ogni volta che aggiungiamo una feature dobbiamo chiederci:

```text
Questa modifica rende Alfred piu' chiaro o solo piu' grande?
```

## Operabilita'

Operabilita' significa che un sistema e' facile da usare e diagnosticare in
ambiente reale.

Per Alfred vuol dire:

- log leggibili;
- errori con contesto;
- artifact completi;
- comandi riproducibili;
- issue facili da scrivere;
- differenza chiara tra bug di Alfred, bug dello script e comportamento atteso.

Gli audit notturni migliorano l'operabilita' perche' obbligano a salvare:

- comandi;
- configurazioni;
- log;
- output JSONL;
- link agli artifact;
- issue collegate.

## Documentazione

La documentazione e' una dimensione di qualita', non un accessorio.

Per Alfred serve a tre pubblici diversi:

| Pubblico | Bisogno |
| --- | --- |
| Utente | Compilare, configurare, lanciare e leggere output. |
| Contributore | Sapere dove modificare, quali test lanciare e come aprire PR. |
| Studente | Capire C, Linux, inotify, architettura, test, ownership e trade-off. |

Regola:

```text
Undocumented behavior is not mature.
```

Una funzionalita' non documentata puo' esistere nel codice, ma non e' pronta
come parte stabile del prodotto.

## Perche' usiamo audit e matrici

La matrice di maturita' non serve a dare voti. Serve a ragionare meglio.

Senza una matrice, potremmo dire:

```text
Il test passa, quindi la feature e' ok.
```

Con la matrice chiediamo invece:

```text
Passa in scenari reali?
E' robusta?
E' affidabile?
E' sicura?
E' coerente?
E' documentata?
E' performante?
E' semplice da mantenere?
```

Questo e' pensiero ingegneristico: non guardare solo il risultato immediato, ma
anche il comportamento nel tempo, il costo di manutenzione, il rischio e la
comprensibilita'.

## Regole guida di Alfred

Le regole che usiamo nella matrice sono:

```text
Security gates maturity.
Incoherence blocks clarity.
Undocumented behavior is not mature.
Performance is separate from correctness.
Architecture debt reduces long-term velocity.
```

Tradotte:

- una feature insicura non e' matura;
- una feature incoerente non e' chiara;
- una feature non documentata non e' pronta;
- una feature corretta non e' automaticamente veloce;
- debito architetturale oggi rallenta lo sviluppo domani.

Queste regole sono il modo in cui Alfred prova a diventare un prodotto ben
fatto, non solo un programma che passa qualche test.
