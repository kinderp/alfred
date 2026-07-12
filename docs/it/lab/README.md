# Alfred Lab

Questa cartella contiene gli scenari Lab concreti.

Per v0 il Lab non e' una UI, non e' un parser e non produce `trace.jsonl`.
Uno scenario Lab e' un documento Markdown strutturato che collega:

- comando o azione umana;
- fatto backend osservato;
- raw event Alfred;
- evento semantico;
- record e output osservabili;
- funzioni attraversate;
- strutture dati coinvolte;
- test che proteggono il comportamento.

Il formato degli scenari e' definito in
[Tracepoint e Alfred Lab MVP](../41-tracepoint-lab-roadmap-mvp.md#formato-scenario-lab-v0).
I tracepoint citati dagli scenari sono documentati in
[Tracepoint Model v0](../42-tracepoint-model-v0.md).

## Scenari disponibili

| Scenario | Stato | Cosa mostra |
| --- | --- | --- |
| [Create file](scenarios/create-file.md) | `stable-doc` | Percorso minimo da `IN_CREATE` a `FILE_CREATED` e sink/output. |
| [Close-write / file-ready](scenarios/file-ready.md) | `stable-doc` | Differenza tra `FILE_MODIFIED` e `FILE_READY`, cioe' file scritto e writer chiuso. |

## Regola v0

Gli scenari Lab v0 devono restare leggibili a mano. Se in futuro servira' un
formato macchina, dovra' essere derivato da scenari gia' verificati, non
inventato prima di avere esempi reali.
