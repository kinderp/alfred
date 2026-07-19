# Audit esplorativi notturni

Questa cartella raccoglie i report degli audit esplorativi di Alfred.
Un audit esplorativo non e' una suite ufficiale come `make test`: e' una
sessione pratica in cui Alfred viene usato come farebbe un utente, seguendo la
documentazione e provando scenari reali per cercare bug, divergenze dal
contratto o comportamenti poco chiari.

## Perche' esistono questi audit

I test ufficiali proteggono contratti precisi. L'audit esplorativo invece serve
a rispondere a domande piu' operative:

- Alfred si comporta come documentato?
- I log prodotti sono comprensibili?
- `raw.log`, `events.log` e `output.jsonl` raccontano la stessa storia?
- Una configurazione errata fallisce in modo chiaro?
- Uno scenario reale ma non ancora coperto dai test ufficiali mostra problemi?

Se un audit trova un bug confermato, il bug deve diventare una issue separata
su GitHub. Se lo scenario e' importante, deve poi essere trasformato in test
ufficiale o golden test.

## Audit notturno e validazione con primi utenti

Un audit notturno e una validazione pre-release non sono intercambiabili.
Nell'audit il maintainer o l'agente conosce gia' Alfred e cerca problemi con
scenari mirati. Nella first-user validation una persona esterna prova anche
installazione, documentazione e modello mentale: gli interventi del
facilitatore, il consenso e la privacy diventano parte dell'evidenza.

Quando una sessione coinvolge un primo utente bisogna seguire
[First-user pre-release validation v0](../55-first-user-pre-release-validation-v0.md).
Gli script e la struttura artifact degli audit possono essere riusati, ma non
devono attivare raccolta nascosta o sostituire il consenso del partecipante.

## Tre modalita' di audit notturno

Gli audit notturni non devono essere tutti uguali. Alfred ha bisogno di tre
modalita' distinte, perche' misurano rischi diversi.

| Modalita' | Durata tipica | Obiettivo | Cosa scopre bene |
| --- | --- | --- | --- |
| Nightly smoke audit | minuti | Ripetere scenari noti e regressioni recenti | rotture evidenti, known failure, problemi di configurazione |
| Nightly user-session audit | 1-3 ore | Simulare una sessione reale di lavoro utente | incoerenze tra azioni realistiche e log, problemi di stabilita' nel tempo |
| Nightly fuzzy audit | variabile, anche molte ore | Generare sequenze casuali o semi-casuali | crash, invarianti violate, combinazioni non previste |

La sessione del 2026-07-01 e' stata uno smoke audit: utile per controllare che
Backend API v0 e Writer Runtime non avessero rotto gli scenari principali, ma
non ancora abbastanza lunga per simulare davvero il lavoro di un utente.

### Smoke audit

Lo smoke audit e' breve e ripetibile. Serve quando:

- e' stato appena fatto un refactor;
- si vuole controllare una PR o un merge recente;
- bisogna verificare se un known failure e' ancora riproducibile;
- si vuole aggiornare velocemente la freschezza della matrice di maturita'.

Esempio: lanciare `run_all.sh`, riprodurre un known failure, aggiungere uno o
due scenari mirati di configurazione.

### User-session audit

Lo user-session audit e' quello piu' vicino all'idea di "usare Alfred come un
utente vero". Non deve essere solo una sequenza di test isolati: deve costruire
una piccola storia operativa.

Esempi di sessione:

- creare e modificare un mini progetto;
- rinominare file e directory mentre Alfred osserva;
- simulare un editor che salva file temporanei;
- eseguire build o script che generano molti file;
- pulire directory, spostare artefatti, cancellare output;
- mantenere Alfred attivo abbastanza a lungo da osservare stabilita', memoria,
  log e comportamento in shutdown.

Questo tipo di audit e' piu' costoso, ma aiuta a capire se Alfred resta
comprensibile e affidabile quando le operazioni sono concatenate come in una
sessione reale.

### Fuzzy audit

Il fuzzy audit non parte da uno scenario umano leggibile. Genera molte sequenze
casuali o semi-casuali e controlla proprieta' generali come:

- Alfred non deve crashare;
- `output.jsonl` deve restare JSONL valido;
- un record non deve avere campi incompatibili tra loro;
- una directory osservata non deve produrre path impossibili;
- un evento semantico non deve riferirsi a un path stale dopo recovery.

Questa distinzione e' importante:

- gli smoke audit proteggono velocemente le regressioni;
- gli user-session audit simulano meglio utenti reali;
- i fuzzy audit cercano bug imprevisti su molte combinazioni.

Per ora gli script in `tests/exploratory/nightly` sono soprattutto smoke e
scenario-based tests. La modalita' user-session e la modalita' fuzzy vanno
introdotte gradualmente, senza confonderle con la suite ufficiale.

## Registrare comandi, aspettative e output

Per i primi utenti reali puo' essere molto utile registrare la sequenza dei
comandi lanciati da terminale insieme ai log di Alfred. Questo permetterebbe di
scrivere report del tipo:

```text
Comandi osservati:
1. mkdir src
2. touch src/main.c
3. mv src/main.c src/app.c

Output atteso:
- DIR_CREATED src
- FILE_CREATED src/main.c
- FILE_RENAMED old=src/main.c new=src/app.c

Output reale:
- ...
```

Vantaggi:

- rende gli audit piu' riproducibili;
- aiuta un utente a spiegare cosa ha fatto;
- aiuta a confrontare azione umana, `events.log`, `raw.log` e `output.jsonl`;
- puo' diventare la base di issue molto piu' precise.

Limiti:

- i comandi shell sono un livello alto: `make`, `npm install` o un editor
  possono generare molte syscall ed eventi filesystem non ovvi;
- le azioni fatte da GUI non passano necessariamente da una shell registrabile;
- un comando non dice sempre quale processo figlio ha prodotto ogni evento;
- senza backend process/syscall, la correlazione comando -> evento resta
  approssimativa.

Quindi il command recorder e' utile come strumento di supporto e riproduzione,
non come fonte di verita' del modello Alfred. La fonte di verita' deve restare
cio' che Alfred osserva dal sistema operativo. In futuro, quando esisteranno
backend per processi, exec e syscall, il recorder potra' essere integrato con
PID, processo padre, cwd, exit status e sessione agente.

Per le operazioni grafiche, la soluzione non e' fingere che tutto passi dalla
shell. Le opzioni future sono:

- chiedere all'utente di annotare manualmente l'azione GUI nel report;
- registrare finestre/app attive solo se disponibile e rispettando privacy;
- usare backend OS piu' bassi, quando esisteranno, per correlare processo,
  file, rete e syscall;
- trattare le azioni GUI come contesto umano, non come evidenza primaria.

## Automonitoraggio di Alfred

Gli audit devono anche pensare al caso in cui Alfred stesso si comporti male:
crash, uso memoria anomalo, CPU alta, blocchi, log corrotti o shutdown non
pulito.

Per questo e' utile prevedere, come roadmap operativa, un layer di
automonitoraggio leggero. Non deve stare nel percorso caldo dell'evento e non
deve trasformare Alfred in un agente pesante. Deve raccogliere solo dati
diagnostici utili quando qualcosa va storto.

Segnali utili:

- exit status e segnale di terminazione;
- ultimo timestamp osservato;
- RSS/memoria residente;
- CPU approssimativa;
- numero file descriptor;
- numero watch;
- dimensione code e record droppati, quando disponibili;
- ultimi N record o righe di log;
- stderr, sanitizer output o core dump se configurato;
- configurazione usata, branch e commit.

In caso di malfunzionamento, Alfred o il runner di audit dovrebbe salvare uno
snapshot locale negli artifact. L'invio automatico a un server va rimandato:
prima servono consenso esplicito, data minimization e regole chiare su privacy
e segreti. Per ora la scelta piu' sicura e' salvare localmente e caricare gli
artifact solo quando l'audit lo richiede.

La maturita' osservata dagli audit e' documentata in
[maturity-matrix.md](maturity-matrix.md).
La matrice include anche dimensioni che non sono ancora misurate in modo
completo, come performance, sicurezza, coerenza e documentazione. Questo e'
intenzionale: il modello deve indicare cosa raccogliere nei prossimi audit, non
solo cio' che e' gia' misurabile dopo la prima notte.

La matrice distingue anche maturita' storica e freschezza della validazione.
Se il codice collegato a una funzionalita' cambia in modo rilevante, la fiducia
nei risultati precedenti puo' diventare `stale` o `needs-revalidation`. Questo
non cancella la memoria storica degli audit, ma evita di trattare una vecchia
validazione come prova valida sul codice corrente.

Per eseguire una sessione completa partendo da un prompt breve, usare il
[playbook operativo](nightly-playbook.md).

## Struttura GitHub

Ogni audit notturno usa una issue madre. La issue madre e' il diario pubblico
della sessione e deve contenere:

| Sezione | Contenuto |
| --- | --- |
| Obiettivo | Cosa si vuole controllare durante la notte. |
| Commit e branch | SHA e branch testati, cosi' il risultato e' riproducibile. |
| Ambiente | OS, VM, build, opzioni rilevanti e configurazioni usate. |
| Scenari | Lista degli scenari provati, con esito `PASS`, `FAIL` o `NEEDS REVIEW`. |
| Artifact | Percorsi locali o allegati GitHub con log e output completi. |
| Bug confermati | Link alle issue figlie create per i problemi reali. |
| Prossimi tentativi | Cosa provare nell'audit successivo. |

La issue madre deve usare almeno queste label:

```text
area:tests
kind:audit
```

Le issue figlie nate dall'audit devono mantenere `kind:audit` e aggiungere il
tipo specifico del risultato: `kind:bug` per bug confermati, `kind:test` per
scenari da promuovere a test ufficiale, `kind:debt` per debiti tecnici o
documentali emersi durante l'audit.

Ogni bug confermato usa una issue figlia dedicata. La issue figlia deve
contenere:

- link alla issue madre;
- scenario minimo di riproduzione;
- comando eseguito;
- configurazione usata;
- comportamento atteso;
- comportamento reale;
- estratti dei log importanti;
- spiegazione del motivo per cui e' un bug o una divergenza dal contratto.

La relazione deve essere bidirezionale:

- la issue figlia cita la issue madre;
- la issue madre contiene il link alla issue figlia.

In questo modo, leggendo la issue madre si vede tutta la notte di audit; leggendo
la issue figlia si vede il problema specifico.

## Struttura nel repository

La struttura stabile e':

```text
docs/it/audit/
  README.md
  2026-06-25-audit-notturno.md
  nightly-playbook.md
  maturity-matrix.md
  maturity-data-template.csv

tests/exploratory/nightly/
  README.md
  nightly_lib.sh
  base-jsonl-lifecycle.sh
  moves-jsonl.sh
  lost-scope-jsonl.sh
  audit-raw-events.sh
  recursive-fast-mkdir-p.sh
  root-file-should-fail.sh
```

Ruoli:

| Percorso | Ruolo |
| --- | --- |
| `docs/it/audit/README.md` | Metodo generale degli audit notturni. |
| `docs/it/audit/YYYY-MM-DD-*.md` | Report storico di una sessione specifica. |
| `tests/exploratory/nightly/README.md` | Come rilanciare gli scenari esplorativi. |
| `tests/exploratory/nightly/*.sh` | Script riproducibili per scenari reali. |

Gli script in `tests/exploratory/nightly` non sono ancora parte della suite
ufficiale. Sono materiale operativo: aiutano a ripetere cio' che e' stato fatto
la notte precedente e a decidere cosa promuovere a test stabile.

## Cosa non committare

I log completi generati da un audit non vanno committati nel repository, salvo
scelta esplicita. Possono diventare grandi, rumorosi e dipendenti dalla
macchina locale.

Nel repository devono entrare:

- report sintetici;
- comandi riproducibili;
- estratti di log significativi;
- link alle issue;
- script esplorativi.

I log completi possono restare:

- sotto `/tmp` durante l'audit;
- come archivio caricato su Google Drive e linkato nella issue madre;
- come allegati o estratti nelle issue GitHub quando sono piccoli o essenziali;
- come artifact di GitHub Actions in futuro, se l'audit verra' automatizzato.

## Caricare gli artifact su Google Drive

A fine audit, gli artifact locali possono essere compressi e caricati sul Google
Drive del maintainer. Questo evita di perdere i dettagli del run e permette di
riaprire il giorno dopo tutti i log senza doverli committare.

Prerequisiti:

- `rclone` installato;
- remote Google Drive configurato, per convenzione `gdrive:`;
- `zip` disponibile;
- `gh` autenticato se si vuole aggiornare automaticamente la issue madre.

Verifica del remote:

```sh
rclone listremotes
rclone lsd gdrive:
```

Comando consigliato:

```sh
tests/exploratory/nightly/upload_audit_artifacts.sh \
  --date 2026-06-25 \
  --issue 29
```

Lo script:

1. cerca le directory `/tmp/alfred-nightly-audit-YYYY-MM-DD*`;
2. crea `/tmp/alfred-nightly-audit-YYYY-MM-DD.zip`;
3. carica lo zip in `gdrive:alfred/audits/`;
4. genera un link condivisibile con `rclone link`;
5. se e' stato passato `--issue`, aggiunge un commento alla issue madre.

Per provare solo la creazione dello zip senza upload:

```sh
tests/exploratory/nightly/upload_audit_artifacts.sh \
  --date 2026-06-25 \
  --dry-run
```

Il link Drive va considerato parte del diario dell'audit: la issue madre resta
l'indice pubblico, mentre Drive conserva il pacchetto completo dei log.

## Quando trasformare un audit in test ufficiale

Uno scenario esplorativo va promosso a test ufficiale quando:

- protegge un contratto pubblico o interno stabile;
- ha un output atteso chiaro;
- non dipende troppo da timing o condizioni locali;
- ha gia' trovato un bug o protegge un bug risolto;
- e' utile a evitare regressioni.

Se lo scenario riguarda il contratto esterno strutturato, preferire un golden
test JSONL. Se riguarda un contratto interno tra moduli, preferire un test C
mirato. Se riguarda diagnostica storica o leggibilita' umana, puo' restare un
test sui log testuali.
