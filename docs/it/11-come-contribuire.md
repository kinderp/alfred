# Come contribuire

Questo capitolo contiene le regole base per contribuire al progetto.

Stato: parziale.

## Prima di modificare codice

1. Leggi la documentazione del componente.
2. Compila con `make`.
3. Controlla lo stato dei file con `git status`.
4. Fai modifiche piccole e verificabili.

Se non conosci ancora il flusso del codice, leggi anche:

```text
docs/it/16-mappa-codice-e-strutture.md
```

Quel capitolo spiega la codebase come lettura guidata: mostra quali funzioni si
chiamano, quali strutture dati vengono mutate e quali eventi fanno partire ogni
passaggio.

## Prima di proporre una modifica

1. Esegui `make`.
2. Esegui `make test` se hai modificato comportamento.
3. Aggiorna la documentazione se hai cambiato architettura o API.
4. Aggiungi commenti solo dove chiariscono responsabilita' o invarianti.

Se modifichi una struttura dati, una funzione centrale o il percorso degli
eventi, aggiorna anche le tabelle e i diagrammi della mappa del codice. La
documentazione deve restare utile a chi legge il progetto per la prima volta.

## Stile commenti

I commenti nel codice devono seguire:

```text
docs/commenting-style.md
```
