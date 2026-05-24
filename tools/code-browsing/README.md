# Browser del codice

Questa cartella contiene gli script aggregati per preparare, avviare, fermare e
controllare i browser del codice usati nel progetto Alfred.

Gli script specifici restano nelle rispettive cartelle:

- `docs/sourcebot-browser/`
- `docs/code-browser/`
- `docs/kythe-browser/`

Gli script in questa cartella non sostituiscono quelli specifici: li chiamano
in sequenza per rendere piu' semplice il flusso di lavoro di studenti e
contributori.

Graphify non e' incluso negli script aggregati perche' non e' ancora stato
integrato nel progetto. Prima serve uno spike separato per scegliere immagine,
volumi, formato dell'indice e valore reale rispetto a Kythe, Sourcebot ed
Elixir. Finche' quello spike non esiste, aggiungerlo a `start-all.sh` darebbe
agli studenti un comando apparentemente ufficiale ma non ancora verificato.

## Prerequisito

Serve Docker funzionante sull'host. Il progetto non installa Docker
automaticamente, perche' l'installazione del daemon dipende dalla distribuzione,
dai permessi dell'utente e dalle policy della macchina.

Per controllare l'ambiente:

```sh
tools/code-browsing/check-docker.sh
```

## Setup completo

```sh
tools/code-browsing/setup-all.sh
```

Lo script:

1. controlla Docker
2. scarica l'immagine Sourcebot
3. inizializza Elixir e genera il suo database
4. inizializza Kythe e genera le serving tables

E' il comando piu' costoso: puo' scaricare immagini, clonare repository esterni,
costruire immagini locali e compilare Alfred per produrre l'indice Kythe.

## Avvio, stop, restart e stato

```sh
tools/code-browsing/start-all.sh
tools/code-browsing/status-all.sh
tools/code-browsing/restart-all.sh
tools/code-browsing/stop-all.sh
```

URL predefiniti:

```text
Sourcebot: http://127.0.0.1:3000
Elixir:    http://127.0.0.1:8080/alfred/workspace/source
Kythe API: http://127.0.0.1:9898
```

## Variabili utili

Gli script aggregati rispettano le stesse variabili degli script specifici:

```sh
SOURCEBOT_PORT=3001 tools/code-browsing/start-all.sh
ALFRED_ELIXIR_PORT=8081 tools/code-browsing/start-all.sh
KYTHE_PORT=9899 tools/code-browsing/start-all.sh
```

Per cambiare nome container o immagine, usare le variabili documentate nelle
guide specifiche.

## Docker Compose

Il file `docker-compose.yml` descrive i tre servizi server. E' pensato come
riferimento e come alternativa per chi preferisce Compose, ma gli script
rimangono il percorso consigliato perche' sanno verificare prerequisiti e
spiegare meglio gli errori.

Compose e' opzionale. Per usarlo serve il plugin `docker compose` oppure il
binario storico `docker-compose`. Se `check-docker.sh` stampa `Compose: non
disponibile`, usare gli script `start-all.sh`, `stop-all.sh` e `status-all.sh`.

Prima di usare Compose bisogna comunque eseguire il setup degli indici:

```sh
tools/code-browsing/setup-all.sh
```

Poi:

```sh
PROJECT_ROOT="$(pwd)" docker compose -f tools/code-browsing/docker-compose.yml up -d
PROJECT_ROOT="$(pwd)" docker compose -f tools/code-browsing/docker-compose.yml down
```

Compose non rigenera gli indici. Per aggiornare Elixir o Kythe usare:

```sh
docs/code-browser/reindex-elixir.sh
docs/kythe-browser/reindex-kythe.sh
```
