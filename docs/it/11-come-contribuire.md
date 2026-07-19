# Come contribuire

Questo capitolo spiega il flusso consigliato per contribuire ad Alfred.
L'obiettivo e' permettere a studenti e contributori esterni di proporre
modifiche senza avere permessi di scrittura diretti sul repository principale.

Il modello usato e':

```text
repository upstream -> fork personale -> branch di lavoro -> pull request
```

`upstream` e' il repository principale:

```text
https://github.com/kinderp/alfred
```

Il fork e' una copia del repository sotto l'account GitHub del contributore.
Il contributore puo' fare push sul proprio fork, ma non sul repository
principale. Le modifiche entrano nel progetto solo tramite pull request,
revisione e merge.

## Perche' usare fork e pull request

Non dare permessi di scrittura diretti ai nuovi contributori riduce il rischio
di modifiche accidentali su `main`. Il branch `main` deve restare una linea
stabile: chi vuole contribuire lavora su una copia, apre una PR e aspetta la
review.

Questo flusso ha alcuni vantaggi:

- il contributore puo' sperimentare liberamente nel proprio fork
- il repository principale resta protetto
- ogni modifica ha una discussione associata
- GitHub Actions puo' eseguire i test automaticamente sulla PR
- la review puo' chiedere correzioni prima del merge

La documentazione ufficiale GitHub chiama il repository originale
`upstream`. Il fork personale ha normalmente il remote `origin`; il repository
principale viene aggiunto come remote `upstream`.

Riferimenti ufficiali GitHub:

- fork di un repository:
  `https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/fork-a-repo`
- sincronizzazione di un fork:
  `https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/syncing-a-fork`

## Struttura GitHub del progetto

Alfred usa GitHub anche come spazio pubblico di pianificazione. Questo serve a
rendere visibile non solo il codice finale, ma anche il processo che porta alle
scelte tecniche. Per uno studente e' importante vedere come una decisione nasce
da domande, alternative, trade-off, test e review; per un contributore e'
importante sapere dove proporre una modifica e dove cercare il contesto.

La struttura corrente e':

```text
Milestone: Writer Runtime v0
    |
    v
Issue madre: #32 Writer Runtime v0: implementation plan
    |
    +--> Project: Alfred Roadmap
    |       |
    |       +--> vista operativa di issue e PR
    |
    +--> Discussions #33 #34 #35
    |       |
    |       +--> ragionamento, alternative e trade-off
    |
    +--> Issue figlie
    |       |
    |       +--> micro-step, bug, test, debiti tecnici
    |
    +--> Pull request
            |
            +--> modifiche verificabili, review e CI
```

| Strumento GitHub | Uso in Alfred | Esempio corrente |
| --- | --- | --- |
| Milestone | Macro-obiettivo con avanzamento misurabile | [`Writer Runtime v0`](https://github.com/kinderp/alfred/milestone/1) |
| Project | Vista operativa di issue, PR e roadmap | [`Repository Projects`](https://github.com/kinderp/alfred/projects), con [`Alfred Roadmap`](https://github.com/users/kinderp/projects/1) collegato al repository |
| Issue madre | Piano tecnico dettagliato di una milestone | [`Writer Runtime v0: implementation plan`](https://github.com/kinderp/alfred/issues/32) |
| Issue figlia | Micro-step, bug o task specifico collegato a un piano | da creare quando il lavoro viene diviso |
| Discussion | Discussione progettuale aperta, con domande e alternative | [`common queue vs per-sink queues`](https://github.com/kinderp/alfred/discussions/33) |
| Pull request | Modifica concreta da revieware e testare | una PR per ogni micro-step coerente |
| Documentazione nel repository | Decisione consolidata, contratto stabile e spiegazione didattica | `docs/it/*.md` |

Quando si crea una issue madre, il body deve indicare subito la roadmap MD
principale della milestone. Questa roadmap non va nascosta in una lista lunga di
documenti: deve apparire vicino al goal come `Primary roadmap`, con link GitHub
cliccabile e una breve spiegazione del perche' e' il riferimento operativo
principale.

Esempio:

```text
Primary roadmap:
Writer Runtime Roadmap v0
https://github.com/kinderp/alfred/blob/main/docs/it/33-writer-runtime-roadmap-v0.md

This document is the main operational reference for the milestone.
```

Le issue madri possono poi avere una sezione piu' ampia, come
`Repository documentation to read first`, con link a contratti, guide e
paragrafi specifici. La differenza e':

- `Primary roadmap`: il documento che guida la milestone corrente;
- `Repository documentation to read first`: altri documenti utili per capire
  contratti, contesto, test e limiti.

## Label GitHub

Le label servono a rendere issue e PR filtrabili. Una issue puo' avere piu'
label: normalmente almeno una `area:*`, una `kind:*` e, se serve, una
`priority:*` o `status:*`.

| Famiglia | Quando usarla | Label iniziali |
| --- | --- | --- |
| `area:*` | Quale parte del progetto e' coinvolta | `area:core`, `area:backend`, `area:writer`, `area:docs`, `area:tests`, `area:security`, `area:performance`, `area:ci` |
| `kind:*` | Che tipo di lavoro e' | `kind:bug`, `kind:design`, `kind:debt`, `kind:roadmap`, `kind:test`, `kind:audit` |
| `priority:*` | Quanto e' urgente o bloccante | `priority:p0`, `priority:p1`, `priority:p2` |
| `status:*` | Stato operativo extra | `status:needs-discussion`, `status:ready`, `status:blocked`, `status:needs-docs` |
| default GitHub | Categorie generiche gia' presenti | `bug`, `documentation`, `enhancement`, `good first issue`, `help wanted`, `question` |

Esempi:

- una issue sul runtime JSONL puo' usare `area:writer`, `area:performance`,
  `kind:design`, `priority:p1`;
- un bug nei test golden puo' usare `area:tests`, `kind:bug`, `kind:test`;
- una issue madre di audit notturno usa `area:tests`, `kind:audit`;
- una issue figlia nata da audit usa `area:tests`, `kind:audit` e poi
  `kind:bug`, `kind:test` o `kind:debt` in base al risultato;
- una proposta ancora aperta puo' usare `kind:design` e
  `status:needs-discussion`;
- una issue adatta a nuovi contributori puo' usare `good first issue` solo se
  il task e' piccolo, ben documentato e non richiede decisioni architetturali.

La regola principale e':

```text
Discussion = ragionamento vivo
documentazione = decisione consolidata
issue = lavoro da fare
pull request = modifica verificabile
```

Una Discussion puo' contenere domande, dubbi, alternative e risposte emerse
durante la progettazione. Non deve essere letta come contratto definitivo. Dopo
che una scelta e' stata presa, la conclusione va riportata nella documentazione
del repository, perche' gli MD sono la fonte stabile per utenti, studenti e
contributori.

Esempio: prima di decidere come implementare il runtime dei writer possiamo
discutere in GitHub se usare una coda comune o una coda per sink. La Discussion
conserva i pro e contro. Quando scegliamo la strada v0, la decisione entra in
`32-writer-api-v0.md`, `33-writer-runtime-roadmap-v0.md` o nel documento piu'
adatto.

## Creare il fork

Dal browser:

1. apri `https://github.com/kinderp/alfred`
2. premi `Fork`
3. scegli il tuo account come proprietario del fork
4. lascia il nome `alfred`, salvo motivi particolari
5. seleziona la copia del solo branch di default se vuoi contribuire solo a
   `main`
6. premi `Create fork`

GitHub spiega che, per contribuire a un progetto, di solito basta copiare il
branch di default. Gli altri branch servono solo se devi lavorare su una linea
di sviluppo specifica.

Con GitHub CLI, se installata e autenticata:

```bash
gh repo fork kinderp/alfred --clone=true --remote=true
```

Questo comando crea il fork e clona il repository in locale. Se preferisci il
browser, clona poi manualmente il tuo fork.

## Clonare il fork

Nel terminale, scegli una directory di lavoro e clona il tuo fork:

```bash
git clone https://github.com/TUO-UTENTE/alfred.git
cd alfred
```

Controlla i remote configurati:

```bash
git remote -v
```

Dovresti vedere `origin` puntare al tuo fork:

```text
origin  https://github.com/TUO-UTENTE/alfred.git (fetch)
origin  https://github.com/TUO-UTENTE/alfred.git (push)
```

Aggiungi il repository principale come `upstream`:

```bash
git remote add upstream https://github.com/kinderp/alfred.git
git remote -v
```

Ora dovresti vedere sia `origin` sia `upstream`:

```text
origin    https://github.com/TUO-UTENTE/alfred.git (fetch)
origin    https://github.com/TUO-UTENTE/alfred.git (push)
upstream  https://github.com/kinderp/alfred.git (fetch)
upstream  https://github.com/kinderp/alfred.git (push)
```

Questa distinzione e' importante:

- `origin`: il tuo fork, dove puoi fare push
- `upstream`: il repository principale, da cui prendi gli aggiornamenti

## Sincronizzare il fork con upstream

Prima di iniziare un lavoro nuovo, aggiorna sempre il tuo `main` locale dalla
versione ufficiale:

```bash
git switch main
git fetch upstream
git merge upstream/main
git push origin main
```

Significato dei comandi:

- `git switch main`: torna sul branch principale locale
- `git fetch upstream`: scarica i commit nuovi dal repository ufficiale
- `git merge upstream/main`: porta quei commit dentro il tuo `main`
- `git push origin main`: aggiorna il `main` del tuo fork su GitHub

Se il tuo `main` non contiene commit personali, il merge sara' normalmente un
fast-forward. Significa che Git sposta semplicemente il puntatore del branch in
avanti, senza creare un commit di merge.

Dal browser GitHub puoi anche usare il pulsante `Sync fork` nel tuo fork. Se
GitHub rileva conflitti, ti chiedera' di risolverli con una pull request
dedicata. Per imparare Git, pero', e' utile conoscere anche la procedura da
riga di comando.

## Creare un branch di lavoro

Non lavorare direttamente su `main`. Crea un branch piccolo e descrittivo:

```bash
git switch main
git pull origin main
git switch -c fix-signal-shutdown
```

Esempi di nomi buoni:

```text
fix-recursive-watch-comment
docs-contributing-forks
test-rename-file-core
refactor-backend-context
```

Il nome deve aiutare a capire lo scopo del lavoro. Evita nomi generici come
`update`, `changes`, `test` o `work`.

## Prima di modificare codice

Prima di toccare i file:

1. leggi la documentazione del componente che vuoi modificare
2. consulta la mappa del codice se non conosci il flusso
3. compila con `make`
4. controlla lo stato con `git status`
5. fai modifiche piccole e verificabili

Se non conosci ancora il flusso del codice, leggi:

```text
docs/it/16-mappa-codice-e-strutture.md
```

Quel capitolo spiega la codebase come lettura guidata: mostra quali funzioni si
chiamano, quali strutture dati vengono mutate e quali eventi fanno partire ogni
passaggio.

## Test locali prima della pull request

Prima di aprire una PR, esegui almeno:

```bash
make test-ci-policy
make
make test
make test-backend-diagnostics
make test-jsonl
```

`make` verifica che il progetto compili.

`make test` esegue la suite core ufficiale end-to-end. Questa suite controlla
il comportamento semantico che gli utenti del programma vedono nel log eventi.

`make test-backend-diagnostics` controlla i log tecnici del backend inotify,
per esempio aggiunta e rimozione dei watch. Questi eventi non sono semantica
utente, ma sono importanti per capire se il backend mantiene correttamente lo
stato di osservazione.

`make test-jsonl` controlla il contratto esterno strutturato di `output.jsonl`.
Questa suite non sostituisce i test testuali: verifica che i record pubblici
importanti siano JSON validi e abbiano campi stabili.

Se modifichi solo documentazione, `make` puo' bastare. Se modifichi codice,
Makefile, test o semantica eventi, esegui anche le suite collegate al contratto
toccato.

## Aggiungere un test

Prima di scrivere un nuovo test, scegli la suite in base al contratto che vuoi
proteggere:

```text
tests/core/     -> comportamento semantico visto dall'utente
tests/backend/  -> diagnostica e stato interno del backend inotify
tests/scanner/  -> attraversamento filesystem usato da scanner/resync
tests/watcher/  -> struttura dati watcher senza kernel reale
```

Regola pratica:

- se il test controlla eventi come `FILE_CREATED`, `DIR_RELOCATED` o
  `FILE_READY`, va in `tests/core/`
- se il test controlla log come `WATCH_ADDED`, `WATCH_REMOVED`,
  `WATCH_RESYNC_FAILED` o watch descriptor, va in `tests/backend/`
- se il test controlla opzioni di attraversamento directory, errori parziali,
  symlink o limiti dello scanner, va in `tests/scanner/`
- se il test controlla campi e transizioni di `watcher_table_t`, va in
  `tests/watcher/`

Quando aggiungi uno scenario:

1. crea il file nella suite corretta
2. segui lo stile dei test vicini, inclusi commenti solo dove servono davvero
3. verifica se il runner esegue gia' automaticamente il nuovo file
4. se il test e' Bash, leggi la guida alle regex in
   `docs/it/10-debugging-test-e-strumenti.md`
5. aggiorna `docs/it/14-scenari-test.md`
6. aggiorna la documentazione specifica del componente toccato
7. esegui la suite mirata e poi i controlli generali necessari

La guida alle regex serve a evitare due errori opposti: pattern troppo larghi,
che passano anche quando l'evento e' sbagliato, e pattern troppo rigidi, che
falliscono per dettagli instabili come directory temporanee o watch descriptor.

Le suite shell non sono tutte uguali. `tests/backend/run_all.sh` esegue
automaticamente tutti gli script `test_*.sh`, quindi aggiungere un nuovo test
backend con quel nome basta per includerlo nel target
`make test-backend-diagnostics`. Altre suite possono avere runner piu'
espliciti: controlla sempre `run_all.sh` prima di assumere che il nuovo file
venga eseguito.

Se aggiungi una nuova categoria di test, non basta creare una directory: devi
aggiungere un target nel Makefile, documentarlo in
`docs/it/09-makefile-e-build-system.md` e spiegare quando usarlo in questa
guida.

## Aggiornare la documentazione

Ogni modifica che cambia comportamento, architettura, API, test, Makefile o
flusso di sviluppo deve aggiornare anche la documentazione italiana.

Esempi:

- cambi una funzione del core: aggiorna la documentazione sul core
- cambi il backend inotify: aggiorna la documentazione sul modulo inotify
- cambi il Makefile: aggiorna il capitolo sul build system
- aggiungi uno scenario di test: aggiorna gli scenari di test
- cambi il processo per contribuire: aggiorna questo file

Se modifichi una struttura dati, una funzione centrale o il percorso degli
eventi, aggiorna anche:

```text
docs/it/16-mappa-codice-e-strutture.md
```

La documentazione deve restare utile a chi legge il progetto per la prima
volta.

## Aprire la pull request

Quando il branch e' pronto:

```bash
git status
git add FILE_MODIFICATI
git commit
git push origin NOME-BRANCH
```

Poi apri GitHub nel browser. GitHub proporra' di creare una pull request dal
branch del tuo fork verso `kinderp/alfred:main`.

GitHub precompila la descrizione usando questo template:

```text
.github/pull_request_template.md
```

Il template non e' burocrazia: serve a rendere ogni PR leggibile nello stesso
modo. Un reviewer deve capire rapidamente scopo, area modificata, test eseguiti,
documentazione aggiornata e possibili effetti sulla semantica degli eventi.

La PR deve comunque spiegare:

- che problema risolve
- quali file o aree modifica
- quali test hai eseguito
- se ci sono scelte tecniche da discutere
- se la documentazione e' stata aggiornata

Le sezioni principali del template sono:

- `Summary`: descrizione breve del cambiamento
- `Scope`: tipo di modifica, per esempio codice, test, documentazione o CI
- `Details`: lista concreta dei cambiamenti principali
- `Verification`: comandi eseguiti localmente
- `Documentation`: documentazione aggiornata o motivazione se non serve
- `Compatibility and Semantics`: impatto su comportamento utente, eventi
  semantici o diagnostica backend
- `Reviewer Notes`: punti su cui vuoi attenzione particolare

Esempio compilato:

```text
## Summary

- Clarifies the recursive watch documentation.
- Keeps backend diagnostics separate from semantic core events.

## Verification

- [x] git diff --check
- [x] make
- [x] make test
- [x] make test-backend-diagnostics
- [x] make test-jsonl

## Documentation

- Updated docs/it/05-modulo-inotify.md.
```

## GitHub Actions sulla PR

Il repository esegue due workflow GitHub Actions su ogni pull request verso
`main` e su ogni push su `main`.

I workflow si trovano in:

```text
.github/workflows/ci.yml
.github/workflows/linux-userspace.yml
```

Il workflow `CI` e' il gate completo di riferimento ed esegue:

```bash
make test-ci-policy
make
make test
make test-cli
make smoke-mvp
make test-backend-diagnostics
make test-jsonl
make test-compatibility-evidence
make test-install
```

Il workflow `Linux userspace compatibility` non ripete tutta la suite. Esegue
tre lane container release/install su Ubuntu 24.04, Debian 13 e Fedora 44:

```text
dipendenze esplicite
-> contesto userspace
-> make test-install
-> smoke script con ALFRED_BIN puntato al binario release
-> compatibility-evidence-v0.json
-> artifact dedicato della lane
```

Queste lane verificano distribuzione, libc, compilatore, pacchetti e utility
userspace differenti. Condividono il kernel del runner GitHub e quindi non
dimostrano compatibilita' con tre kernel diversi.

Se la CI fallisce, la PR non va mergiata finche' il problema non e' stato
capito. Un test rosso non significa sempre che la tua modifica sia sbagliata,
ma significa che il comportamento atteso e il comportamento reale non coincidono
piu'. Prima si legge l'errore, poi si decide se correggere il codice, correggere
il test o aggiornare esplicitamente il contratto documentato.

## Come leggere il file YAML della CI

Il file `.github/workflows/ci.yml` e' scritto in YAML. YAML e' un formato basato
su chiavi, valori e indentazione. L'indentazione e' parte della sintassi:
spostare una riga a destra o a sinistra puo' cambiarne il significato.

Il workflow corrente e':

```yaml
name: CI

on:
  pull_request:
    branches:
      - main
  push:
    branches:
      - main

permissions:
  contents: read

jobs:
  test:
    name: Build and test
    runs-on: ubuntu-latest
    env:
      ALFRED_KEEP_TEST_LOGS: "1"

    steps:
      - name: Check out repository
        uses: actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd # v6.0.2

      - name: Install build dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential man-db python3

      - name: Validate GitHub Actions pinning policy
        run: make test-ci-policy

      - name: Build
        run: make

      - name: Run official core suite
        run: make test

      - name: Run CLI tests
        run: make test-cli

      - name: Run MVP smoke test
        run: make smoke-mvp

      - name: Run backend diagnostics
        run: make test-backend-diagnostics

      - name: Run JSONL golden tests
        run: make test-jsonl

      - name: Validate compatibility evidence contract
        run: make test-compatibility-evidence

      - name: Run staged installation tests
        run: make test-install

      - name: Upload test logs on failure
        if: failure()
        uses: actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a # v7.0.1
        with:
          name: alfred-test-logs
          if-no-files-found: ignore
          path: |
            tests/core/*.log
            tests/cli/*.log
            tests/backend/*.log
            tests/jsonl/*.log
            tests/jsonl/*.jsonl
            /tmp/alfred_mvp_smoke.*/**
            /tmp/alfred_install_test.*/**
```

`name` e' il nome visibile del workflow nell'interfaccia GitHub Actions. Qui e'
`CI`, cioe' Continuous Integration.

`on` definisce quando il workflow deve partire. In questo progetto parte in due
casi:

- `pull_request` verso `main`
- `push` su `main`

`pull_request` serve a controllare automaticamente le modifiche proposte dai
contributori prima del merge. `push` su `main` serve a verificare anche lo stato
del branch principale dopo un merge.

`branches` limita l'evento a uno o piu' branch. La lista:

```yaml
branches:
  - main
```

significa: esegui il workflow solo quando la PR punta a `main` o quando il push
arriva su `main`.

`permissions` dichiara i permessi concessi al token automatico usato da GitHub
Actions. Qui usiamo:

```yaml
permissions:
  contents: read
```

Questo segue il principio del minimo privilegio: la CI deve leggere il codice,
non modificarlo. Per compilare e testare Alfred non serve permesso di scrittura
sul repository.

`jobs` contiene uno o piu' lavori. Un job e' un insieme di passi eseguiti su una
macchina virtuale GitHub. Qui abbiamo un solo job:

```yaml
jobs:
  test:
```

`test` e' l'identificatore interno del job. Il nome mostrato nell'interfaccia e'
invece:

```yaml
name: Build and test
```

`runs-on` sceglie l'ambiente dove il job viene eseguito:

```yaml
runs-on: ubuntu-latest
```

Significa che GitHub avvia una macchina Linux Ubuntu gestita da GitHub. Questo
e' adatto al progetto perche' Alfred usa API Linux come inotify.

`env` definisce variabili d'ambiente disponibili nei passi del job:

```yaml
env:
  ALFRED_KEEP_TEST_LOGS: "1"
```

Nel progetto questa variabile dice agli script di test di non cancellare
`raw.log`, `events.log` ed `errors.log` alla fine del test. In locale il valore
predefinito e' diverso: i log vengono rimossi durante la cleanup normale. In CI
li conserviamo per poterli caricare come artifact se qualcosa fallisce.

`steps` e' la lista dei passi del job. Ogni elemento della lista comincia con
`-`. I passi vengono eseguiti in ordine: se un passo fallisce, il job fallisce e
i passi successivi normalmente non vengono eseguiti.

Un passo puo' usare un'action gia' pronta:

```yaml
- name: Check out repository
  uses: actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd # v6.0.2
```

`uses` indica un'action esterna. In questo caso `actions/checkout` scarica il
codice della PR dentro la macchina virtuale, cosi' i comandi successivi possono
compilare il repository. Il valore dopo `@` e' lo SHA completo e immutabile del
commit corrispondente alla release `v6.0.2`; il commento mantiene leggibile la
versione senza affidare l'esecuzione a un tag mobile.

Un passo puo' anche eseguire comandi shell:

```yaml
- name: Build
  run: make
```

`run` contiene il comando da eseguire. Se il comando termina con codice diverso
da zero, GitHub considera fallito il passo.

`if` permette di eseguire uno step solo in certe condizioni:

```yaml
if: failure()
```

`failure()` e' una funzione di GitHub Actions. Significa: esegui questo step
solo se uno step precedente del job e' fallito. La usiamo per caricare i log
solo quando servono davvero.

Il blocco:

```yaml
run: |
  sudo apt-get update
  sudo apt-get install -y build-essential
```

usa `|` per scrivere piu' righe di shell nello stesso campo YAML. In questo caso
aggiorna l'indice dei pacchetti e installa gli strumenti base di compilazione C,
come compilatore, linker e make. `man-db` fornisce il comando `man` richiesto
dalla suite staged: la lane di riferimento deve fallire, non saltare il
rendering delle sei pagine.

`with` passa parametri a un'action usata con `uses`. Nel nostro workflow:

```yaml
with:
  name: alfred-test-logs
  if-no-files-found: ignore
  path: |
    tests/core/*.log
    tests/cli/*.log
    tests/backend/*.log
    tests/jsonl/*.log
    tests/jsonl/*.jsonl
    /tmp/alfred_mvp_smoke.*/**
    /tmp/alfred_install_test.*/**
```

Questi campi configurano `actions/upload-artifact` alla release `v7.0.1`,
fissata al relativo SHA completo:

- `name`: nome dell'artifact scaricabile dalla pagina della run
- `if-no-files-found`: non fallire se non trova log da caricare
- `path`: lista dei file da includere nell'artifact, compresi gli stage
  temporanei conservati quando fallisce il test install

`*.log` e' un pattern: significa "tutti i file che finiscono con `.log`".

I passi di verifica rispecchiano comandi disponibili anche a un contributore in
locale:

```yaml
- name: Validate GitHub Actions pinning policy
  run: make test-ci-policy

- name: Build
  run: make

- name: Run official core suite
  run: make test

- name: Run CLI tests
  run: make test-cli

- name: Run MVP smoke test
  run: make smoke-mvp

- name: Run backend diagnostics
  run: make test-backend-diagnostics

- name: Run JSONL golden tests
  run: make test-jsonl

- name: Validate compatibility evidence contract
  run: make test-compatibility-evidence

- name: Run staged installation tests
  run: make test-install

- name: Upload test logs on failure
  if: failure()
  uses: actions/upload-artifact@043fb46d1a93c77aae656e7c1c64a875d1fc6a0a # v7.0.1
```

Questa scelta e' intenzionale: la CI non deve avere una procedura misteriosa
diversa da quella umana. Deve automatizzare gli stessi comandi documentati per i
contributori. `make test-install` resta ultimo perche' ricostruisce il profilo
release dopo che le suite ASan/UBSan hanno gia' verificato la build debug.

### Policy di pinning delle GitHub Actions

Ogni action esterna usata nei file sotto `.github/workflows/` deve rispettare
due requisiti:

1. il riferimento dopo `@` deve essere uno SHA Git completo, minuscolo e di 40
   caratteri;
2. la stessa riga deve terminare con un commento di release leggibile nella
   forma `# vX.Y.Z`.

Un tag come `actions/checkout@v6` e' leggibile, ma puo' essere spostato dal
repository che pubblica l'action. Lo SHA seleziona invece esattamente il codice
revisionato. Il commento rende immediato capire quale release ufficiale
corrisponde al commit senza indebolire il pin. Questa policy segue la
[raccomandazione GitHub per l'uso sicuro delle action di terze parti](https://docs.github.com/en/actions/reference/security/secure-use#using-third-party-actions).

Il controllo locale e CI e':

```bash
make test-ci-policy
```

Lo script usa soltanto la libreria standard di Python, scandisce sia i file
`.yml` sia gli eventuali `.yaml`, ignora action locali e immagini
`docker://`, rifiuta revisioni mobili o commenti di versione mancanti e
fallisce anche se non trova alcuna action esterna. Il job `CI` esegue questo
controllo prima della build; non serve ripeterlo in ogni lane della matrice
perche' la scansione copre tutti i workflow del repository.

Per aggiornare un'action bisogna leggere le release note ufficiali, risolvere
la release scelta nel relativo SHA, aggiornare insieme SHA e commento e
verificare sia il job di riferimento sia la matrice userspace quando l'action
e' condivisa. Il pin immutabile protegge dallo spostamento del tag, non rende
automaticamente affidabile il codice upstream: provenienza, modifiche e
permessi restano parte della review. Alfred mantiene per ora questi due
aggiornamenti in PR piccole e dedicate; introdurre automazione come Dependabot
non e' giustificato finche' il numero di dipendenze CI resta cosi' limitato.

Se in futuro aggiungiamo un nuovo controllo obbligatorio, per esempio un
formatter o una suite di test aggiuntiva, bisogna aggiornare il workflow
interessato e questa guida. Per il gate completo il file e':

- `.github/workflows/ci.yml`

Per la compatibilita' userspace il file e':

- `.github/workflows/linux-userspace.yml`

### Come funziona la matrice userspace

Una `matrix` GitHub Actions crea piu' job dallo stesso template. Alfred usa
`matrix.include` per associare a ogni lane tre informazioni:

| Campo | Esempio | Significato |
| --- | --- | --- |
| `lane` | `debian-13` | nome stabile mostrato nella UI |
| `image` | `debian:13-slim` | userspace container eseguito |
| `package_manager` | `apt` | ramo che installa le dipendenze della lane |

Il job gira comunque su `runs-on: ubuntu-latest`; la chiave `container.image`
sposta i comandi dentro l'immagine selezionata. Questo cambia lo userspace, ma
non avvia un kernel guest. Il workflow stampa quindi sia l'immagine sia
`uname -a` e marca esplicitamente il kernel come condiviso con il runner.

La strategia usa:

```yaml
fail-fast: false
```

Se Fedora fallisce, Ubuntu e Debian continuano. Questo costa qualche minuto in
piu', ma fornisce una diagnosi migliore: possiamo distinguere un errore comune
da un problema legato a una sola distribuzione.

Le dipendenze sono dichiarate per famiglia e non date per scontate. Le lane
Debian-like usano `apt`, Fedora usa `dnf`; tutte installano compilatore, Make,
Bash, utility GNU/Linux, Python e `man`. La matrice deve fallire se una pagina
man non viene renderizzata o se lo smoke runtime non osserva gli effetti
attesi.

Dopo `make test-install` la lane non usa il target `make smoke-mvp`, perche'
quel target dipende dal normale `all` debug. Richiama invece
`tests/smoke/mvp_smoke.sh` con `ALFRED_BIN` puntato a `./alfred`: cosi' lo smoke
prova lo stesso artefatto release gia' usato dal test staged e Fedora non deve
installare ASan/UBSan solo per un rilink debug estraneo allo scopo della lane.

L'installazione dei pacchetti richiede `root`, ma i test no. Prima di eseguire
Alfred, il workflow assegna il checkout a UID/GID 1001 e usa `setpriv` per
ridurre i privilegi. In questo modo il controllo sulla sorgente illeggibile del
test staged resta reale: `root` potrebbe leggere il file anche con i bit di
lettura rimossi e produrre un risultato ingannevole.

Le due invocazioni usano anche `--no-new-privs`. Oltre a cambiare UID, GID e
gruppi supplementari, questo chiede al kernel di impedire ai processi di
ottenere nuovi privilegi attraverso una successiva `execve()`, per esempio
eseguendo un programma setuid o setgid. E' un rafforzamento del confine usato
dalla CI, non una sandbox completa: il codice continua a essere eseguito nel
container del job e resta soggetto alle risorse e agli accessi concessi da
GitHub Actions.

### Evidence versionata e artifact per lane

Alla fine del percorso, anche se staged-install o smoke falliscono, lo step con
`if: always()` prova a eseguire
`tools/ci/compatibility_evidence.py`. Il file prodotto e':

```text
.alfred-ci/compatibility-evidence-v0.json
```

e viene caricato per 30 giorni con un nome distinto per lane:

```text
alfred-compatibility-evidence-ubuntu-24.04
alfred-compatibility-evidence-debian-13
alfred-compatibility-evidence-fedora-44
```

Poiche' `.alfred-ci` e' una directory nascosta, lo step di upload abilita
esplicitamente `include-hidden-files: true`; il `path` resta limitato al solo
JSON previsto e non carica indiscriminatamente altri file nascosti del checkout.

Un artifact GitHub Actions e' un file prodotto dal job e conservato fuori dal
repository. Non e' un commit e non modifica il prodotto. Serve a poter
scaricare in seguito la prova esatta associata a una run, invece di dipendere
solo dal testo temporaneo del log.

Il campo `source_revision` indica la revisione realmente testata dal workflow,
non sempre l'HEAD visibile del branch. In una run `push` e' normalmente il
commit pubblicato; in una run `pull_request`, `${{ github.sha }}` puo' indicare
il merge commit sintetico `refs/pull/<numero>/merge` creato da GitHub. Per
ricostruire la provenienza bisogna quindi leggere insieme `source_revision`,
`ci.run_id`, `ci.run_attempt` e i metadati della run/PR. Una differenza fra
branch HEAD e merge ref non e' da sola un errore: significa che GitHub ha
verificato l'integrazione proposta con la base corrente.

Il record contiene esclusivamente campi ammessi: versione schema, revisione,
run CI, lane, immagine, distribuzione, architettura, libc, compilatore,
filesystem della root sorgente, kernel e relativo scope, profilo build,
sanitizer ed esiti. Non
contiene hostname, utente, path del workspace, segreti o dump completo delle
variabili d'ambiente. Una sonda indisponibile produce `unknown`: il generatore
non inventa un valore e non trasforma l'assenza di prova in successo.

Lo step puo' essere eseguito solo dopo il bootstrap delle dipendenze. Se il
runner viene cancellato forzatamente, Python non e' stato installato o il job
non puo' piu' eseguire step finali, l'artifact puo' mancare; il workflow usa
`if-no-files-found: warn` per non nascondere il fallimento originale dietro un
secondo errore di upload. L'assenza dell'artifact resta visibile e non vale come
evidenza positiva.

Per scaricarlo: apri la run, scorri alla sezione `Artifacts` e scegli il nome
della lane. Il JSON permette di distinguere subito, per esempio, un Fedora
userspace sul kernel condiviso del runner da una futura VM con kernel guest.

La matrice non e' un benchmark: durata di download e installazione pacchetti
domina il tempo del job. Non va usata per confrontare le prestazioni di Alfred
fra distribuzioni.

## Leggere output ed errori su GitHub Actions

Quando una pull request viene aperta o aggiornata, GitHub mostra lo stato della
CI nella pagina della PR. Se tutto passa, il controllo diventa verde. Se un
comando fallisce, il controllo diventa rosso e bisogna leggere i log della run.

Percorso dal browser:

1. apri il repository su GitHub
2. entra nel tab `Actions`
3. seleziona il workflow `CI`
4. apri la run piu' recente o quella collegata alla PR
5. clicca il job `Build and test`
6. apri lo step fallito

Gli step principali sono:

- `Build`
- `Run official core suite`
- `Run CLI tests`
- `Run MVP smoke test`
- `Run backend diagnostics`
- `Run JSONL golden tests`
- `Validate compatibility evidence contract`
- `Run staged installation tests`

Ogni step mostra l'output standard e l'output di errore del comando eseguito.
Questo significa che nello step `Run official core suite` vedrai l'output di:

```bash
make test
```

Nello step `Run backend diagnostics` vedrai l'output di:

```bash
make test-backend-diagnostics
```

Se uno script di test stampa un messaggio `FAIL`, quel messaggio appare nel log
dello step. Per esempio:

```text
Running test_move_file.sh
FAIL: missing FILE_MOVED event
```

In quel caso la cosa giusta da fare non e' correggere a caso. Bisogna capire
quale contratto e' cambiato:

- il codice ha introdotto una regressione?
- il test si aspetta un comportamento vecchio?
- la documentazione descrive un comportamento diverso dal codice?
- manca un aggiornamento agli scenari di test?

GitHub permette anche di scaricare tutti i log testuali della run:

1. apri la run
2. usa il menu con i tre puntini
3. scegli `Download log archive`

In piu', quando la CI fallisce, il workflow carica un artifact chiamato:

```text
alfred-test-logs
```

L'artifact contiene i log runtime prodotti dai test, se presenti:

```text
tests/core/raw.log
tests/core/events.log
tests/core/errors.log
tests/backend/raw.log
tests/backend/events.log
tests/backend/errors.log
```

Questi file sono piu' dettagliati dell'output visibile nello step. L'output
dello step dice quale test e' fallito; i log runtime aiutano a capire quali
eventi raw, semantici o diagnostici sono stati realmente prodotti da Alfred.

Per scaricare l'artifact:

1. apri la run fallita
2. cerca la sezione `Artifacts`
3. clicca `alfred-test-logs`
4. scarica ed estrai lo zip

L'artifact viene caricato solo su fallimento, per non conservare log inutili
quando tutto passa.

La matrice userspace segue una regola diversa: il piccolo artifact
`alfred-compatibility-evidence-<lane>` viene caricato sia su successo sia su
fallimento, quando lo step finale puo' essere eseguito. E' evidenza di
compatibilita' e provenienza, non un dump diagnostico dei log Alfred.

## Review e merge

Il contributore non deve fare push diretto su `main`.

Il processo corretto e':

1. il contributore apre una PR dal proprio fork
2. la CI esegue i test
3. un maintainer legge il diff
4. eventuali commenti vengono risolti con nuovi commit sullo stesso branch
5. quando test e review sono ok, il maintainer fa merge

### Come gestire i finding della review

Un finding e' un problema tecnico individuato durante la review: bug, contratto
ambiguo, test mancante, rischio di regressione, documentazione insufficiente o
responsabilita' architetturale nel modulo sbagliato.

Per Alfred i finding importanti devono essere lasciati come commenti inline sul
codice della PR. Un commento inline e' preferibile a un riepilogo generico
perche' collega il problema alla riga, al file e al diff preciso che lo ha
generato.

Quando un finding viene risolto, il commit di fix deve essere tracciabile dalla
PR:

```text
finding inline nella PR
-> commit che applica il fix
-> risposta al finding con SHA-1 del commit e spiegazione
```

Il messaggio del commit deve dire esplicitamente quale finding risolve. Nel body
del commit includere:

```text
Fixes review finding:
- PR: https://github.com/kinderp/alfred/pull/N
- Finding: https://github.com/kinderp/alfred/pull/N#discussion_rID
```

Subito dopo il commit, aggiungere una risposta al commento inline del finding.
La risposta deve essere in inglese e deve contenere:

- SHA-1 del commit di fix;
- spiegazione sintetica del problema;
- spiegazione della soluzione applicata;
- test o documentazione aggiornati, se rilevanti.

Esempio:

```text
Fixed in 31cb28f87117b8fe7bcc5395d50e2bf855d65754.

The fix makes init fail closed when the writer still has pending buffered
bytes. This prevents accidental JSONL ledger loss during reinitialization.

I also added a regression test that seeds pending bytes, calls init, and
verifies that the buffer and used counter are preserved.
```

Questa procedura rende la review una fonte storica consultabile: dopo mesi si
puo' leggere il finding, vedere il commit che lo ha risolto e capire perche' la
soluzione e' stata scelta.

Se devi aggiornare la PR dopo una review:

```bash
git status
git add FILE_MODIFICATI
git commit
git push origin NOME-BRANCH
```

La PR si aggiorna automaticamente perche' punta a quel branch.

Non usare `git push --force` a meno che un maintainer lo chieda esplicitamente.
Per un contributore nuovo, commit aggiuntivi chiari sono piu' semplici da
rivedere e meno rischiosi.

### Validatori e contratti pubblici interni

Quando una modifica introduce o aggiorna un validatore di contratto, i casi
rifiutati dal validatore devono essere documentati esplicitamente. Un test che
fallisce non basta: un contributore deve poter leggere il contratto prima di
toccare il codice.

Regola pratica:

```text
se il validatore rifiuta un caso,
quel caso deve comparire in header/docstring, MD di contratto, guida didattica
o contributori, test e pagine man quando il contratto e' visibile fuori dal
codice.
```

Per esempio, `alfred_backend_ops_is_minimally_valid()` rifiuta:

- descriptor `ops` nullo;
- nome backend nullo o vuoto;
- versione API diversa da Backend API v0;
- descriptor capabilities nullo;
- nome capabilities nullo o vuoto;
- nome ops e nome capabilities incoerenti;
- versione ops e versione capabilities incoerenti;
- capability flags vuote;
- callback lifecycle mancanti.

Se in futuro questo elenco cambia, la PR deve aggiornare insieme:

- header o docstring della funzione;
- documento di contratto, per esempio `30-backend-api-v0.md`;
- guida didattica, se il concetto e' utile agli studenti;
- test mirati;
- man page collegata, se il contratto e' esposto come riferimento operativo.

## Stile dei commenti nel codice

I commenti nel codice devono seguire:

```text
docs/commenting-style.md
```

I commenti nel codice sono in inglese. La documentazione didattica in
`docs/it/` e' in italiano.

Un buon commento spiega responsabilita', invarianti, contratti o motivazioni
tecniche. Non deve ripetere letteralmente quello che il codice gia' dice.
