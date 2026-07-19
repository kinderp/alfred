# First-user pre-release validation v0

Questo documento definisce il protocollo della prima validazione pre-release
di Alfred con utenti esterni al normale ciclo di sviluppo. La milestone non
aggiunge funzionalita': verifica se una persona nuova riesce a installare,
capire, avviare, osservare e fermare l'MVP usando soltanto la superficie
pubblica documentata.

La GitHub Milestone di riferimento e' la
[Milestone #16](https://github.com/kinderp/alfred/milestone/16). Il piano vivo,
le issue figlie e le prove di completamento sono nella
[issue madre #279](https://github.com/kinderp/alfred/issues/279).

## Obiettivo

La domanda principale e':

```text
Un primo utente riesce a usare correttamente l'MVP senza conoscere il codice,
senza affidarsi alla chat dei maintainer e senza esporre dati personali?
```

La validazione deve produrre evidenza utile per:

- scoprire errori di installazione, configurazione, runtime o output;
- trovare istruzioni ambigue, mancanti o difficili da seguire;
- misurare il tempo necessario per ottenere il primo evento utile;
- distinguere problemi del prodotto da problemi dell'ambiente o del protocollo;
- scegliere il prossimo lavoro sulla base di problemi osservati, non di ipotesi.

## Cosa dimostra e cosa non dimostra

Una sessione positiva dimostra soltanto che una specifica revisione ha
funzionato nello specifico ambiente e negli scenari registrati. Non trasforma
automaticamente quella distribuzione o quel kernel in una piattaforma
supportata.

| Metodo | Autore principale | Scopo | Non dimostra |
| --- | --- | --- | --- |
| Suite automatica | CI e maintainer | Contratti deterministici e regressioni note | Usabilita' reale o assenza di bug non modellati |
| Audit notturno | Maintainer o agente | Esplorazione, sessioni lunghe e combinazioni insolite | Esperienza indipendente di un nuovo utente |
| First-user validation | Partecipante esterno con facilitatore | Onboarding, comprensibilita', uso reale ed evidenza ambientale | Fuzzing, benchmark, supporto generale o copertura kernel |

I tre livelli sono complementari. Un problema trovato da un utente deve
diventare un test stabile quando esiste un contratto riproducibile; non deve
rimanere soltanto una nota qualitativa.

## Target minimo v0

La milestone punta ad almeno:

- tre sessioni indipendenti;
- due ambienti Linux gestiti indipendentemente;
- una sessione per partecipante della durata indicativa di 45-90 minuti;
- un report durevole e sanificato per ogni sessione.

Il campione e' deliberatamente piccolo. Serve a trovare problemi evidenti e a
validare il protocollo, non a produrre statistiche sulla popolazione degli
utenti Linux.

## Ruoli

| Ruolo | Responsabilita' |
| --- | --- |
| Partecipante | Esegue i passi, descrive cosa comprende e segnala risultati inattesi. |
| Facilitatore | Spiega il consenso, registra gli interventi e non anticipa la soluzione. |
| Maintainer | Riproduce, classifica e trasforma i problemi confermati in issue figlie. |
| Reviewer | Controlla che claim, prove, privacy e criteri di uscita siano coerenti. |

Il facilitatore puo' fermare una procedura per ragioni di sicurezza, ma ogni
aiuto necessario va registrato: un intervento non documentato falserebbe la
misura dell'onboarding.

## Consenso, privacy e sicurezza

Prima della sessione il partecipante deve sapere:

- quali comandi eseguira';
- quali metadati dell'ambiente saranno raccolti;
- quali log e output saranno ispezionati;
- dove resteranno gli artifact grezzi;
- quali estratti sanificati potranno essere pubblicati su GitHub;
- che puo' interrompere la sessione prima della raccolta finale.

Regole obbligatorie:

1. usare una root temporanea dedicata, mai la home reale, `~/.ssh`, file di
   credenziali o un progetto confidenziale;
2. non copiare nei file di prova token, segreti, dati personali o contenuti
   aziendali;
3. eseguire build, runtime e prove come utente non privilegiato;
4. usare `sudo` soltanto, se necessario e accettato dal partecipante, per
   installare dipendenze di sistema note;
5. non attivare telemetria, registrazione nascosta del terminale o upload
   automatici;
6. sanificare username, hostname, path privati, variabili d'ambiente, indirizzi
   e contenuti non necessari prima di pubblicare evidenza;
7. conservare gli artifact completi solo localmente o in una destinazione
   approvata esplicitamente dal partecipante;
8. verificare il cleanup della root temporanea e dei processi Alfred al termine.

L'assenza di evidenza non vale come `PASS`. Un artifact che non puo' essere
pubblicato per ragioni di privacy puo' restare locale, ma il report deve
indicare che esiste, chi lo conserva e quali conclusioni limitate supporta.

## Evidenza minima dell'ambiente

Ogni report deve associare l'esito alla revisione realmente provata. I campi
minimi sono:

| Campo | Perche' serve |
| --- | --- |
| Session ID pseudonimo | Collega report e artifact senza pubblicare il nome del partecipante. |
| Source revision | Identifica esattamente il codice provato. |
| Data e durata UTC | Colloca temporalmente l'evidenza. |
| Distribuzione e versione | Descrive lo userspace. |
| Architettura e libc | Evidenzia differenze ABI/toolchain rilevanti. |
| Compilatore e versione | Rende riproducibile la build. |
| Kernel osservato e scope | Distingue host, container e VM senza claim eccessivi. |
| Filesystem della root di prova | Aiuta a interpretare semantica e limiti inotify. |
| Profilo di build | Distingue debug, sanitizer e release. |
| Comandi esatti | Permette la riproduzione. |
| Interventi del facilitatore | Misura dove la documentazione non e' stata sufficiente. |

Le sonde non disponibili vanno registrate come `unknown`, non indovinate. I
campi della compatibility evidence v0 possono essere riusati, ma una sessione
utente non e' una lane CI.

## Flusso della sessione

### 1. Preflight

Il facilitatore:

- spiega consenso e confini dei dati;
- assegna il Session ID;
- registra revisione e ambiente;
- verifica che la root di prova sia vuota e sacrificabile;
- controlla che non esista un altro processo Alfred attivo per la sessione.

### 2. Onboarding senza suggerimenti

Il partecipante parte dal README pubblico o dalla sua traduzione italiana e
prova a:

1. identificare i requisiti;
2. compilare con `make`;
3. leggere `./alfred --help` e `./alfred --version`;
4. validare la configurazione con `./alfred --check-config`;
5. spiegare con parole proprie quali file di log si aspetta.

All'inizio dell'onboarding senza suggerimenti si fissa un unico istante `T0`.
Da quel punto si misurano separatamente:

- `time-to-first-command`: da `T0` al primo comando documentato che termina con
  exit status `0`;
- `time-to-first-event`: da `T0` al primo evento semantico atteso che il
  partecipante riesce a riconoscere in `events.log` o `output.jsonl`.

Si registra inoltre ogni punto in cui il facilitatore deve intervenire. Gli
interventi non fermano i timer: restano un dato separato che spiega perche' il
tempo osservato puo' essere maggiore o non rappresentare uso indipendente.

### 3. Installazione staged non-root

La prova di installazione usa una directory temporanea controllata dal
partecipante:

```bash
stage_dir=$(mktemp -d)
make release
make DESTDIR="$stage_dir" PREFIX=/usr install
"$stage_dir/usr/bin/alfred" --version
make DESTDIR="$stage_dir" PREFIX=/usr uninstall
```

Questo esercita il contratto installabile senza modificare il sistema. Non e'
un test di pacchetto `.deb`/`.rpm`, di systemd o di installazione privilegiata.

### 4. Percorso runtime rappresentativo

Il partecipante prepara una root temporanea e una configurazione esplicita con
JSONL abilitato, quindi avvia Alfred usando la sintassi documentata. Dentro la
root esegue operazioni comprensibili e annotate:

- creazione e modifica di file;
- close-write/file ready;
- rename nella stessa directory;
- move tra directory osservate;
- creazione e cancellazione di directory;
- cancellazione di file;
- arresto con `Ctrl+C`.

Per ogni operazione si confrontano azione, risultato atteso, `raw.log`,
`events.log`, `errors.log` e `output.jsonl` quando pertinenti. Il comando shell
e' contesto di riproduzione, non prova certa del processo che ha causato ogni
evento.

### 5. Sessione user-like

Il partecipante usa Alfred per 45-90 minuti su un mini progetto sacrificabile:
puo' modificare file con un editor, creare directory, eseguire una piccola
build e pulire gli output. Le azioni GUI vengono annotate manualmente; non si
introduce registrazione invasiva dello schermo o delle applicazioni.

La sessione cerca:

- crash, blocchi o shutdown incompleto;
- crescita anomala percepibile di CPU, memoria, file descriptor o log;
- eventi mancanti, duplicati o incoerenti;
- divergenze tra testo e JSONL;
- messaggi difficili da interpretare;
- passaggi documentali che richiedono conoscenza implicita.

Questa fase resta esplorativa: non e' un benchmark, un soak test o un fuzzy
test. Misure prestazionali diventano claim soltanto dopo un protocollo di
benchmark dedicato.

### 6. Debrief e cleanup

Al termine si raccolgono:

- cosa il partecipante pensava facesse Alfred prima e dopo la sessione;
- passi riusciti senza aiuto;
- passi falliti o ambigui;
- comandi esatti ed exit status rilevanti;
- estratti sanificati dei log;
- stato di cleanup della root, dello stage e dei processi;
- suggerimenti, mantenendoli separati dai bug confermati.

## Classificazione degli esiti

Ogni scenario usa uno dei seguenti stati:

| Stato | Significato |
| --- | --- |
| `PASS` | Risultato atteso osservato con evidenza sufficiente. |
| `FAIL` | Contratto documentato violato o risultato riproducibile errato. |
| `NEEDS_REVIEW` | Osservazione reale ma causa o contratto ancora incerti. |
| `BLOCKED` | Ambiente o prerequisito impedisce la prova. |
| `NOT_RUN` | Scenario non eseguito; non contribuisce alla maturita'. |

Un commento del partecipante nasce come osservazione. Diventa finding
confermato solo dopo confronto con il contratto e riproduzione, oppure dopo una
classificazione esplicita come problema documentale.

Severita' iniziale:

- `P0`: perdita di dati, esposizione di segreti o rischio immediato grave;
- `P1`: crash, falso successo, output strutturato non affidabile, installazione
  pericolosa o impossibilita' di completare il percorso principale;
- `P2`: problema non bloccante, ambiguita', ergonomia o debito circoscritto.

Ogni finding confermato usa una issue figlia dedicata con ambiente, revisione,
passi, atteso, reale, log sanificati, severita' e link bidirezionale alla issue
madre. Le correzioni seguono il normale ciclo PR e devono aggiungere test quando
il comportamento e' automatizzabile.

## Metriche v0

Le metriche servono a confrontare sessioni, non a costruire una classifica:

- `time-to-first-command` e `time-to-first-event`, misurati dallo stesso `T0`;
- percentuale di passi completati senza aiuto;
- numero e tipo degli interventi del facilitatore;
- numero di `FAIL`, `NEEDS_REVIEW` e `BLOCKED`;
- numero di divergenze testo/JSONL;
- crash, exit non-zero inattesi e cleanup falliti;
- ambiguita' documentali per sezione;
- finding confermati e test di regressione derivati.

I risultati possono contribuire alla matrice di maturita' degli audit, ma non
si sommano automaticamente ai test precedenti. Se il codice della funzionalita'
cambia in modo sostanziale, l'evidenza diventa `needs-revalidation` e resta
soltanto come storia della revisione provata.

## Artifact e report

Una sessione puo' usare questa struttura locale:

```text
/tmp/alfred-first-user-SESSION_ID/
  environment.txt
  commands.txt
  expected-actual.md
  raw.log
  events.log
  errors.log
  output.jsonl
  report.md
```

Prima di qualsiasi upload va prodotta una copia sanificata. Il repository
conserva protocollo, template, report sintetici, comandi riproducibili e link;
non deve ricevere automaticamente log completi o file del partecipante.

## Criteri di uscita

La milestone puo' essere chiusa quando:

- protocollo, guida partecipante e template report sono revisionati e merged;
- e' stata completata una rehearsal in ambiente controllato;
- sono registrate almeno tre sessioni indipendenti su due ambienti gestiti
  indipendentemente, oppure esiste un blocker esplicito e documentato;
- ogni risultato non-pass e' stato classificato;
- non restano finding `P0` o `P1` aperti sul percorso principale;
- artifact e report pubblici rispettano privacy e provenienza;
- il report finale raccomanda il prossimo step con evidenza.

## Non obiettivi

Questa milestone non aggiunge:

- openSUSE, Arch, Gentoo o altre lane CI;
- VM, kernel guest, `virtme-ng` o claim multi-kernel;
- packaging `.deb`/`.rpm`, systemd o installazione di sistema automatizzata;
- fanotify, audit, eBPF, policy o enforcement;
- telemetria, upload automatici o registrazione nascosta del terminale;
- fuzzing, benchmark, formal verification o supporto generale di una distro;
- modifiche a Event Model v0, Backend API v0 o Writer Runtime senza finding
  separato.

## Ordine di roadmap

L'ordine corrente e':

1. chiusura del pinning GitHub Actions e della milestone installabilita';
2. first-user pre-release validation v0;
3. espansione userspace openSUSE e Arch tramite
   [issue #278](https://github.com/kinderp/alfred/issues/278), se i risultati
   utente non impongono prima un fix;
4. VM/kernel reali, metodi formali e backend futuri solo quando il beneficio e'
   collegato a un rischio misurabile.

## Documenti collegati

- [MVP operational usability v0](49-mvp-operational-usability-v0.md)
- [Installability and Linux compatibility v0](53-installability-linux-compatibility-v0.md)
- [Stato funzionalita'](26-stato-funzionalita.md)
- [Qualita' del prodotto software](35-qualita-prodotto-software.md)
- [Audit esplorativi notturni](audit/README.md)
- [Registro milestone](37-roadmap-milestone-progetto.md)
