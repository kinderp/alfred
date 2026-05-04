# alfred

Il progetto prevede la realizzazione di un software per il monitoraggio di file e cartelle a partire dalla directory radice del progetto.
Deve essere possibile passare, da riga di comando, all'avvio del programma la lista di directory da monitorare. Per ogni cartella in
questa lista deve essere aggiunto un watch per una serie di eventi da monitorare (vedi sotto per i dettagli sugli eventi).
Tutti gli eventi monitorati devono essere stampati su schermo e scritti su un file "report.txt".

## Eventi da monitorare
  
1. **IN_CREATE** ( creazione di un file )
2. **IN_DELETE** ( rimozione di un file )
3. **IN_MOVED_FROM - IN_MOVED_TO** ( rinominazione o spostamento di un file)
4. **IN_ISDIR**  ( creazione di una nuova directory )
     
  __Nota__: Nel caso 4 ( IN_ISDIR ) deve essere aggiunto un watch per la nuova directory
