# Workspace/session runtime context v0

Questo documento descrive il primo micro-step di implementazione dopo
[Workspace/session runtime schema v0](44-workspace-session-runtime-schema-v0.md).

La milestone precedente ha deciso il contratto concettuale dei tre campi:

```text
workspace_root
workspace_id
ledger_session_id
```

Questo step implementa solo il contesto runtime minimo dentro l'applicazione.
Non cambia i record, non cambia JSONL e non introduce policy o Agent Guard.

## Obiettivo

L'obiettivo e' avere un punto unico, owned da `app_t`, dove Alfred puo'
conservare il contesto dichiarato della run:

```text
config file
    |
    v
config_t.workspace_session
    |
    | app_init_workspace_session_context()
    v
app_t.workspace_session
```

Questo prepara i passi futuri senza far pagare subito un costo per evento.

## Campi configurabili

Le chiavi di configurazione sono opzionali:

| Chiave | Dove viene salvata | Significato |
| --- | --- | --- |
| `workspace_root` | `config_t.workspace_session.workspace_root` e poi `app_t.workspace_session.workspace_root` | Root filesystem dichiarata come perimetro osservazionale della run |
| `workspace_id` | `config_t.workspace_session.workspace_id` e poi `app_t.workspace_session.workspace_id` | Identificatore opaco del workspace dichiarato |
| `ledger_session_id` | `config_t.workspace_session.ledger_session_id` e poi `app_t.workspace_session.ledger_session_id` | Identificatore opaco della run/ledger Alfred |

Un campo assente significa: Alfred non ha quel contesto.

Un campo presente ma vuoto viene rifiutato:

```text
workspace_root=
workspace_id=
ledger_session_id=
```

Il motivo e' contrattuale: una stringa vuota renderebbe ambiguo il significato
fra "non configurato", "configurato male" e "intenzionalmente vuoto". Per v0 la
scelta e' fail-fast durante `config_load()`.

## Cosa non cambia

Questo micro-step non modifica:

- `alfred_record_t`;
- `alfred_record_clone_owned()`;
- `alfred_record_queue_push()`;
- `alfred_record_queue_pop()`;
- dispatcher;
- sink;
- writer JSONL;
- golden JSONL;
- backend inotify;
- core semantico filesystem.

Quindi il percorso caldo degli eventi resta:

```text
backend/core -> alfred_record_t corrente -> queue/output corrente
```

Il contesto workspace/sessione e' laterale e app-owned. I record accodati non
contengono puntatori borrowed verso questo contesto.

## Ownership e lifetime

Ci sono due strutture:

| Struttura | Owner | Lifetime |
| --- | --- | --- |
| `workspace_session_config_t` | `config_t` | Dalla `config_defaults()` fino allo shutdown dell'app |
| `app_workspace_session_context_t` | `app_t` | Dalla `app_init_workspace_session_context()` fino allo shutdown dell'app |

Entrambe usano buffer inline, non allocazioni dinamiche. Questo significa:

- niente `malloc`;
- niente `free`;
- niente ownership esterna;
- niente puntatori borrowed verso stringhe del file di configurazione;
- niente cleanup speciale.

`config_load()` copia le stringhe dal file nei buffer di configurazione.
`app_init_workspace_session_context()` copia le stringhe dalla configurazione
nel contesto runtime dell'app.

La seconda copia sembra ridondante, ma serve a fissare un confine chiaro:

```text
config_t = valori caricati
app_t.workspace_session = contesto runtime pubblicato per la run
```

Se in futuro arrivera' reload di configurazione, writer asincroni o cambio di
sessione, questo confine impedira' di mutare in place un contesto gia' letto da
altri componenti.

## Funzioni coinvolte

### `config_defaults()`

Inizializza `config_t` a zero con `memset()`.

Effetto sui nuovi campi:

```text
has_workspace_root = 0
has_workspace_id = 0
has_ledger_session_id = 0
workspace_root[0] = '\0'
workspace_id[0] = '\0'
ledger_session_id[0] = '\0'
```

Questo rende i campi assenti per default.

### `config_load()`

Legge il file `key=value`.

Per le nuove chiavi chiama `parse_required_string()`:

```text
workspace_root=...
workspace_id=...
ledger_session_id=...
```

Se il valore e' valido:

1. viene copiato nel buffer inline;
2. il relativo flag `has_*` viene impostato a `1`.

Se il valore e' vuoto o troppo lungo:

```text
config_load() -> ERR_CONFIG
```

### `parse_required_string()`

Controlla tre cose:

1. puntatori validi;
2. stringa non vuota;
3. stringa abbastanza corta da entrare nel buffer senza troncamento.

Il rifiuto del troncamento e' importante: un `workspace_id` troncato potrebbe
diventare un identificatore diverso da quello scelto dall'utente.

### `app_init_workspace_session_context()`

Copia i valori opzionali da `app->config.workspace_session` a
`app->workspace_session`.

Viene chiamata dentro `app_init()` dopo il parsing della configurazione e prima
di inizializzare logger, core, backend e output pipeline.

La posizione nel lifecycle e' intenzionale:

```text
config_defaults()
config_load()
config_set_event_engine()
app_init_workspace_session_context()
logger_init()
alfred_create()
app_init_output_pipeline()
backend_ops->init()
backend_ops->start()
```

Il contesto viene quindi pubblicato prima che Alfred inizi a osservare eventi.

### `app_init()`

Resta il composition root: carica configurazione, pubblica il contesto runtime,
inizializza logger, core, output e backend.

Il backend non riceve `app->workspace_session`, perche' questi campi non sono
evidenza osservata da inotify.

## Esempio configurazione

```text
workspace_root=/home/antonio/alfred
workspace_id=ws-alfred-local
ledger_session_id=ls-2026-07-14-local
```

Interpretazione corretta:

- Alfred conosce un workspace dichiarato per questa run;
- Alfred puo' in futuro usare `workspace_id` e `ledger_session_id` per
  raggruppare output o metadata;
- non significa che ogni evento sia dentro il workspace;
- non significa che un agente AI abbia causato gli eventi;
- non abilita blocco, policy o would-block.

## Test

Il test focused e':

```bash
bash tests/backend/test_workspace_session_config.sh
```

Verifica:

- default assenti;
- parsing di valori validi;
- rifiuto di `workspace_root=`;
- rifiuto di `workspace_id=`;
- rifiuto di `ledger_session_id=`;
- rifiuto di identificatori troppo lunghi.

Questo test e' volutamente solo di configurazione. I test JSONL arriveranno solo
quando questi campi diventeranno output pubblico.

## Debiti rimandati

Restano fuori da questo micro-step:

- opzioni CLI dedicate;
- generazione automatica di `workspace_id`;
- generazione automatica di `ledger_session_id`;
- normalizzazione/canonicalizzazione di `workspace_root`;
- record metadata/sessione JSONL;
- enrichment per-record JSONL;
- golden JSONL;
- policy `inside_workspace` / `outside_workspace`;
- `agent_session_id`;
- attribuzione processo/agente;
- benchmark refresh.

Il benchmark refresh non e' necessario per questo step perche' il contesto viene
letto una volta in avvio e non attraversa il percorso caldo degli eventi.

## Stato di chiusura

La milestone `Workspace/session runtime context v0` e' chiusa come primo
sottoinsieme runtime.

Esito consolidato:

- `config_t.workspace_session` contiene i tre valori opzionali caricati da
  configurazione;
- `app_t.workspace_session` contiene la copia runtime app-owned, pubblicata
  durante `app_init()`;
- `workspace_root`, `workspace_id` e `ledger_session_id` sono assenti per
  default;
- valori presenti ma vuoti vengono rifiutati da `config_load()` con
  `ERR_CONFIG`;
- identificatori `workspace_id` e `ledger_session_id` troppo lunghi vengono
  rifiutati invece di essere troncati;
- le stringhe usano buffer inline e non richiedono `malloc`, `free` o cleanup
  dedicato;
- il backend inotify non riceve questo contesto;
- il core filesystem non usa questo contesto per semantica create/delete/move;
- `alfred_record_t`, queue, dispatcher, sink e writer JSONL restano invariati.

PR di riferimento:

- runtime implementation: [PR #160](https://github.com/kinderp/alfred/pull/160)
- milestone closure: [PR #162](https://github.com/kinderp/alfred/pull/162)
- merge commit [710bf15](https://github.com/kinderp/alfred/commit/710bf15)
- commit principali:
  [b764e7b](https://github.com/kinderp/alfred/commit/b764e7b),
  [2be734e](https://github.com/kinderp/alfred/commit/2be734e)

Validazioni registrate:

```bash
bash tests/backend/test_workspace_session_config.sh
cd tests/backend && bash test_workspace_session_config.sh
git diff --check
make test-backend-diagnostics
```

Questa chiusura non rende ancora pubblico il contesto in JSONL. Il prossimo
passo, se scelto, dovra' essere una milestone o issue dedicata per il record
metadata/sessione, con golden JSONL e benchmark gate se cambia output, record o
percorso caldo.
