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
make
make test
make test-backend-diagnostics
```

`make` verifica che il progetto compili.

`make test` esegue la suite core ufficiale end-to-end. Questa suite controlla
il comportamento semantico che gli utenti del programma vedono nel log eventi.

`make test-backend-diagnostics` controlla i log tecnici del backend inotify,
per esempio aggiunta e rimozione dei watch. Questi eventi non sono semantica
utente, ma sono importanti per capire se il backend mantiene correttamente lo
stato di osservazione.

Se modifichi solo documentazione, `make` puo' bastare. Se modifichi codice,
Makefile, test o semantica eventi, esegui anche entrambe le suite.

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

La PR deve spiegare:

- che problema risolve
- quali file o aree modifica
- quali test hai eseguito
- se ci sono scelte tecniche da discutere
- se la documentazione e' stata aggiornata

Esempio di descrizione:

```text
Summary
- Clarifies the recursive watch documentation.
- Keeps backend diagnostics separate from semantic core events.

Verification
- make
- make test
- make test-backend-diagnostics

Documentation
- Updated docs/it/05-modulo-inotify.md.
```

## GitHub Actions sulla PR

Il repository esegue una GitHub Action su ogni pull request verso `main` e su
ogni push su `main`.

Il workflow si trova in:

```text
.github/workflows/ci.yml
```

La CI esegue:

```bash
make
make test
make test-backend-diagnostics
```

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

    steps:
      - name: Check out repository
        uses: actions/checkout@v4

      - name: Install build dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential

      - name: Build
        run: make

      - name: Run official core suite
        run: make test

      - name: Run backend diagnostics
        run: make test-backend-diagnostics
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

`steps` e' la lista dei passi del job. Ogni elemento della lista comincia con
`-`. I passi vengono eseguiti in ordine: se un passo fallisce, il job fallisce e
i passi successivi normalmente non vengono eseguiti.

Un passo puo' usare un'action gia' pronta:

```yaml
- name: Check out repository
  uses: actions/checkout@v4
```

`uses` indica un'action esterna. `actions/checkout@v4` scarica il codice della
PR dentro la macchina virtuale, cosi' i comandi successivi possono compilare il
repository.

Un passo puo' anche eseguire comandi shell:

```yaml
- name: Build
  run: make
```

`run` contiene il comando da eseguire. Se il comando termina con codice diverso
da zero, GitHub considera fallito il passo.

Il blocco:

```yaml
run: |
  sudo apt-get update
  sudo apt-get install -y build-essential
```

usa `|` per scrivere piu' righe di shell nello stesso campo YAML. In questo caso
aggiorna l'indice dei pacchetti e installa gli strumenti base di compilazione C,
come compilatore, linker e make.

Gli ultimi tre passi rispecchiano il controllo che deve fare anche un
contributore in locale:

```yaml
- name: Build
  run: make

- name: Run official core suite
  run: make test

- name: Run backend diagnostics
  run: make test-backend-diagnostics
```

Questa scelta e' intenzionale: la CI non deve avere una procedura misteriosa
diversa da quella umana. Deve automatizzare gli stessi comandi documentati per i
contributori.

Se in futuro aggiungiamo un nuovo controllo obbligatorio, per esempio un
formatter o una suite di test aggiuntiva, bisogna aggiornare due posti:

- `.github/workflows/ci.yml`
- questa guida contributori

## Review e merge

Il contributore non deve fare push diretto su `main`.

Il processo corretto e':

1. il contributore apre una PR dal proprio fork
2. la CI esegue i test
3. un maintainer legge il diff
4. eventuali commenti vengono risolti con nuovi commit sullo stesso branch
5. quando test e review sono ok, il maintainer fa merge

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

## Stile dei commenti nel codice

I commenti nel codice devono seguire:

```text
docs/commenting-style.md
```

I commenti nel codice sono in inglese. La documentazione didattica in
`docs/it/` e' in italiano.

Un buon commento spiega responsabilita', invarianti, contratti o motivazioni
tecniche. Non deve ripetere letteralmente quello che il codice gia' dice.
