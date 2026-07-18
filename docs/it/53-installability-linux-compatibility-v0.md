# Installability and Linux compatibility v0

Questa milestone rende l'MVP corrente di Alfred installabile con un contratto
piccolo e verificabile e definisce come raccogliere evidenza di compatibilita'
su distribuzioni e kernel Linux diversi.

I due obiettivi sono collegati. Prima di eseguire Alfred in piu' ambienti serve
sapere quale artefatto stiamo provando, dove viene installato e quali file fanno
parte del prodotto. Una matrice che compila soltanto il repository non prova
ancora che l'installazione destinata all'utente sia corretta.

La regola centrale della milestone e':

```text
testare un ambiente non significa supportare tutti gli ambienti simili;
supportare un ambiente richiede evidenza ripetibile e limiti dichiarati
```

## Obiettivo

L'obiettivo v0 e':

```text
sorgenti Alfred
-> build release
-> installazione staged senza root
-> verifica binario e pagine man installate
-> prima matrice CI di distribuzioni/userspace
-> evidenza ambiente salvata e interpretabile
```

La milestone deve produrre:

- un layout di installazione esplicito;
- target Makefile compatibili con staging tramite `DESTDIR`;
- una procedura di uninstall limitata ai file posseduti da Alfred;
- test focalizzati sull'artefatto installato;
- una prima matrice piccola di distribuzioni Linux;
- una tassonomia che separa compatibilita' userspace e kernel;
- criteri per introdurre in futuro VM, kernel specifici, analisi statica e
  fuzzing.

## Perche' ora

Le milestone precedenti hanno stabilizzato:

- il backend inotify di riferimento;
- Event Model v0 e Backend API v0 staged;
- Writer Runtime v0 e JSONL;
- CLI minima e parser v0;
- smoke test MVP;
- README e pagine man inglesi/italiane.

Un utente puo' quindi compilare e provare Alfred dal repository, ma manca ancora
un confine di installazione. Il Makefile non espone oggi `install`, `uninstall`,
`PREFIX` o `DESTDIR`; la CI esegue la suite sul runner `ubuntu-latest`, ma non
separa ancora build debug sanitizzata, build release, distribuzione userspace e
kernel effettivamente usato.

Questa e' la fase corretta per chiudere il gap senza riaprire il percorso caldo
o aggiungere nuovi backend.

## Non obiettivi

Questa milestone non implementa:

- pacchetti `.deb` o `.rpm`;
- repository APT, DNF o RPM;
- immagini container di distribuzione del prodotto;
- service unit `systemd`, daemon mode o gestione del servizio;
- supporto macOS o Windows;
- nuovi backend come fanotify, audit o eBPF;
- policy, Agent Guard o enforcement;
- modifiche a Event Model v0, Backend API v0 o Writer Runtime;
- `tmt`, `testcloud`, `virtme-ng`, CodeQL o ClusterFuzzLite nel primo
  micro-step;
- una promessa di compatibilita' con ogni distribuzione o kernel Linux.

## Le dimensioni della compatibilita'

Dire "Alfred funziona su Linux" e' troppo ambiguo. Linux comprende piu'
dimensioni indipendenti.

| Dimensione | Esempi | Cosa puo' cambiare | Evidenza necessaria |
| --- | --- | --- | --- |
| Distribuzione | Ubuntu, Debian, Fedora | pacchetti, path, shell, tool di build | build/install/test nell'immagine |
| Libreria C | glibc, musl | API disponibili, linking, comportamento runtime | build e test sulla libc reale |
| Compilatore | GCC, Clang | warning, sanitizer, estensioni, ottimizzazioni | build separate con versioni registrate |
| Kernel | LTS vecchio, LTS corrente, stable recente | semantica inotify, flag, ordering, errori | boot ed esecuzione sul kernel guest reale |
| Filesystem | ext4, XFS, tmpfs, overlayfs | rename, inode, mount, watch e timing | scenario eseguito sul filesystem reale |
| Architettura | x86_64, arm64 | ABI, alignment, atomicita', toolchain | runner o VM dell'architettura reale |
| Virtualizzazione | host, container, VM, QEMU/KVM | namespace, mount, privilegi, timing | ambiente dichiarato e riproducibile |

Una sola riga verde in CI non copre automaticamente tutte queste dimensioni.

## Container e kernel: distinzione fondamentale

Un container cambia soprattutto lo userspace:

```text
container Ubuntu
container Debian
container Fedora
        |
        +-> pacchetti, libc, compilatore e utility differenti
        +-> stesso kernel del runner host
```

Quindi una matrice container puo' dimostrare che Alfred compila, si installa e
supera test selezionati in piu' userspace. Non puo' dimostrare che Alfred ha
funzionato su tre kernel diversi.

Per provare kernel diversi serve avviare un guest o un kernel specifico:

```text
immagine VM + kernel guest
oppure
virtme-ng + kernel selezionato
        |
        +-> uname -r realmente diverso
        +-> semantica inotify del kernel provato
```

Questa distinzione deve comparire nei report e nelle future dichiarazioni di
supporto.

## Architettura dei test a livelli

Il sistema proposto cresce per livelli. Ogni livello risponde a una domanda
diversa e viene introdotto solo quando compra evidenza utile.

```text
L0  test locali e suite Makefile
 |
L1  GitHub Actions reference job
 |
L2  container matrix distribuzioni/userspace
 |
L3  tmt come piano comune, solo se serve orchestrare piu' provider
 |
L4  testcloud/libvirt con VM complete
 |
L5  virtme-ng per versioni kernel mirate

Lente trasversale di qualita':
ASan/UBSan | CodeQL | libFuzzer/ClusterFuzzLite
```

I livelli non sostituiscono quelli precedenti. Una VM non rende inutile un
test unitario veloce; un fuzzer non sostituisce uno scenario end-to-end; CodeQL
non dimostra che inotify funziona su un kernel specifico.

## Livello 0 - test locali e suite Makefile

Questo livello esiste gia'. Comprende:

- `make test-core`;
- `make test-cli`;
- `make smoke-mvp`;
- `make test-backend-diagnostics`;
- `make test-jsonl`;
- test scanner e watcher;
- benchmark manuali separati.

Vantaggi:

- feedback rapido;
- facile riproduzione locale;
- nessun provider esterno;
- diagnostica vicina al codice.

Limiti:

- prova una sola macchina alla volta;
- eredita kernel, filesystem, compiler e pacchetti dell'host;
- non conserva automaticamente una matrice storica di ambienti.

## Livello 1 - GitHub Actions reference job

La CI corrente usa `ubuntu-latest`, compila con `make` ed esegue le suite
ufficiali. Questo resta il job di riferimento completo.

Il Makefile corrente include ASan e UBSan nella build debug predefinita, quindi
la suite CI principale e' gia' instrumentata. `make release` rimuove invece i
sanitizer, ma la CI corrente non costruisce ancora esplicitamente il profilo
release.

Il debito da chiudere durante questa milestone e' separare chiaramente:

```text
debug/sanitized evidence -> trova memory error e undefined behavior
release/install evidence -> prova l'artefatto destinato all'utente
```

Non bisogna installare per errore il binario debug sanitizzato come se fosse il
prodotto release.

## Livello 2 - container matrix per distribuzioni

La prima estensione CI concreta dovrebbe essere una matrice piccola, non una
lista di ogni distribuzione esistente.

Una candidata ragionevole per v0 e':

| Famiglia | Valore della prova | Scope iniziale |
| --- | --- | --- |
| Ubuntu LTS | ambiente di riferimento vicino al job corrente | build release, stage install, CLI/smoke |
| Debian stable | userspace conservativo e packaging Debian-like | build release, stage install, test selezionati |
| Fedora stable | toolchain e packaging differenti dalla famiglia Debian | build release, stage install, test selezionati |

La selezione finale delle immagini deve essere fatta nell'audit CI. I tag
devono essere intenzionali: un tag mobile come `latest` aumenta la copertura
nel tempo ma riduce la riproducibilita'; un tag fissato rende il risultato piu'
riproducibile ma richiede aggiornamenti pianificati.

Alpine/musl e' interessante, ma non deve entrare automaticamente in v0. Alfred
usa `gnu99`, Bash nei test e oggi viene sviluppato soprattutto su glibc. Una
lane musl sarebbe una vera estensione di portabilita', con possibili modifiche
al build/test contract; deve avere una issue e criteri propri.

### Cosa eseguire nella matrice v0

La matrice non deve duplicare tutta la CI completa su ogni immagine senza un
motivo. Il percorso minimo candidato e':

```text
installare dipendenze build
-> registrare ambiente
-> make release
-> make stage-install-test
-> eseguire un sottoinsieme runtime rappresentativo
```

Se un test dipende da timing o privilegi non disponibili nel container, deve
essere classificato e non semplicemente disabilitato senza spiegazione.

## Livello 3 - tmt per i piani

`tmt` permette di descrivere piani che separano discovery, provisioning,
preparazione, esecuzione, reporting e cleanup. Il suo valore emerge quando lo
stesso insieme di test deve essere eseguito attraverso piu' provisioner.

Vantaggi:

- piani dichiarativi versionati;
- selezione di test per tag o contesto;
- stesso modello per container, VM o host remoti;
- artifact e risultati organizzati per run.

Costi:

- nuova dipendenza Python e nuovo linguaggio di metadata;
- secondo livello di orchestrazione sopra Make e Bash;
- onboarding e debugging piu' complessi;
- rischio di duplicare la tassonomia gia' presente nelle suite Alfred.

Decisione v0:

```text
non introdurre tmt finche' non esistono almeno due provider reali
o duplicazione sostanziale dei piani di test
```

Quando verra' adottato, `tmt` dovra' inizialmente orchestrare i target Make
esistenti, non riscrivere tutta la suite.

## Livello 4 - testcloud/libvirt per VM complete

Una VM completa combina userspace, kernel guest, init system, mount e
filesystem piu' vicini a una macchina reale. `testcloud` con libvirt e' adatto
a provisioning ripetibile di immagini cloud e si integra con il provisioner
virtuale di `tmt`.

Vantaggi:

- kernel guest reale distinto dal runner;
- maggiore fedelta' della distribuzione;
- possibilita' di provare installazione, reboot, permessi e servizi;
- isolamento piu' forte rispetto ai container.

Costi:

- immagini grandi e lente da scaricare;
- KVM/nested virtualization e permessi da gestire;
- maggiore variabilita' e costo CI;
- cleanup, caching e diagnostica piu' complessi.

Decisione v0: progettare il livello, non implementarlo. Le VM dovrebbero essere
job schedulati o manuali prima di diventare gate su ogni PR.

## Livello 5 - virtme-ng per kernel specifici

`virtme-ng` permette di costruire o avviare rapidamente un kernel selezionato
in QEMU usando una vista copy-on-write del sistema host. Per Alfred e'
interessante per test mirati su:

- disponibilita' e fallback di flag inotify;
- differenze tra kernel LTS;
- self-event, overflow e ordering osservato;
- regressioni kernel-specifiche riproducibili;
- eventuali backend futuri che dipendono da capability kernel.

Vantaggi:

- selezione esplicita della versione kernel;
- ciclo piu' rapido di una VM completa per prove kernel-focused;
- buon isolamento per crash o esperimenti distruttivi;
- utile per confrontare una regressione tra due versioni.

Limiti:

- non rappresenta automaticamente uno userspace completo di distribuzione;
- richiede QEMU/KVM e dipendenze dedicate;
- kernel minimali possono non includere feature non richieste nella config;
- non e' adatto come job obbligatorio su ogni piccola PR.

Prima di costruire la matrice kernel bisogna decidere:

- kernel minimo dichiarato;
- LTS supportati o solo testati;
- architettura iniziale;
- filesystem e mount da usare;
- scenari stabili da eseguire;
- frequenza e politica per i fallimenti.

La matrice candidata futura non deve usare numeri casuali. Dovrebbe includere:

```text
oldest-supported
current-LTS
latest-stable
```

finche' `oldest-supported` non e' deciso, Alfred deve parlare di kernel
"testati", non di kernel "supportati".

## ASan e UBSan

AddressSanitizer e UndefinedBehaviorSanitizer sono strumenti dinamici: trovano
errori solo nei percorsi realmente eseguiti.

ASan e' utile per:

- accessi fuori limite;
- use-after-free;
- double free e invalid free;
- leak rilevabili dal runtime.

UBSan e' utile per:

- overflow signed;
- shift invalidi;
- puntatori null o misaligned;
- altre operazioni con comportamento indefinito.

Alfred li usa gia' nella build debug predefinita. La milestone deve verificare
che questa evidenza resti visibile nella CI e non venga confusa con la build
release. I sanitizer aumentano tempo, memoria e dimensione del binario: non
sono benchmark di prestazione e non sono il runtime da installare.

## CodeQL

CodeQL analizza il codice senza richiedere che uno scenario runtime raggiunga
il percorso difettoso. Per C puo' trovare classi di problemi di correttezza e
sicurezza complementari a compiler warning e sanitizer.

Vantaggi:

- analisi automatica delle PR e del branch principale;
- risultati SARIF integrati in GitHub;
- copertura di data flow e pattern di sicurezza;
- disponibile per repository pubblici GitHub.

Costi e limiti:

- workflow e configurazione separati;
- possibili alert da valutare e classificare;
- non prova comportamento runtime, semantica inotify o portabilita';
- la modalita' di build deve essere scelta consapevolmente per il C compilato.

Decisione v0: tracciarlo come lane di sicurezza indipendente, da introdurre in
una issue dedicata dopo che install/build CI e' stabile.

## libFuzzer e ClusterFuzzLite

libFuzzer e' coverage-guided: genera e muta byte per raggiungere nuovi percorsi
di una funzione target. ClusterFuzzLite porta questi target in CI, conserva
crash input, corpus e report di copertura.

Il fuzzing funziona meglio su funzioni:

- pure o quasi pure;
- veloci;
- deterministiche;
- bounded;
- capaci di accettare input arbitrario senza terminare il processo.

Possibili candidati Alfred da valutare in futuro:

- parser di singole righe di configurazione, dopo un confine testabile puro;
- parser di token per mask/eventi;
- escaping e formattazione JSONL con record sintetici bounded;
- validator di descriptor/capabilities e input strutturati.

Non e' opportuno iniziare dal runtime inotify end-to-end: timing, filesystem,
stato globale e processi renderebbero il target lento e poco deterministico.

Decisione v0: non aggiungere ClusterFuzzLite finche' non esiste almeno un fuzz
target piccolo, un corpus iniziale e un contratto per trasformare ogni crash in
un test di regressione ordinario.

## Frequenza dei diversi livelli

| Frequenza | Livelli candidati | Motivo |
| --- | --- | --- |
| Ogni modifica locale | test focused, `git diff --check` | feedback rapido |
| Ogni PR | reference CI, sanitizer, stage-install test | gate principale |
| Ogni PR rilevante a build/portabilita' | container matrix | costo limitato ma maggiore |
| Nightly o settimanale | VM complete, fuzzing batch | piu' tempo e artifact |
| Manuale o schedulato | virtme-ng kernel matrix | costo kernel/QEMU e diagnosi dedicata |
| Push/main o schedule | CodeQL | analisi indipendente dalla suite runtime |

La frequenza potra' cambiare dopo avere misurato durata, flakiness e valore dei
finding. Un job lento che non trova problemi non deve diventare permanente per
inerzia.

## Contratto di installazione proposto

Il contratto v0 deve seguire il modello comune dei Makefile Unix:

| Variabile | Default candidato | Ruolo |
| --- | --- | --- |
| `PREFIX` | `/usr/local` | prefisso logico dell'installazione |
| `DESTDIR` | vuoto | root temporanea per staging/package build |
| `BINDIR` | `$(PREFIX)/bin` | directory del binario |
| `MANDIR` | `$(PREFIX)/share/man` | root delle pagine man |

Layout candidato:

```text
$(DESTDIR)$(PREFIX)/bin/alfred
$(DESTDIR)$(PREFIX)/share/man/man1/alfred.1
$(DESTDIR)$(PREFIX)/share/man/man5/alfred.conf.5
$(DESTDIR)$(PREFIX)/share/man/man7/alfred-events.7
$(DESTDIR)$(PREFIX)/share/man/it/man1/alfred.1
$(DESTDIR)$(PREFIX)/share/man/it/man5/alfred.conf.5
$(DESTDIR)$(PREFIX)/share/man/it/man7/alfred-events.7
```

Esempio di staging senza root:

```bash
stage_dir=$(mktemp -d)
make release
make DESTDIR="$stage_dir" PREFIX=/usr install
```

Il test deve poi usare i path dentro `stage_dir`, senza modificare `/usr` o
`/usr/local` reali.

### Regole di sicurezza per uninstall

`uninstall` deve:

- rimuovere solo i file elencati dal contratto Alfred;
- non usare glob ampi;
- non rimuovere directory di sistema condivise;
- rispettare `DESTDIR` e `PREFIX`;
- essere verificabile prima su una staging root temporanea.

La milestone non deve introdurre un uninstall ricorsivo o basato su path non
validati.

## Test dell'artefatto installato

Il futuro test staging deve verificare almeno:

1. presenza e permessi del binario;
2. presenza delle sei pagine man;
3. assenza di file non dichiarati;
4. `alfred --version` dal path staged;
5. `alfred --help` dal path staged;
6. `alfred --check-config` dal path staged;
7. rendering delle pagine man staged quando `man` e' disponibile;
8. uninstall staged senza lasciare file posseduti da Alfred.

Il test non deve richiedere root e non deve scrivere fuori dalla directory
temporanea.

## Evidenza ambiente

Ogni lane di compatibilita' dovrebbe registrare almeno:

```text
distribution_id
distribution_version
kernel_release
architecture
libc
compiler
compiler_version
filesystem_type, quando rilevante
virtualization/container mode
build_profile
sanitizers_enabled
```

Comandi tipici:

```bash
cat /etc/os-release
uname -a
getconf GNU_LIBC_VERSION || true
gcc --version
clang --version
stat -f -c %T .
```

Non tutti i comandi esistono su ogni distribuzione. Lo script futuro deve
degradare in modo esplicito e scrivere `unknown`, non fallire o inventare il
valore.

## Linguaggio delle dichiarazioni di supporto

Usare tre stati distinti:

| Stato | Significato |
| --- | --- |
| Supportato | ambiente incluso nel contratto, con CI ripetibile e politica di regressione |
| Testato | una prova registrata e' passata, ma non esiste ancora una promessa stabile |
| Non testato | nessuna evidenza corrente; non significa automaticamente incompatibile |

Esempio corretto:

```text
Tested in CI on Ubuntu LTS, Debian stable and Fedora stable userspaces.
Kernel compatibility is tracked separately; container jobs use the runner kernel.
```

Esempio sbagliato:

```text
Supported on all Linux distributions and kernels.
```

## Criteri per aggiungere un nuovo ambiente

Una distribuzione, kernel o architettura deve entrare nella matrice quando
almeno una di queste condizioni e' vera:

- rappresenta utenti reali o una installazione richiesta;
- esercita una libc/toolchain differente;
- copre il kernel minimo o un LTS dichiarato;
- riproduce un bug reale;
- valida una capability necessaria a un backend futuro;
- aggiunge evidenza non gia' equivalente a una lane esistente.

Non aggiungiamo ambienti solo per aumentare il numero di badge verdi.

## Checklist della milestone

| Item | Stato | Note |
| --- | --- | --- |
| Setup milestone, issue madre e roadmap | In progress | Issue madre #261 e issue figlia #262; PR setup da aprire |
| Audit Makefile, artefatti e CI corrente | Todo | Mappare profilo debug/release, path e dipendenze |
| Contratto `PREFIX` / `DESTDIR` / install / uninstall | Todo | Nessuna scrittura root nei test |
| Test stage-install | Todo | Verificare binario, man page, CLI e cleanup |
| Prima matrice distribuzioni/userspace | Todo | Container non equivalgono a kernel diversi |
| Evidenza ambiente e wording di supporto | Todo | Distinguere supported/tested/untested |
| Readiness closure | Todo | Documenti, CI e issue GitHub allineati |

## Test e validazione

Per il setup solo documentale:

```bash
git diff --check
```

Quando verra' modificato il Makefile:

```bash
make fclean
make release
make test-install
make test-cli
make smoke-mvp
```

Quando verra' modificata la CI, il PR dovra' mostrare:

- esito del job di riferimento;
- esito di ogni lane della matrice;
- environment evidence per ogni lane;
- artifact diagnostici quando un job fallisce;
- spiegazione di eventuali test esclusi.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- Alfred dispone di installazione staged senza root;
- layout e ownership dei file installati sono documentati;
- il test install verifica binario, pagine man e cleanup;
- la build release viene provata separatamente dalla build sanitizzata;
- una matrice userspace piccola produce evidenza ripetibile;
- nessun documento confonde container e kernel coverage;
- gli strumenti rimandati hanno criteri di adozione espliciti;
- README, man page, roadmap, issue checklist e traceability sono allineati.

## Fonti tecniche

- [GitHub Actions matrix jobs](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/run-job-variations)
- [tmt plans](https://tmt.readthedocs.io/en/stable/spec/plans.html)
- [tmt virtual/testcloud provisioning](https://tmt.readthedocs.io/en/stable/plugins/provision.html)
- [virtme-ng](https://github.com/arighi/virtme-ng)
- [CodeQL for compiled languages](https://docs.github.com/en/code-security/concepts/code-scanning/codeql/codeql-for-compiled-languages)
- [Clang AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [Clang UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [LLVM libFuzzer](https://llvm.org/docs/LibFuzzer.html)
- [ClusterFuzzLite](https://google.github.io/clusterfuzzlite/)

## Collegamenti

- GitHub Milestone: [Installability and Linux compatibility v0](https://github.com/kinderp/alfred/milestone/15)
- Issue madre: [#261](https://github.com/kinderp/alfred/issues/261)
- Setup documentale: [#262](https://github.com/kinderp/alfred/issues/262)
- [Post-MVP documentation and man pages v0](51-post-mvp-documentation-man-pages-v0.md)
- [MVP operational usability v0](49-mvp-operational-usability-v0.md)
- [Roadmap CLI e pagina man](19-roadmap-cli-e-man-page.md)
- [Makefile e build system](09-makefile-e-build-system.md)
- [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
- [Qualita' prodotto software](35-qualita-prodotto-software.md)
- [Registro milestone](37-roadmap-milestone-progetto.md)
