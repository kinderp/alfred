# Audit installazione e assunzioni CI v0

Questo documento fotografa come Alfred viene compilato, eseguito e verificato
prima di introdurre `make install`, `make uninstall` e la matrice Linux
userspace.

E' un audit dello stato corrente, non la descrizione di funzionalita' gia'
implementate. Quando usa parole come "proposto" o "futuro", sta indicando il
contratto che verra' implementato nei micro-step successivi.

## Stato successivo all'audit

L'audit fotografa intenzionalmente il repository prima del micro-step
[#266](https://github.com/kinderp/alfred/issues/266). Quel micro-step ha poi
implementato il contratto raccomandato senza modificare il runtime C:

- il Makefile espone `PREFIX`, `DESTDIR`, `BINDIR`, `MANDIR`, `install` e
  `uninstall`;
- `install` e' copy-only e valida layout e sette sorgenti prima di modificare
  la destinazione;
- `uninstall` rimuove soltanto i sette path canonici e conserva file sentinella
  estranei;
- `make test-install` costruisce release e prova layout standard e custom,
  modi, CLI, sei pagine man e casi negativi;
- il job CI di riferimento installa `man-db`, esegue la suite staged per ultima
  e conserva lo stage temporaneo come artifact in caso di fallimento.

La issue [#268](https://github.com/kinderp/alfred/issues/268) aggiunge il livello
userspace successivo senza cambiare questa fotografia storica:

- `.github/workflows/linux-userspace.yml` esegue lane container separate per
  `ubuntu:24.04`, `debian:13-slim` e `fedora:44`;
- ogni lane installa dipendenze esplicite, registra il contesto, esegue
  `make test-install` e poi lo smoke script sullo stesso binario release come
  UID/GID non privilegiato;
- il job completo `ubuntu-latest` resta separato e continua a fornire la
  copertura debug ASan/UBSan;
- i container condividono il kernel del runner e non costituiscono evidenza
  multi-kernel.

Le sezioni che seguono restano al passato come evidenza storica del problema e
delle motivazioni. Il contratto vigente e' descritto nella
[roadmap della milestone](53-installability-linux-compatibility-v0.md), nella
[guida Makefile](09-makefile-e-build-system.md) e nella
[guida ai test](10-debugging-test-e-strumenti.md).

## Domanda dell'audit

La domanda principale e':

```text
quali file costituiscono oggi il prodotto Alfred,
come vengono costruiti,
quali dipendenze implicite hanno i test,
e cosa deve verificare una futura installazione staged?
```

L'audit serve a evitare quattro errori:

- installare un binario debug con dipendenze dai sanitizer;
- copiare file generati, log o oggetti che non appartengono al prodotto;
- dichiarare una distribuzione compatibile senza avere installato le
  dipendenze realmente usate dai test;
- implementare `uninstall` con pattern troppo larghi che potrebbero cancellare
  file non posseduti da Alfred.

## Esito sintetico

Lo stato osservato e':

| Area | Stato corrente | Gap da chiudere |
| --- | --- | --- |
| Binario | `make` e `make release` producono `./alfred` | nessun target di installazione |
| Oggetti | `build/obj/core/**.{o,d}` | correttamente non installabili |
| Man page | sei sorgenti roff EN/IT sotto `docs/man` | non vengono installate |
| Configurazione | nessun file di esempio tracciato | non installare config in v0 |
| Build debug | `-O0`, debug info, ASan e UBSan | non e' l'artefatto utente |
| Build release | `-O2 -DNDEBUG`, senza sanitizer | deve alimentare lo stage install |
| CI | un job completo su `ubuntu-latest` | nessuna build release o matrice userspace |
| Kernel | kernel del runner GitHub | nessuna evidenza multi-kernel |
| Test shell | Bash e utility GNU/Linux | dipendenze non ancora inventariate nella CI |
| JSONL/smoke | usano anche `python3` | `python3` e' implicito nel runner corrente |

La conclusione e' che il prossimo passo non deve ancora creare pacchetti. Deve
aggiungere un contratto Make piccolo e verificabile:

```text
make release
-> make DESTDIR=<stage> PREFIX=/usr install
-> verifica file, modi e CLI senza runtime
-> make DESTDIR=<stage> PREFIX=/usr uninstall
-> verifica che siano spariti solo i file Alfred
```

In questo percorso `install` e' deliberatamente copy-only: non dipende da
`all` o `release` e non compila. Prima di modificare lo stage esegue un
preflight read-only di tutti i sette artefatti sorgente: `./alfred` deve essere
un file regolare leggibile ed eseguibile e ciascuna pagina man deve essere un
file regolare leggibile. E' il test staged a eseguire `make release` come
operazione precedente e non privilegiata.

La separazione evita che un comune `sudo make install` ricompili dentro il
checkout creando file posseduti da root. V0 non puo' pero' dimostrare da solo
che un `./alfred` gia' presente sia davvero release: fuori dal test staged,
costruire il profilo corretto resta una precondizione documentata del chiamante.

## Metodo usato

L'audit ha letto:

- `Makefile`;
- `.github/workflows/ci.yml`;
- `.gitignore`;
- le sei pagine sotto `docs/man`;
- gli script delle suite core, CLI, smoke, backend e JSONL;
- README, guida Makefile, guida test e roadmap CLI/man page.

Ha inoltre eseguito:

```bash
make -n all
make -n release
make release
file ./alfred
ldd ./alfred
./alfred --version
./alfred --help
```

Questi comandi descrivono la macchina di audit. Non trasformano automaticamente
quel risultato in un requisito universale per ogni distribuzione.

## Build corrente

### Artefatto principale

Il Makefile definisce:

```make
TARGET := alfred
```

Il linker scrive il risultato nella root del repository:

```text
./alfred
```

Il percorso non e' ancora configurabile separatamente. Non esistono
`BINDIR`, `PREFIX` o `DESTDIR`.

### Oggetti e dependency file

I sorgenti vengono tradotti in:

```text
build/obj/core/app/src/*.o
build/obj/core/core/src/*.o
build/obj/core/modules/inotify/src/*.o
```

L'opzione `-MMD -MP` genera accanto agli oggetti i file `.d` usati da GNU Make
per ricostruire quando cambiano gli header.

Questi file sono cache di build:

- appartengono al checkout;
- possono essere cancellati da `make clean`;
- non devono essere copiati in uno stage;
- non devono essere rimossi da `make uninstall`.

### Profilo debug

`make` usa:

```text
-g3 -O0 -DDEBUG
-fsanitize=address
-fsanitize=undefined
```

Il linker aggiunge gli stessi sanitizer. Sulla macchina dell'audit il binario
debug dipendeva anche da `libasan` e `libubsan`.

Questo profilo serve a trovare errori durante i test. Non e' un artefatto
portabile da consegnare a un utente, perche' richiede runtime sanitizer
compatibili e ha caratteristiche di memoria e prestazioni differenti.

### Profilo release

`make release` esegue una ricostruzione completa tramite `re` e usa:

```text
-O2 -DNDEBUG
```

Non passa sanitizer al linker. Sulla macchina Ubuntu x86-64 dell'audit il
binario release risultava dinamico e dipendeva soltanto da `libc.so.6` e dal
loader ELF mostrato da `ldd`.

Questo risultato e' evidenza locale, non una promessa ABI per ogni Linux.
Distribuzione, libc e architettura possono cambiare il risultato.

### Profilo e directory oggetto condivisi

Debug e release usano lo stesso `OBJ_DIR`. `make release` e' sicuro perche'
passa da `fclean`, ma una successiva invocazione di `make all` non ha i flag di
compilazione come dipendenze esplicite degli oggetti.

La conseguenza pratica per v0 e':

```text
ogni prova release/install deve partire da make release
e non deve assumere che un make all successivo ricompili gli oggetti
```

Separare directory debug/release potrebbe rendere la build piu' rigorosa, ma
non e' necessario per implementare il primo stage install. Resta un possibile
debito del build system.

## Target Makefile correnti

| Target | Comportamento osservato | Relazione con install v0 |
| --- | --- | --- |
| `all` | banner, directory, oggetti, link debug | non installare questo profilo |
| `release` | `fclean` e rebuild ottimizzata | sorgente del binario installato |
| `clean` | rimuove `build/` | non deve toccare file installati |
| `fclean` | rimuove `build/` e `./alfred` | non equivale a uninstall |
| `re` | `fclean` seguito da `all` | rebuild debug |
| `run` | esegue `./alfred` | percorso sviluppatore, non installato |
| `test*` | build e suite Bash focalizzate | validano il checkout |
| `smoke-mvp` | scenario utente sul binario del checkout | candidato riusabile con `ALFRED_BIN` |
| `perf*` | benchmark manuali | fuori dal test install v0 |
| `valgrind`, `gdb` | strumenti locali | dipendenze opzionali sviluppatore |
| `format`, `scan`, `tidy` | clang-format, cppcheck, clang-tidy | corsie sviluppo separate |

Mancano oggi:

- `install`;
- `uninstall`;
- `stage-install-test` o nome equivalente;
- variabili di layout;
- una lista unica dei file posseduti dall'installazione.

Il contratto raccomandato non deve colmare questo gap facendo dipendere
`install` da `all` o `release`. Prima deve verificare tutti i sette artefatti
sorgente senza modificare la destinazione; soltanto dopo un preflight completo
puo' creare directory e copiare con modi espliciti. Se una sorgente manca, non
e' regolare o non rispetta i requisiti di eseguibilita'/leggibilita', lo stage
deve restare intatto. `uninstall` non deve avere dipendenze di build.

## Inventario degli artefatti installabili

### File che appartengono a install v0

| Sorgente repository | Destinazione proposta con `PREFIX=/usr` | Modo richiesto |
| --- | --- | --- |
| `alfred` release | `$(DESTDIR)/usr/bin/alfred` | `0755` |
| `docs/man/man1/alfred.1` | `$(DESTDIR)/usr/share/man/man1/alfred.1` | `0644` |
| `docs/man/man5/alfred.conf.5` | `$(DESTDIR)/usr/share/man/man5/alfred.conf.5` | `0644` |
| `docs/man/man7/alfred-events.7` | `$(DESTDIR)/usr/share/man/man7/alfred-events.7` | `0644` |
| `docs/man/it/man1/alfred.1` | `$(DESTDIR)/usr/share/man/it/man1/alfred.1` | `0644` |
| `docs/man/it/man5/alfred.conf.5` | `$(DESTDIR)/usr/share/man/it/man5/alfred.conf.5` | `0644` |
| `docs/man/it/man7/alfred-events.7` | `$(DESTDIR)/usr/share/man/it/man7/alfred-events.7` | `0644` |

I modi devono essere imposti dalla ricetta con uno strumento come `install -m`.
Non bisogna ereditare i modi del checkout o della directory condivisa della VM.
Lo stage in `/tmp` permette anche di verificare i modi su un filesystem locale
quando il checkout e' montato con semantica dei permessi semplificata.

Le destinazioni canoniche devono essere costruite da
`$(DESTDIR)$(BINDIR)` per il binario e da `$(DESTDIR)$(MANDIR)` per le pagine
man. `BINDIR` e `MANDIR` sono path logici senza `DESTDIR`, derivano per default
da `PREFIX` e possono essere sovrascritti separatamente. Install e uninstall
devono usare la stessa lista dei sette path risultanti, senza ricostruirli in
modo indipendente da `PREFIX`.

### File che non appartengono a install v0

| File o famiglia | Motivo dell'esclusione |
| --- | --- |
| `build/**` | cache di compilazione |
| `raw.log`, `events.log`, `errors.log` | output runtime posseduto dall'utente |
| `output.jsonl` | output opzionale scelto dalla configurazione |
| README e `docs/it/**` | documentazione sorgente, non man page runtime |
| test e benchmark | strumenti del checkout, non prodotto installato |
| header C | non esiste ancora un SDK pubblico installabile |
| sorgenti C | non appartengono al runtime installato |
| configurazione | non esiste un esempio canonico tracciato |
| unita' systemd | non implementate |

L'assenza di una configurazione di esempio e' una scelta importante: il primo
`install` non deve inventare `/etc/alfred.conf`, sovrascrivere configurazioni
utente o promettere una policy di upgrade non ancora decisa.

## Ownership di install e uninstall

Nel contesto del packaging, ownership significa:

```text
Alfred sa esattamente quali file ha copiato
e puo' rimuovere soltanto quelli
```

Il futuro `uninstall` deve quindi usare la stessa lista di destinazioni di
`install`. Non deve usare comandi ampi come:

```text
rm -rf $(DESTDIR)$(PREFIX)
rm -rf $(DESTDIR)$(MANDIR)
rm $(DESTDIR)$(MANDIR)/*/alfred*
```

Questi pattern potrebbero cancellare file di altri programmi o directory
condivise. Dopo avere rimosso i sette file, v0 puo' lasciare le directory
condivise al sistema o al package manager.

## Dipendenze di build

La build principale assume:

| Dipendenza | Perche' serve | Stato corrente |
| --- | --- | --- |
| GNU Make | sintassi, target e dependency include | installato da `build-essential` in CI |
| compilatore C | C99 con estensioni GNU | `CC := gcc`, sovrascrivibile da command line GNU Make |
| header libc Linux | API POSIX/GNU e inotify | forniti dal toolchain della distribuzione |
| linker | produce ELF dinamico | fornito dal toolchain |
| shell ricette | `mkdir`, `rm`, `printf` | `/bin/sh` e utility base |

La build non usa oggi una libreria JSON esterna. Il writer JSONL e' codice
Alfred. Questo riduce le dipendenze runtime, ma non elimina le dipendenze degli
script di test.

## Dipendenze delle suite

Gli script sono Bash, non script POSIX `sh`. Le suite ufficiali invocate dalla
CI usano almeno:

| Famiglia | Esempi | Perche' serve |
| --- | --- | --- |
| shell | `bash` | array, `[[ ... ]]`, trap e funzioni degli script |
| toolchain | `cc`, `gcc` | molti test compilano helper C focalizzati |
| coreutils | `timeout`, `mktemp`, `stat`, `wc`, `head`, `tail`, `sleep` | controllo processi, file e timing |
| file utilities | `mkdir`, `rmdir`, `mv`, `touch`, `chmod`, `find` | generazione scenari filesystem |
| text tools | `grep`, `sed`, `cut` | verifica log e campi |
| process control | `kill`, `wait` | shutdown e status del processo Alfred |
| Python | `python3` | parsing strutturale JSONL nello smoke e nei golden |

La CI installa esplicitamente soltanto `build-essential`. Bash, Python 3,
coreutils, grep, sed e findutils sono disponibili nell'immagine GitHub corrente,
ma sono dipendenze implicite.

Una container matrix non deve affidarsi alla stessa fortuna. Ogni immagine
deve installare esplicitamente il set minimo necessario alla lane che esegue.

### Dipendenza futura della lane stage-install

Il test staged non esiste ancora e quindi `man` non e' una dipendenza delle
suite CI correnti. Diventera' pero' una dipendenza obbligatoria della futura
lane stage-install di riferimento:

| Dipendenza futura | Nome comune del pacchetto | Perche' servira' |
| --- | --- | --- |
| renderer pagine man | `man-db` su Debian/Ubuntu; il nome varia per distribuzione | eseguire `man -l` su tutte le sei pagine installate |

La lane dovra' installare il pacchetto corretto per la propria distribuzione o
verificare esplicitamente che `man` esista prima del test. Questa dipendenza
futura non deve essere retrodatata come evidenza della CI corrente e non puo'
essere omessa silenziosamente quando la lane verra' implementata.

## Suite e artefatto installato

Non tutte le suite sono adatte a provare un binario installato:

- molti test backend compilano helper direttamente dai sorgenti;
- diverse suite assumono il checkout come working directory;
- vari script usano `../../alfred`, anche se molti accettano `ALFRED_BIN`;
- log e configurazioni temporanee vengono creati nella directory del test;
- scanner e watcher hanno build focalizzate proprie.

Il test install v0 deve quindi essere dedicato e piccolo. Deve verificare:

1. esistenza dei sette file nello stage;
2. modo `0755` del binario e `0644` delle man page;
3. `--version` e `--help` tramite il path staged;
4. `--check-config` con una configurazione temporanea valida;
5. rendering con `man -l` di ciascuna delle sei man page;
6. fallimento preflight senza modifiche per binario non leggibile e mancante;
7. override non standard di `BINDIR` e `MANDIR` per install e uninstall;
8. uninstall dei soli sette file;
9. conservazione di un file sentinella estraneo nelle directory condivise.

La lane stage-install di riferimento deve rendere disponibile `man` e deve
fallire se il comando manca o se anche una sola delle sei pagine non viene
renderizzata. Non e' ammesso trasformare l'assenza del renderer in uno skip
silenzioso. Una lane ridotta di una distribuzione puo' omettere questo controllo
soltanto se dichiara esplicitamente la riduzione nell'evidenza della lane e se
la lane di riferimento obbligatoria continua a possedere il controllo completo.

I casi negativi del preflight devono coprire separatamente un binario regolare
non leggibile e una delle sei sorgenti man temporaneamente indisponibile. Ogni
caso deve aspettarsi uno status non-zero e verificare che sotto `DESTDIR` non
sia stata creata o modificata alcuna destinazione. Il test e' non-root, percio'
il controllo di leggibilita' e' significativo; il cleanup deve ripristinare
permessi e sorgenti anche in caso di errore.

Lo smoke MVP puo' essere aggiunto come prova successiva passando
`ALFRED_BIN=<stage>/usr/bin/alfred`. Non deve sostituire il test di ownership
dell'installazione.

## GitHub Actions corrente

Il workflow corrente ha un solo job:

```text
runner: ubuntu-latest
package esplicito: build-essential
build: make
suite: test, test-cli, smoke-mvp, test-backend-diagnostics, test-jsonl
```

### Cosa prova

- build debug GCC con ASan/UBSan;
- core semantics;
- parser CLI;
- smoke utente dal checkout;
- diagnostica backend;
- contratto JSONL golden;
- comportamento inotify sul kernel del runner GitHub.

### Cosa non prova

- `make release`;
- installazione o uninstall;
- modi e destinazioni degli artefatti;
- rendering delle man page;
- compilatori diversi;
- libc diverse;
- distribuzioni diverse;
- kernel guest diversi;
- scanner e watcher come target separati;
- benchmark, Valgrind, CodeQL o fuzzing.

Il comando `sudo` nel workflow serve soltanto a installare i pacchetti della
CI. Alfred e i suoi test inotify correnti non vengono eseguiti come root.

## Portabilita' osservata

### Linux e inotify

Il prodotto corrente e' intenzionalmente Linux-specifico perche' include il
backend inotify. La matrice userspace deve misurare compatibilita' fra ambienti
Linux; non trasforma Alfred in un prodotto POSIX generico.

### glibc e musl

L'evidenza attuale e' glibc. Alpine/musl non deve essere aggiunta come semplice
riga verde attesa: e' una vera estensione di portabilita' che puo' richiedere
modifiche a warning, feature macro, utility e test.

### Bash e utility GNU

La CLI Alfred segue un subset POSIX/GNU documentato, ma la suite di sviluppo
non e' POSIX `sh`. Una distribuzione candidata deve fornire Bash e le utility
richieste, oppure deve ricevere un lavoro di portabilita' separato.

### Architettura

L'audit e' stato eseguito su x86-64. Una build in container x86-64 non prova
ARM64. L'architettura deve comparire nell'evidenza ambiente e in ogni futura
dichiarazione di supporto.

## Versione del prodotto

`./alfred --version` restituisce oggi:

```text
alfred 1.0.0
```

La stringa `1.0.0` compare in piu' punti del core. Questo non blocca lo stage
install, ma sarebbe fragile per packaging e release: un pacchetto deve avere
una fonte versione unica e verificabile.

Decisione v0:

- non cambiare versione in questo audit;
- non introdurre ancora packaging `.deb` o `.rpm`;
- aprire un debito dedicato prima della prima release distribuita come
  pacchetto.

La versione del prodotto, la versione Event Model v0 e la versione Backend API
v0 sono contratti distinti e non devono essere dedotte l'una dall'altra.

## Gap ordinati per priorita'

### Necessari per il prossimo micro-step

1. variabili `PREFIX`, `DESTDIR`, `BINDIR` e `MANDIR`;
2. lista unica dei sette file installati costruita da `BINDIR` e `MANDIR`;
3. ricetta `install` copy-only con preflight di tutte le sorgenti e modi
   espliciti;
4. ricetta `uninstall` senza dipendenze di build;
5. stage test non-root che esegue prima `make release`, con file sentinella;
6. documentazione Makefile aggiornata.

### Necessari prima della container matrix

1. lane release/install separata dalla build debug;
2. dipendenze userspace esplicite per ogni immagine;
3. script di environment evidence;
4. immagini/tag scelti intenzionalmente;
5. classificazione dei test eseguibili in container.

### Rimandati

- directory oggetto separate debug/release;
- fonte versione unica;
- configurazione di esempio installabile;
- package `.deb`/`.rpm`;
- systemd;
- man database refresh;
- Alpine/musl;
- ARM64;
- VM, kernel specifici, tmt, CodeQL e fuzzing.

## Raccomandazione per il prossimo micro-step

Implementare insieme contratto Makefile e test staged minimo, perche' una
ricetta `install` senza test di ownership sarebbe troppo facile da rendere
pericolosa.

Il cambiamento deve restare limitato a:

```text
Makefile
tests/install/run_all.sh
documentazione Makefile/installazione
```

Non deve modificare codice C. Il test deve usare una directory temporanea e non
deve richiedere privilegi.

## Checklist dell'audit

| Voce | Stato | Evidenza |
| --- | --- | --- |
| Artefatti build inventariati | Done | `alfred`, `build/obj/core`, `.o`, `.d` |
| Profili debug/release distinti | Done | sanitizer debug, release senza sanitizer |
| Man page inventariate | Done | tre EN e tre IT |
| Config installabile verificata | Done | nessun esempio canonico tracciato |
| Dipendenze suite inventariate | Done | Bash, toolchain, utility, Python 3 |
| Dipendenza futura stage-install | Done | `man`; pacchetto comune `man-db` su Debian/Ubuntu |
| Assunzioni CI inventariate | Done | un job `ubuntu-latest`, `build-essential` |
| Gap install ordinati | Done | contratto staged prima della matrice |
| Codice runtime modificato | No | audit documentale |

## Collegamenti

- [Installability and Linux compatibility v0](53-installability-linux-compatibility-v0.md)
- [Makefile e build system](09-makefile-e-build-system.md)
- [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
- [Come contribuire](11-come-contribuire.md)
- [Roadmap CLI e pagine man](19-roadmap-cli-e-man-page.md)
- GitHub Milestone: [#15](https://github.com/kinderp/alfred/milestone/15)
- Issue madre: [#261](https://github.com/kinderp/alfred/issues/261)
- Issue audit: [#264](https://github.com/kinderp/alfred/issues/264)
- Issue implementazione staged: [#266](https://github.com/kinderp/alfred/issues/266)
