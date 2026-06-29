# Visione: Alfred come Observation Runtime

Questo documento descrive una direzione molto futura di Alfred. Non e' scope
corrente e non autorizza da solo refactor o feature fuori dalla milestone
inotify. Serve come bussola architetturale: Alfred oggi resta un motore Linux
per eventi filesystem, record strutturati, backend API, writer e test; la
visione lunga e' progettare questi pezzi come primo laboratorio di un runtime
piu' generale per osservazioni verificabili.

## Tesi principale

La tesi e':

```text
Alfred non deve essere l'intelligenza.
Alfred deve diventare il sistema che rende l'esperienza osservabile,
verificabile e riutilizzabile da sistemi intelligenti.
```

In questa visione Alfred non e' solo un programma che raccoglie eventi. E' un
substrato operativo che trasforma segnali eterogenei in osservazioni dotate di
tempo, fonte, evidenza, confidenza e relazioni. Queste osservazioni possono poi
alimentare memoria, knowledge graph, world model, planner, policy engine,
agent runtime, audit e spiegazioni.

La forma breve e':

```text
Alfred Security oggi.
Universal Observation Runtime domani.
Runtime dell'esperienza come visione lunga.
```

## Perche' non basta parlare di eventi

Un evento e' qualcosa che succede:

- un file viene creato;
- un processo viene eseguito;
- una connessione viene aperta;
- una policy viene violata.

Un sistema intelligente pero' non lavora solo con eventi. Lavora anche con:

- segnali grezzi;
- misure;
- percezioni;
- inferenze;
- previsioni;
- decisioni;
- azioni;
- esiti;
- feedback;
- revisioni della memoria;
- errori di previsione;
- prove.

Per questo il concetto piu' generale e' `Observation`.

```text
Observation
    |
    +--> RawSignal
    +--> Event
    +--> Measurement
    +--> Perception
    +--> Inference
    +--> Prediction
    +--> Plan
    +--> Action
    +--> Outcome
    +--> Feedback
    +--> LearningEvent
    +--> Evidence
```

Oggi Alfred implementa soprattutto eventi filesystem. La scelta importante e'
non progettare il core come se il filesystem fosse il mondo intero.

## Alfred non rappresenta la realta'

Un punto delicato: Alfred non dovrebbe dire di rappresentare direttamente "la
realta'". Una camera non produce verita': produce pixel. Un modello vision non
produce verita': produce una classificazione con una confidenza. Un backend
kernel non produce intenzioni: produce fatti osservati da una sorgente.

La formulazione corretta e':

```text
Alfred rappresenta osservazioni, prove, inferenze e credenze sulla realta'.
```

Questa distinzione protegge il progetto da un errore grave: confondere dati
grezzi, semantica, inferenze e decisioni.

Esempio nel filesystem:

```text
Raw evidence:
  inotify segnala IN_MODIFY

Normalized event:
  filesystem file.modified

Semantic event:
  FILE_MODIFIED su path noto

Inference:
  file sensibile modificato

Policy decision:
  would_block oppure alert
```

Esempio in un dominio visuale futuro:

```text
Raw signal:
  frame video

Perception:
  object.detected cup confidence=0.86

Inference:
  cup_near_table_edge

Prediction:
  cup_may_fall probability=0.71

Action:
  move_cup_to_center

Outcome:
  cup_stable
```

Il modello deve poter distinguere questi livelli.

## Ruolo di Alfred in un sistema intelligente

Una architettura intelligente non centrata solo su LLM potrebbe essere:

```text
Sensors / adapters
filesystem, processi, rete, browser, video, GPS, robot, app
        |
        v
Alfred Observation Runtime
normalizza osservazioni, conserva prove, assegna tempo/fonte/confidenza
        |
        v
Memory layer
memoria episodica, stato corrente, memoria semantica
        |
        v
Knowledge graph / projections
entita', relazioni, stato derivato dal log
        |
        v
World model
prevede stati futuri e stima incertezza
        |
        v
Planner / policy / cost system
sceglie azioni e valuta vincoli
        |
        v
Action executors
tool, app, agenti, sistemi, robot
        |
        v
Feedback verso Alfred
```

In questo schema Alfred non e' il cervello. E' piu' vicino a:

- sistema nervoso afferente, per portare segnali affidabili;
- registro episodico, per ricordare cosa e' accaduto;
- livello di provenance, per sapere da dove arriva ogni fatto;
- bus di osservazioni, per collegare sensori, memoria, planner e azioni.

## Dove entra l'LLM

L'LLM non sparisce. Semplicemente non e' l'unico centro del sistema.

Ruoli adatti a un LLM:

- interfaccia linguistica;
- traduzione da linguaggio naturale a query o policy;
- spiegazione degli eventi osservati;
- generazione di report;
- aiuto nel debug;
- proposta di regole;
- riassunto della memoria.

Ruoli che non dovrebbero dipendere solo da un LLM:

- memoria fattuale;
- provenance;
- decisioni di sicurezza deterministiche;
- controllo del percorso caldo;
- enforcement;
- stato corrente del sistema;
- replay verificabile;
- audit delle azioni.

Quindi Alfred puo' essere il livello che fornisce esperienza strutturata a un
LLM, non un semplice wrapper intorno a un LLM.

## Architettura esagonale e sensori

La visione richiede una architettura a porte e adapter.

```text
                 Alfred Core
        observation model, provenance,
        record log, projection, policy
              /       |        \
             /        |         \
 filesystem sensor  process sensor  future sensors
 inotify/fanotify   eBPF/audit      video/GPS/browser
```

Il core non deve sapere se una osservazione arriva da:

- `inotify`;
- `fanotify`;
- eBPF;
- audit;
- una camera;
- un browser;
- un calendario;
- un planner;
- un modello AI.

Ogni sorgente deve passare da un adapter che dichiara cosa osserva, quali
garanzie offre e quali limiti ha.

Nel codice corrente il termine `backend` resta corretto. Nel modello
architetturale futuro, pero', un backend e' un tipo di sensore:

```text
backend = implementazione tecnica concreta
sensor = ruolo architetturale: osservare un dominio
adapter = traduttore verso il modello comune Alfred
```

## Log append-only e projection

Una idea centrale e':

```text
il log dice cosa e' successo;
la projection dice cosa crediamo sia vero adesso.
```

Esempio:

```text
Log:
  FILE_CREATED a.txt
  FILE_RENAMED a.txt -> b.txt
  FILE_DELETED b.txt

Projection:
  il file non esiste piu'
```

Questa separazione e' utile gia' oggi per Alfred Security:

- debug;
- replay;
- audit;
- test golden;
- Lab;
- stato watch;
- stato file;
- correlazione processo/file/rete futura.

Ed e' indispensabile domani per knowledge graph e world model. Lo stato non
dovrebbe essere la fonte primaria: dovrebbe essere derivato da osservazioni
versionate e riproducibili.

## Ciclo osservazione, previsione, azione, feedback

Un sistema intelligente robusto dovrebbe poter registrare questo ciclo:

```text
S_t      = stato creduto del mondo
O_t      = nuova osservazione
B_t      = belief aggiornato
P_t+1    = previsione dello stato futuro
A_t      = azione scelta
O_t+1    = esito osservato
E_t      = errore di previsione
U_t      = aggiornamento memoria/modello
```

In forma operativa:

```text
vedo qualcosa
-> aggiorno cosa credo del mondo
-> prevedo cosa succedera'
-> scelgo un'azione
-> osservo cosa succede davvero
-> misuro l'errore tra previsione e outcome
-> aggiorno memoria, regole o modello
```

Per Alfred Security un esempio concreto e':

```text
osservo molti file modificati rapidamente
-> progetto stato: directory sotto modifica massiva
-> predico rischio ransomware-like
-> propongo azione: would-block o alert
-> osservo se le modifiche continuano
-> aggiorno rischio e report
```

Questa e' una forma minima di world model digitale: non serve video, robotica o
AI generale per iniziare.

## Opportunita'

Questa visione crea opportunita' importanti:

- differenzia Alfred da un semplice wrapper di inotify;
- rende sensato investire in provenance, replay e record strutturati;
- prepara backend multipli senza legare il core a Linux;
- apre la strada ad Agent Action Ledger e guardrail per agenti AI;
- permette spiegabilita' e audit migliori di un sistema LLM-only;
- rende possibile un Lab che spiega non solo cosa e' accaduto, ma da quali
  osservazioni nasce una decisione;
- crea una barriera competitiva nel modello dati, non solo nel codice.

## Rischi

La visione e' ambiziosa e puo' danneggiare il progetto se introdotta male.

Rischi principali:

- overengineering: introdurre astrazioni prima di averne bisogno;
- scope explosion: provare a fare filesystem, video, GPS, robotica, knowledge
  graph e world model insieme;
- modello universale non validato: disegnare un envelope bello ma inutilizzabile
  su backend reali;
- marketing troppo grande: promettere una piattaforma cognitiva quando il codice
  deve ancora chiudere il backend inotify;
- perdita di performance: provenance, evidence e relazioni possono costare se
  finiscono nel percorso caldo;
- superficie d'attacco: piu' sensori e plugin significano piu' input non fidati.

La regola per evitarli e':

```text
la visione guida l'architettura;
non espande lo scope della milestone corrente.
```

## Cosa fare ora

Azioni utili subito:

- mantenere chiara la separazione tra backend, adapter, core, dispatcher e
  writer;
- non rendere il record comune troppo inotify-specific;
- documentare provenance, source, confidence ed evidence come direzione futura;
- mantenere JSONL come output, non come API primaria;
- progettare capabilities dei backend in modo esplicito;
- preparare replay come futura esigenza architetturale;
- distinguere sempre osservazione, inferenza, decisione e azione.

Azioni da non fare ora:

- rinominare tutto in `alfred_observation_t`;
- introdurre RDF, Neo4j o knowledge graph nel runtime;
- iniziare video, immagini o GPS;
- promettere world model nel README pubblico;
- aggiungere campi pesanti al percorso caldo senza benchmark;
- iniziare un policy engine completo prima di chiudere i contratti base.

## Formula guida

La formulazione operativa piu' equilibrata e':

```text
Costruiamo Alfred Security come se un giorno dovesse diventare
il primo organo sensoriale di un sistema cognitivo piu' ampio.
```

Questa frase tiene insieme concretezza e visione. Alfred deve restare utile
oggi come motore filesystem/security, ma le sue fondamenta devono evitare scelte
che renderebbero impossibile domani passare a osservazioni, memoria, previsione
e feedback.
