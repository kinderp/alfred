# Regole operative per continuare l'integrazione

Questo file raccoglie le regole pratiche emerse durante l'integrazione del core.
Serve come memoria di lavoro: se la sessione viene interrotta o ripresa in un
altro momento, leggere questo file prima di continuare.

## Metodo di lavoro

- Procedere per passi piccoli e verificabili.
- Discutere prima strategia, semantica e trade-off; modificare il codice solo
  dopo l'accordo esplicito.
- Non anticipare refactor grandi se il passo corrente non e' stato approvato.
- Dopo ogni passo, fermarsi e chiarire il prossimo punto prima di proseguire.
- Se una decisione e' delicata, documentare anche il ragionamento, non solo il
  risultato.
- Se una spiegazione data in chat riguarda una scelta reale del codice, riportare
  quella spiegazione anche negli `.md` o nei commenti del codice.
- Se una domanda fatta in chat mostra che architettura, codice o concetto non
  sono chiari, prima del commit controllare la documentazione collegata. Se la
  spiegazione manca o e' troppo debole, aggiornarla con una versione didattica
  della risposta; se invece e' gia' chiara, indicare il file e la sezione cosi'
  il maintainer puo' decidere se evitare ulteriore peso documentale.
- Quando utile, citare il commit che introduce o spiega una scelta, cosi' gli
  studenti possono risalire alla modifica concreta.
- Una modifica non banale deve lasciare una traccia di orientamento: aggiornare
  almeno uno tra documentazione architetturale, contratto API, scenario/test,
  diagramma, ADR o checklist di review. Se non aggiorna nulla, chiedersi se la
  modifica sta aggiungendo complessita' non tracciata.
- Quando cambia una regola operativa che guida il comportamento degli agenti AI
  o il flusso di lavoro ricorrente, controllare anche se esistono skill Codex
  collegate da aggiornare. Per esempio, modifiche a commit, PR, review,
  finding, issue, milestone, label o tracciabilita' GitHub devono riallineare
  anche la skill personale `alfred-pr-commit-rules`, cosi' la procedura eseguita
  dagli agenti resta coerente con la documentazione del repository.

## Tracciamento GitHub di roadmap e decisioni

Quando un lavoro supera il singolo micro-step, usare GitHub come struttura di
coordinamento pubblica:

```text
Milestone
    |
    v
Issue madre
    |
    +--> Project
    +--> Discussions
    +--> Issue figlie
    +--> Pull request
```

- `Milestone`: macro-obiettivo misurabile, per esempio
  [`Writer Runtime v0`](https://github.com/kinderp/alfred/milestone/1).
- `Project`: vista operativa di issue, PR e roadmap, per esempio
  [`Repository Projects`](https://github.com/kinderp/alfred/projects), dove il
  Project v2 [`Alfred Roadmap`](https://github.com/users/kinderp/projects/1) e'
  collegato al repository.
- `Issue madre`: piano tecnico dettagliato della milestone, per esempio
  [`Writer Runtime v0: implementation plan`](https://github.com/kinderp/alfred/issues/32).
- `Issue figlia`: micro-step, bug, test o task specifico collegato alla issue
  madre.
- `Discussion`: ragionamento progettuale aperto, con domande, alternative,
  risposte e trade-off.
- `Pull request`: modifica concreta e verificabile.
- `docs/it`: decisione consolidata, contratto stabile e spiegazione didattica.
- `Registro milestone`: vista cronologica versionata delle milestone, con
  durata stimata/reale, dipendenze, priorita' e collegamenti a GitHub.

Ogni milestone non banale deve essere registrata anche nel
[Registro milestone del progetto](37-roadmap-milestone-progetto.md). La riga
deve indicare almeno stato, finestra prevista, data di fine orientativa, durata
stimata, dipendenze, motivazione della priorita', cosa sblocca e link a
milestone GitHub, issue madre e documenti collegati. Alla chiusura bisogna
aggiornare durata reale, sintesi dell'esito e lavoro rimandato.

Nel registro cronologico la colonna `Milestone` deve contenere il link
cliccabile al documento MD locale della milestone o della roadmap primaria,
quando esiste. La colonna `GitHub` resta il punto per GitHub Milestone, issue
madre, PR, discussion e altri link di tracciabilita'. Se una milestone non ha
ancora un documento dedicato o una GitHub Milestone, indicarlo chiaramente nelle
colonne corrispondenti.

Le milestone nuove devono partire con una data di fine orientativa esplicita.
La data va inserita nella GitHub Milestone; se per un limite operativo non e'
possibile farlo subito, deve comparire almeno nella issue madre e nel registro.
Non e' una promessa rigida: serve per stimare la velocita' del progetto e
capire quando una milestone sta crescendo troppo. Se lo scope cambia o una
review scopre problemi importanti, aggiornare la data e scrivere il motivo
nella issue madre o nel registro.

Regola di separazione:

```text
GitHub Discussions conserva il processo di pensiero.
La documentazione del repository conserva la decisione finale.
```

Quindi, quando in chat emerge una discussione architetturale importante, va
riassunta o riportata in una GitHub Discussion se non e' ancora una decisione
stabile. Quando invece scegliamo una strada, la scelta deve essere tradotta in
documentazione stabile negli `.md` appropriati. Le Discussions non sostituiscono
la documentazione: aiutano a capire perche' una decisione e' maturata.

Per `Writer Runtime v0` le prime Discussions di riferimento sono:

- [`common queue vs per-sink queues`](https://github.com/kinderp/alfred/discussions/33)
- [`fail-closed, best-effort sinks and backpressure`](https://github.com/kinderp/alfred/discussions/34)
- [`when to introduce a real worker thread`](https://github.com/kinderp/alfred/discussions/35)

### Label GitHub

Quando si crea o aggiorna una issue o una PR, applicare label utili alla
ricerca e alla roadmap. Evitare troppe label decorative: devono dire area,
tipo, priorita' o stato operativo.

Famiglie correnti:

- `area:*`: parte del progetto, per esempio `area:core`, `area:backend`,
  `area:writer`, `area:docs`, `area:tests`, `area:security`,
  `area:performance`, `area:ci`.
- `kind:*`: tipo di lavoro, per esempio `kind:bug`, `kind:design`,
  `kind:debt`, `kind:roadmap`, `kind:test`, `kind:audit`.
- `priority:*`: urgenza, cioe' `priority:p0`, `priority:p1`, `priority:p2`.
- `status:*`: stato operativo, per esempio `status:needs-discussion`,
  `status:ready`, `status:blocked`, `status:needs-docs`.

Regola pratica:

```text
almeno una area + almeno un kind
priority solo se aiuta davvero a ordinare il lavoro
status solo se segnala una condizione operativa reale
```

Per gli audit esplorativi notturni usare sempre `area:tests` + `kind:audit`.
Se l'audit apre una issue figlia per un bug confermato, aggiungere anche
`kind:bug`. Se invece l'audit produce solo lavoro di copertura o promozione di
uno scenario a test stabile, aggiungere `kind:test` o `kind:debt` secondo il
caso.

### Issue madri e rimandi alla documentazione

Quando si crea o aggiorna una issue madre, la sezione di contesto documentale
non deve essere una lista secca di path. Deve aiutare un lettore a capire la
issue anche se non legge subito tutti gli MD.

Regola:

- ogni issue madre deve avere subito dopo il goal un blocco `Primary roadmap`
  con link GitHub cliccabile al documento roadmap MD corrispondente;
- il blocco `Primary roadmap` deve spiegare in una frase perche' quel documento
  e' il riferimento operativo principale della milestone;
- usare link GitHub cliccabili ai documenti;
- quando utile, linkare anche i paragrafi specifici;
- aggiungere un riassunto breve di cosa dice ogni documento;
- distinguere chiaramente fra issue come piano operativo e documentazione come
  fonte stabile del contratto;
- mantenere nella issue madre una tabella `Implementation traceability` che
  colleghi ogni elemento della checklist a commit, PR o issue figlie rilevanti;
- aggiornare quella tabella a ogni progress update significativo, indicando
  cosa e' concluso, cosa e' in corso e quali commit sono solo preparatori;
- per ogni micro-step non banale di una issue madre, creare una issue figlia
  dedicata e una PR dedicata. La issue figlia deve linkare la issue madre e la
  PR; la PR deve linkare la issue figlia con `Closes #...` quando il merge deve
  chiuderla; la issue madre deve linkare la issue figlia e la PR nella
  tracciabilita' o in un commento di update;
- quando GitHub espone la funzione nativa `Create sub-issue -> add existing
  issue`, aggiungere anche la issue figlia come sub-issue nativa della issue
  madre. Se l'automazione disponibile non espone questa funzione, lasciare
  almeno un link bidirezionale esplicito e annotare che il collegamento nativo
  andra' completato dalla UI;
- se una Discussion contiene il ragionamento, linkarla dalla issue madre e poi
  trasferire la decisione finale negli MD.

Esempio di forma corretta:

```text
Primary roadmap:
[Writer Runtime Roadmap v0](https://github.com/kinderp/alfred/blob/main/docs/it/33-writer-runtime-roadmap-v0.md)
e' il riferimento operativo della milestone: descrive pipeline corrente,
queue/drain boundary, coda bounded, micro-step e criteri di completamento.

[Writer API v0](https://github.com/kinderp/alfred/blob/main/docs/it/32-writer-api-v0.md)
spiega perche' i writer devono restare fuori dal percorso caldo. Le sezioni
piu' rilevanti sono Percorso caldo, Output pipeline sperimentale, Ownership e
Record Queue v0.

Sintesi: il backend produce record, il confine caldo termina alla coda bounded,
i writer stanno a valle e non devono bloccare il collector.

Implementation traceability:
| Checklist item | Status | Commits / PRs | Notes |
| --- | --- | --- | --- |
| Document current synchronous output pipeline | Done | commit link | Mappa pipeline corrente |
| Define hot-path boundary | In progress | commit link, PR link | Codice parziale, worker ancora futuro |
```

## Bootstrap di una nuova sessione agente

Quando una nuova sessione viene aperta con Codex o con un altro agente AI, non
bisogna leggere tutta la documentazione indiscriminatamente. La documentazione
di Alfred e' ormai abbastanza ampia: leggere tutto gonfia il contesto e puo'
mescolare contratti correnti, roadmap futura e note storiche.

Il bootstrap minimo e' invece:

1. leggere `AGENTS.md` nella root del repository;
2. leggere questo file;
3. leggere `27-guida-lettura-documentazione.md`;
4. leggere `documentation-progress.md`;
5. leggere `31-milestone-inotify-reference-backend.md`;
6. scegliere gli altri documenti in base al task corrente.

Per il lavoro corrente su Backend API v0, Event Model v0, adapter, record,
writer, output strutturato e backend inotify leggere almeno:

- `29-event-model-v0.md`
- `30-backend-api-v0.md`
- `32-writer-api-v0.md`
- `05-modulo-inotify.md`
- `07-flusso-eventi.md`
- `20-matrice-eventi-inotify.md`
- `22-contratto-log.md`
- `26-stato-funzionalita.md`

`24-roadmap-ai-agent-guardrail.md` descrive la direzione strategica futura:
Alfred come runtime security layer per agenti AI. Va letta per non perdere la
visione, ma non autorizza a implementare Agent Guard completo, fanotify, eBPF,
Windows, macOS, dashboard o policy engine durante la milestone inotify, salvo
richiesta esplicita.

`38-visione-observation-runtime.md` e
`39-principi-architetturali-futuri.md` descrivono una visione ancora piu'
generale: Alfred come runtime di osservazioni, provenance, memoria, replay e
feedback per sistemi intelligenti. Vanno letti quando si introduce un campo
comune, una nuova API o una astrazione che potrebbe vincolare il futuro modello
di record. Non autorizzano a implementare knowledge graph, world model, LLM
integration, sensori video/GPS/browser o AI generale durante la milestone
corrente.

## Principi di ragionamento dell'agente

Queste regole valgono quando un agente AI aiuta a modificare o documentare il
progetto. Sono ispirate anche alle guideline "Karpathy-inspired" raccolte nel
repository `multica-ai/andrej-karpathy-skills`, ma adattate al nostro flusso di
lavoro e alla documentazione didattica di Alfred.

- Pensare prima di modificare: dichiarare assunzioni, dubbi e trade-off prima di
  toccare codice o documentazione quando il passo non e' banale.
- Non nascondere confusione: se una richiesta puo' essere interpretata in piu'
  modi, fermarsi e proporre le interpretazioni invece di sceglierne una in
  silenzio.
- Semplicita' prima: implementare il minimo necessario per il passo concordato,
  senza astrazioni speculative o opzioni configurabili non richieste.
- Modifiche chirurgiche: toccare solo i file necessari al passo corrente. Se si
  nota codice morto o migliorabile fuori contesto, segnalarlo o metterlo nel
  TODO, ma non cambiarlo senza accordo.
- Ogni riga modificata deve poter essere collegata alla richiesta corrente, a un
  test, a un bug o a un aggiornamento documentale necessario.
- Obiettivi verificabili: per ogni passo tecnico chiarire come si controlla il
  risultato, per esempio con `make`, `make test-core`, un test backend o una
  verifica mirata. Gli scenari shadow sono storici e non sono piu' verifiche
  correnti.
- Prima di fare refactor, distinguere sempre:
  - comportamento osservabile
  - responsabilita' dei moduli
  - pulizia interna
  - compatibilita' storica rimasta solo nella documentazione
- Se una modifica serve solo a ridurre complessita' interna, documentare perche'
  non cambia la semantica osservabile.
- Prima di chiudere un blocco di 5-7 commit o prima di aprire una PR importante,
  fare una review architetturale: controllare responsabilita' cambiate, nuove
  dipendenze, API toccate, rischio sul path caldo, test golden necessari e
  documentazione da riallineare.

Questi principi non devono rallentare correzioni ovvie o modifiche puramente
documentali. Servono soprattutto nei passaggi non banali: refactor, semantica
degli eventi, test, architettura, interfacce pubbliche e strumenti di sviluppo.

## Regola del percorso caldo

Il percorso caldo di Alfred deve restare cortissimo:

```text
evento OS
-> collector/backend
-> normalizzazione minima
-> alfred_record_t
-> enqueue su coda/ring buffer
```

Nel percorso caldo non devono entrare writer, serializzazione costosa, file I/O,
socket I/O, `fprintf()`, `fflush()`, lock pesanti, allocazioni non necessarie,
dashboard, Lab, report o policy pesante.

I bridge sincroni correnti verso `logger_raw()`, `logger_event()` e
`logger_error()` sono accettati solo come passaggi temporanei di migrazione
verso `alfred_record_t` e output compatibile. Non rappresentano
l'architettura finale ad alte prestazioni.

La regola contrattuale e':

```text
Il backend non aspetta il writer.
```

Quando una modifica introduce o tocca output, writer, sink, logger, JSONL,
socket o formati binari, leggere anche `32-writer-api-v0.md` e verificare che
la serializzazione resti fuori dal percorso caldo target.

## Regola del costo architetturale

Ogni astrazione architetturale nuova deve giustificare il proprio costo.
L'obiettivo di Alfred resta:

```text
piccolo, veloce, performante, affidabile, robusto,
semplice, manutenibile, ben documentato e chiaro.
```

Quindi porte, adapter, dispatcher, code, plugin-like API, callback, copie owned,
projection, writer o nuovi livelli intermedi non devono essere introdotti solo
per eleganza architetturale. Devono comprare almeno uno tra:

- chiarezza reale;
- testabilita';
- separazione delle responsabilita';
- estensibilita' necessaria;
- sicurezza;
- affidabilita';
- operabilita';
- prestazioni misurate o migliorabili.

Se una astrazione tocca il percorso caldo, aggiunge copie, allocazioni, lock,
I/O, dispatch indiretto o buffering, bisogna misurarne il costo con benchmark
mirati o spiegare perche' il costo e' fuori dal path caldo. La PR o il commit
devono indicare almeno:

- costo atteso;
- beneficio atteso;
- benchmark eseguito o benchmark da aggiungere;
- scenario baseline usato per il confronto;
- motivo per cui il costo e' accettabile.

Regola guida:

```text
Architecture must pay rent.
```

Una architettura piu' astratta e' accettabile solo se rende Alfred piu' chiaro,
piu' verificabile, piu' sicuro o piu' estendibile senza violare il budget di
performance.

La discussione progettuale di riferimento e':
[Architecture direction: lightweight ports-and-adapters for Alfred](https://github.com/kinderp/alfred/discussions/44).

## Uso di indici semantici e grafi del codice

Alfred puo' essere esplorato con strumenti diversi, che hanno scopi diversi:

- `rg` e lettura diretta dei file: percorso normale per modifiche piccole e
  localizzate.
- Sourcebot/Elixir: browser umani per cercare e leggere il codice.
- Kythe: indice semantico e cross-reference C, utile per interrogazioni su
  simboli, definizioni, riferimenti e possibili mappe automatiche.
- Graphify: possibile knowledge graph piu' ampio, pensato per agenti AI e
  capace di collegare codice, documentazione, diagrammi e decisioni progettuali.

Regola pratica:

```text
Prima restringere il campo con indici/grafi quando il problema e' ampio;
poi aprire e verificare sempre i file sorgente reali prima di modificare.
```

Non usare Kythe o Graphify per ogni task. Per un cambio locale e chiaro, `rg` e
lettura diretta bastano. Usarli invece quando:

- bisogna capire l'impatto di un refactor
- bisogna sapere chi chiama o usa una funzione/struttura dati
- bisogna aggiornare la mappa del codice o la documentazione architetturale
- bisogna controllare se una spiegazione documentale e' ancora coerente con il
  codice
- un agente deve orientarsi nella codebase senza aprire molti file irrilevanti
- si vuole generare o validare una vista `funzione -> chiamanti -> chiamati`

Kythe va preferito per domande tecniche sui simboli C indicizzati, per esempio:

```text
dove e' definito questo simbolo?
quali file vede l'indice?
quali riferimenti conosce il grafo?
```

Graphify, se verra' integrato, andra' preferito per domande piu' larghe, per
esempio:

```text
quali documenti spiegano questa parte del codice?
quali moduli sono collegati a questa decisione architetturale?
quale cluster di file partecipa al flusso inotify -> raw -> core?
```

Limite fondamentale: un indice puo' essere vecchio, incompleto o impreciso.
Prima di usare una risposta di Kythe/Graphify per cambiare codice, controllare
sempre:

- che l'indice sia stato rigenerato dopo modifiche rilevanti
- che i file sorgente reali confermino la relazione trovata
- che i test o gli scenari documentati verifichino il comportamento atteso

Quando una scelta o una mappa viene ricavata da Kythe o Graphify, indicarlo
nella documentazione come supporto all'analisi, non come fonte unica di verita'.

## Commit

I commit devono seguire sempre queste regole:

- messaggio in inglese
- descrizione dettagliata in inglese
- lista finale `Modified files:`
- nessuna riga vuota tra gli elementi della lista dei file
- includere solo i file del passo corrente
- non committare file locali non tracciati, log generati o esperimenti fuori
  task

Quando un commit introduce o modifica un percorso di chiamata rilevante, il body
deve includere anche una breve spiegazione in inglese delle funzioni e
sottofunzioni principali coinvolte. La spiegazione deve chiarire:

- quale funzione e' il punto di ingresso;
- quali helper chiama;
- quale responsabilita' ha ogni helper;
- se cambia ownership, I/O, hot path, API o comportamento osservabile.

Questa regola vale soprattutto per architettura, ownership, pipeline, API,
callback, funzioni ponte, writer, sink, dispatcher, queue e percorso caldo. Non
serve per modifiche banali, typo, link, rename piccoli o commit puramente
editoriali. Se la spiegazione supera un riepilogo breve, documentarla negli MD e
nel commit rimandare al documento aggiornato.

### Commit che risolvono finding di review

Quando una PR riceve finding tecnici, il flusso obbligatorio e':

1. inserire i finding come commenti inline nella PR, non solo come riepilogo
   generale;
2. risolvere ogni finding con un commit monoscopo quando possibile;
3. dopo il fix, aggiungere una risposta al commento inline del finding;
4. nella risposta indicare il commit che risolve il finding come link Markdown
   cliccabile alla pagina GitHub del commit, usando lo SHA breve come testo del
   link, e spiegare in inglese la soluzione applicata;
5. nel messaggio del commit corrispondente indicare che il commit risolve quel
   finding, citando la PR e il link al commento/finding.

Ogni finding risolto deve lasciare anche una spiegazione in inglese nel
commento inline del finding, indipendentemente dal fatto che il maintainer
l'abbia chiesta esplicitamente in chat. La risposta al finding non deve dire
solo "fixed": deve spiegare qual era il rischio, come e' stato risolto e quale
test o contratto impedisce la regressione.

Se in chat e' stata gia' data una spiegazione del finding e della soluzione
scelta, quella spiegazione non deve restare solo nella conversazione: va
tradotta o sintetizzata in inglese. Quando si chiude il finding, riportare
sempre la spiegazione:

- nella risposta al commento inline, insieme al link al commit;
- nel body del commit che risolve il finding.

La versione nel commento e nel commit puo' essere sintetizzata, ma deve
conservare il ragionamento tecnico: qual era il rischio, perche' il codice
precedente era insufficiente, quale comportamento nuovo chiude il problema e
quale test lo blocca.

Questa regola serve a trasformare la review in documentazione storica: chi legge
la PR deve poter partire dal finding, arrivare al commit che lo risolve e capire
perche' la soluzione e' corretta.

Formato consigliato nel body del commit:

```text
Fixes review finding:
- PR: https://github.com/kinderp/alfred/pull/N
- Finding: https://github.com/kinderp/alfred/pull/N#discussion_rID

Explain in English what the finding reported and how this commit fixes it.
```

Formato consigliato per la risposta al commento inline:

```text
Fixed in [d406dec3](https://github.com/kinderp/alfred/commit/d406dec30eae64eaa8ae737981fde6eafb8a4774).

Explain in English what changed and why this closes the finding.
```

### Aggiornamento della descrizione PR dopo review multiple

### Modalita' operative per PR e commit

Le regole di commit, PR, finding e review possono essere applicate in due
modalita' operative. Se il maintainer non specifica la modalita', usare la
modalita' supervisionata. Gli alias brevi sono convenzioni testuali del
progetto: funzionano quando il messaggio arriva all'agente. Se un client
intercetta i comandi slash prima di inviarli, usare il trigger testuale lungo.

Modalita' supervisionata:

- trigger consigliato: `modalita' supervisionata`;
- alias breve: `/apcr super`;
- spiegare prima i passaggi significativi;
- chiedere conferma prima di push, apertura PR, fix di finding o decisioni che
  cambiano contratto, scope, architettura o documentazione rilevante;
- spiegare ogni finding e la soluzione prevista prima di applicarla quando il
  maintainer sta guidando la review;
- fermarsi dopo i passaggi principali se il maintainer sta controllando il
  ciclo.

Modalita' autonoma PR loop:

- trigger consigliato: `modalita' autonoma PR loop`;
- alias breve: `/apcr auto`;
- non chiedere conferma per passaggi meccanici gia' regolati: branch, issue
  figlia, commit, push, apertura o aggiornamento PR, label, milestone, commenti
  GitHub, review, finding inline, fix, risposta ai finding e aggiornamento del
  body PR;
- ripetere review, fix e aggiornamento PR finche' due review consecutive non
  trovano nuovi finding;
- fermarsi quando la PR ha due review consecutive pulite e chiedere al
  maintainer se vuole mergiare;
- fermarsi prima se emergono scelte di prodotto, cambio di scope, cambio di
  contratto non deducibile, fallimento CI non banale, limite di permessi o
  lavoro fuori milestone.

Una PR deve restare in `draft` finche' non ha superato due round di review
consecutivi senza nuovi finding. Solo dopo due review consecutive pulite si puo'
decidere di marcarla pronta o mergiarla. Questa regola evita di considerare
stabile una PR subito dopo il primo giro senza problemi, soprattutto quando le
review precedenti hanno trovato ambiguita' di contratto, bug, rischi di
performance, ownership o copertura test insufficiente.

Quando una PR riceve piu' round di review, la descrizione creata dal template
deve essere aggiornata dopo ogni round significativo. La PR non deve contenere
solo lo stato iniziale del branch: deve diventare anche una traccia storica di
che cosa le review successive hanno trovato e di come i finding sono stati
risolti.

### Dimensioni da controllare durante una review PR

Quando si chiede una review accurata di una PR, il reviewer deve cercare
finding rispetto a queste dimensioni. La frase guida e':

```text
Alfred deve restare piccolo, leggero, veloce, performante, affidabile, sicuro,
facilmente estendibile, interoperabile, robusto, semplice, ben documentato,
manutenibile, facilmente modificabile e chiaro, con API e contratti non
ambigui e adeguatamente coperti.
```

Durante la review controllare almeno:

- correttezza funzionale: il comportamento implementato corrisponde al contratto
  dichiarato e agli scenari attesi;
- chiarezza dei contratti: API, ownership, lifecycle, error model, schema,
  record, writer, backend e test non devono lasciare casi ambigui;
- copertura test: i casi nuovi, rifiutati, limite, regressivi e di ownership
  devono essere coperti dal tipo di test giusto;
- semplicita' e manutenibilita': il codice deve essere comprensibile,
  modificabile e privo di astrazioni speculative;
- performance e leggerezza: il path caldo non deve ricevere I/O, lock,
  allocazioni, serializzazione o dispatch costosi non necessari;
- robustezza e affidabilita': errori, cleanup, retry, overflow, stato parziale,
  idempotenza e shutdown devono avere comportamento definito;
- sicurezza: input, path, limiti, permessi, fail-closed/fail-open, leakage di
  dati e uso futuro in contesti agent/runtime devono essere considerati;
- interoperabilita' ed estendibilita': la modifica non deve chiudere la strada
  a backend, writer, formati, OS o integrazioni future gia' previste;
- osservabilita' e operabilita': log, diagnostica, metriche, errori e CI devono
  permettere di capire cosa succede in produzione o nei test;
- coerenza: naming, semantica, configurazione, documentazione e comportamento
  devono essere allineati fra moduli e con le decisioni precedenti;
- tracciabilita': issue, PR, commit, finding, review round e documentazione
  devono permettere di ricostruire perche' e' stata fatta la modifica.

Formato consigliato da aggiungere alla descrizione della PR:

```text
## Review round N

Summary:
- Brief English summary of what this review focused on.
- Brief English summary of the architectural or correctness risk found.

Findings:
- Finding: https://github.com/kinderp/alfred/pull/N#discussion_rID
  Fix: [short-sha](https://github.com/kinderp/alfred/commit/full-sha) - short explanation of the fix.
- Finding: https://github.com/kinderp/alfred/pull/N#discussion_rID
  Fix: [short-sha](https://github.com/kinderp/alfred/commit/full-sha) - short explanation of the fix.
```

La sezione `Review round N` deve essere scritta in inglese, come il resto della
PR pubblica. Non usare forme come `Review update #1`: in Markdown GitHub
trasforma automaticamente `#1`, `#2` e forme simili in link a issue o pull
request, creando riferimenti ambigui e non intenzionali. Deve indicare:

- il numero del round di review;
- il senso della review, cioe' che tipo di rischio o parte del codice e' stata
  controllata;
- la lista puntata dei finding;
- per ogni finding, il commit che lo risolve come link Markdown cliccabile alla
  pagina GitHub del commit;
- una spiegazione breve ma chiara del perche' il fix chiude il finding.

Questa regola affianca, ma non sostituisce, i commenti inline: il commento
inline resta il punto preciso nel codice, mentre la descrizione della PR offre
una vista ordinata dell'evoluzione della review.

Le regole di stile del messaggio seguono le pratiche raccolte in
`Git Commit Best Practices`:

- fare commit puliti e monoscopo
- committare spesso su feature branch, senza aspettare un cambio enorme
- scrivere messaggi significativi, utili a reviewer e lettori futuri
- usare il modo imperativo e il tempo presente nel subject:
  `change`, non `changed` o `changes`
- usare il body per spiegare che cosa e' cambiato e perche'
- tenere il subject breve, idealmente intorno a 50 caratteri
- spezzare il body intorno a 72 caratteri quando e' pratico
- lasciare una riga vuota tra subject e body
- non chiudere il subject con un punto
- rimuovere punteggiatura non necessaria

Quando il tipo e' utile, usare il formato:

```text
<type>(<optional scope>): <subject>
```

Tipi ammessi:

- `feat`: nuova funzionalita' utente
- `fix`: correzione di bug utente
- `docs`: modifiche alla documentazione
- `style`: formattazione senza cambio di comportamento
- `refactor`: refactor del codice di produzione
- `test`: aggiunta o refactor di test
- `chore`: manutenzione ordinaria senza cambio di comportamento
- `build`: modifiche a build, tooling o dipendenze
- `perf`: miglioramento prestazionale

Nel nostro progetto il prefisso `<type>` e' consigliato ma non obbligatorio
per i commit gia' avviati con lo stile storico. Restano invece obbligatori il
testo in inglese, la descrizione dettagliata e la lista finale dei file senza
righe vuote tra gli elementi.

Esempio di formato:

```text
refactor: move legacy shadow lifecycle

Detailed English explanation of what changed and why.

Additional technical context if useful.

Modified files:
- path/one.c
- path/two.md
```

## Documentazione

- Tenere sempre aggiornata la documentazione in `docs/it`.
- La documentazione deve essere in italiano, dettagliata e didattica.
- Spiegare sempre:
  - cosa fa il codice
  - dove si trova
  - perche' e' stato scritto cosi'
  - quali alternative sono state scartate o rimandate
  - quali effetti ha sui test e sul comportamento osservabile
- Scrivere pensando a studenti con poca esperienza: evitare salti logici e
  spiegare i concetti teorici quando servono.
- La guida sul codice deve essere impostata come una lettura guidata della
  codebase: non solo elenco di API, ma percorso ragionato in cui un lettore meno
  esperto viene accompagnato da flusso runtime, funzioni, responsabilita',
  strutture dati, campi modificati e motivazioni. Quando emergono concetti C,
  Linux, inotify, callback, puntatori a funzione o strutture dati, rimandare ai
  capitoli dedicati come glossario, guida C o documenti architetturali.
- Aggiornare `docs/it/documentation-progress.md` quando cambiano codice, test o
  documentazione rilevante.
- Aggiornare i file di scenario quando cambiano eventi attesi o semantica.
- Quando un test controlla log o eventi, inserire in testa al file un blocco
  `Expected log contract` che descriva le righe attese in `raw.log`,
  `events.log` o nello stream di log usato dal test, le righe vietate e il
  significato dello scenario. Questo vale anche per test C che usano un logger
  temporaneo invece dei file `.log` del runtime.
- Scegliere il tipo di test in base al contratto da proteggere:
  - contratti interni fra moduli, strutture dati, ownership, queue,
    dispatcher, sink e writer vanno coperti con test C unitari o di
    integrazione mirata
  - comportamenti pubblici end-to-end devono essere fissati progressivamente
    con golden test JSONL, perche' `output.jsonl` e' il contratto esterno
    strutturato
  - i test testuali su `raw.log`, `events.log` ed `errors.log` restano in
    parallelo come compatibilita' storica, debug umano e supporto didattico;
    non devono essere l'unico contratto stabile dei nuovi comportamenti
- Aggiornare `docs/commenting-style.md` e `docs/commenting-progress.md` se il
  passo riguarda commenti nel codice o stile dei commenti.
- Quando si documentano flussi complessi, strutture dati o responsabilita' tra
  moduli, aggiungere anche una vista grafica quando aiuta: diagrammi Mermaid,
  tabelle `campo -> scritto da -> letto da`, sequence diagram e schemi
  statici. Se il processo e' naturalmente dinamico, descrivere anche i frame
  logici che in futuro potrebbero generare GIF, video o viste interattive.
  Mermaid resta la scelta semplice per gli `.md`; animazioni reali vanno
  progettate come passo successivo con strumenti dedicati.

## Audit esplorativi notturni

Gli audit esplorativi notturni servono a usare Alfred come un utente reale e a
cercare divergenze dal contratto che non sono ancora coperte dalla suite
ufficiale. Non sostituiscono `make test` e non devono trasformarsi in una massa
di log committati nel repository.

Regole operative:

- quando il maintainer autorizza esplicitamente un audit autonomo, l'agente non
  deve fermarsi a chiedere conferme operative: deve poter lanciare comandi,
  raccogliere log, usare `gh`/GitHub API, creare issue, aggiornare issue e
  collegare sub-issue senza attendere ulteriori `yes`;
- questa autonomia vale per il perimetro dell'audit approvato. Non autorizza
  modifiche arbitrarie al codice, refactor, fix o merge non richiesti;
- creare una issue madre GitHub per ogni audit, con data, branch, commit,
  ambiente, scenari provati, risultati, artifact e prossimi tentativi;
- aprire una issue figlia separata per ogni bug confermato;
- collegare sempre issue madre e issue figlia in entrambe le direzioni:
  - se GitHub supporta le sub-issue, usare anche la relazione reale
    `create sub issue -> add existing issue`, o la mutation/API equivalente
    `addSubIssue`;
  - se la relazione sub-issue non e' disponibile, mantenere almeno link
    testuali espliciti nel body della issue madre e della issue figlia;
- aggiornare la issue madre ogni volta che viene aperta una issue figlia,
  indicando scenario, esito, link alla issue e stato della riproduzione;
- mettere nel repository solo report, script riproducibili ed estratti
  significativi, non log completi generati automaticamente;
- conservare i log completi sotto `/tmp` durante l'audit o allegarli alla issue
  solo quando sono davvero necessari;
- a fine audit, comprimere gli artifact locali e caricarli su Google Drive con
  `tests/exploratory/nightly/upload_audit_artifacts.sh`, poi linkare l'archivio
  nella issue madre;
- quando uno scenario esplorativo diventa stabile e importante, promuoverlo a
  test ufficiale nella suite corretta.

Percorsi stabili:

- `docs/it/audit/`: metodo generale e report storici degli audit;
- `docs/it/audit/nightly-playbook.md`: procedura compatta per avviare una
  sessione notturna autonoma con un prompt breve;
- `tests/exploratory/nightly/`: script rilanciabili per scenari esplorativi.

Gli script in `tests/exploratory/nightly` sono intenzionalmente fuori dalle
suite ufficiali. Vanno usati per ripetere cio' che e' stato fatto in un audit,
capire se un bug e' ancora presente e decidere cosa trasformare in test
contrattuale.

## Verifiche

Dopo aver discusso il passo e prima di modificare, consultare il codice come
farebbe un contributore umano:

- usare `rg` e lettura diretta dei file per cambi piccoli e localizzati
- usare Sourcebot quando serve cercare simboli, path o parole chiave dal browser
- usare Elixir quando serve navigare il codice C in modo piu' tradizionale
- usare Kythe solo come supporto semantico per restringere il campo, poi
  verificare sempre sui file reali

Guide operative:

- [Browser del codice](../../tools/code-browsing/README.md): script aggregati
  per controllare Docker, fare setup, avviare, fermare, riavviare e controllare
  Sourcebot, Elixir e Kythe insieme
- [Sourcebot](../sourcebot-browser/README.md): avvio, stop, log, reset e query
  utili; URL locale predefinito `http://127.0.0.1:3000`
- [Elixir](../code-browser/README.md): setup, reindex, avvio e stop; URL locale
  predefinito `http://127.0.0.1:8080/alfred/workspace/source`
- [Kythe](../kythe-browser/README.md): setup, reindex, server API e query CLI

Questi strumenti servono a capire il codice prima di modificarlo. Non
sostituiscono build, test e lettura del sorgente reale.
Per un esempio dettagliato di ricognizione con Kythe, `rg`, `sed` e lettura
della documentazione collegata, vedere
[Debugging, test e strumenti](10-debugging-test-e-strumenti.md).

Dopo modifiche al codice eseguire normalmente la procedura standard pre-commit:

```bash
git diff --check
make
make test
make test-backend-diagnostics
```

L'ordine e' intenzionale:

- `git diff --check` trova spazi finali, righe con whitespace problematico e
  difetti banali nel diff prima di perdere tempo con build e test.
- Il primo `make` verifica la build normale core-only, cioe' il percorso
  predefinito del progetto.
- `make test` verifica il percorso core end-to-end ufficiale.
- `make test-backend-diagnostics` verifica i log diagnostici del backend
  inotify, per esempio `WATCH_ADDED` e `WATCH_REMOVED`.

In queste regole, "core end-to-end ufficiale" significa che Alfred viene
eseguito davvero, legge eventi dal backend inotify reale, li converte in
`alfred_raw_event_t`, li passa al core e verifica lo stream semantico finale
prodotto dal core. Non e' un test unitario isolato: e' il percorso runtime
ufficiale `inotify -> raw -> core -> events.log`.

`ENABLE_LEGACY_SHADOW=1` era una variante di build temporanea usata durante la
migrazione: compilava anche il dispatcher legacy (`events.c`) e la
`move_cache`, cosi' il progetto poteva confrontare il vecchio comportamento
legacy con il core. La variante e' stata rimossa dal Makefile: la build
ottenuta con `make` e' l'unica build supportata e non include quei file.

Differenza minima da ricordare:

- core: stream semantico ufficiale e default del progetto
- shadow: modalita' diagnostica storica di confronto legacy/core, non piu'
  supportata dal runtime corrente

Per i dettagli, leggere:

- [Debugging, test e strumenti](10-debugging-test-e-strumenti.md)
- [Makefile e build system](09-makefile-e-build-system.md)
- [Roadmap unificata dopo i dossier](25-roadmap-unificata-dossier.md)

Questa procedura deve essere riproducibile anche senza strumenti di AI: ogni contributore
deve poterla eseguire manualmente prima di proporre o committare una modifica.
Se un comando fallisce, fermarsi, leggere l'errore e non proseguire con i passi
successivi finche' il problema non e' compreso.

Se il passo tocca move/rename, watch ricorsivi o raw event sintetici, eseguire
test core o backend mirati. Gli script in `tests/shadow/` sono storici e non
vanno usati come verifica ordinaria dopo la rimozione del dispatcher legacy.

## Stato semantico corrente

- Il runtime di default e' `event_engine=core`.
- `ALFRED_EVENT_ENGINE=shadow` e' un valore invalido; l'unico valore valido e'
  `core`.
- I test funzionali storici sono memoria della fase legacy/shadow, non verifica
  ordinaria del runtime corrente.
- I test core verificano lo stream semantico plain del core.
- `events.c`, `events.h`, `move_cache.c` e `move_cache.h` sono stati rimossi.
- `WATCH_ADDED` e `WATCH_REMOVED` sono diagnostica backend, non semantica core.
- `FILE_MODIFIED` e `FILE_READY` non sono duplicati: sono fasi diverse della
  vita del file.
- Nel core, move+rename e' un solo evento `RELOCATED`, non una coppia
  `MOVED` + `RENAMED`.
- Lo scan ricorsivo delle directory e' stato considerato stato backend, non
  semantica core.
- L'overflow inotify e' rimandato: e' una policy di recovery/resync, non uno
  scenario semantico base stabile.

## Commit chiave recenti

- `60323c6 Introduce explicit inotify backend boundary`
- `533f5a1 Encapsulate inotify descriptor and watchers`
- `e3d1234 Confine legacy move cache to events dispatcher`
- `37d68f4 Move legacy shadow dispatch behind backend`
- `93d210a Make core event engine the default`

Questi commit descrivono il passaggio dal legacy dispatcher al core come stream
ufficiale. Lo shadow mode non e' piu' uno strumento di confronto corrente: resta
solo come storia della migrazione.
