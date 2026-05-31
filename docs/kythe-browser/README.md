# Kythe per Alfred

Questa cartella contiene una configurazione sperimentale per usare
[Kythe](https://www.kythe.io/) come sistema di indicizzazione semantica e
cross-reference per Alfred.

Kythe e' piu' complesso di Elixir: non indicizza semplicemente il repository.
Deve prima osservare le compilazioni C, produrre unita' `.kzip`, indicizzarle,
scrivere un graphstore, convertire il graphstore in serving tables e infine
servirle con una UI web.

## Stato

Questa configurazione e' parallela a `docs/code-browser/`, che contiene Elixir.
Non rimuove Elixir e non modifica il Makefile del progetto.

Nota importante: Kythe indicizza correttamente il C tramite `cxx_extractor` e
`cxx_indexer`, ma la release binaria usata qui non include la directory
`web/ui` della sample UI. Quindi questa configurazione fornisce un server
HTTP/API Kythe e gli strumenti CLI di interrogazione, non ancora una interfaccia
grafica completa paragonabile a Elixir. La documentazione ufficiale descrive
Kythe come un ecosistema con indexer C++/Java, un server di cross-reference e
codice di esempio per UI; la UI va recuperata o costruita separatamente se si
vuole trasformare questo setup in un browser visuale completo.

## A cosa serve praticamente

Senza una GUI pronta, Kythe non e' lo strumento migliore per far leggere il
codice agli studenti. Per quello sono piu' immediati Elixir e Sourcebot.
Kythe diventa interessante quando non vogliamo solo "cercare testo", ma vogliamo
interrogare relazioni strutturate prodotte dall'analisi del codice.

Esempi pratici:

- trovare quali file e quali simboli sono noti all'indice
- distinguere, quando l'indice lo consente, tra una parola trovata come testo e
  un simbolo riconosciuto
- chiedere riferimenti e definizioni tramite API o CLI invece di scorrere molti
  file manualmente
- costruire in futuro grafi reali `funzione -> chiamanti -> chiamati`
- verificare automaticamente se una pagina di documentazione che dice
  "`app_run()` chiama X e Y" e' ancora coerente con il codice
- generare materiale didattico a partire dal codice, per esempio mappe di
  funzioni, file, strutture dati e relazioni fra moduli

Quindi Kythe non va visto come sostituto immediato di un browser grafico. Va
visto come una base dati semantica su cui, eventualmente, costruire strumenti
nostri.

## Perche' puo' servire anche a un agente

Un agente che lavora sul codice, come Codex, consuma contesto quando apre file,
legge porzioni di sorgente e cerca stringhe. Su una codebase grande questo puo'
diventare costoso: molte ricerche testuali producono risultati rumorosi e spesso
bisogna aprire piu' file prima di capire quali punti sono davvero rilevanti.

Kythe puo' aiutare in modo diverso:

- una query su simboli o file puo' restringere prima il campo di lettura
- le cross-reference possono indicare punti di definizione e uso senza dover
  fare molte ricerche `rg`
- un agente puo' chiedere "dove compare questo simbolo nel grafo?" e poi aprire
  solo i file necessari
- le API possono essere usate da script che producono riassunti, mappe o
  controlli automatici della documentazione

Questo non elimina la lettura del codice: per modificare il progetto bisogna
comunque controllare i file reali, le firme, i contratti e i test. Pero' puo'
ridurre il lavoro esplorativo iniziale e quindi anche il numero di token usati
per leggere materiale non rilevante.

Nel progetto Alfred questo vantaggio diventera' piu' utile se in futuro
costruiremo uno script o una piccola UI che interroga Kythe e produce viste
mirate, per esempio:

- "mostrami la definizione e gli usi di `alfred_engine_process_raw`"
- "mostrami tutte le funzioni che toccano la tabella dei watch"
- "genera una mappa dei file coinvolti nel percorso `inotify -> raw -> core`"
- "controlla se la lettura guidata cita funzioni che non esistono piu'"

Fino a quel momento Kythe resta utile, ma secondario rispetto a Sourcebot ed
Elixir per la navigazione quotidiana.

## Prerequisiti

Servono:

- `curl`
- `python3`
- `tar`
- Docker

Lo script scarica una release binaria di Kythe da GitHub. La fase di
estrazione e indicizzazione gira dentro un container Ubuntu 24.04, per evitare
problemi di compatibilita' tra i binari Kythe e la glibc del sistema host.
La versione predefinita e' `v0.0.68`, scelta per avere una UI `http_server`
compatibile con l'host usato in laboratorio. Versioni piu' recenti possono
richiedere una glibc piu' nuova anche per alcuni strumenti eseguiti fuori dal
container.

## Setup

Dalla root del progetto:

```sh
docs/kythe-browser/setup-kythe.sh
```

Per forzare una versione specifica:

```sh
KYTHE_VERSION=v0.0.68 docs/kythe-browser/setup-kythe.sh
```

## Indicizzazione

```sh
docs/kythe-browser/reindex-kythe.sh
```

Lo script:

1. carica la release Kythe locale
2. costruisce l'immagine Docker locale `alfred-kythe:local`, se serve
3. pulisce gli artefatti Kythe precedenti
4. compila Alfred nel container con
   `CC=docs/kythe-browser/kythe-gcc-wrapper.sh`
5. produce file `.kzip` per le compilazioni C
6. esegue `cxx_indexer`
7. scrive il graphstore
8. genera le serving tables

Il graphstore LevelDB e le serving tables intermedie vengono scritti in `/tmp`
dentro il container. Questa scelta evita problemi con alcuni filesystem
condivisi usati da Docker, che possono non supportare bene le primitive
richieste da LevelDB. Alla fine le serving tables vengono copiate in
`docs/kythe-browser/.work/serving/`, dove la UI locale puo' leggerle.

## Avvio server Kythe

```sh
docs/kythe-browser/start-kythe.sh
```

URL predefinito:

```text
http://127.0.0.1:9898
```

Per usare un'altra porta:

```sh
KYTHE_PORT=9899 docs/kythe-browser/start-kythe.sh
```

Anche il server HTTP viene avviato in Docker. Il motivo e' lo stesso della fase
di generazione: le tabelle Kythe sono LevelDB e in questo ambiente non vengono
aperte correttamente dal filesystem condiviso del progetto. Lo script copia le
serving tables in `/tmp` dentro il container e pubblica il servizio su
`127.0.0.1`.
I log si leggono con:

```sh
docker logs alfred-kythe-http-9898
```

## Stop

```sh
docs/kythe-browser/stop-kythe.sh
```

## Restart e stato

```sh
docs/kythe-browser/restart-kythe.sh
docs/kythe-browser/status-kythe.sh
```

`restart-kythe.sh` ferma e riavvia solo il server HTTP Kythe. Non rigenera le
serving tables: se il codice e' cambiato e si vuole aggiornare l'indice, usare
prima `reindex-kythe.sh`.

`status-kythe.sh` mostra il container Docker atteso, l'eventuale id registrato
in `.work/http_server.container` e una risposta HTTP locale. Un `404` sulla
root del server non indica necessariamente un problema: Kythe espone soprattutto
API e viste specifiche, non una homepage applicativa completa.

## Verifiche rapide

Dopo l'avvio:

```sh
curl -s -o /tmp/kythe-alfred.html -w '%{http_code}\n' \
  http://127.0.0.1:9898
```

Deve stampare un codice HTTP raggiungibile. Senza sample UI puo' essere `404`
sulla root, ma la connessione deve riuscire.

Per interrogare Kythe via CLI:

```sh
source docs/kythe-browser/.work/env.sh
kythe --api=http://127.0.0.1:9898 ls kythe://alfred
kythe --api=http://127.0.0.1:9898 ls 'kythe://alfred?path=app/src'
kythe --api=http://127.0.0.1:9898 identifier app_run
```

Per ispezionare gli artefatti:

```sh
find docs/kythe-browser/.work/kzip -type f
find docs/kythe-browser/.work/serving -type f | head
```

## Limiti rispetto a Elixir

- Kythe richiede una build strumentata.
- In questa configurazione manca ancora una UI grafica completa.
- Il valore sta soprattutto nelle cross-reference semantiche.
- La qualita' dell'indice dipende da quanto il `cxx_extractor` riesce a
  interpretare le compilazioni C del progetto.

## File generati

Tutto cio' che pesa o viene rigenerato vive sotto:

```text
docs/kythe-browser/.work/
```

Questa directory e' ignorata da Git.
