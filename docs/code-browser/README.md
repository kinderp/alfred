# Code browser Elixir per Alfred

Questa cartella contiene gli script per usare
[Bootlin Elixir](https://github.com/bootlin/elixir) come browser locale del
codice di Alfred.

Elixir e' un cross-referencer per codice C/C++: mostra i file sorgenti in una
interfaccia web, rende cliccabili gli identificatori e permette di cercare
definizioni e riferimenti. Per gli studenti e' utile per leggere la codebase in
modo esplorativo: si parte da una funzione, si seguono le chiamate e si torna ai
tipi dati collegati.

## Cosa viene creato

Gli script creano tutto sotto:

```text
docs/code-browser/.work/
```

Questa directory e' ignorata da Git. Contiene:

- il clone locale di Bootlin Elixir
- l'immagine Docker costruita localmente
- il bare repository usato da Elixir per Alfred
- il database di cross-reference generato da Elixir

Il repository di lavoro di Alfred non viene modificato. L'unica etichetta usata
da Elixir, `workspace`, viene creata nel bare repository dentro `.work`, non nel
repository principale.

## Prerequisiti

Serve Docker funzionante sulla macchina.

La prima esecuzione scarica il repository di Bootlin Elixir e costruisce
l'immagine Docker. Puo' richiedere alcuni minuti perche' l'immagine compila
anche Universal Ctags, che Elixir usa per trovare le definizioni C.

## Prima inizializzazione

Dalla root del progetto:

```sh
docs/code-browser/setup-elixir.sh
```

Lo script:

1. clona `https://github.com/bootlin/elixir`
2. costruisce l'immagine Docker `alfred-elixir:local`
3. crea un bare repository per Alfred in `.work/elixir-data/alfred/repo`
4. importa i commit del repository corrente
5. crea o aggiorna il tag `workspace` sul commit `HEAD`
6. genera il database Elixir per il progetto

Per usare piu' CPU durante l'indicizzazione:

```sh
ELIXIR_THREADS=4 docs/code-browser/setup-elixir.sh
```

## Avviare il browser

```sh
docs/code-browser/start-elixir.sh
```

Poi apri:

```text
http://127.0.0.1:8080/alfred/workspace/source
```

Verifica rapida da terminale:

```sh
curl -s -o /tmp/alfred-elixir-source.html -w '%{http_code}\n' \
  http://127.0.0.1:8080/alfred/workspace/source
```

Deve stampare `200`.

Per controllare anche la ricerca di un identificatore C:

```sh
curl -s \
  'http://127.0.0.1:8080/api/ident/alfred/inotify_backend_poll?version=workspace&family=C'
```

La risposta deve contenere la definizione in
`modules/inotify/src/inotify_backend.c` e almeno un riferimento da `app/src/app.c`.

Per usare una porta diversa:

```sh
ALFRED_ELIXIR_PORT=8081 docs/code-browser/start-elixir.sh
```

## Aggiornare l'indice

Elixir indicizza commit Git, non lo stato non committato della working
directory. Dopo aver fatto nuovi commit:

```sh
docs/code-browser/reindex-elixir.sh
```

Questo aggiorna il bare repository interno, sposta il tag `workspace` sul nuovo
`HEAD` e rigenera il database.

Se hai modifiche non committate e vuoi vederle in Elixir, prima committale su
un branch di lavoro. Elixir e' pensato per navigare versioni Git riproducibili,
non file temporanei non tracciati.

## Fermare il browser

```sh
docs/code-browser/stop-elixir.sh
```

## Restart e stato

```sh
docs/code-browser/restart-elixir.sh
docs/code-browser/status-elixir.sh
```

`restart-elixir.sh` ferma e riavvia il container senza rigenerare il database.
Serve quando il processo web e' stato fermato o quando si vuole cambiare porta
tramite `ALFRED_ELIXIR_PORT`.

`status-elixir.sh` mostra lo stato Docker del container e prova una richiesta
HTTP alla pagina principale del browser. Se il database non e' aggiornato, lo
stato puo' essere corretto ma il codice mostrato puo' riferirsi all'ultimo
commit indicizzato: in quel caso usare `reindex-elixir.sh`.

## Come usarlo per studiare Alfred

Percorso consigliato:

1. apri `app/src/main.c`
2. segui la chiamata verso `app_init()`
3. passa a `app_run()`
4. entra in `inotify_backend_poll()`
5. segui la consegna del raw event verso `handle_backend_event()`
6. entra in `alfred_process()`
7. torna alle strutture dati dichiarate negli header del core

Quando trovi una funzione:

- clicca sul nome per cercare definizioni e riferimenti
- confronta la definizione con il commento nel codice
- torna alla guida italiana in `docs/it/16-mappa-codice-e-strutture.md` per
  capire il ruolo architetturale della funzione

Elixir e la documentazione italiana hanno scopi diversi:

- Elixir aiuta a navigare il codice reale
- la documentazione spiega perche' il codice e' organizzato in quel modo

## URL utili

- sorgenti Alfred: `http://127.0.0.1:8080/alfred/workspace/source`
- ricerca identificatore: usa la casella di ricerca in alto nella pagina
- esempio di funzione da cercare: `inotify_backend_poll`
- esempio di struttura da cercare: `inotify_backend_context_t`

## Problemi comuni

### Docker non e' disponibile

Verifica:

```sh
docker --version
```

Se Docker richiede permessi, esegui gli script con un utente abilitato a usare
Docker.

### La pagina non si apre

Controlla che il container sia in esecuzione:

```sh
docker ps
```

Se la porta `8080` e' occupata:

```sh
ALFRED_ELIXIR_PORT=8081 docs/code-browser/start-elixir.sh
```

### Non vedo l'ultima modifica

Elixir mostra il tag `workspace` generato dall'ultimo comando di setup o
reindex. Dopo un nuovo commit esegui:

```sh
docs/code-browser/reindex-elixir.sh
```
