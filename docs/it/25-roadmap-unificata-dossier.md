# Roadmap unificata dopo i dossier

Questo capitolo riassume le decisioni emerse dalla lettura dei dossier in
`dossier/`:

- `Dossier_Alfred_unificato_architettura_prodotto_lab.pdf`
- `Dossier_Alfred_architettura_prodotto_mercato.pdf`
- `alfred_lab_dossier.pdf`

I dossier spostano il ragionamento da "finire il backend inotify" a "costruire
Alfred come piattaforma". Questo non cancella il lavoro fatto sul core, sui log,
sul resync e sulla matrice inotify. Lo mette pero' in un ordine piu' chiaro:
prima stabilizzare il modello degli eventi e l'API dei backend, poi serializzare
in JSONL, poi costruire Alfred Lab e solo dopo aggiungere backend complessi come
fanotify, audit o eBPF.

## Decisione principale

La scelta piu' importante e':

```text
JSONL non e' la Backend API.
JSONL e' un output.
```

Un backend non deve scrivere direttamente righe JSONL. Un backend deve produrre
record strutturati verso Alfred. Poi un writer puo' serializzare quei record in
testo, JSONL, MessagePack, Protobuf, socket binaria, SQLite o altro.

Questa distinzione evita tre errori:

- fare parsing di stringhe per ricostruire eventi
- legare i plugin futuri a un formato di log
- dover rifare tutto quando vorremo output piu' performanti

## Ordine consigliato

L'ordine consigliato per i prossimi filoni e':

1. **Event Model v0**
2. **Backend API v0**
3. **Refactor inotify verso la Backend API**
4. **Output strutturato JSONL**
5. **Tracepoint logici per test e Alfred Lab**
6. **Alfred Lab MVP**
7. **Performance suite**
8. **Backend fanotify/audit/eBPF**

Questo ordine e' volutamente conservativo. Se partissimo subito dalle
prestazioni o da eBPF, rischieremmo di ottimizzare o estendere un contratto non
ancora stabile.

## Fase 1 - Event Model v0

Obiettivo: definire il modello dati comune di Alfred.

Il dossier distingue tre livelli:

| Livello | Significato | Esempio |
| --- | --- | --- |
| Raw backend event | Fatto vicino al backend o al kernel | `IN_MOVED_FROM`, `fanotify permission`, syscall `openat` |
| Normalized Alfred event | Fatto tradotto in schema comune | `action=rename`, `old_path=/a`, `new_path=/b`, `source=inotify` |
| Semantic event | Evento gia' correlato e interpretabile | `DIR_RENAMED`, `WORKSPACE_ESCAPE`, `FILE_READY` |

Oggi Alfred ha gia' `alfred_raw_event_t` e `alfred_event_t`, ma manca una
decisione formale su:

- `schema_version`
- `event_id`
- `source_module`
- `sensor_id`
- `host_id`
- `subject`
- `action`
- `target`
- `context`
- `control`

Questa fase dovrebbe produrre un documento dedicato, per esempio:

```text
docs/it/29-event-model-v0.md
```

Il documento ora esiste: [Event Model v0](29-event-model-v0.md). Solo dopo
questa fase ha senso disegnare JSONL con precisione.

## Fase 2 - Backend API v0

Obiettivo: definire come un backend/plugin parla con Alfred.

Il documento ora esiste: [Backend API v0](30-backend-api-v0.md). La specifica
definisce lifecycle, target management, emit sink basato su Event Model v0,
capabilities, ownership, error model e mapping del backend inotify corrente.

La Backend API deve coprire almeno:

- lifecycle: `init`, `start`, `poll`, `stop`, `destroy`
- target osservati: `add_target`, `remove_target`
- emissione eventi raw strutturati
- emissione diagnostica strutturata
- capabilities
- modello errori
- ownership memoria
- versioning API/ABI

Per ora la scelta resta:

```text
fase iniziale: backend linkati staticamente
fase futura: plugin dinamici .so solo dopo API stabile
```

Questo e' importante per studenti e contributori: una API statica e' piu'
semplice da testare, debuggare e documentare. I plugin dinamici aggiungono
problemi di ABI, `dlopen()`, path, sicurezza, versioning e ownership tra
moduli.

## Fase 3 - Refactor inotify

Obiettivo: rendere `inotify` il primo backend conforme alla Backend API.

Non significa riscrivere tutto. Significa mappare il codice corrente su
responsabilita' piu' esplicite:

| Funzione logica | Stato corrente |
| --- | --- |
| init/destroy | gia' presente in forma concreta nel backend inotify |
| start/stop | parzialmente incorporato nell'app e nel backend |
| poll | esiste in `inotify_backend_poll()` |
| add/remove target | oggi e' intrecciato con watch manager e app |
| emit raw | esiste come callback verso il core |
| emit diagnostic | oggi e' ancora prevalentemente testuale |
| capabilities | non ancora strutturate |

Questa fase e' il punto in cui ha senso riprendere anche debiti come
`IN_MASK_CREATE`, separazione tra subscription mask e bit riconosciuti,
configurazione backend-specifica e diagnostica strutturata.

## Fase 4 - JSONL

Obiettivo: aggiungere output macchina stabile.

I dossier indicano due stream principali:

```text
events.log     -> lettura umana
events.jsonl   -> lettura macchina, test, Lab, integrazioni
trace.jsonl    -> debug interno, code trace, Alfred Lab
```

Il JSONL deve contenere `schema_version`. Questo permette di evolvere il formato
senza rompere subito test, Lab e strumenti esterni.

Il JSONL non deve sostituire subito il testo. Per gli studenti e il debug
manuale, `events.log`, `raw.log` ed `errors.log` restano utili.

## Fase 5 - Tracepoint logici

Obiettivo: rendere test, Lab e documentazione meno fragili.

Un tracepoint logico e' un nome stabile che descrive un fatto interno, per
esempio:

```text
RAW_EVENT_READ
WATCH_ADDED
WATCH_STALE
MOVE_FROM_STORED
MOVE_MATCH_FOUND
SEMANTIC_EVENT_EMITTED
ERROR_REPORTED
```

Il Lab non dovrebbe dipendere da "questa funzione alla riga 123 ha stampato
questa stringa". Dovrebbe dipendere da tracepoint stabili e da eventi
strutturati.

## Fase 6 - Alfred Lab MVP

Obiettivo: costruire uno strato didattico e dimostrativo sopra i test.

Il Lab non deve sostituire la suite Bash/C. Deve usare i test e gli scenari gia'
esistenti per mostrare:

```text
comando umano
-> evento kernel/backend
-> raw Alfred
-> evento semantico
-> spiegazione
-> funzioni coinvolte
```

Uno scenario Lab dovrebbe contenere:

- obiettivo
- comandi da eseguire
- output raw atteso
- evento semantico atteso
- spiegazione didattica
- funzioni coinvolte
- test Bash/C collegati
- cause frequenti di fallimento

## Fase 7 - Performance suite

Le prestazioni restano un obiettivo di Alfred, ma non sono il primo passo dopo
i dossier.

Prima servono:

- modello eventi stabile
- API backend chiara
- output strutturato
- tracepoint e metriche minime

Solo dopo ha senso costruire benchmark piu' seri su:

- throughput eventi
- latenza di dispatch
- costo del logging testuale vs JSONL
- costo della recovery lost-scope
- costo di startup ricorsivo
- overhead di serializzazione
- backpressure e drop policy

Questo evita di misurare una forma del sistema che cambiera' subito.

## Fase 8 - Backend complessi

Fanotify, audit ed eBPF sono importanti, ma non conviene introdurli prima di
Event Model v0 e Backend API v0.

Motivo:

- `fanotify` non e' un semplice "inotify piu' potente"
- audit/eBPF portano process context, syscall, rete e privilegi
- ogni backend ha limiti e capabilities diverse
- senza API comune rischieremmo tre integrazioni incompatibili

Questi backend devono arrivare quando Alfred sa gia' rappresentare:

- chi ha agito
- quale azione e' stata osservata
- quale oggetto e' stato toccato
- quale contesto/sessione riguarda
- se il backend puo' solo osservare o anche bloccare

## Impatto sul prossimo step

Dopo la lettura dei dossier, il prossimo step migliore non e' una nuova feature
inotify e non e' una suite performance completa.

Il prossimo step migliore e':

```text
usare Backend API v0 come base per il refactor inotify
```

Subito dopo:

```text
introdurre record C, adapter e diagnostica strutturata
```

Solo dopo questi due documenti conviene decidere se il primo refactor sara':

- diagnostica strutturata
- JSONL
- refactor inotify verso `alfred_backend_ops_t`
- separazione output sink
- performance suite

## Rimandi

- [Roadmap plugin backend](23-roadmap-plugin-backend.md)
- [Roadmap AI agent guardrail](24-roadmap-ai-agent-guardrail.md)
- [Event Model v0](29-event-model-v0.md)
- [Backend API v0](30-backend-api-v0.md)
- [Contratto dei log](22-contratto-log.md)
- [Roadmap scanner e resync](21-roadmap-scanner-resync.md)
- [Matrice eventi inotify](20-matrice-eventi-inotify.md)
