# Guida alla sessione first-user

Questa guida accompagna una singola sessione di
`First-user pre-release validation v0`. Il facilitatore conserva la guida
completa e mostra al partecipante una fase alla volta; il partecipante puo'
leggerla interamente dopo che i tempi di onboarding sono stati registrati.
Questa disclosure progressiva permette a una persona che non conosce Alfred di
provare la documentazione pubblica senza ricevere in anticipo le risposte.

Il contratto generale, i limiti dei claim e i criteri di uscita restano in
[First-user pre-release validation v0](../55-first-user-pre-release-validation-v0.md).
I risultati della sessione vanno registrati nel
[template di report](report-template.md).

## Scopo della sessione

La sessione verifica se il partecipante riesce a:

1. capire dai documenti cosa fa Alfred;
2. compilare e interrogare la CLI;
3. provare l'installazione staged senza privilegi;
4. avviare Alfred su una root sacrificabile;
5. riconoscere eventi testuali e JSONL;
6. fermare Alfred e pulire l'ambiente;
7. spiegare cosa e' stato chiaro o ambiguo.

Una sessione positiva riguarda una revisione e un ambiente precisi. Non
dimostra supporto generale della distribuzione, copertura di kernel differenti,
assenza di bug, performance, fuzzing o sicurezza completa.

## Ruoli e regola di osservazione

Il partecipante esegue i comandi e descrive ad alta voce cosa si aspetta. Il
facilitatore:

- presenta consenso e confini di sicurezza;
- registra tempi, interventi e risultati;
- interviene solo se il partecipante chiede aiuto, resta bloccato o rischia di
  usare dati reali;
- non trasforma un'osservazione in bug prima della riproduzione;
- non registra terminale, schermo o applicazioni senza consenso separato.

Durante l'onboarding il partecipante riceve soltanto consenso, obiettivo della
fase e link ai README. Le sezioni con comandi canonici sono riferimenti del
facilitatore: vanno mostrate solo dopo che il passo e' terminato, e' stato
classificato `BLOCKED` oppure il partecipante ha chiesto un intervento da
registrare. Dalla fase staged-install in poi la sezione corrente puo' essere
condivisa integralmente.

Ogni aiuto viene annotato nel report con momento, motivo e informazione data.
Un passo completato dopo un suggerimento non vale come completato in autonomia.

## Prima di iniziare: consenso e sicurezza

Il facilitatore deve leggere al partecipante queste informazioni:

- verranno raccolti revisione sorgente, distribuzione, kernel osservato,
  architettura, libc, compilatore, filesystem di prova, comandi, esiti, tempi e
  log Alfred pertinenti;
- non verranno raccolti nome reale, username, hostname, variabili d'ambiente
  complete, credenziali o file personali;
- GitHub ricevera' soltanto report ed estratti sanificati;
- gli artifact completi resteranno locali o in una destinazione approvata;
- il partecipante puo' interrompere la sessione prima della pubblicazione.

La sessione puo' iniziare solo dopo avere marcato il consenso nel report.

Regole non negoziabili:

- non osservare `$HOME`, `~/.ssh`, directory di credenziali o progetti reali;
- non inserire segreti o dati personali nei file di prova;
- non eseguire Alfred o i test come root;
- usare `sudo` solo per eventuali dipendenze note e con consenso esplicito;
- non fare upload automatici;
- fermarsi se un comando risolve fuori dalla root temporanea prevista.

## Materiale necessario

- checkout pulito di Alfred sulla revisione da provare;
- Linux, `make`, compilatore C, Bash, Python 3 e utility standard;
- `man` per verificare le pagine staged;
- due terminali: uno per Alfred e uno per le azioni filesystem;
- cronometro o timestamp per i due tempi di onboarding;
- copia locale del [template di report](report-template.md).

Il facilitatore non deve correggere preventivamente dipendenze mancanti: se il
README non permette al partecipante di identificarle, il blocco e' un risultato
da registrare.

## Preparazione della sessione

Il Session ID deve essere pseudonimo e contenere solo lettere ASCII, numeri e
trattini, per esempio `FU-20260719-01`. Dalla root del repository:

```bash
session_id=FU-20260719-01
tools/first-user/session-context.sh create "$session_id"
```

Il comando crea la root sacrificabile, `watch/`, `run/`, `stage/`, la copia del
report, `commands.txt` e `session.env`. La root ha modo `0700`; il file di
contesto ha modo `0600` e resta locale perche' contiene i path reali della
sessione. L'helper stampa un unico comando `source ... load ...` gia' quotato
per la shell. Eseguire quel comando nella shell corrente e conservarlo nel
transcript locale: sara' il bootstrap esplicito di ogni nuovo terminale.

`load` non usa `eval` e non esegue il contenuto di `session.env`. Prima di
assegnare `repo_root`, `session_root`, `watch_root`, `run_root` e `stage_root`
verifica versione, chiavi, proprietario, modo `0600`, path canonici e
discendenza dalla root `/tmp/alfred-first-user-SESSION_ID.XXXXXX`. Il solo
risultato accettabile e':

```text
session context OK
```

Se il file manca, e' un symlink, e' stato modificato, ha permessi diversi o un
path esce dalla root sacrificabile, il bootstrap fallisce senza assegnare un
contesto parziale. Un tentativo fallito cancella anche le variabili di una
sessione caricata in precedenza, cosi' i comandi successivi non possono usare
silenziosamente path stale. Non ricostruire i path a mano e non proseguire. Il
path completo e' materiale locale da sanificare; nel report pubblico usare
`<SESSION_ROOT>`, `<WATCH_ROOT>` e `<REPO_ROOT>`.

### Transcript manuale dei comandi

Da questo momento il facilitatore aggiorna `commands.txt` mentre la sessione e'
in corso. Per ogni comando registra numero progressivo, timestamp UTC, fase,
terminale e testo esatto eseguito. Per una azione GUI annota applicazione,
azione e path sintetico interessato. Errori di battitura e tentativi falliti
restano nel transcript: fanno parte dell'evidenza di onboarding.

Il transcript e' manuale e limitato alla sessione. Non usare `history`, il
comando `script`, keylogger, screen recording, terminal capture o altri sistemi
automatici: potrebbero raccogliere comandi precedenti, output, segreti o dati
estranei. `commands.txt` resta grezzo e locale fino alla sanificazione; prima
della pubblicazione i path reali vengono sostituiti con i placeholder definiti
dal protocollo.

## Evidenza ambiente minimizzata

Eseguire e riportare gli output utili in forma sanificata:

```bash
: "${watch_root:?eseguire prima il bootstrap della sessione}"
git rev-parse HEAD
date -u +%Y-%m-%dT%H:%M:%SZ
uname -s
uname -r
uname -m
getconf GNU_LIBC_VERSION 2>/dev/null || printf 'unknown\n'
cc --version | sed -n '1p'
make --version | sed -n '1p'
stat -f -c '%T' "$watch_root" 2>/dev/null || printf 'unknown\n'
if [ -r /etc/os-release ]; then
    . /etc/os-release
    printf '%s %s\n' "${NAME:-unknown}" "${VERSION_ID:-unknown}"
else
    printf 'unknown\n'
fi
```

Non usare `uname -a`, `env`, `set` o dump completi della configurazione: possono
contenere hostname, path e dati non necessari. Una sonda non disponibile vale
`unknown`, non `PASS` e non una stima.

## Fase 1: onboarding senza suggerimenti

Il partecipante riceve il [README italiano](../../../README.it.md) o il
[README inglese](../../../README.md), ma non le sezioni successive di questa
guida. Il facilitatore gli chiede di compilare Alfred, trovare help e versione
e validare la configurazione usando soltanto la documentazione pubblica. `T0`
viene registrato quando inizia la lettura:

```bash
date -u +%Y-%m-%dT%H:%M:%SZ
date +%s
```

I comandi seguenti sono il riferimento del facilitatore e non vanno mostrati
prima di avere registrato l'esito del passo:

```bash
make
./alfred --help
./alfred --version
./alfred --check-config
```

Registrare:

- `time-to-first-command`: secondi da `T0` al primo comando documentato con
  exit status `0`;
- documenti consultati e parole cercate;
- errori, dubbi e interventi del facilitatore;
- cosa il partecipante pensa contengano `raw.log`, `events.log`, `errors.log`
  e `output.jsonl`.

Non fermare il timer `time-to-first-event`: termina soltanto nella fase runtime
quando il partecipante riconosce un evento semantico atteso.

## Fase 2: installazione staged non-root

Il partecipante usa il contratto pubblico senza modificare il sistema:

```bash
: "${repo_root:?eseguire prima il bootstrap della sessione}"
: "${stage_root:?eseguire prima il bootstrap della sessione}"
make release
make DESTDIR="$stage_root" PREFIX=/usr install
"$stage_root/usr/bin/alfred" --version
man "$stage_root/usr/share/man/man1/alfred.1"
make DESTDIR="$stage_root" PREFIX=/usr uninstall
test ! -e "$stage_root/usr/bin/alfred"
```

Risultato minimo atteso:

- build release riuscita;
- binario staged eseguibile;
- pagina man renderizzabile;
- uninstall rimuove gli artifact Alfred senza cancellare lo stage root.

Se `man` manca, classificare `BLOCKED` o problema di onboarding secondo
l'evidenza; non installarlo silenziosamente durante la misura.

## Fase 3: configurazione JSONL

Creare una configurazione dedicata sotto la root temporanea:

```bash
: "${repo_root:?eseguire prima il bootstrap della sessione}"
: "${run_root:?eseguire prima il bootstrap della sessione}"
printf '%s\n' \
    'output_enabled=true' \
    'output_format=jsonl' \
    'output_buffer_size=65536' \
    'output_log=output.jsonl' \
    > "$run_root/alfred.conf"

"$repo_root/alfred" -c "$run_root/alfred.conf" --check-config
```

Il controllo deve terminare con exit status `0` senza avviare il runtime. Non
pubblicare la configurazione se e' stata modificata con path o valori privati.

## Fase 4: runtime rappresentativo

Aprire il primo terminale ed eseguire come primo comando il bootstrap stampato
dalla preparazione. Procedere soltanto dopo `session context OK`, quindi:

```bash
: "${repo_root:?contesto sessione non caricato}"
: "${run_root:?contesto sessione non caricato}"
: "${watch_root:?contesto sessione non caricato}"
cd "$run_root"
"$repo_root/alfred" -c "$run_root/alfred.conf" "$watch_root"
```

Attendere che `events.log` mostri `WATCH_ADDED` per la root prima di iniziare.
Aprire il secondo terminale, eseguire lo stesso bootstrap e attendere
`session context OK`. Eseguire poi una operazione alla volta:

```bash
: "${watch_root:?contesto sessione non caricato}"
printf 'alpha\n' > "$watch_root/draft.txt"
printf 'beta\n' >> "$watch_root/draft.txt"
mv "$watch_root/draft.txt" "$watch_root/report.txt"
mkdir "$watch_root/inbox" "$watch_root/archive"
printf 'item\n' > "$watch_root/inbox/item.txt"
mv "$watch_root/inbox/item.txt" "$watch_root/archive/item.txt"
rm "$watch_root/archive/item.txt"
rmdir "$watch_root/inbox" "$watch_root/archive"
```

Il partecipante osserva i log senza richiedere conteggi esatti di eventi raw.
I segnali semantici rappresentativi sono:

| Azione | Segnale da riconoscere |
| --- | --- |
| Startup | `WATCH_ADDED` per la root |
| Scrittura file | `FILE_CREATED`, modifica e/o `FILE_READY` pertinenti |
| Rename stesso parent | `FILE_RENAMED` con old/new path |
| Directory nuove | `DIR_CREATED` e relativi watch diagnostici |
| Move tra parent osservati | `FILE_MOVED` con old/new path |
| Delete | `FILE_DELETED` e `DIR_DELETED` pertinenti |

Al primo evento semantico riconosciuto registrare timestamp e
`time-to-first-event` rispetto allo stesso `T0` della fase 1.

Fermare Alfred nel primo terminale con `Ctrl+C`. Uno shutdown pulito non deve
essere sostituito da `kill -9`. Registrare exit status e ogni ritardo o errore.

## Fase 5: controllo degli output

Dentro `run_root` devono essere valutati:

```text
raw.log
events.log
errors.log
output.jsonl
```

`errors.log` puo' essere vuoto: l'assenza di errori non autorizza a inventare
evidenza. Verificare che ogni riga JSONL non vuota sia JSON valido:

```bash
python3 -c 'import json, sys; [json.loads(line) for line in sys.stdin if line.strip()]' \
    < "$run_root/output.jsonl"
```

Per ogni scenario registrare nel report:

- comando o azione GUI annotata;
- comportamento atteso;
- comportamento osservato;
- estratto minimo che supporta la conclusione;
- stato `PASS`, `FAIL`, `NEEDS_REVIEW`, `BLOCKED` o `NOT_RUN`.

Il comando descrive l'intento umano, ma non dimostra quale processo o syscall
abbia causato ogni evento. Alfred v0 non offre ancora attribution completa.

## Fase 6: sessione user-like

Per 45-90 minuti il partecipante lavora soltanto dentro `watch_root` su un mini
progetto sacrificabile. Puo' usare editor, compilatore e comandi normali per:

- creare una piccola struttura `src/` e `build/`;
- modificare e rinominare file;
- generare e pulire output di build;
- spostare artifact tra directory;
- usare operazioni GUI annotate manualmente.

Non introdurre workload segreti o directory reali per rendere la prova piu'
realistica. Registrare crash, blocchi, crescita evidente di risorse o log,
shutdown anomalo, eventi incomprensibili e divergenze testo/JSONL. Queste sono
osservazioni esplorative, non benchmark.

## Debrief

Il facilitatore chiede, senza suggerire la risposta:

1. Come descriveresti Alfred a un altro utente?
2. Quale documento o messaggio ti ha aiutato di piu'?
3. Dove hai dovuto indovinare?
4. Quale log useresti per capire un evento? Perche'?
5. Cosa ti aspettavi che Alfred facesse ma non ha fatto?
6. Quale passaggio non ripeteresti senza aiuto?

Feedback, richiesta di feature e bug restano categorie separate nel report.

## Sanificazione obbligatoria

Prima di pubblicare report o estratti:

- sostituire repository e root con `<REPO_ROOT>`, `<SESSION_ROOT>` e
  `<WATCH_ROOT>`;
- rimuovere username, hostname, IP, token, email e identificatori personali;
- non pubblicare output di `env`, cronologia shell o file non creati per la
  sessione;
- ridurre i log alle righe necessarie, conservando timestamp e tipo evento;
- verificare che `commands.txt` non contenga path o argomenti privati;
- non pubblicare `session.env` o il bootstrap con i path reali;
- registrare chi ha eseguito il controllo di sanificazione;
- mantenere gli artifact grezzi locali salvo consenso separato.

Una sanitizzazione incompleta blocca la pubblicazione. Non trasforma lo
scenario in `FAIL`: e' un problema di gestione dell'evidenza.

## Cleanup

Con Alfred gia' fermo:

```bash
: "${repo_root:?contesto sessione non caricato}"
: "${stage_root:?contesto sessione non caricato}"
cd "$repo_root"
make DESTDIR="$stage_root" PREFIX=/usr uninstall
test ! -e "$stage_root/usr/bin/alfred"
```

Verificare manualmente che non restino processi Alfred della sessione. Prima di
rimuovere `session_root`, decidere se gli artifact grezzi devono restare locali
per la riproduzione. Non cancellarli prima del triage e non caricarli senza
sanificazione e consenso. `session.env` viene eliminato insieme alla root e non
deve essere copiato nel repository o in uno storage pubblico.

## Dopo la sessione

Il partecipante consegna il report al facilitatore. Il maintainer:

1. controlla provenienza e sanificazione;
2. riproduce ogni `FAIL` e `NEEDS_REVIEW`;
3. distingue bug, documentazione, ambiente, richiesta e debito di ricerca;
4. crea una issue figlia dedicata per ogni finding confermato;
5. mantiene link bidirezionali con la issue madre della sessione;
6. non aumenta automaticamente la maturita' se manca evidenza.
