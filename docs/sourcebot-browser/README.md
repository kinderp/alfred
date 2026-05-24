# Sourcebot per Alfred

Questa cartella contiene un setup locale per provare
[Sourcebot](https://github.com/sourcebot-dev/sourcebot) come browser web del
codice di Alfred.

Sourcebot non usa Kythe: e' una piattaforma self-hosted basata su Docker che
fornisce una UI web moderna per cercare e leggere codice. Sotto il cofano usa
Zoekt per la ricerca indicizzata e Universal Ctags per individuare simboli.
Per il nostro uso didattico e' interessante perche' offre subito file explorer,
syntax highlighting, ricerca regex, filtri per linguaggio e navigazione via web.

## Perche' provarlo

Rispetto a Kythe, Sourcebot e' meno orientato al grafo semantico profondo, ma
ha una interfaccia utente pronta. Questo lo rende piu' vicino a quello che serve
agli studenti quando devono esplorare una codebase C senza conoscere ancora
tutta la struttura del progetto.

Rispetto a Elixir, Sourcebot e' piu' moderno e piu' ricco come esperienza di
ricerca. Di contro, e' piu' pesante: richiede un container applicativo completo
con database interno e cache persistente.

## Limite importante

La documentazione Sourcebot indica che la code navigation completa, cioe'
hover, go-to-definition e find-references integrati nella UI, richiede una
licenza Enterprise. La ricerca testuale, il file explorer, i filtri e la
ricerca dei simboli indicizzati restano comunque utili per il nostro scenario.

## Cosa viene installato

Il setup non installa pacchetti sul sistema host. Usa Docker per scaricare ed
eseguire l'immagine:

```text
ghcr.io/sourcebot-dev/sourcebot:v4.17.3
```

Dentro il container Sourcebot avvia piu' servizi coordinati:

- applicazione web Next.js sulla porta interna `3000`
- backend worker che legge `config.json`, sincronizza i repository e avvia
  l'indicizzazione
- Zoekt, il motore di ricerca indicizzato usato per cercare velocemente nel
  codice
- Universal Ctags, usato durante l'indicizzazione per riconoscere definizioni
  di simboli quando possibile
- Postgres e Redis embedded, usati internamente da Sourcebot

Questa architettura spiega perche' Sourcebot e' piu' pesante di Elixir: non e'
solo un piccolo server statico, ma una applicazione completa con database,
cache e job di indicizzazione.

## File principali

- `config.json`: configura Sourcebot per indicizzare il repository locale
  Alfred montato nel container come `/repos/alfred`.
- `start-sourcebot.sh`: avvia Sourcebot su `127.0.0.1:3000`.
- `stop-sourcebot.sh`: ferma e rimuove il container locale.
- `status-sourcebot.sh`: mostra stato container e risposta HTTP.
- `.work/`: appoggi locali eventuali, ignorati da Git.

## Avvio rapido

Dalla root del progetto:

```sh
docs/sourcebot-browser/start-sourcebot.sh
```

Poi apri:

```text
http://127.0.0.1:3000
```

Il primo avvio puo' richiedere tempo per tre motivi:

1. Docker deve scaricare l'immagine Sourcebot se non e' gia' presente.
2. Sourcebot inizializza Postgres e applica le migrazioni.
3. Il backend indicizza Alfred e genera gli shard Zoekt.

Durante questa fase la pagina puo' aprirsi prima che la ricerca sia pronta. In
quel caso non e' necessariamente un errore: controlla i log e attendi la fine
dell'indicizzazione.

Per usare un'altra porta:

```sh
SOURCEBOT_PORT=3001 docs/sourcebot-browser/start-sourcebot.sh
```

Quando cambi porta, ricordati di usare la stessa variabile anche per stato e
stop se hai cambiato anche il nome del container tramite
`SOURCEBOT_CONTAINER_NAME`.

## Stop normale

```sh
docs/sourcebot-browser/stop-sourcebot.sh
```

Questo comando rimuove il container `alfred-sourcebot`, ma non cancella il
volume dati `alfred-sourcebot-data`. La scelta e' intenzionale: in questo modo
al riavvio Sourcebot non deve ricreare database e indice da zero.

## Stato e log

```sh
docs/sourcebot-browser/status-sourcebot.sh
docker logs -f alfred-sourcebot
```

`status-sourcebot.sh` mostra due cose:

- lo stato Docker del container
- la risposta HTTP locale seguendo i redirect

Un output come questo indica che la UI risponde:

```text
NAMES              STATUS          PORTS
alfred-sourcebot   Up ...          127.0.0.1:3000->3000/tcp

HTTP locale:
200 ...
```

Al primo avvio Sourcebot deve sincronizzare e indicizzare il repository. Se la
pagina si apre ma non mostra subito risultati, controlla i log e attendi la fine
della fase di indexing.

Comandi utili sui log:

```sh
docker logs --tail 120 alfred-sourcebot
docker logs -f alfred-sourcebot
```

Nei log e' normale vedere:

- inizializzazione di Postgres al primo avvio
- applicazione delle migrazioni Prisma
- avvio di Redis, Zoekt, backend e web
- messaggi del backend worker
- caricamento degli shard Zoekt

Un messaggio come `Anonymous access enabled` conferma che la modalita' anonima
locale e' attiva.

## Reset completo

Per fermare Sourcebot e cancellare anche dati, database e indice:

```sh
docs/sourcebot-browser/stop-sourcebot.sh
docker volume rm alfred-sourcebot-data
```

Usa il reset completo quando:

- vuoi ripartire da una configurazione pulita
- hai cambiato drasticamente `config.json`
- Sourcebot resta in uno stato incoerente dopo prove o aggiornamenti

Non serve farlo dopo ogni modifica al codice Alfred: Sourcebot reindicizza
periodicamente e puo' anche essere riavviato mantenendo il volume.

## Configurazione locale

Il setup monta:

```text
/lab/progetto_3a_inf -> /repos/alfred:ro
alfred-sourcebot-data -> /data
docs/sourcebot-browser/config.json -> /etc/sourcebot/config.json:ro
```

Il repository e' montato read-only. Sourcebot richiede che il repository locale
abbia `remote.origin.url` configurato; Alfred lo ha gia', quindi puo' essere
indicizzato come connessione Git locale.

I dati applicativi non vengono messi sotto `/lab`, ma in un Docker named volume
chiamato `alfred-sourcebot-data`. In questo ambiente e' importante perche'
Postgres, usato internamente da Sourcebot, richiede ownership POSIX corretta sul
data directory e il bind mount del workspace puo' non comportarsi come un
filesystem Linux locale. Per azzerare completamente Sourcebot:

```sh
docs/sourcebot-browser/stop-sourcebot.sh
docker volume rm alfred-sourcebot-data
```

Per rendere la prova piu' semplice:

- `SOURCEBOT_TELEMETRY_DISABLED=true` disabilita la telemetria.
- `FORCE_ENABLE_ANONYMOUS_ACCESS=true` abilita accesso anonimo locale.
- `AUTH_URL=http://localhost:3000` configura l'URL pubblico locale.
- `AUTH_SECRET=...` imposta un secret stabile per le sessioni locali.

Queste impostazioni sono pensate per laboratorio locale, non per una istanza
pubblica esposta su Internet.

### Perche' usiamo un Docker named volume

Il primo tentativo con `/lab/progetto_3a_inf/docs/sourcebot-browser/.work/data`
montato come bind mount puo' fallire perche' Postgres controlla proprietario e
permessi della directory dati. In alcuni ambienti di laboratorio il workspace
non si comporta come un filesystem Linux locale completo per questi controlli.

Per evitare il problema, `/data` viene montato su un named volume Docker:

```text
alfred-sourcebot-data
```

Questo volume e' gestito da Docker e fornisce a Postgres una directory con
ownership coerente.

### Come Sourcebot vede Alfred

Nel container il repository viene montato cosi':

```text
/repos/alfred
```

La connessione in `config.json` usa:

```json
{
  "type": "git",
  "url": "file:///repos/alfred"
}
```

Sourcebot richiede che una directory locale sia una repository Git valida e che
abbia `remote.origin.url`. Questo serve per attribuire al repository un'identita'
Git leggibile nella UI. Alfred soddisfa gia' questo requisito.

Il mount e' read-only (`:ro`): Sourcebot puo' leggere e indicizzare Alfred, ma
non deve modificare i file del progetto.

## Uso della UI

Apri:

```text
http://127.0.0.1:3000
```

La UI puo' mostrare una pagina iniziale o una home con ricerca e repository.
Per l'uso didattico il percorso consigliato e':

1. apri il repository Alfred
2. usa il file explorer per entrare in `app/`, `modules/inotify/`, `core/` o
   `tests/`
3. cerca una funzione o una struttura dati nella barra di ricerca
4. apri il file sorgente
5. confronta il codice con la documentazione italiana in `docs/it/`

Sourcebot e' utile soprattutto per passare rapidamente da una parola chiave a
tutti i punti del codice che la usano. Per esempio, cercare
`ALFRED_EVENT_RELOCATED` permette di vedere dove compare l'evento semantico,
mentre cercare `watch_descriptor` aiuta a seguire la gestione dei watch inotify.

## Query utili

Esempi da provare nella barra di ricerca Sourcebot:

```text
app_run
lang:c app_run
repo:alfred app_run
sym:app_run
path:app/src app_run
recursive_create_nested_dir
ALFRED_EVENT_RELOCATED
```

Significato pratico:

- `app_run`: cerca il testo `app_run` ovunque.
- `lang:c app_run`: limita la ricerca ai file riconosciuti come C.
- `repo:alfred app_run`: limita la ricerca al repository Alfred.
- `sym:app_run`: cerca definizioni di simbolo riconosciute da ctags.
- `path:app/src app_run`: cerca solo nei path che contengono `app/src`.
- `recursive_create_nested_dir`: trova il caso del bug `mkdir -p` e la
  documentazione/test collegati.
- `ALFRED_EVENT_RELOCATED`: segue la semantica core di move+rename unificato.

Altre ricerche utili durante lo studio del progetto:

```text
inotify_backend_poll
watch_manager_add_recursive
alfred_engine_process_raw
alfred_move_insert
FILE_READY
ENABLE_LEGACY_SHADOW
event_engine_mode
```

Se una query produce troppi risultati, restringila con `path:` o `lang:c`.
Per esempio:

```text
path:core/src FILE_READY
path:modules/inotify raw
path:tests/core DIR_RELOCATED
```

Per gli studenti, Sourcebot va presentato come strumento di lettura e ricerca:
prima si cerca un simbolo o una parola chiave, poi si apre il file, poi si segue
la documentazione italiana in `docs/it/` per capire responsabilita' e flusso.

## Troubleshooting

### La pagina non risponde

Controlla il container:

```sh
docs/sourcebot-browser/status-sourcebot.sh
docker logs --tail 120 alfred-sourcebot
```

Se il container non esiste o e' uscito, riavvia:

```sh
docs/sourcebot-browser/start-sourcebot.sh
```

### Errore Docker permission denied

Il comando deve poter parlare con il Docker daemon. Nell'ambiente Codex serve
eseguire lo script con permesso Docker; su una macchina locale l'utente deve
essere nel gruppo Docker oppure deve usare la configurazione prevista dal
sistema.

### Postgres segnala problemi di ownership

Questo problema era possibile usando un bind mount sotto `/lab`. Il setup
attuale usa `alfred-sourcebot-data`, quindi non dovrebbe comparire. Se compare
dopo modifiche locali, fai reset completo:

```sh
docs/sourcebot-browser/stop-sourcebot.sh
docker volume rm alfred-sourcebot-data
docs/sourcebot-browser/start-sourcebot.sh
```

### La UI si apre ma non trovo risultati

Possibili cause:

- Sourcebot sta ancora indicizzando.
- Il repository non e' stato riconosciuto come Git repository locale.
- `remote.origin.url` manca o e' stato rimosso.
- La query e' troppo restrittiva.

Controlla:

```sh
git config --get remote.origin.url
docker logs -f alfred-sourcebot
```

### Voglio cambiare porta

Avvia con:

```sh
SOURCEBOT_PORT=3001 docs/sourcebot-browser/start-sourcebot.sh
```

Poi apri:

```text
http://127.0.0.1:3001
```

## Confronto rapido

| Strumento | Punto forte | Limite |
| --- | --- | --- |
| Elixir | Browser leggero e specifico per codice C/Linux | UI meno moderna |
| Kythe | Modello semantico e cross-reference profonde | GUI non pronta nella release binaria |
| Sourcebot | UI moderna, ricerca indicizzata, file explorer | Code navigation completa Enterprise |

Per ora la scelta piu' ragionevole e' tenerli separati: Elixir come browser
didattico leggero, Kythe come esperimento semantico avanzato, Sourcebot come
alternativa moderna da valutare sul campo.
