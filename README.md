# alfred

Il progetto prevede la realizzazione di un software per il monitoraggio di file e cartelle a partire dalla directory radice del progetto.
Deve essere possibile passare, da riga di comando all'avvio del programma, la lista di directory da monitorare. Per ogni cartella in
questa lista deve essere aggiunto un watch per una serie di eventi da monitorare (vedi sotto per i dettagli sugli eventi).
Tutti gli eventi monitorati devono essere stampati su schermo e scritti su un file "report.txt". Sotto un esempio di come strutturare il file di report.

```
[2026-05-03 14:21:15] IN_DELETE  (512) - /lab/progetto_3a_inf/uno/uno.txt
[2026-05-03 14:21:17] IN_CREATE  (256) - /lab/progetto_3a_inf/uno/uno.txt
[2026-05-03 14:21:25] IN_MOVED_FROM  (64) - /lab/progetto_3a_inf/uno/uno.txt
[2026-05-03 14:21:25] IN_MOVED_TO  (128) - /lab/progetto_3a_inf/due/uno.txt
[2026-05-03 14:25:38] IN_CREATE  (1073742080) - /lab/progetto_3a_inf/uno/a
[2026-05-03 17:37:36] IN_CREATE IN_ISDIR  (1073742080) - /lab/progetto_3a_inf/uno/b
[2026-05-03 17:48:32] IN_CREATE IN_ISDIR  (1073742080) - /lab/progetto_3a_inf/uno/c
[2026-05-03 17:49:42] IN_CREATE IN_ISDIR  (1073742080) - /lab/progetto_3a_inf/uno/d
```

## Eventi da monitorare
  
1. **IN_CREATE** ( creazione di un file )
2. **IN_DELETE** ( rimozione di un file )
3. **IN_MOVED_FROM - IN_MOVED_TO** ( rinominazione o spostamento di un file)
4. **IN_ISDIR | IN_CREATE**  ( creazione di una nuova directory )
5. **IN_ISDIR | IN_DELETE**  ( rimozione di una nuova directory )
     
  __Nota__: Nel caso 4 ( IN_ISDIR ) deve essere aggiunto un nuovo watch per la nuova directory da monitorare
