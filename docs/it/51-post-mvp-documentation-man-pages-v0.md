# Post-MVP documentation and man pages v0

Questa milestone riprende un debito dichiarato alla chiusura di
`MVP operational usability v0`: dopo la stabilizzazione di CLI, README, pagine
man e smoke test, bisogna riallineare la documentazione pubblica e le pagine man
al comportamento reale di Alfred.

Il punto importante e' il tempismo. Prima di `CLI parser v0` sarebbe stato
prematuro ampliare troppo le pagine man, perche' opzioni come `-c`,
`--config=FILE`, `-h`, `-V` e `--` non erano ancora parte del contratto
definitivo. Ora invece la superficie utente minima e' abbastanza stabile da
fare un passaggio di qualita' sui documenti pubblici.

## Obiettivo

L'obiettivo e':

```text
rendere la documentazione pubblica coerente, utile e proporzionata all'MVP reale
```

In pratica:

- README inglese e README italiano devono descrivere lo stesso prodotto;
- le pagine man inglesi e italiane devono essere normative, concise e complete
  per l'uso corrente;
- gli `.md` didattici possono restare piu' estesi, con spiegazioni per studenti
  e contributori;
- le feature future devono restare roadmap, non promesse operative;
- ogni aggiornamento deve essere verificabile con `man -l` e, quando tocca
  comandi reali, con test o smoke test collegati.

## Perche' ora

Le milestone recenti hanno chiuso molti contratti user-facing:

- `MVP operational usability v0` ha aggiunto CLI minima, smoke test, README e
  pagine man base;
- `CLI parser v0` ha reso espliciti `-c FILE`, `--config FILE`,
  `--config=FILE`, `--`, `-h`, `-V`, `--help`, `--version` e
  `--check-config`;
- JSONL, session metadata, smoke test e log compatibili hanno gia' una
  documentazione tecnica stabile in `docs/it`;
- i debiti futuri come `--print-config`, packaging man page, nuovi backend e
  Agent Guard sono dichiarati.

Quindi il rischio non e' piu' "manca un parser". Il rischio ora e' la deriva
documentale: README, man page e documenti didattici potrebbero raccontare parti
diverse dello stesso prodotto.

## Non obiettivi

Questa milestone non implementa:

- nuove opzioni CLI;
- `--print-config`;
- installazione o packaging delle pagine man;
- generazione automatica delle pagine man;
- traduzione completa di `docs/it` in `docs/en`;
- modifiche a runtime, parser, backend, Event Model, Writer Runtime o JSONL;
- fanotify, eBPF, audit, Windows, macOS;
- policy, Agent Guard, dashboard o Observation Runtime.

Se durante l'audit emerge un bug reale nel codice, deve diventare una issue
separata. Questa milestone deve evitare di nascondere modifiche comportamentali
dentro un passaggio di documentazione.

## Superfici da controllare

| Superficie | Ruolo | Cosa deve contenere | Cosa non deve contenere |
| --- | --- | --- | --- |
| `README.md` | Primo ingresso pubblico in inglese | Build, prova rapida, CLI stabile, config minima, log, JSONL opt-in, limiti principali. | Spiegazioni didattiche lunghe o promesse future non implementate. |
| `README.it.md` | Primo ingresso pubblico in italiano | Lo stesso contratto del README inglese, con lingua naturale per utenti italiani. | Divergenze funzionali rispetto al README inglese. |
| `docs/man/man1/alfred.1` | Riferimento comando inglese | Synopsis, opzioni, exit status, file prodotti, esempi brevi. | Roadmap lunga o dettagli interni del core. |
| `docs/man/man5/alfred.conf.5` | Riferimento config inglese | Chiavi supportate, valori ammessi, default, esempi validi, errori comuni. | Chiavi future non implementate come se fossero disponibili. |
| `docs/man/man7/alfred-events.7` | Riferimento eventi inglese | Log compatibili, JSONL, livelli record, limiti del modello eventi corrente. | Semantica futura di process/network/policy come contratto attuale. |
| `docs/man/it/...` | Riferimenti localizzati | Stesso contratto delle pagine inglesi, tradotto e verificabile con `man -l`. | Spiegazioni divergenti o piu' promettenti della variante inglese. |
| `docs/it` | Documentazione didattica e architetturale | Razionale, trade-off, esempi, call path, spiegazioni per studenti. | Testo normativo utente non sincronizzato con README/man page. |

La distinzione e' intenzionale:

```text
README introduce.
man page normano.
docs/it spiegano.
roadmap rimanda.
```

## Regola sul contenuto delle man page

Le pagine man non devono diventare un libro. Devono pero' essere abbastanza
complete da permettere a un utente di usare Alfred senza leggere tutta la
documentazione didattica.

Contenuti adatti alle man page:

- sintassi CLI;
- opzioni supportate e rifiutate;
- exit status;
- file letti e scritti;
- chiavi di configurazione;
- esempi brevi;
- limiti importanti che evitano aspettative sbagliate;
- rimando esplicito ai documenti piu' estesi.

Contenuti da lasciare negli MD:

- storia delle decisioni;
- spiegazioni lunghe per studenti;
- discussioni su alternative architetturali;
- benchmark estesi;
- roadmap futura dettagliata;
- call graph completi e diagrammi didattici.

## Checklist proposta

| Item | Stato | Note |
| --- | --- | --- |
| Setup milestone, issue madre e roadmap | In progress | Issue madre #244, issue figlia #245 e questo documento. |
| Audit README inglese/italiano | Todo | Verificare che descrivano lo stesso contratto corrente. |
| Audit man page inglesi | Todo | Controllare `alfred(1)`, `alfred.conf(5)`, `alfred-events(7)`. |
| Audit man page italiane | Todo | Allineare contenuto e tono alle pagine inglesi. |
| Promuovere dettagli tecnici stabili | Todo | Portare nelle man page solo cio' che serve come riferimento operativo. |
| Validare rendering man page | Todo | Eseguire `man -l` su tutte le pagine EN/IT toccate. |
| Readiness closure | Todo | Confermare coerenza, debiti futuri e assenza di scope runtime. |

## Test e validazione

Per modifiche solo documentali:

```bash
git diff --check
```

Per modifiche alle pagine man:

```bash
man -l docs/man/man1/alfred.1 >/dev/null
man -l docs/man/man5/alfred.conf.5 >/dev/null
man -l docs/man/man7/alfred-events.7 >/dev/null
man -l docs/man/it/man1/alfred.1 >/dev/null
man -l docs/man/it/man5/alfred.conf.5 >/dev/null
man -l docs/man/it/man7/alfred-events.7 >/dev/null
```

Per modifiche che toccano esempi CLI o contratti utente:

```bash
make test-cli
make smoke-mvp
```

Per modifiche che toccano JSONL o log:

```bash
make test-jsonl
make test-backend-diagnostics
```

La validazione deve restare proporzionata al rischio. Una correzione di link non
richiede tutta la suite; una modifica di esempi CLI o config invece deve essere
provata.

## Criteri di chiusura

La milestone puo' chiudersi quando:

- README inglese e italiano sono allineati al comportamento corrente;
- le sei pagine man EN/IT renderizzano con `man -l`;
- le pagine man non promettono feature future come se fossero implementate;
- `--print-config`, packaging man page e traduzione completa `docs/en` restano
  debiti espliciti o hanno issue separate;
- gli esempi CLI/config/log presenti nei documenti sono coerenti con
  `CLI parser v0` e `MVP operational usability v0`;
- il registro milestone e la issue madre sono aggiornati.

## Collegamenti

- GitHub Milestone: [Post-MVP documentation and man pages v0](https://github.com/kinderp/alfred/milestone/14)
- Issue madre: [#244](https://github.com/kinderp/alfred/issues/244)
- Setup roadmap: [#245](https://github.com/kinderp/alfred/issues/245)
- MVP usability precedente: [49-mvp-operational-usability-v0.md](49-mvp-operational-usability-v0.md)
- CLI parser v0: [50-cli-parser-v0.md](50-cli-parser-v0.md)
- Roadmap CLI e pagina man: [19-roadmap-cli-e-man-page.md](19-roadmap-cli-e-man-page.md)
- Registro milestone: [37-roadmap-milestone-progetto.md](37-roadmap-milestone-progetto.md)
