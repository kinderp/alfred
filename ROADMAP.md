## 🎯 **ROADMAP**

**FASE 1** — _Functional regression suite_ ✅

* test automatici bash
* verifica eventi
* regressioni
* sanitizer integration

**FASE 2** — _Coverage completa eventi_

Aggiungere test per:

* IN_ATTRIB
* IN_MODIFY
* IN_CLOSE_WRITE
* IN_DELETE_SELF
* IN_MOVE_SELF
* IN_IGNORED
* queue overflow
* symlink
* hardlink
* cross-filesystem moves
* deep recursive trees
* race conditions

**FASE 3** — _Stress tests Python_

Creare workload seri:

* create storm
* rename storm
* concurrent writers
* recursive explosion
* move storms
* throughput tests

**FASE 4** — _Benchmarking e performance_

Misurare:

* events/sec
* latency
* memory usage
* CPU usage
* watch scaling
* queue overflow thresholds

e magari produrre:

CSV
grafici
report automatici

**FASE 5** — _CI/CD automation_

Automatizzare tutto con:

* GitHub Actions
* sanitizer build
* release build
*regression suite automatica

così ogni commit viene validato.
