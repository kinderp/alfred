# Configurazione Codex Sol Ultra e sub-agenti

Questo documento spiega come replicare su una macchina nuova una
configurazione Codex orientata al lavoro su Alfred con modello `gpt-5.6-sol`,
ragionamento `ultra` e supporto multi-agent.

Il documento non riguarda il runtime di Alfred. Riguarda l'ambiente di lavoro
degli agenti AI usati per sviluppare, documentare, fare review e verificare il
progetto.

## Obiettivo

Vogliamo avere una configurazione scaricabile dal repository e installabile su
un'altra macchina, per esempio quella usata a scuola, senza copiare dati
personali o sensibili.

Il repository contiene quindi:

- un template `config.toml` sanificato;
- tre agenti locali di esempio;
- uno script di installazione che copia i file nella cartella Codex corretta;
- questa spiegazione per capire cosa viene installato e cosa non viene
  installato.

## File nel repository

| File | Dove viene installato | Cosa contiene |
| --- | --- | --- |
| `tools/codex/sol-ultra/config.toml` | `~/.codex/config.toml` | Modello, reasoning effort, feature multi-agent e limiti globali dei sub-agenti. |
| `tools/codex/sol-ultra/agents/deep-reviewer.toml` | `~/.codex/agents/deep-reviewer.toml` | Agente per review architetturali e contratti. |
| `tools/codex/sol-ultra/agents/code-explorer.toml` | `~/.codex/agents/code-explorer.toml` | Agente read-only per mappare codice e documentazione. |
| `tools/codex/sol-ultra/agents/test-runner.toml` | `~/.codex/agents/test-runner.toml` | Agente per proporre ed eseguire validazioni mirate. |
| `tools/codex/install-sol-ultra.sh` | Non viene copiato | Script che installa il profilo nella home Codex dell'utente. |

La cartella Codex globale dell'utente e' `~/.codex` se `CODEX_HOME` non e'
impostata. Se `CODEX_HOME` e' impostata, lo script usa quella directory.

## Cosa non viene copiato

Il template non include:

- `auth.json`;
- token;
- credenziali;
- cache plugin;
- stato OAuth;
- percorsi personali della macchina di sviluppo;
- trust specifici di progetto;
- configurazioni locali non necessarie al profilo Sol Ultra.

Questo e' intenzionale: il file nel repository deve poter essere scaricato e
copiato su macchine diverse senza esportare lo stato privato della macchina di
sviluppo.

## Installazione rapida

Dalla root del repository:

```bash
sh tools/codex/install-sol-ultra.sh
```

Se `~/.codex/config.toml` esiste gia', lo script si ferma per evitare di
sovrascrivere una configurazione personale senza una scelta esplicita. In quel
caso si puo' installare forzando la sovrascrittura:

```bash
sh tools/codex/install-sol-ultra.sh --force
```

Quando usa `--force`, lo script salva prima una copia del vecchio file in:

```text
~/.codex/backups/config.toml.before-sol-ultra-YYYYMMDDTHHMMSSZ
```

Questo permette di ripristinare la configurazione precedente se la macchina
aveva gia' impostazioni personali, plugin o progetti fidati.

## Verifica dopo installazione

Dopo l'installazione, riavviare Codex e verificare:

```bash
codex --strict-config --help
codex features list
```

Nel secondo comando ci aspettiamo di vedere almeno:

```text
multi_agent    stable    true
```

Se il modello `gpt-5.6-sol` non e' disponibile nella macchina o nell'account
usato, Codex segnalerà un errore quando prova a usare quel modello. In quel
caso bisogna scegliere un modello disponibile oppure aggiornare l'ambiente
Codex.

## Significato dei campi principali

Il template contiene:

```toml
model = "gpt-5.6-sol"
model_reasoning_effort = "ultra"

[features]
multi_agent = true
fast_mode = true

[agents]
max_threads = 8
max_depth = 1
job_max_runtime_seconds = 1800
interrupt_message = true
```

`model = "gpt-5.6-sol"` seleziona il modello di default per la sessione Codex.

`model_reasoning_effort = "ultra"` chiede il massimo livello di ragionamento
supportato dal modello e dalla superficie Codex in uso.

`multi_agent = true` abilita il supporto multi-agent quando disponibile. La
vecchia chiave `multi_agent_mode` non deve essere usata: nel catalogo locale
risulta rimossa.

`fast_mode = true` lascia attiva la modalita' veloce dove supportata dalla
superficie Codex. Non sostituisce `model_reasoning_effort`: sono impostazioni
diverse.

`max_threads = 8` limita quanti sub-agenti possono lavorare in parallelo. Il
limite serve a evitare fan-out incontrollato, consumo eccessivo di risorse e
review difficili da seguire.

`max_depth = 1` permette all'agente principale di creare sub-agenti diretti, ma
impedisce ai sub-agenti di creare altri sub-agenti. Per Alfred e' una scelta
prudente: usiamo parallelismo, ma manteniamo il lavoro leggibile.

`job_max_runtime_seconds = 1800` limita ogni job sub-agent a 30 minuti.

`interrupt_message = true` permette di ricevere messaggi quando un job
sub-agent viene interrotto.

## Agenti inclusi

`deep-reviewer` serve per review pignole su architettura, ownership,
contratti, test, sicurezza, performance e manutenibilita'. Non sostituisce la
review dell'agente principale: produce un secondo punto di vista.

`code-explorer` serve per mappare codice e documentazione senza modificare
file. E' utile quando bisogna capire dove vive una funzione, quale contratto
tocca o quali documenti leggere.

`test-runner` serve per proporre ed eseguire validazioni mirate. Deve
preferire test piccoli e riproducibili prima di suite piu' ampie.

## Esempi di prompt

Per usare sub-agenti in modo esplicito:

```text
Usa i sub-agenti: deep-reviewer per rischi architetturali,
code-explorer per mappa codice/documentazione e test-runner per validazione.
Aspetta tutti i risultati e poi sintetizza.
```

Per una review PR Alfred:

```text
Usa deep-reviewer e test-runner. Fai review pignola della PR:
piccola, leggera, veloce, performante, affidabile, sicura, estendibile,
interoperabile, robusta, semplice, documentata, manutenibile e con contratti
API non ambigui e coperti.
```

## Ripristino manuale

Se serve ripristinare un backup:

```bash
cp ~/.codex/backups/config.toml.before-sol-ultra-YYYYMMDDTHHMMSSZ ~/.codex/config.toml
```

Sostituire `YYYYMMDDTHHMMSSZ` con il timestamp reale del backup.

## Nota su "adaptive thinking"

Non abbiamo trovato una chiave separata `adaptive_thinking` da configurare nel
file TOML. La parte rilevante per questa impostazione e' la scelta del modello
e del reasoning effort:

```toml
model = "gpt-5.6-sol"
model_reasoning_effort = "ultra"
```

Quindi nel repository documentiamo quello che la configurazione puo' controllare
in modo esplicito: modello, reasoning effort, feature multi-agent e limiti dei
sub-agenti.
