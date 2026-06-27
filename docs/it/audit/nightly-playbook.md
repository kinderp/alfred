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

## Quando aprire una issue figlia

Aprire una issue figlia solo se il problema e' riproducibile o ha evidenza
sufficiente.

La issue figlia deve contenere:

- link alla issue madre;
- scenario minimo;
- comando eseguito;
- configurazione;
- comportamento atteso;
- comportamento reale;
- estratti log;
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
