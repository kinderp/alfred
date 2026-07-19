# Rehearsal focalizzata FU-20260719-R2: context handoff

## Stato e scopo

- Tipo: rehearsal focalizzata controllata dal maintainer.
- Stato report: `REVIEWED`.
- Revisione provata: `4b89e482b3ba5b54e2343c35a046462cb672b7a8`.
- Finding verificato: [#287](https://github.com/kinderp/alfred/issues/287).
- Pull request: [#288](https://github.com/kinderp/alfred/pull/288).
- Rehearsal originaria: [FU-20260719-R1](FU-20260719-R1-report.md).
- Partecipante esterno: nessuno.

Questa prova verifica esclusivamente il fix del passaggio di contesto tra
terminali. Non sostituisce una sessione first-user indipendente, non misura
onboarding o performance e non contribuisce al requisito delle tre sessioni
esterne della milestone.

## Metodo

Il maintainer ha eseguito `session-context.sh create` dalla root del
repository. Il comando ha creato una root `0700`, un `session.env` `0600` e ha
stampato un bootstrap shell-safe. Il bootstrap e' stato poi eseguito senza
modifiche in processi Bash distinti:

1. shell di preparazione per build e configurazione;
2. terminale 1 con Alfred in foreground;
3. terminale 2 con le azioni filesystem;
4. shell separata di verifica degli output dopo `Ctrl+C`.

Nessun comando dei terminali ha ricevuto `repo_root`, `run_root`, `watch_root`
o `stage_root` tramite variabili iniettate dall'orchestrazione. Ogni processo
ha ottenuto i path soltanto dal bootstrap e ha stampato
`session context OK`.

Il [transcript sanificato](FU-20260719-R2-context-handoff-commands.txt) conserva
la sequenza con `<REPO_ROOT>`, `<SESSION_ROOT>` e `<WATCH_ROOT>`. Il file
`session.env`, il bootstrap contenente path reali e i log completi non sono
pubblicati.

## Risultati

| Controllo | Stato | Evidenza |
| --- | --- | --- |
| Creazione root privata | `PASS` | root `0700`, context `0600` |
| Bootstrap shell corrente | `PASS` | `session context OK` |
| Bootstrap terminale 1 | `PASS` | Alfred avviato in foreground |
| Bootstrap terminale 2 | `PASS` | azioni risolte sotto `<WATCH_ROOT>` |
| Shell di verifica separata | `PASS` | JSONL parseabile e processi assenti |
| Create/ready/rename/delete | `PASS` | eventi semantici rappresentativi presenti |
| Move tra directory watched | `PASS` | `FILE_MOVED` old/new dopo attesa dei watch |
| Shutdown | `PASS` | `Ctrl+C`, exit `0`, nessun processo residuo |
| Casi di manomissione automatici | `PASS` | `make test-first-user` |

Estratti sanificati:

```text
session context OK
[EVENT] FILE_CREATED path=<WATCH_ROOT>/draft.txt
[EVENT] FILE_READY path=<WATCH_ROOT>/draft.txt
[EVENT] FILE_RENAMED from=<WATCH_ROOT>/draft.txt to=<WATCH_ROOT>/report.txt
[EVENT] FILE_MOVED from=<WATCH_ROOT>/inbox-paced/item.txt to=<WATCH_ROOT>/archive-paced/item.txt
[EVENT] FILE_DELETED path=<WATCH_ROOT>/archive-paced/item.txt
```

`errors.log` era vuoto. Dopo le due esecuzioni runtime locali gli artifact
contenevano 63 righe raw, 63 righe events e 59 righe JSONL parseabili. I log
sono stati usati solo per il controllo e poi eliminati con la root.

## Primo tentativo compresso

Nel primo passaggio il terminale 2 ha creato, usato e rimosso le due directory
senza attendere esplicitamente i rispettivi `WATCH_ADDED`. La sequenza ha
provato correttamente il context handoff, ma non ha prodotto evidenza
sufficiente per `FILE_MOVED`. Non e' stata classificata come regressione:
contraddiceva l'istruzione della guida di eseguire e osservare una operazione
alla volta.

Il tratto e' stato ripetuto aspettando entrambi i nuovi watch prima di creare e
spostare il file. La seconda esecuzione ha prodotto `FILE_MOVED`. Questa nota e'
conservata per evitare di presentare la sola ripetizione positiva come se il
primo tentativo non fosse avvenuto.

## Artifact locali

| Artifact | SHA-256 locale | Pubblicazione |
| --- | --- | --- |
| `raw.log` | `411c7dd7347a7e182e267cf9ca918e9b967c85ef1b5a83e81f23a6d0acfea77d` | eliminato |
| `events.log` | `0d242cb1b77e211a66863cc86ef0b29df26bf9f49bdd9bb99ffb4bc96bee8064` | eliminato, estratti minimi sopra |
| `errors.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | eliminato, file vuoto |
| `output.jsonl` | `1045cd2054793bd5afd4355f6689fbf64cf34e1fd998ab05c2c923e85e7c6d70` | eliminato, parsing riuscito |
| `session.env` | non registrato | `LOCAL_ONLY`, eliminato |

## Esito

Il finding #287 e' risolto sulla revisione provata: una shell nuova puo'
inizializzare il contesto con un solo comando, i path non dipendono dalla
cronologia della shell e i casi di contesto mancante o manomesso falliscono
prima di assegnare variabili operative.

Il finding separato [#285](https://github.com/kinderp/alfred/issues/285) resta
fuori scope. La prova ha osservato ancora l'attuale forma JSONL dei path di
move, ma non la usa per valutare il context handoff.
