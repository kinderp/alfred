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

Un utente poteva quindi compilare e provare Alfred dal repository, ma mancava
un confine di installazione. Il micro-step [#266](https://github.com/kinderp/alfred/issues/266)
chiude questo primo gap con `install`, `uninstall`, `PREFIX`, `DESTDIR` e una
suite staged obbligatoria nel job `ubuntu-latest`. Restano da separare e
misurare distribuzioni userspace e kernel effettivamente usati.

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
- `make test-install`;
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
ufficiali. Questo resta il job di riferimento completo. Come ultimo controllo,
`make test-install` ricostruisce il profilo release e prova l'artefatto staged;
la posizione finale evita che quel rebuild nasconda copertura debug
ASan/UBSan alle suite precedenti.

Il Makefile corrente include ASan e UBSan nella build debug predefinita, quindi
la suite CI principale e' gia' instrumentata. `make release` rimuove invece i
sanitizer ed e' invocato esplicitamente da `make test-install` prima della copia
nello stage.

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

La matrice v0 implementata dalla issue
[#268](https://github.com/kinderp/alfred/issues/268) usa tre tag di release
espliciti:

| Lane | Immagine container | Valore della prova | Scope iniziale |
| --- | --- | --- | --- |
| Ubuntu LTS | `ubuntu:24.04` | ambiente di riferimento vicino al job corrente | build release, stage install, CLI/smoke |
| Debian stable | `debian:13-slim` | userspace conservativo e packaging Debian-like | build release, stage install, CLI/smoke |
| Fedora stable | `fedora:44` | toolchain e packaging differenti dalla famiglia Debian | build release, stage install, CLI/smoke |

I tag fissano la release della distribuzione invece di seguire `latest`. Questo
rende la prova piu' interpretabile, ma non rende immutabili i byte
dell'immagine: gli aggiornamenti della stessa release possono cambiare i
pacchetti disponibili. Aggiornare una lane richiede quindi una PR dedicata che
registri il motivo, i nuovi dati ambiente e l'esito delle prove.

Alpine/musl e' interessante, ma non deve entrare automaticamente in v0. Alfred
usa `gnu99`, Bash nei test e oggi viene sviluppato soprattutto su glibc. Una
lane musl sarebbe una vera estensione di portabilita', con possibili modifiche
al build/test contract; deve avere una issue e criteri propri.

### Cosa esegue la matrice v0

La matrice non duplica tutta la CI completa su ogni immagine. Il percorso e':

```text
installare dipendenze build
-> registrare ambiente
-> make test-install
   -> make release
   -> verifica installazione staged, CLI e man page
-> ALFRED_BIN=./alfred bash tests/smoke/mvp_smoke.sh
   -> scenario runtime inotify sullo stesso artefatto release
```

Il workflow e' `.github/workflows/linux-userspace.yml`. Ogni lane installa
esplicitamente toolchain, Bash, utility di test, Python e renderer `man`; non
eredita implicitamente i pacchetti del runner Ubuntu. `fail-fast: false`
permette alle altre distribuzioni di completare anche se una lane fallisce,
cosi' il report distingue un problema generale da uno specifico userspace.

Il bootstrap dei pacchetti avviene come `root`, come normalmente accade nei
job container. Prima dei test il workflow assegna il checkout a UID/GID 1001 e
usa `setpriv` per eseguire `make test-install` e lo smoke script senza
privilegi. Questo e' parte del contratto: eseguire la suite staged come `root`
renderebbe non significativa la prova della sorgente intenzionalmente non
leggibile e non rappresenterebbe il percorso non-root raccomandato.

Il comando imposta anche `no_new_privs`: dopo la riduzione di identita', una
successiva `execve()` non puo' riacquistare privilegi tramite bit setuid/setgid
o meccanismi equivalenti. Questa proprieta' rende piu' rigoroso il confine fra
il bootstrap `root` dei pacchetti e il codice del repository eseguito dai test.
Non trasforma pero' il job in una sandbox completa e non sostituisce
l'isolamento del container o i permessi minimi del token GitHub.

Il job completo `.github/workflows/ci.yml` resta l'autorita' per tutte le suite
debug con ASan/UBSan. Le lane container usano invece il profilo release e il
percorso install/smoke: non sono benchmark e non sostituiscono la CI completa.
La lane richiama direttamente lo script smoke con `ALFRED_BIN` per non passare
dal target `make smoke-mvp`, che dipende da `all` e appartiene al normale
percorso debug. In questo modo install e smoke provano lo stesso binario release
e non richiedono runtime sanitizer nelle immagini container.

GitHub esegue i container sullo stesso kernel del runner `ubuntu-latest`.
Per questo ogni lane stampa `kernel_scope=shared-github-runner-kernel`; tre job
verdi dimostrano tre userspace provati, non tre kernel provati.

Oltre alla diagnostica immediata nel log, la issue
[#270](https://github.com/kinderp/alfred/issues/270) introduce un formato JSON
v0 versionato e un artifact stabile per ogni lane. Il formato e il linguaggio
pubblico `tested`/`supported`/`untested` sono descritti piu avanti.

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

## Contratto di installazione implementato

Il contratto v0 segue il modello comune dei Makefile Unix/GNU:

| Variabile | Default corrente | Ruolo |
| --- | --- | --- |
| `PREFIX` | `/usr/local` | prefisso logico dell'installazione |
| `DESTDIR` | vuoto | root temporanea per staging/package build |
| `BINDIR` | `$(PREFIX)/bin` | path logico del binario, senza `DESTDIR` |
| `MANDIR` | `$(PREFIX)/share/man` | root logica delle pagine man, senza `DESTDIR` |

Layout canonico:

```text
$(DESTDIR)$(BINDIR)/alfred
$(DESTDIR)$(MANDIR)/man1/alfred.1
$(DESTDIR)$(MANDIR)/man5/alfred.conf.5
$(DESTDIR)$(MANDIR)/man7/alfred-events.7
$(DESTDIR)$(MANDIR)/it/man1/alfred.1
$(DESTDIR)$(MANDIR)/it/man5/alfred.conf.5
$(DESTDIR)$(MANDIR)/it/man7/alfred-events.7
```

`BINDIR` e `MANDIR` derivano in modo ricorsivo da `PREFIX`, cosi' un override
di `PREFIX` aggiorna entrambi i default. Un override esplicito di `BINDIR` o
`MANDIR` sostituisce invece soltanto quel path logico. `DESTDIR` viene aggiunto
una sola volta dalla ricetta e non deve comparire nel valore di queste due
variabili.

`BINDIR` e `MANDIR` devono essere path assoluti e non possono contenere un
segmento `..`. `DESTDIR` puo' essere vuoto oppure assoluto e rispetta lo stesso
divieto. `validate-install-layout` applica queste precondizioni prima che
`install` o `uninstall` tocchino la destinazione: un errore di configurazione
non deve trasformare una ricetta di copia o rimozione in un'operazione su un
path ambiguo.

Questo controllo e' lessicale e non promette isolamento da una gerarchia
ostile. Le utility di installazione e rimozione possono seguire componenti
symlink intermedi; inoltre un controllo shell separato sarebbe soggetto a race
prima della copia. Il contratto v0 richiede quindi che l'amministratore o il
package builder controlli l'intero albero di destinazione. Lo staging non-root
e' il percorso di test raccomandato; un `DESTDIR` controllato da terzi non deve
essere usato con privilegi elevati.

Esempio di staging senza root:

```bash
stage_dir=$(mktemp -d)
make release
make DESTDIR="$stage_dir" PREFIX=/usr install
```

Il confine build/install v0 e' esplicito:

- `install` copia soltanto artefatti gia' costruiti;
- `install` non dipende da `all` o `release` e non invoca il compilatore;
- prima di creare directory o copiare file, `install` esegue un preflight
  read-only di tutti i sette artefatti sorgente;
- `$(TARGET)` deve essere un file regolare, leggibile ed eseguibile e ciascuna
  delle sei pagine man deve essere un file regolare leggibile;
- se una precondizione fallisce, `install` termina senza creare o modificare
  nulla sotto `DESTDIR`;
- il test staged esegue `make release` come passo separato prima di `install`;
- un eventuale comando privilegiato deve riguardare soltanto la copia, non la
  compilazione dentro il checkout.

Questa separazione evita sia di installare automaticamente la build debug
ASan/UBSan sia di creare oggetti posseduti da root con `sudo make install`.
Il primo contratto non puo' riconoscere dai soli byte di `./alfred` con quali
flag sia stato compilato: la build release resta responsabilita' del chiamante
e del test staged finche' i profili non avranno artefatti o metadati separati.

Il test usa i path dentro `stage_dir`, senza modificare `/usr` o `/usr/local`
reali. Le variabili interne `ALFRED_INSTALL_BINARY_SOURCE` e
`ALFRED_MAN*_SOURCE` sono punti di iniezione riservati alla suite: permettono di
simulare una sorgente illeggibile o mancante senza alterare file tracciati nel
checkout. Non fanno parte del layout pubblico destinato all'utente.

### Regole di sicurezza per uninstall

`uninstall` deve:

- rimuovere solo i file elencati dal contratto Alfred;
- non usare glob ampi;
- non rimuovere directory di sistema condivise;
- rispettare `DESTDIR`, `PREFIX`, `BINDIR` e `MANDIR`;
- usare la stessa lista canonica di destinazioni usata da `install`;
- essere verificabile prima su una staging root temporanea.

La milestone non deve introdurre un uninstall ricorsivo o basato su path non
validati.

## Test dell'artefatto installato

`make test-install` esegue `make release` e poi
`tests/install/run_all.sh`. La suite verifica:

1. presenza e permessi del binario;
2. presenza delle sei pagine man;
3. assenza di entry non dichiarate, compresi symlink e altri nodi non regolari;
4. `alfred --version` dal path staged;
5. `alfred --help` dal path staged;
6. `alfred --check-config` dal path staged;
7. rendering con `man -l` di ciascuna delle sei pagine man staged;
8. fallimento preflight senza modifiche a `DESTDIR` per ogni classe di sorgente;
9. override di `BINDIR` e `MANDIR` rispettato da install e uninstall;
10. default `/usr/local` distinto dall'override packaging `PREFIX=/usr`;
11. rifiuto di layout relativi o contenenti `..`;
12. uninstall staged idempotente senza lasciare file posseduti da Alfred.

Il test non richiede root e non scrive nel prefisso installato dell'host.
`make release` aggiorna normalmente `build/` e `./alfred` nel checkout; le
operazioni install/uninstall, i sentinel e gli artifact dei casi negativi
restano invece nella directory creata con `mktemp`. In caso di fallimento, la
CI pubblica questi artifact sotto `/tmp/alfred_install_test.*`; localmente
`ALFRED_KEEP_TEST_LOGS=1` conserva lo stage per l'ispezione.

La lane stage-install di riferimento deve installare o rendere esplicitamente
disponibile `man` e deve fallire se il renderer manca o se una delle sei pagine
non viene renderizzata. L'assenza di `man` non e' un successo e non puo' essere
uno skip silenzioso. Una lane ridotta della matrice puo' omettere il rendering
solo dichiarandolo nella propria evidenza, mentre la lane di riferimento
obbligatoria continua a verificare tutte le pagine.

Il test negativo del preflight deve coprire separatamente le due classi di
artefatto: un binario regolare non leggibile e una sorgente man temporaneamente
indisponibile. In entrambi i casi deve verificare sia lo status non-zero sia
l'assenza di qualsiasi nuova directory o file nello stage. Il test deve
ripristinare permessi e sorgenti anche quando una verifica fallisce; essendo
non-root, il controllo di leggibilita' resta significativo.

Il caso mancante usa deliberatamente l'ultima sorgente della sequenza,
`ALFRED_MAN7_IT_SOURCE`. In questo modo il test non dimostra soltanto che un
errore iniziale e' innocuo: dimostra che tutte le sorgenti precedenti possono
superare il controllo senza che install inizi a modificare lo stage prima di
avere completato il preflight.

## Evidenza ambiente v0

Ogni lane prova a produrre
`.alfred-ci/compatibility-evidence-v0.json` tramite
`tools/ci/compatibility_evidence.py` e lo carica per 30 giorni come
`alfred-compatibility-evidence-<lane>`. L'upload abilita i file nascosti perche'
la directory inizia con `.`, ma il path resta vincolato a questo solo JSON. Il
record ha questo envelope:

```json
{
  "schema": "alfred.compatibility-evidence",
  "schema_version": 0,
  "generated_at_utc": "2026-07-18T12:00:00Z",
  "source_revision": "0123456789abcdef0123456789abcdef01234567",
  "ci": {
    "provider": "github-actions",
    "run_id": "123456",
    "run_attempt": "1"
  },
  "lane": "debian-13",
  "environment": {
    "container_image": "debian:13-slim",
    "container_mode": "github-actions-job-container",
    "distribution_id": "debian",
    "distribution_version": "13",
    "architecture": "x86_64",
    "libc": {"name": "glibc", "version": "2.41"},
    "compiler": {"id": "gcc", "version": "14.2.0"},
    "source_tree_filesystem_type": "overlayfs",
    "kernel_release": "6.x.y",
    "kernel_scope": "shared-github-runner-kernel"
  },
  "build": {
    "profile": "release",
    "sanitizers_enabled": false
  },
  "results": {
    "staged_install": "passed",
    "mvp_smoke": "passed"
  }
}
```

Il record salva revisione e run per la provenienza, non per dichiarare supporto
permanente. `kernel_scope=shared-github-runner-kernel` impedisce di interpretare
tre lane userspace come tre kernel guest. `run_id` e `run_attempt` sono stringhe
decimali per non dipendere dai limiti numerici di consumer JSON diversi.

`source_tree_filesystem_type` descrive il filesystem che ospita la root del
checkout, ricavata dal percorso del generatore; non descrive necessariamente la
root del container e non dipende dalla directory corrente del chiamante. Le
sonde raccolgono soltanto campi ammessi, leggono al massimo 4096 byte e
limitano ogni processo a 5 secondi. Il limite e' applicato mentre il processo
scrive, non dopo averne caricato tutto l'output; superamento, timeout ed errore
producono `unknown`. Ogni probe usa una sessione separata: sui percorsi falliti
viene terminato il process group, cosi' anche un discendente che eredita stdout
non resta attivo, e il figlio diretto viene sempre raccolto. Non vengono
salvati hostname, utente, path del workspace, variabili
d'ambiente complete o segreti. Se `getconf`, `cc`, `stat` o un dato di sistema
non sono disponibili, il campo corrispondente vale `unknown`; l'assenza di un
dato non fa fallire una prova altrimenti valida e non autorizza a inventarlo.

Gli outcome GitHub sono normalizzati in `passed`, `failed`, `not_run`,
`cancelled` e `unknown`. Questi valori sono distinti: in particolare `not_run`
non e' un successo. Il file viene scritto atomicamente solo dopo la validazione
completa, cosi' un input invalido o un processo interrotto non sostituisce un
record precedente con JSON parziale.

Gli step finali usano `if: always()`, quindi provano a conservare evidence anche
quando staged-install o smoke falliscono. Una cancellazione forzata, un errore
nel bootstrap prima che Python sia disponibile o l'impossibilita' di eseguire
gli step finali possono comunque impedire la creazione: l'artifact mancante e'
assenza di evidenza, non un esito positivo.

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

## Esito della readiness audit v0

La readiness audit finale e' tracciata dalla
[issue #272](https://github.com/kinderp/alfred/issues/272). E' stata eseguita
il 19 luglio 2026 sulla revisione
[`9e2b05c`](https://github.com/kinderp/alfred/commit/9e2b05c99235a2e8a71fe3c27b70e7b9b48b17c5),
dopo avere corretto nella
[issue #273](https://github.com/kinderp/alfred/issues/273) una race del solo
fixture di test delle sonde bounded. La correzione non ha cambiato runtime,
schema evidence, limiti di produzione o contratto di installazione.

La superficie locale verificata e' stata:

| Prova | Esito | Cosa dimostra |
| --- | --- | --- |
| `make test` | Passed | core, correlazione e strutture dati di base |
| `make test-cli` | Passed | contratto del parser CLI |
| `make test-scanner` | Passed | scanner filesystem |
| `make test-watcher` | Passed | wrapper watcher focalizzato |
| `make test-backend-diagnostics` | Passed | diagnostica e percorso backend reale |
| `make test-jsonl` | Passed | contratto pubblico JSONL rappresentativo |
| `make test-compatibility-evidence` | Passed | generatore, validatore, limiti e lifecycle delle sonde |
| `PYTHONOPTIMIZE=1 make test-compatibility-evidence` | Passed | il contratto non dipende dagli `assert` Python |
| `make smoke-mvp` | Passed | percorso utente MVP end-to-end |
| `make test-install` | Passed | release, sette artifact staged, CLI/man page, preflight, ownership e uninstall |

`make test-install` e' stato eseguito per ultimo perche' ricostruisce il profilo
release. La VM locale ha stampato un avviso di clock skew su alcuni dependency
file con timestamp poco piu' avanti dell'orologio corrente; il target ha
comunque ricostruito la release e completato tutte le verifiche staged. Questo
e' un limite ambientale da annotare, non evidenza di incompatibilita' del
runtime.

### Coerenza della documentazione

L'audit non ha controllato soltanto i comandi. Ha confrontato anche le superfici
che descrivono il contratto a utenti, contributori e studenti:

| Superficie | Verifica ed esito |
| --- | --- |
| `README.md` e `README.it.md` | comandi install/staging, tre userspace testati, assenza di promessa stabile e kernel condiviso allineati |
| `09-makefile-e-build-system.md` | variabili di layout, sette artifact, build release, preflight e trust boundary allineati |
| `10-debugging-test-e-strumenti.md` | target staged/evidence e fixture bounded corretto dalla issue #273 allineati |
| `11-come-contribuire.md` | job di riferimento, matrice userspace, dipendenze e artifact CI allineati |
| sei pagine man EN/IT | tutte renderizzate dal test staged; nessun claim di compatibilita' in conflitto |
| `54-audit-installazione-ci-v0.md` | fotografia storica preservata e collegata allo stato implementato |
| `37-roadmap-milestone-progetto.md` | stato, durata reale, risultato e lavoro rimandato aggiornati |
| `27-guida-lettura-documentazione.md` | corretto l'unico drift trovato: il layout non e' piu' soltanto proposto, ma implementato |

Non sono emerse altre divergenze. In particolare, nessuna delle superfici
controllate trasforma le lane container in copertura multi-kernel o gli
ambienti testati in una promessa generale di supporto.

Sul commit auditato sono passati sia il
[job CI di riferimento](https://github.com/kinderp/alfred/actions/runs/29673681506)
sia la
[matrice userspace](https://github.com/kinderp/alfred/actions/runs/29673681495).
I tre artifact scaricati e analizzati contengono:

| Lane | Compilatore | libc | Risultati | Scope kernel |
| --- | --- | --- | --- | --- |
| Ubuntu 24.04 | GCC 13.3.0 | glibc 2.39 | staged-install e smoke `passed` | runner condiviso `6.17.0-1020-azure` |
| Debian 13 | GCC 14.2.0 | glibc 2.41 | staged-install e smoke `passed` | runner condiviso `6.17.0-1020-azure` |
| Fedora 44 | GCC 16.1.1 | glibc 2.43 | staged-install e smoke `passed` | runner condiviso `6.17.0-1020-azure` |

Tutti e tre dichiarano schema `alfred.compatibility-evidence` versione 0,
`source_revision=9e2b05c99235a2e8a71fe3c27b70e7b9b48b17c5`, run
`29673681495`, architettura `x86_64`, filesystem della source tree
`ext2/ext3`, profilo release senza sanitizer e
`kernel_scope=shared-github-runner-kernel`.

Questa evidenza chiude il perimetro v0: Alfred e' **testato**, non ancora
promesso come supportato, nei tre userspace elencati. Restano esplicitamente
rimandati packaging `.deb`/`.rpm`, systemd, musl, altre architetture, VM con
guest kernel, virtme-ng, tmt, CodeQL e fuzzing. Questi elementi non sono
fallimenti della milestone: sono livelli distinti che richiedono una futura
issue e criteri di adozione gia' descritti in questo documento.

Il job CI della PR di chiusura e' passato, ma GitHub ha anche segnalato che
`actions/checkout@v4` e `actions/upload-artifact@v4` dichiarano ancora il
runtime Node.js 20 deprecato e vengono forzati sul runtime Node.js 24. Il
warning non invalida i risultati v0; resta pero' un debito di affidabilita' e
supply-chain tracciato nella
[issue #276](https://github.com/kinderp/alfred/issues/276). La scelta della
versione sostitutiva per entrambe le action e della policy di pinning richiede
una modifica CI separata, non un aggiornamento opportunistico dentro questa
chiusura.

## Checklist della milestone

| Item | Stato | Note |
| --- | --- | --- |
| Setup milestone, issue madre e roadmap | Done | Issue madre #261, issue figlia #262 e PR #263 |
| Audit Makefile, artefatti e CI corrente | Done | Issue #264 e [audit dedicato](54-audit-installazione-ci-v0.md) |
| Contratto `PREFIX` / `DESTDIR` / install / uninstall | Done | Issue #266; copia di sette file e rimozione esatta senza root nei test |
| Test stage-install | Done | Issue #266; binario, sei man page, CLI, layout, preflight e ownership |
| Prima matrice distribuzioni/userspace | Done | Issue #268; Ubuntu 24.04, Debian 13 e Fedora 44, tutte sul kernel condiviso del runner |
| Evidenza ambiente e wording di supporto | Done | Issue #270; JSON v0 per lane, artifact sempre tentato e claim pubblici limitati a userspace testati |
| Readiness closure | Done | [Issue #272](https://github.com/kinderp/alfred/issues/272) e [PR #275](https://github.com/kinderp/alfred/pull/275); suite locali, CI, tre artifact e dichiarazioni pubbliche verificati |

## Test e validazione

Per il setup solo documentale:

```bash
git diff --check
```

Per verificare il contratto Makefile corrente:

```bash
make fclean
make release
make test-install
make test-compatibility-evidence
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
- Evidence userspace v0: [#270](https://github.com/kinderp/alfred/issues/270)
- [Post-MVP documentation and man pages v0](51-post-mvp-documentation-man-pages-v0.md)
- [MVP operational usability v0](49-mvp-operational-usability-v0.md)
- [Roadmap CLI e pagina man](19-roadmap-cli-e-man-page.md)
- [Makefile e build system](09-makefile-e-build-system.md)
- [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
- [Qualita' prodotto software](35-qualita-prodotto-software.md)
- [Registro milestone](37-roadmap-milestone-progetto.md)
