# First-user session report: SESSION_ID

> Copiare questo file per una sola sessione. Non inserire nome reale, username,
> hostname, segreti o path privati. Sostituire i placeholder e rimuovere le
> istruzioni non necessarie soltanto dopo avere completato la sessione.

## Stato del report

- Session ID pseudonimo: `FU-YYYYMMDD-NN`
- Stato: `DRAFT | SANITIZED | REVIEWED | CLOSED`
- Visibilita' artifact grezzi: `LOCAL_ONLY | APPROVED_STORAGE`
- Revisore sanificazione: `<ROLE_OR_PSEUDONYM>`
- Issue madre sessione: `<URL_OR_NOT_CREATED>`
- Issue madre milestone: https://github.com/kinderp/alfred/issues/279

## Consenso e confini

- [ ] Il partecipante conosce dati, comandi e log raccolti.
- [ ] Il partecipante conosce la destinazione degli artifact.
- [ ] Il partecipante sa che puo' interrompere la sessione.
- [ ] La prova usa soltanto una root temporanea sacrificabile.
- [ ] Non vengono osservati home, credenziali o progetti confidenziali.
- [ ] Non e' attiva registrazione nascosta di terminale, schermo o telemetria.
- [ ] L'eventuale uso di `sudo` riguarda solo dipendenze note ed e' stato
      accettato.

Note sul consenso, senza dati identificativi:

```text
<NOTES_OR_NONE>
```

## Revisione e ambiente

| Campo | Valore sanificato | Metodo |
| --- | --- | --- |
| Source revision | `<FULL_GIT_SHA>` | `git rev-parse HEAD` |
| Data UTC | `<YYYY-MM-DD>` | `date -u` |
| Inizio UTC | `<TIMESTAMP>` | `date -u` |
| Durata totale | `<MINUTES>` | cronometro/session timestamps |
| Distribuzione | `<NAME VERSION>` | `/etc/os-release` |
| Architettura | `<ARCH>` | `uname -m` |
| Libc | `<LIBC_OR_UNKNOWN>` | `getconf GNU_LIBC_VERSION` |
| Compilatore | `<COMPILER VERSION>` | prima riga di `cc --version` |
| Make | `<MAKE VERSION>` | prima riga di `make --version` |
| Kernel osservato | `<KERNEL RELEASE>` | `uname -r` |
| Kernel scope | `<HOST/VM_GUEST/SHARED_CONTAINER/UNKNOWN>` | ambiente sessione |
| Filesystem root di prova | `<TYPE_OR_UNKNOWN>` | `stat -f` sulla root |
| Profilo build | `DEBUG | RELEASE` | comando eseguito |
| Documentazione usata | `<README_IT/README_EN/MAN/OTHER>` | osservazione |

Interpretazione limitata dell'ambiente:

```text
Questa sessione prova la revisione e l'ambiente sopra indicati. Non costituisce
una promessa generale di supporto della distribuzione o del kernel.
```

## Metriche onboarding

- `T0` UTC: `<TIMESTAMP>`
- `T0` epoch locale: `<SECONDS>`
- `time-to-first-command`: `<SECONDS | NOT_REACHED>`
- Primo comando riuscito: `<COMMAND | NONE>`
- `time-to-first-event`: `<SECONDS | NOT_REACHED>`
- Primo evento riconosciuto: `<EVENT_TYPE | NONE>`
- Passi completati senza aiuto: `<COMPLETED>/<TOTAL>`
- Numero interventi facilitatore: `<COUNT>`

I due tempi partono dallo stesso `T0`. Gli interventi non azzerano i timer.

## Interventi del facilitatore

| N. | Fase/timestamp | Motivo | Informazione fornita | Passo poi completato? |
| --- | --- | --- | --- | --- |
| 1 | `<VALUE>` | `<BLOCK_OR_SAFETY>` | `<EXACT_HELP>` | `<YES/NO>` |

Se non ci sono interventi, sostituire la riga con `None`.

## Risultati sintetici

Usare solo `PASS`, `FAIL`, `NEEDS_REVIEW`, `BLOCKED` o `NOT_RUN`.

| Fase | Stato | Evidenza principale | Note |
| --- | --- | --- | --- |
| Requisiti e build debug | `<STATE>` | `<COMMAND/EXCERPT>` | `<NOTES>` |
| Help/version/check-config | `<STATE>` | `<COMMAND/EXIT>` | `<NOTES>` |
| Build release | `<STATE>` | `<COMMAND/EXIT>` | `<NOTES>` |
| Installazione staged | `<STATE>` | `<COMMAND/FILES>` | `<NOTES>` |
| Man page staged | `<STATE>` | `<COMMAND/EXIT>` | `<NOTES>` |
| Uninstall staged | `<STATE>` | `<COMMAND/FILES>` | `<NOTES>` |
| Config JSONL | `<STATE>` | `<COMMAND/EXIT>` | `<NOTES>` |
| Startup e `WATCH_ADDED` | `<STATE>` | `<LOG EXCERPT>` | `<NOTES>` |
| Create/modify/file-ready | `<STATE>` | `<LOG/JSONL>` | `<NOTES>` |
| Rename stesso parent | `<STATE>` | `<LOG/JSONL>` | `<NOTES>` |
| Move tra parent | `<STATE>` | `<LOG/JSONL>` | `<NOTES>` |
| Directory create/delete | `<STATE>` | `<LOG/JSONL>` | `<NOTES>` |
| Delete file | `<STATE>` | `<LOG/JSONL>` | `<NOTES>` |
| JSONL parseabile | `<STATE>` | `<PYTHON EXIT>` | `<NOTES>` |
| Shutdown `Ctrl+C` | `<STATE>` | `<EXIT/LOG>` | `<NOTES>` |
| Sessione user-like | `<STATE>` | `<SUMMARY>` | `<NOTES>` |
| Cleanup | `<STATE>` | `<CHECK>` | `<NOTES>` |

## Dettaglio scenari

Duplicare questa sezione per ogni scenario non banale.

### Scenario N: TITLE

- Fase: `<PHASE>`
- Comando o azione GUI annotata:

```text
<SANITIZED COMMAND OR ACTION>
```

- Risultato atteso:

```text
<EXPECTED CONTRACT>
```

- Risultato osservato:

```text
<ACTUAL RESULT>
```

- Exit status: `<INTEGER | NOT_APPLICABLE | UNKNOWN>`
- Stato: `PASS | FAIL | NEEDS_REVIEW | BLOCKED | NOT_RUN`
- Evidenza minima sanificata:

```text
<RAW.LOG / EVENTS.LOG / ERRORS.LOG / OUTPUT.JSONL EXCERPT>
```

- Perche' l'evidenza supporta lo stato:

```text
<REASONING>
```

## Inventario artifact

Non inserire URL pubblici prima della sanificazione.

| Artifact | Presente | Visibilita' | SHA-256 opzionale | Note |
| --- | --- | --- | --- | --- |
| `environment.txt` | `<YES/NO>` | `<LOCAL/SANITIZED>` | `<HASH_OR_NONE>` | `<NOTES>` |
| `commands.txt` | `<YES/NO>` | `<LOCAL/SANITIZED>` | `<HASH_OR_NONE>` | `<NOTES>` |
| `raw.log` | `<YES/NO>` | `<LOCAL/EXCERPT>` | `<HASH_OR_NONE>` | `<NOTES>` |
| `events.log` | `<YES/NO>` | `<LOCAL/EXCERPT>` | `<HASH_OR_NONE>` | `<NOTES>` |
| `errors.log` | `<YES/NO>` | `<LOCAL/EXCERPT>` | `<HASH_OR_NONE>` | `<NOTES>` |
| `output.jsonl` | `<YES/NO>` | `<LOCAL/EXCERPT>` | `<HASH_OR_NONE>` | `<NOTES>` |
| `report.md` | `YES` | `SANITIZED` | `<HASH_OR_NONE>` | `<NOTES>` |

Artifact mancanti e motivo:

```text
<MISSING ARTIFACTS OR NONE>
```

Un artifact mancante non e' evidenza positiva.

## Sessione user-like

- Durata: `<MINUTES>`
- Mini progetto usato: `<SANITIZED DESCRIPTION>`
- Tipi di operazioni: `<EDITOR | BUILD | MOVE | CLEANUP | GUI | OTHER>`
- Crash o segnali: `<NONE OR DETAILS>`
- Shutdown anomali: `<NONE OR DETAILS>`
- Crescita evidente CPU/RSS/fd/log: `<NONE | OBSERVED | NOT_MEASURED>`
- Divergenze testo/JSONL: `<NONE OR DETAILS>`
- Eventi incomprensibili: `<NONE OR DETAILS>`

Queste osservazioni non sono benchmark o misure prestazionali controllate.

## Feedback del partecipante

### Descrizione di Alfred dopo la sessione

```text
<PARAPHRASED RESPONSE WITHOUT IDENTITY DATA>
```

### Parte piu' chiara

```text
<RESPONSE>
```

### Parte piu' ambigua

```text
<RESPONSE>
```

### Passi che richiederebbero ancora aiuto

```text
<RESPONSE>
```

### Richieste o suggerimenti

```text
<FEEDBACK; DO NOT CLASSIFY AS BUG YET>
```

## Osservazioni da sottoporre a triage

| ID locale | Stato sessione | Tipo iniziale | Sintesi | Riproduzione richiesta |
| --- | --- | --- | --- | --- |
| `OBS-01` | `<FAIL/NEEDS_REVIEW>` | `<BUG?/DOCS?/ENV?/REQUEST?/RESEARCH?>` | `<SUMMARY>` | `<YES/NO>` |

Una osservazione non e' ancora un finding confermato. Il maintainer deve
confrontarla con il contratto e riprodurla o classificarla esplicitamente.

## Triage maintainer

| Osservazione | Riprodotta | Classificazione finale | Severita' | Issue figlia |
| --- | --- | --- | --- | --- |
| `OBS-01` | `<YES/NO/PARTIAL>` | `<BUG/DOCS/ENVIRONMENT/REQUEST/DEBT/NOT_A_BUG>` | `<P0/P1/P2/NONE>` | `<URL_OR_NONE>` |

Motivazione delle classificazioni:

```text
<REASONING AND CONTRACT REFERENCES>
```

## Checklist di sanificazione

- [ ] Nome reale, username e hostname rimossi.
- [ ] Email, IP, token, credenziali e identificatori personali rimossi.
- [ ] Path sostituiti con `<REPO_ROOT>`, `<SESSION_ROOT>` e `<WATCH_ROOT>`.
- [ ] Nessun dump completo di ambiente o cronologia shell incluso.
- [ ] Nessun contenuto estraneo ai file sintetici della sessione incluso.
- [ ] Comandi e argomenti controllati singolarmente.
- [ ] Estratti log ridotti alle righe necessarie.
- [ ] Link pubblici aggiunti solo dopo il controllo.
- [ ] Artifact grezzi mantenuti locali o in storage approvato.
- [ ] Revisore e data del controllo registrati.

Esito sanificazione: `PASS | BLOCKED`

Note:

```text
<NOTES_OR_NONE>
```

## Cleanup

- [ ] Alfred fermato senza `kill -9`, salvo malfunzionamento documentato.
- [ ] Nessun processo Alfred della sessione resta attivo.
- [ ] Binario e man page staged rimossi con `uninstall`.
- [ ] Root temporanea rimossa oppure conservata localmente per triage.
- [ ] Nessun artifact caricato senza consenso e sanificazione.

## Conclusione della sessione

- Esito complessivo: `PASS | FAIL | NEEDS_REVIEW | BLOCKED`
- Finding confermati P0: `<COUNT>`
- Finding confermati P1: `<COUNT>`
- Finding confermati P2: `<COUNT>`
- Problemi documentali: `<COUNT>`
- Osservazioni non ancora risolte: `<COUNT>`
- Sessione utilizzabile per la milestone: `YES | NO`

Motivazione finale:

```text
<EVIDENCE-BASED CONCLUSION>
```

Prossimo passo raccomandato:

```text
<REPRODUCE | OPEN ISSUE | FIX DOCS | REPEAT SESSION | ACCEPT EVIDENCE>
```
