# Alfred

Alfred e' un motore di eventi filesystem Linux scritto in C.

Osserva uno o piu' alberi di directory con `inotify`, registra fatti raw del
backend e produce un flusso di eventi semantici attraverso un core dedicato.

Questa e' la traduzione italiana del README pubblico. Il contratto operativo di
riferimento resta il [README inglese](README.md); questa pagina serve a rendere
l'MVP corrente piu' accessibile agli studenti e ai nuovi contributori italiani.

## Stato

Alfred e' in sviluppo attivo.

Il percorso runtime corrente e':

```text
Linux inotify -> evento raw Alfred -> evento semantico del core
```

Il vecchio percorso storico di confronto legacy/shadow e' stato rimosso dal
runtime attivo. Il core engine e' ora il motore eventi ufficiale.

## Funzionalita'

- osservazione ricorsiva di directory con Linux `inotify`
- log degli eventi raw del backend
- eventi semantici per file e directory
- eventi di creazione, cancellazione, modifica e file pronto dopo close-write
- eventi di creazione e cancellazione directory
- correlazione di move, rename e relocate
- diagnostica backend per aggiunta e rimozione dei watch
- suite end-to-end del core
- suite di diagnostica backend
- CI GitHub Actions per pull request e `main`

## Requisiti

- Linux
- `make`
- una toolchain C, per esempio `gcc`
- strumenti shell standard usati dagli script di test
- Python 3 per i test JSONL, smoke e compatibility evidence

Su Debian o Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y build-essential
```

Per eseguire anche il test di installazione staged serve `man`:

```bash
sudo apt-get install -y man-db
```

### Ambienti testati

Alfred viene testato in CI sugli userspace Ubuntu 24.04, Debian 13 e Fedora 44.
Sono ambienti testati, non una promessa stabile di supporto. Le lane container
condividono il kernel del runner GitHub Actions, quindi non costituiscono tre
test indipendenti del kernel. La compatibilita' kernel viene tracciata a parte.

Ogni lane prova a pubblicare un artifact di compatibility evidence versionato
con revisione sorgente, identita' userspace, toolchain, profilo di build, scope
del kernel ed esiti normalizzati dei test staged-install e smoke. Un artifact
mancante non e' evidenza positiva. La
[roadmap della compatibilita' Linux](docs/it/53-installability-linux-compatibility-v0.md)
descrive il contratto e distingue ambienti testati, supportati e non testati.

## Avvio rapido

Clona e compila:

```bash
git clone https://github.com/kinderp/alfred.git
cd alfred
make
```

Compila il profilo release e installa il binario con le sei pagine man inglesi
e italiane:

```bash
make release
sudo make install
```

`make install` esegue soltanto copie: non compila mai come root e fallisce se
non trova un eseguibile gia' costruito, leggibile ed eseguibile. Il prefisso
predefinito e' `/usr/local`. Package builder e test locali possono usare uno
stage senza root:

```bash
stage_dir=$(mktemp -d)
make release
make DESTDIR="$stage_dir" PREFIX=/usr install
make DESTDIR="$stage_dir" PREFIX=/usr uninstall
```

`uninstall` rimuove soltanto il binario Alfred e le sei pagine man. Non rimuove
directory condivise, configurazioni utente o log runtime.

La gerarchia di destinazione deve essere controllata dall'amministratore o dal
package builder che esegue il comando. V0 usa normali operazioni sui path del
filesystem, che possono seguire componenti symlink gia' presenti: non eseguire
un'installazione privilegiata con un `DESTDIR` controllato da un attaccante.
Lo staging non-root mostrato sopra rende esplicito questo confine di fiducia e
ne limita l'impatto.

Avvia Alfred su uno o piu' path:

```bash
./alfred /path/da/osservare
```

Mostra uso breve e versione senza avviare il runtime:

```bash
./alfred --help
./alfred -h
./alfred --version
./alfred -V
```

Valida la configurazione corrente senza avviare il runtime:

```bash
./alfred --check-config
ALFRED_CONFIG=./alfred.conf ./alfred --check-config
./alfred -c ./alfred.conf --check-config
./alfred --config ./alfred.conf --check-config
./alfred --config=./alfred.conf --check-config
```

Avvia Alfred con un file di configurazione:

```bash
ALFRED_CONFIG=./alfred.conf ./alfred /path/da/osservare
./alfred -c ./alfred.conf /path/da/osservare
./alfred --config ./alfred.conf /path/da/osservare
./alfred --config=./alfred.conf /path/da/osservare
./alfred -- /path/da/osservare
```

Configurazione minima dell'output JSONL opzionale:

```text
output_enabled=false
output_format=jsonl
output_buffer_size=65536
output_log=output.jsonl
```

Il default mantiene invariati i log correnti. Impostando
`output_enabled=true`, Alfred aggiunge output JSONL per il percorso record gia'
cablato, preservando i log di compatibilita'.

I dettagli di configurazione sono documentati in:
[livello applicazione](docs/it/04-livello-applicazione.md) e
[debugging, test e strumenti](docs/it/10-debugging-test-e-strumenti.md).

Ferma Alfred con `Ctrl+C`.

## Log

Alfred scrive tre log runtime nella directory corrente:

```text
raw.log
events.log
errors.log
```

`raw.log` contiene informazioni raw del backend inotify.

`events.log` contiene eventi semantici emessi dal core ed eventi diagnostici
backend come `WATCH_ADDED` e `WATCH_REMOVED`.

`errors.log` contiene diagnostica di errore.

I log runtime sono ignorati da Git e non devono essere committati.

## Test

Compila il progetto:

```bash
make
```

Esegui la suite core end-to-end ufficiale:

```bash
make test
```

Esegui i test CLI:

```bash
make test-cli
```

Esegui lo smoke test MVP breve:

```bash
make smoke-mvp
```

Questo compila Alfred, controlla `--help`, `--version` e `--check-config`,
avvia il runtime su una directory temporanea con output JSONL abilitato, crea
un file, lo rinomina, crea una directory e valida `raw.log`, `events.log` e
`output.jsonl`. E' un controllo rapido orientato all'utente, non sostituisce le
suite complete core, backend o JSONL.

Esegui la diagnostica backend:

```bash
make test-backend-diagnostics
```

Esegui i test del componente scanner:

```bash
make test-scanner
```

Esegui la suite del contratto di installazione staged senza root:

```bash
make test-install
```

Il test ricostruisce il profilo release, controlla layout predefinito e
override, esegue la CLI informativa staged, renderizza tutte le sei pagine man
e verifica preflight e ownership di uninstall. In CI e' intenzionalmente
l'ultima suite perche' lascia il checkout nel profilo release.

Valida lo schema e il generatore della compatibility evidence versionata:

```bash
make test-compatibility-evidence
```

I workflow CI eseguono build, core, CLI, smoke MVP, diagnostica backend, test
JSONL, compatibility evidence, installazione staged e matrice userspace Linux
sulle pull request verso `main` e sui push a `main`.

Per conservare i log runtime durante il debug locale dei test:

```bash
ALFRED_KEEP_TEST_LOGS=1 make test
ALFRED_KEEP_TEST_LOGS=1 make test-backend-diagnostics
```

## Documentazione

La documentazione dettagliata e' mantenuta in italiano:

- [Indice documentazione italiana](docs/it/README.md)
- [Istruzioni per agenti AI di coding](AGENTS.md)
- [Guida alla lettura della documentazione](docs/it/27-guida-lettura-documentazione.md)
- [Regole operative](docs/it/00-regole-operative.md)
- [Panoramica progetto](docs/it/01-panoramica-progetto.md)
- [Architettura](docs/it/02-architettura-generale.md)
- [Struttura repository](docs/it/03-struttura-cartelle.md)
- [Livello applicazione](docs/it/04-livello-applicazione.md)
- [Backend inotify](docs/it/05-modulo-inotify.md)
- [Core engine](docs/it/06-core-engine.md)
- [Flusso eventi](docs/it/07-flusso-eventi.md)
- [Guida C usato nel progetto](docs/it/08-guida-c-usato-nel-progetto.md)
- [Makefile e build system](docs/it/09-makefile-e-build-system.md)
- [Debugging, test e strumenti](docs/it/10-debugging-test-e-strumenti.md)
- [Guida per contribuire](docs/it/11-come-contribuire.md)
- [Semantica eventi](docs/it/13-semantica-eventi.md)
- [Scenari test](docs/it/14-scenari-test.md)
- [Mappa codice e strutture dati](docs/it/16-mappa-codice-e-strutture.md)
- [Roadmap documentazione avanzata](docs/it/17-roadmap-documentazione-avanzata.md)
- [Note sul modello licenze](docs/it/18-modello-licenze.md)
- [Roadmap CLI e pagina man](docs/it/19-roadmap-cli-e-man-page.md)
- [Matrice eventi inotify](docs/it/20-matrice-eventi-inotify.md)
- [Roadmap scanner e resync](docs/it/21-roadmap-scanner-resync.md)
- [Contratto log](docs/it/22-contratto-log.md)
- [Roadmap plugin backend](docs/it/23-roadmap-plugin-backend.md)
- [Roadmap AI agent guardrail](docs/it/24-roadmap-ai-agent-guardrail.md)
- [Roadmap unificata dossier](docs/it/25-roadmap-unificata-dossier.md)
- [Matrice funzionalita' supportate](docs/it/26-stato-funzionalita.md)
- [Audit documentazione e debiti dichiarati](docs/it/28-audit-documentazione-e-debiti.md)
- [Event Model v0](docs/it/29-event-model-v0.md)
- [Backend API v0](docs/it/30-backend-api-v0.md)
- [Milestone backend inotify di riferimento](docs/it/31-milestone-inotify-reference-backend.md)
- [MVP operational usability v0](docs/it/49-mvp-operational-usability-v0.md)
- [CLI parser v0](docs/it/50-cli-parser-v0.md)
- [Post-MVP documentation and man pages v0](docs/it/51-post-mvp-documentation-man-pages-v0.md)
- [Glossario](docs/it/glossario.md)
- [Stato documentazione](docs/it/documentation-progress.md)

Le pagine man draft sono disponibili localmente:

- [alfred(1)](docs/man/man1/alfred.1)
- [alfred.conf(5)](docs/man/man5/alfred.conf.5)
- [alfred-events(7)](docs/man/man7/alfred-events.7)

Le traduzioni italiane delle pagine man sono in:

- [alfred(1) italiano](docs/man/it/man1/alfred.1)
- [alfred.conf(5) italiano](docs/man/it/man5/alfred.conf.5)
- [alfred-events(7) italiano](docs/man/it/man7/alfred-events.7)

Si possono leggere senza installarle con:

```bash
man -l docs/man/man1/alfred.1
man -l docs/man/man5/alfred.conf.5
man -l docs/man/man7/alfred-events.7
man -l docs/man/it/man1/alfred.1
man -l docs/man/it/man5/alfred.conf.5
man -l docs/man/it/man7/alfred-events.7
```

Gli strumenti di navigazione del codice sono documentati separatamente:

- [Strumenti di code browsing](docs/code-browser/README.md)
- [Sourcebot browser](docs/sourcebot-browser/README.md)
- [Kythe browser](docs/kythe-browser/README.md)
- [Script aggregati di code browsing](tools/code-browsing/README.md)

I commenti nel codice sono scritti in inglese. La documentazione didattica e'
attualmente in italiano.

## Contribuire

I nuovi contributori dovrebbero lavorare da un fork e aprire una pull request
verso `main`.

Inizia da qui:

- [Guida per contribuire](docs/it/11-come-contribuire.md)
- [Stile dei commenti](docs/commenting-style.md)

Prima di aprire una pull request, esegui:

```bash
git diff --check
make
make test
make test-cli
make smoke-mvp
make test-jsonl
make test-backend-diagnostics
make test-compatibility-evidence
```

## Roadmap

Il lavoro a breve termine include:

- documentazione inglese aggiuntiva
- progettazione di overflow e risincronizzazione
- pulizia degli archivi storici legacy/shadow
- altre mappe del codice, diagrammi e percorsi guidati di lettura
- separazione continua tra diagnostica backend e comportamento semantico core

## Licenza

Nessuna licenza e' stata ancora selezionata.

Finche' non viene aggiunto un file `LICENSE`, questo repository non deve essere
considerato un pacchetto open source con permessi di riuso chiaramente concessi.
La scelta della licenza verra' discussa prima di presentare il progetto come
riutilizzabile da terzi.

La direzione prevista e' un core open source con la possibilita' di licenze
separate commerciali o source-available per moduli avanzati futuri. Vedi le
note italiane sul modello licenze in
[docs/it/18-modello-licenze.md](docs/it/18-modello-licenze.md).
