# Roadmap documentazione avanzata

Questo documento raccoglie le idee per far evolvere la documentazione di Alfred
oltre i Markdown attuali. L'obiettivo non e' sostituire la guida didattica, ma
aggiungere strumenti che aiutino studenti e contributori a passare dalla
spiegazione al codice reale navigabile.

La regola principale e':

```text
la guida scritta resta la sorgente didattica primaria;
i grafi e le viste generate servono a controllare, navigare e visualizzare.
```

## Problema da risolvere

Oggi `docs/it` contiene gia' spiegazioni, tabelle, Mermaid flow chart e
sequence diagram. Questo e' utile, ma non basta per tutti i casi.

Per capire bene una codebase C servono anche:

- link diretti tra funzioni, file e strutture dati
- call graph e caller graph
- grafi dei moduli
- viste navigabili del codice
- animazioni o walkthrough dei flussi piu' difficili
- controlli automatici per capire se la documentazione e' rimasta coerente con
  il codice

Questi strumenti devono essere usati con attenzione: un grafo troppo grande o
generato senza contesto puo' essere piu' confuso che utile.

## Livelli della documentazione desiderata

La documentazione ideale per Alfred dovrebbe avere piu' livelli.

### 1. Guida narrativa

E' il livello gia' presente in `docs/it`.

Serve a spiegare:

- perche' il progetto e' fatto in un certo modo
- quali responsabilita' hanno app, backend, core e legacy shadow
- quali decisioni sono state prese
- quali alternative sono state scartate o rimandate
- quali concetti C/Linux servono per leggere il codice

Questo livello deve restare scritto a mano e in italiano.

### 2. Diagrammi statici piccoli

Qui Mermaid resta adatto.

Usi corretti:

- flow chart brevi
- sequence diagram
- stati principali
- passaggi di uno scenario
- relazioni tra pochi componenti

Limite: quando il grafo cresce, Mermaid diventa difficile da leggere e da
impaginare nei Markdown.

### 3. Grafi generati del codice

Per grafi piu' grandi serve uno strumento piu' adatto, come Graphviz/DOT.

Usi possibili:

- call graph
- caller graph
- grafo file -> funzioni
- grafo struttura dati -> funzioni che leggono/scrivono campi
- grafo modulo -> modulo

Questi grafi devono essere filtrati. Un grafo completo di tutto il programma
probabilmente sarebbe troppo grande; meglio generare grafi mirati, per esempio:

```text
app_run()
inotify_backend_poll()
inotify_backend_init()
watch_manager_add_recursive()
alfred_engine_process_raw()
```

### 4. Documentazione API navigabile

Per questo il candidato naturale e' Doxygen, eventualmente integrato con
Graphviz.

Doxygen puo' leggere codice C, commenti strutturati, funzioni, struct, enum e
macro. Con Graphviz puo' generare anche diagrammi, call graph e caller graph.

Possibile output:

```text
docs/generated/doxygen/html/
```

Questo livello aiuterebbe studenti e contributori a cliccare da una funzione
alla sua definizione, vedere parametri, tipi collegati e grafi locali.

### 5. Sito documentale integrato

Se la documentazione cresce, potremmo valutare Sphinx + Breathe + Doxygen.

Ruoli:

- Sphinx: sito documentale, capitoli, navigazione, indice, ricerca
- Doxygen: parsing C e API reference
- Breathe: ponte tra output XML di Doxygen e pagine Sphinx
- Graphviz: grafi e diagrammi

Questo approccio e' piu' pesante, ma e' usato in progetti grandi per combinare
guide scritte a mano e API generate dal codice.

### 6. Grafi semantici e knowledge graph

Kythe e un futuro Graphify rientrano qui.

Kythe:

- indicizza il codice C tramite build strumentata
- produce dati semantici interrogabili
- puo' aiutare a trovare simboli, file, definizioni e riferimenti
- puo' supportare script futuri che generano mappe o controllano la coerenza
  della documentazione

Graphify:

- potrebbe collegare codice, documentazione, diagrammi e decisioni progettuali
- sarebbe utile per domande piu' ampie, non solo per simboli C
- va ancora valutato sperimentalmente prima di inserirlo nel workflow ufficiale

Regola: Kythe e Graphify possono aiutare a restringere il campo, ma ogni
risultato va sempre verificato sui file sorgente reali.

### 7. Animazioni e walkthrough

Per animazioni vere servono strumenti diversi dai semplici diagrammi Markdown.

Possibili strumenti:

- D2: diagrammi testuali con link, tooltip e supporto ad animazioni
- VHS: registrazioni terminali trasformabili in GIF/video
- asciinema: registrazioni terminali leggere e riproducibili
- SVG/HTML generato da script nostri

Usi possibili:

- mostrare come si esegue la procedura pre-commit
- mostrare l'uso di Sourcebot
- mostrare query Kythe CLI
- animare il flusso `inotify -> raw -> core`
- animare l'aggiunta/rimozione di watch nella watcher table
- animare la correlazione move/rename nel core

## Esempi online da studiare

Non esiste uno standard perfetto unico. Vale la pena studiare progetti diversi.

### Zephyr Project

Riferimento:

```text
https://docs.zephyrproject.org/latest/project/documentation.html
```

Perche' e' interessante:

- progetto C/embedded grande
- usa Doxygen per API documentation
- collega documentazione, API e test
- ha una struttura didattica e contributiva molto matura

### Linux Kernel documentation

Riferimento:

```text
https://docs.kernel.org/6.0/doc-guide/sphinx.html
```

Perche' e' interessante:

- progetto C enorme
- usa Sphinx per documentazione tecnica
- supporta rendering di diagrammi e Graphviz/DOT
- mostra come gestire documentazione grande senza perdere struttura

### QEMU documentation

Riferimento:

```text
https://www.qemu.org/docs/master/
```

Perche' e' interessante:

- progetto C grande e complesso
- documentazione sviluppatore organizzata in Sphinx
- utile come esempio di documentazione per contributori tecnici

### Sphinx + Breathe + Doxygen

Riferimento:

```text
https://devblogs.microsoft.com/cppblog/clear-functional-c-documentation-with-sphinx-breathe-doxygen-cmake/
```

Perche' e' interessante:

- spiega una pipeline pratica
- combina guide scritte a mano e API generate
- usa Doxygen per capire C/C++ e Sphinx per presentare la documentazione

### Doxygen + Graphviz

Riferimento:

```text
https://www.doxygen.nl/manual/diagrams.html
```

Perche' e' interessante:

- Doxygen puo' generare grafi usando Graphviz/dot
- utile per call graph, caller graph e diagrammi collegati al codice
- primo candidato da provare su Alfred

### Kroki

Riferimento:

```text
https://docs.kroki.io/kroki/
```

Perche' e' interessante:

- renderizza molti formati da testo
- supporta Mermaid, Graphviz, PlantUML, C4, D2 e altri
- potrebbe diventare un backend unico per generare immagini dai diagrammi

### D2

Riferimento:

```text
https://d2lang.com/
```

Perche' e' interessante:

- diagrammi testuali
- supporto a link e tooltip
- supporto ad animazioni
- candidato per diagrammi interattivi futuri

### VHS e asciinema

Riferimenti:

```text
https://github.com/charmbracelet/vhs
https://docs.asciinema.org/
```

Perche' sono interessanti:

- documentano sessioni terminali
- permettono walkthrough riproducibili
- utili per mostrare procedure manuali, test e strumenti di sviluppo

## Roadmap proposta

### Fase 1: decidere il modello documentale

Prima di installare strumenti, chiarire il modello:

- `docs/it` resta la sorgente didattica primaria
- `docs/generated/` conterra' solo output rigenerabile
- gli strumenti generati non sostituiscono la spiegazione manuale
- ogni grafo deve avere uno scopo didattico preciso

Output atteso:

```text
docs/it/17-roadmap-documentazione-avanzata.md
```

### Fase 2: spike Doxygen + Graphviz

Creare una configurazione sperimentale, per esempio:

```text
docs/code-reference/
```

Obiettivo:

- generare HTML locale con funzioni, struct e file
- abilitare Graphviz
- provare call graph e caller graph su un sottoinsieme utile

Possibile comando futuro:

```bash
make docs-code
```

Domande da valutare:

- i grafi generati sono leggibili?
- le funzioni statiche nei `.c` vengono documentate bene?
- i link aiutano davvero gli studenti?
- il risultato e' migliore di Sourcebot per leggere il codice?

### Fase 3: grafi mirati generati

Se Doxygen/Graphviz e' utile, aggiungere grafi filtrati.

Esempi:

```text
app_run() call graph
inotify_backend_poll() flow
watch_manager_add_recursive() discovery graph
alfred_engine_process_raw() semantic graph
```

Questi grafi dovrebbero essere piccoli e collegati a capitoli precisi della
guida didattica.

### Fase 4: sito documentale integrato

Valutare Sphinx + Breathe + Doxygen solo dopo aver visto se Doxygen/Graphviz e'
utile da solo.

Motivo: Sphinx e' potente, ma introduce piu' complessita' e un secondo formato
di scrittura. Non va aggiunto se la documentazione Markdown resta sufficiente.

### Fase 5: Graphify spike

Valutare Graphify in parallelo, ma senza metterlo sul percorso critico.

Input da indicizzare:

```text
app/
core/
modules/
tests/
docs/it/
docs/sourcebot-browser/
docs/kythe-browser/
```

Domande da provare:

- quali documenti spiegano `inotify_backend_poll()`?
- quali file partecipano al flusso `inotify -> raw -> core`?
- quali decisioni documentali riguardano `RELOCATED`?
- quali test fissano la semantica move/rename?

Tenere Graphify solo se produce risposte utili e verificabili.

### Fase 6: animazioni

Solo dopo avere grafi e scenari stabili, scegliere lo strumento per animare.

Candidati:

- D2 per diagrammi animati e interattivi
- VHS/asciinema per terminal walkthrough
- SVG/HTML generato per animazioni specifiche delle strutture dati

Primi scenari animabili:

- procedura pre-commit
- `mkdir -p` e raw create sintetici
- move+rename -> `RELOCATED`
- close-write -> `FILE_READY`
- aggiunta/rimozione watch

## Decisione provvisoria

Per ora non integriamo tutto. La prossima prova concreta dovrebbe essere:

```text
Doxygen + Graphviz su Alfred
```

Motivo:

- e' il candidato piu' naturale per C
- puo' produrre call graph e caller graph
- si integra bene con il codice commentato
- permette di capire subito se i grafi generati sono davvero didattici

Sourcebot resta il browser pratico per leggere codice. Kythe resta indice
semantico sperimentale. Graphify resta candidato per un secondo spike orientato
a knowledge graph e documentazione.
