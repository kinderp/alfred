# Modello licenze

Questo capitolo non sceglie ancora la licenza definitiva di Alfred. Serve a
documentare la direzione desiderata e le parole corrette da usare quando
discutiamo di open source, moduli commerciali e codice visibile.

La scelta finale della licenza ha conseguenze legali e commerciali. Prima di
pubblicare Alfred come progetto riutilizzabile da terzi, bisognera' scegliere
una licenza precisa e, se il progetto diventera' commerciale, valutare anche un
parere legale.

## Obiettivo

L'obiettivo discusso per Alfred e':

```text
Alfred core                    -> open source
backend inotify base           -> open source
documentazione e test base     -> open source
backend avanzati futuri        -> possibile licenza commerciale/source-available
```

Esempi di backend avanzati futuri:

- modulo `fanotify`
- modulo `ebpf`
- eventuali integrazioni enterprise

L'idea e' mantenere libero e studiabile il cuore del progetto, ma lasciare la
possibilita' di offrire moduli piu' avanzati con una licenza piu' restrittiva o
a pagamento.

## Open source e source-available non sono sinonimi

Un punto importante: "il codice si vede" non significa automaticamente "open
source".

Nel linguaggio comune molti usano "open source" per indicare codice pubblicato
su GitHub. Tecnicamente, pero', una licenza open source deve concedere liberta'
di uso, modifica e ridistribuzione secondo criteri riconosciuti, come quelli
della Open Source Initiative.

Riferimenti utili:

- Open Source Definition:
  `https://opensource.org/osd`
- elenco licenze approvate OSI:
  `https://opensource.org/licenses`

Se un modulo e' leggibile pubblicamente ma richiede pagamento per essere usato
in produzione, o vieta l'uso commerciale senza licenza separata, quel modulo e'
piu' correttamente `source-available`, non open source in senso stretto.

Quindi la formulazione corretta per Alfred potrebbe essere:

```text
Alfred is an open-source core with optional commercial source-available modules.
```

Non:

```text
Everything is open source, but some modules require payment to use.
```

La seconda frase crea confusione, perche' mischia due modelli diversi.

## Possibile struttura del progetto

Per evitare confusione tecnica e legale, i moduli con licenza diversa dovrebbero
essere separati chiaramente dal core.

Una possibilita' dentro lo stesso repository:

```text
app/                  licenza open source scelta per Alfred base
core/                 licenza open source scelta per Alfred base
modules/inotify/      licenza open source scelta per Alfred base
modules/fanotify/     licenza commerciale/source-available futura
modules/ebpf/         licenza commerciale/source-available futura
docs/                 licenza documentazione da decidere
tests/                licenza open source scelta per Alfred base
```

Una possibilita' ancora piu' pulita, quando il progetto crescera':

```text
kinderp/alfred              core open source
kinderp/alfred-fanotify     modulo commerciale/source-available
kinderp/alfred-ebpf         modulo commerciale/source-available
```

Repository separati rendono piu' semplice:

- dichiarare licenze diverse
- gestire accesso e distribuzione
- accettare contributi con regole diverse
- evitare che codice con vincoli commerciali venga confuso con il core

## Licenze possibili per il core

La licenza del core non e' ancora scelta. Le famiglie principali da discutere
sono:

### Licenze permissive

Esempi:

- MIT
- BSD-2-Clause
- BSD-3-Clause
- Apache-2.0

Caratteristiche:

- facilitano adozione e integrazione
- permettono uso commerciale
- permettono fork proprietari
- richiedono in genere conservazione copyright e avvisi di licenza

`Apache-2.0` e' piu' lunga di MIT/BSD, ma include anche una concessione esplicita
sui brevetti. Questo puo' essere utile in progetti tecnici, anche se per Alfred
va valutato se la complessita' aggiunta sia necessaria.

### Licenze copyleft

Esempi:

- GPLv3
- AGPLv3

Caratteristiche:

- proteggono di piu' la liberta' del codice derivato
- impongono condizioni piu' forti sulla redistribuzione
- possono rendere piu' difficile l'integrazione in prodotti proprietari

Una licenza copyleft puo' essere coerente se l'obiettivo principale e' impedire
che qualcuno prenda il core, lo migliori e distribuisca una versione chiusa
senza restituire modifiche.

## Moduli commerciali o source-available

Per moduli avanzati futuri, come `fanotify` o `ebpf`, le opzioni sono diverse.

Possibili modelli:

- codice visibile ma uso commerciale vietato senza licenza
- uso gratuito per studio, didattica o sviluppo locale
- uso in produzione dietro licenza a pagamento
- licenza temporaneamente source-available con conversione open source dopo un
  certo periodo
- repository privato, con sorgente visibile solo ai clienti

Licenze e modelli da studiare prima di scegliere:

- Business Source License:
  `https://mariadb.com/bsl-faq-adopting/`
- PolyForm licenses:
  `https://polyformproject.org/licenses/`

Questi modelli non vanno chiamati automaticamente open source. Sono utili se si
vuole far leggere il codice, ma mantenere un controllo commerciale sull'uso.

## Dual licensing

Un'altra strategia possibile e' il dual licensing.

Con dual licensing lo stesso codice viene distribuito sotto due licenze diverse.
Per esempio:

```text
licenza open source copyleft  -> uso comunitario
licenza commerciale           -> uso proprietario senza obblighi copyleft
```

Questo modello puo' funzionare solo se il progetto controlla i diritti sul
codice o se i contributori hanno accettato una policy che permette il dual
licensing.

Per questo motivo, prima di accettare contributi esterni importanti, bisogna
decidere anche la policy contributiva.

## Contributi esterni e ownership

La licenza non riguarda solo chi usa il progetto. Riguarda anche chi contribuisce
codice.

Domande da risolvere:

- i contributori concedono il codice solo sotto la licenza del progetto?
- serve un Developer Certificate of Origin, cioe' DCO?
- serve un Contributor License Agreement, cioe' CLA?
- vogliamo poter cambiare licenza in futuro?
- vogliamo poter offrire moduli commerciali basati anche su contributi esterni?

Per un progetto didattico piccolo, una policy semplice puo' bastare all'inizio.
Per un progetto con componenti commerciali, invece, la policy va scritta bene.

## Decisione provvisoria per Alfred

Finche' non scegliamo una licenza definitiva:

- il README deve dire che la licenza non e' ancora selezionata
- non dobbiamo presentare Alfred come pacchetto gia' riutilizzabile da terzi
- non dobbiamo accettare contributi grandi senza chiarire la licenza
- i moduli futuri commerciali vanno progettati come componenti separati
- la parola "open source" va usata solo per le parti che avranno una licenza open
  source vera

Formula provvisoria consigliata:

```text
Alfred plans to use an open-source license for the core project. Future advanced
backends may use separate commercial or source-available licensing. The final
license has not been selected yet.
```

## Prossimi passi

Prima di aggiungere un file `LICENSE`, bisogna decidere:

1. licenza del core
2. licenza della documentazione
3. policy per contributi esterni
4. se i moduli avanzati resteranno nello stesso repository o in repository
   separati
5. modello commerciale desiderato per moduli come `fanotify` ed `ebpf`

Una volta scelta la licenza, bisognera' aggiornare:

- `README.md`
- `docs/it/11-come-contribuire.md`
- questo capitolo
- eventuali header di licenza nei file sorgente, se decidiamo di usarli
