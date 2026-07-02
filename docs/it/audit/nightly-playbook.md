# Playbook operativo per test notturni

Questo playbook e' la procedura compatta da seguire quando il maintainer scrive:

```text
inizia sessione test notturni seguendo regole operative
```

Da quel momento l'audit e' autorizzato come sessione autonoma. L'agente deve
procedere senza chiedere ulteriori conferme operative per i comandi necessari
all'audit, incluse interazioni GitHub, creazione issue, aggiornamento issue,
sub-issue, upload artifact e commenti sulla issue madre.

Questa autonomia non autorizza fix di codice, refactor, merge o modifiche
funzionali non richieste. L'obiettivo e' osservare Alfred, raccogliere prove e
documentare bug o divergenze.

## Obiettivo della sessione

Una sessione notturna deve:

1. usare Alfred come farebbe un utente reale;
2. provare scenari riproducibili;
3. controllare `raw.log`, `events.log`, `output.jsonl` ed eventuali errori;
4. aprire issue per bug confermati;
5. aggiornare la issue madre con risultati e link;
6. caricare gli artifact completi su Google Drive;
7. aggiornare la matrice di maturita' se emergono dati utili;
8. lasciare un report finale leggibile al mattino.

## Scegliere la modalita'

Prima di iniziare, dichiarare quale modalita' di audit si sta eseguendo.

| Modalita' | Quando usarla | Durata indicativa |
| --- | --- | --- |
| `nightly smoke audit` | Dopo PR, merge o refactor per controllare regressioni note | minuti |
| `nightly user-session audit` | Quando si vuole simulare il lavoro di un utente reale | 1-3 ore |
| `nightly fuzzy audit` | Quando si vogliono cercare crash e violazioni di invarianti con sequenze generate | variabile |

La modalita' va scritta nella issue madre. Se non viene specificata, partire da
`nightly smoke audit`, perche' e' la piu' breve e riproducibile.

### Nightly smoke audit

Procedura minima:

1. build;
2. `tests/exploratory/nightly/run_all.sh`;
3. known failure rilevanti;
4. uno o due scenari extra vicini alle modifiche recenti;
5. report e upload artifact.

### Nightly user-session audit

Procedura minima:

1. scegliere una storia utente realistica;
2. avviare Alfred e lasciarlo attivo per tutta la sessione;
3. eseguire comandi concatenati, non solo test isolati;
4. registrare comandi principali, tempi approssimativi e aspettative;
5. controllare coerenza tra azioni, `raw.log`, `events.log` e `output.jsonl`;
6. controllare stabilita' del processo Alfred: exit status, memoria, stderr e
   shutdown;
7. aprire issue solo per bug o divergenze con evidenza sufficiente.

Esempi di storie utente:

- lavorare su un mini progetto C;
- simulare salvataggi di editor con file temporanei;
- generare e pulire directory di build;
- spostare asset tra directory;
- eseguire script che creano molti file in piu' fasi.

### Nightly fuzzy audit

La modalita' fuzzy va introdotta con cautela. Non deve confrontare solo output
letterali, ma invarianti generali:

- Alfred non crasha;
- JSONL resta valido riga per riga;
- non compaiono record strutturalmente incompatibili;
- non ci sono path impossibili;
- non ci sono riferimenti semantici a path stale non dichiarati;
- gli errori producono diagnostica, non silenzio.

Per ora questa modalita' e' roadmap: prima servono generatori, seed
riproducibili, limiti di durata e artifact leggibili.

## Bootstrap iniziale

Prima di lanciare test:

```sh
git status --short --branch
git rev-parse --short HEAD
make all
```

Annotare:

- branch corrente;
- commit testato;
- eventuali file non tracciati gia' presenti;
- data UTC;
- esito della build.

Non pulire la working tree con comandi distruttivi. I file non tracciati locali
vanno ignorati se non appartengono all'audit.

## Issue madre

Ogni notte deve avere una issue madre GitHub. Titolo consigliato:

```text
Nightly exploratory audit: YYYY-MM-DD
```

La issue madre deve contenere:

- obiettivo dell'audit;
- branch e commit;
- ambiente;
- scenari pianificati;
- stato iniziale;
- link agli artifact quando disponibili;
- bug confermati e issue figlie;
- riepilogo finale.

Label standard:

```text
area:tests
kind:audit
```

Usare `status:ready` se l'audit e' una sessione operativa pianificata. Usare
`status:needs-discussion` solo se la issue madre serve soprattutto a discutere
scenari o interpretazioni prima di eseguire l'audit.

Se una issue madre per quella data esiste gia', riusarla e aggiornarla. Se non
esiste, crearla.

## Esecuzione degli scenari base

Lanciare la suite esplorativa base:

```sh
tests/exploratory/nightly/run_all.sh
```

`run_all.sh` esegue gli scenari che devono passare e salta i known failure. Per
includere anche i known failure:

```sh
INCLUDE_KNOWN_FAILURES=1 tests/exploratory/nightly/run_all.sh
```

Il known failure va incluso quando si vuole verificare se un bug aperto e' ancora
riproducibile.

## Scenari extra

Dopo gli scenari base, scegliere scenari extra in base a:

- funzionalita' poco matura nella matrice;
- issue aperte;
- documentazione che dichiara comportamenti non ancora provati;
- aree cambiate negli ultimi commit;
- edge case vicini a bug recenti.

Esempi:

```sh
tests/exploratory/nightly/root-file-should-fail.sh
tests/exploratory/nightly/recursive-fast-mkdir-p.sh
```

Per scenari non ancora scriptati, creare una cartella sotto `/tmp` e annotare
comandi, config, log attesi e log reali nella issue madre.

## Registrare comandi e aspettative

Negli user-session audit, registrare almeno i comandi principali lanciati da
terminale e l'output Alfred atteso ad alto livello. La registrazione puo' essere
manuale nel report o automatizzata in futuro da un wrapper.

Esempio:

```text
Command trace:
1. mkdir src
2. touch src/main.c
3. mv src/main.c src/app.c

Expected Alfred story:
- DIR_CREATED src
- FILE_CREATED src/main.c
- FILE_RENAMED old=src/main.c new=src/app.c
```

Questa traccia non e' una fonte di verita' completa. I comandi sono un livello
alto: un singolo comando puo' aprire processi figli, creare file temporanei o
fare operazioni non ovvie. Quando Alfred avra' backend process/syscall, la
traccia comandi potra' essere correlata con PID, cwd, exit status e sessione.

Per azioni fatte da interfaccia grafica, annotare manualmente l'azione nel
report quando e' importante. Non assumere che una GUI sia osservabile tramite
shell. La correlazione affidabile richiedera' backend OS piu' bassi o
integrazioni dedicate.

## Analisi dei log

Per ogni scenario fallito o sospetto, controllare in questo ordine:

1. `assertions.log`;
2. `errors.log`;
3. `events.log`;
4. `raw.log`;
5. `output.jsonl`;
6. `alfred.stderr`;
7. `alfred.stdout`;
8. `exit.status`, se presente.

Regola pratica:

- `assertions.log` spiega cosa non e' tornato nello script;
- `events.log` mostra semantica e diagnostica;
- `raw.log` mostra eventi normalizzati del backend;
- `output.jsonl` mostra il contratto strutturato;
- `stderr` puo' contenere sanitizer o errori di runtime.

## Snapshot di automonitoraggio

Quando uno scenario e' lungo, fuzzy o sospetto, raccogliere anche segnali sul
processo Alfred stesso. Il runner dovrebbe salvare negli artifact:

- exit status;
- segnale di terminazione, se presente;
- stderr e stdout;
- ultimi record/log prima del problema;
- memoria residente approssimativa, se disponibile;
- CPU approssimativa, se disponibile;
- numero file descriptor e watch, se disponibile;
- branch, commit e configurazione usata.

Se Alfred crasha, si blocca o usa risorse in modo anomalo, aprire una issue
figlia solo se gli artifact permettono di spiegare:

```text
cosa stava facendo l'utente
cosa ci si aspettava
cosa ha fatto Alfred
quale segnale indica crash, hang, leak o consumo anomalo
```

L'invio automatico di snapshot a un server non e' parte dello scope corrente.
Per ora gli snapshot restano artifact locali o archivio Drive collegato alla
issue madre. Prima di qualsiasi upload automatico servono consenso esplicito,
data minimization e protezione dei segreti.

## Quando aprire una issue figlia

Aprire una issue figlia solo se il problema e' riproducibile o ha evidenza
sufficiente.

La issue figlia deve contenere:

- link alla issue madre;
- scenario minimo;
- comando eseguito;
- traccia comandi o azioni utente, se disponibile;
- configurazione;
- comportamento atteso;
- comportamento reale;
- estratti log;
- eventuale snapshot di automonitoraggio;
- motivo per cui e' bug o divergenza dal contratto;
- link agli artifact completi, se disponibili.

Label consigliate:

```text
bug confermato:
area:tests
kind:audit
kind:bug

scenario da promuovere a test:
area:tests
kind:audit
kind:test

debito emerso durante audit:
area:tests
kind:audit
kind:debt
```

Subito dopo:

1. aggiungere il link della issue figlia nella issue madre;
2. se GitHub lo permette, collegare la issue figlia come sub-issue reale con
   `addSubIssue` o con il bottone `Create sub-issue -> Add existing issue`;
3. verificare che la issue figlia mostri la issue madre come parent.

## Upload artifact

A fine audit, caricare gli artifact locali:

```sh
tests/exploratory/nightly/upload_audit_artifacts.sh \
  --date YYYY-MM-DD \
  --issue ISSUE_MADRE
```

Lo script:

1. comprime `/tmp/alfred-nightly-audit-YYYY-MM-DD*`;
2. carica lo zip su `gdrive:alfred/audits/`;
3. genera il link Google Drive;
4. commenta la issue madre.

Per prova senza upload:

```sh
tests/exploratory/nightly/upload_audit_artifacts.sh \
  --date YYYY-MM-DD \
  --dry-run
```

## Aggiornare maturita' funzionalita'

Dopo l'audit, aggiornare se utile:

```text
docs/it/audit/maturity-data-template.csv
docs/it/audit/maturity-matrix.md
```

Non serve aggiornare la matrice per ogni scenario banale. Aggiornarla quando:

- una funzionalita' viene provata da un nuovo scenario significativo;
- un bug abbassa la fiducia;
- un known failure viene risolto;
- uno scenario viene ripetuto in piu' notti;
- viene aggiunta una nuova dimensione di valutazione.

Quando l'audit avviene dopo modifiche importanti al codice, aggiornare anche la
freschezza della validazione. La maturita' storica non va cancellata
automaticamente, ma la confidence deve riflettere il rischio introdotto dalle
modifiche recenti.

Regola operativa:

```text
modifica bassa  -> maturita' invariata
modifica media  -> freshness `stale`, pianificare audit mirato
modifica alta   -> freshness `needs-revalidation`
bug confermato  -> maturita' degradata + `needs-revalidation`
audit superato  -> freshness torna `fresh` per lo scope verificato
```

Prima di aggiornare una funzionalita' come matura, controllare:

1. ultimo commit validato;
2. file o moduli toccati dagli ultimi cambi;
3. se gli scenari eseguiti coprono davvero la funzionalita';
4. se il controllo riguarda text log, JSONL, backend, core o performance.

## Report finale in chat

Alla fine lasciare un report con:

- issue madre;
- issue figlie create;
- link Google Drive;
- scenari passati;
- scenari falliti;
- known failure riprodotti;
- comandi principali eseguiti;
- eventuali parti non completate;
- prossimo audit consigliato.

Il report deve distinguere chiaramente:

- bug di Alfred;
- bug dello script di audit;
- comportamento previsto ma poco documentato;
- scenario inconclusivo da riprovare.

## Checklist rapida

```text
[ ] Leggere regole operative audit.
[ ] Rilevare branch e commit.
[ ] Eseguire build.
[ ] Creare o aggiornare issue madre.
[ ] Lanciare run_all.sh.
[ ] Lanciare eventuali scenari extra.
[ ] Analizzare log dei fallimenti.
[ ] Aprire issue figlie per bug confermati.
[ ] Collegare sub-issue reali quando possibile.
[ ] Caricare artifact su Google Drive.
[ ] Aggiornare issue madre con link e riepilogo.
[ ] Aggiornare matrice maturita' se utile.
[ ] Lasciare report finale in chat.
```
