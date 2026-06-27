# Audit esplorativi notturni

Questa cartella raccoglie i report degli audit esplorativi di Alfred.
Un audit esplorativo non e' una suite ufficiale come `make test`: e' una
sessione pratica in cui Alfred viene usato come farebbe un utente, seguendo la
documentazione e provando scenari reali per cercare bug, divergenze dal
contratto o comportamenti poco chiari.

## Perche' esistono questi audit

I test ufficiali proteggono contratti precisi. L'audit esplorativo invece serve
a rispondere a domande piu' operative:

- Alfred si comporta come documentato?
- I log prodotti sono comprensibili?
- `raw.log`, `events.log` e `output.jsonl` raccontano la stessa storia?
- Una configurazione errata fallisce in modo chiaro?
- Uno scenario reale ma non ancora coperto dai test ufficiali mostra problemi?

Se un audit trova un bug confermato, il bug deve diventare una issue separata
su GitHub. Se lo scenario e' importante, deve poi essere trasformato in test
ufficiale o golden test.

## Differenza rispetto ai fuzzy test

Gli audit notturni attuali non sono fuzzy test in senso stretto. Sono scenari
esplorativi riproducibili: un umano sceglie operazioni realistiche, le codifica
in script, raccoglie log e valuta se Alfred si comporta come documentato.

Un fuzzy test vero genererebbe automaticamente molte sequenze casuali o
semi-casuali e controllerebbe proprieta' generali come "non crashare",
"produrre sempre JSONL valido" o "non generare record contraddittori".

Questa distinzione e' importante:

- gli audit esplorativi sono leggibili e spiegano bene il comportamento reale;
- i fuzzy test sono piu' adatti a scoprire bug imprevisti su moltissime
  combinazioni;
- in futuro Alfred potra' avere entrambi, ma oggi gli script nightly sono
  scenario-based tests.

La maturita' osservata dagli audit e' documentata in
[maturity-matrix.md](maturity-matrix.md).
La matrice include anche dimensioni che non sono ancora misurate in modo
completo, come performance, sicurezza, coerenza e documentazione. Questo e'
intenzionale: il modello deve indicare cosa raccogliere nei prossimi audit, non
solo cio' che e' gia' misurabile dopo la prima notte.

La matrice distingue anche maturita' storica e freschezza della validazione.
Se il codice collegato a una funzionalita' cambia in modo rilevante, la fiducia
nei risultati precedenti puo' diventare `stale` o `needs-revalidation`. Questo
non cancella la memoria storica degli audit, ma evita di trattare una vecchia
validazione come prova valida sul codice corrente.

Per eseguire una sessione completa partendo da un prompt breve, usare il
[playbook operativo](nightly-playbook.md).

## Struttura GitHub

Ogni audit notturno usa una issue madre. La issue madre e' il diario pubblico
della sessione e deve contenere:

| Sezione | Contenuto |
| --- | --- |
| Obiettivo | Cosa si vuole controllare durante la notte. |
| Commit e branch | SHA e branch testati, cosi' il risultato e' riproducibile. |
| Ambiente | OS, VM, build, opzioni rilevanti e configurazioni usate. |
| Scenari | Lista degli scenari provati, con esito `PASS`, `FAIL` o `NEEDS REVIEW`. |
| Artifact | Percorsi locali o allegati GitHub con log e output completi. |
| Bug confermati | Link alle issue figlie create per i problemi reali. |
| Prossimi tentativi | Cosa provare nell'audit successivo. |

La issue madre deve usare almeno queste label:

```text
area:tests
kind:audit
```

Le issue figlie nate dall'audit devono mantenere `kind:audit` e aggiungere il
tipo specifico del risultato: `kind:bug` per bug confermati, `kind:test` per
scenari da promuovere a test ufficiale, `kind:debt` per debiti tecnici o
documentali emersi durante l'audit.

Ogni bug confermato usa una issue figlia dedicata. La issue figlia deve
contenere:

- link alla issue madre;
- scenario minimo di riproduzione;
- comando eseguito;
- configurazione usata;
- comportamento atteso;
- comportamento reale;
- estratti dei log importanti;
- spiegazione del motivo per cui e' un bug o una divergenza dal contratto.

La relazione deve essere bidirezionale:

- la issue figlia cita la issue madre;
- la issue madre contiene il link alla issue figlia.

In questo modo, leggendo la issue madre si vede tutta la notte di audit; leggendo
la issue figlia si vede il problema specifico.

## Struttura nel repository

La struttura stabile e':

```text
docs/it/audit/
  README.md
  2026-06-25-audit-notturno.md
  nightly-playbook.md
  maturity-matrix.md
  maturity-data-template.csv

tests/exploratory/nightly/
  README.md
  nightly_lib.sh
  base-jsonl-lifecycle.sh
  moves-jsonl.sh
  lost-scope-jsonl.sh
  audit-raw-events.sh
  recursive-fast-mkdir-p.sh
  root-file-should-fail.sh
```

Ruoli:

| Percorso | Ruolo |
| --- | --- |
| `docs/it/audit/README.md` | Metodo generale degli audit notturni. |
| `docs/it/audit/YYYY-MM-DD-*.md` | Report storico di una sessione specifica. |
| `tests/exploratory/nightly/README.md` | Come rilanciare gli scenari esplorativi. |
| `tests/exploratory/nightly/*.sh` | Script riproducibili per scenari reali. |

Gli script in `tests/exploratory/nightly` non sono ancora parte della suite
ufficiale. Sono materiale operativo: aiutano a ripetere cio' che e' stato fatto
la notte precedente e a decidere cosa promuovere a test stabile.

## Cosa non committare

I log completi generati da un audit non vanno committati nel repository, salvo
scelta esplicita. Possono diventare grandi, rumorosi e dipendenti dalla
macchina locale.

Nel repository devono entrare:

- report sintetici;
- comandi riproducibili;
- estratti di log significativi;
- link alle issue;
- script esplorativi.

I log completi possono restare:

- sotto `/tmp` durante l'audit;
- come archivio caricato su Google Drive e linkato nella issue madre;
- come allegati o estratti nelle issue GitHub quando sono piccoli o essenziali;
- come artifact di GitHub Actions in futuro, se l'audit verra' automatizzato.

## Caricare gli artifact su Google Drive

A fine audit, gli artifact locali possono essere compressi e caricati sul Google
Drive del maintainer. Questo evita di perdere i dettagli del run e permette di
riaprire il giorno dopo tutti i log senza doverli committare.

Prerequisiti:

- `rclone` installato;
- remote Google Drive configurato, per convenzione `gdrive:`;
- `zip` disponibile;
- `gh` autenticato se si vuole aggiornare automaticamente la issue madre.

Verifica del remote:

```sh
rclone listremotes
rclone lsd gdrive:
```

Comando consigliato:

```sh
tests/exploratory/nightly/upload_audit_artifacts.sh \
  --date 2026-06-25 \
  --issue 29
```

Lo script:

1. cerca le directory `/tmp/alfred-nightly-audit-YYYY-MM-DD*`;
2. crea `/tmp/alfred-nightly-audit-YYYY-MM-DD.zip`;
3. carica lo zip in `gdrive:alfred/audits/`;
4. genera un link condivisibile con `rclone link`;
5. se e' stato passato `--issue`, aggiunge un commento alla issue madre.

Per provare solo la creazione dello zip senza upload:

```sh
tests/exploratory/nightly/upload_audit_artifacts.sh \
  --date 2026-06-25 \
  --dry-run
```

Il link Drive va considerato parte del diario dell'audit: la issue madre resta
l'indice pubblico, mentre Drive conserva il pacchetto completo dei log.

## Quando trasformare un audit in test ufficiale

Uno scenario esplorativo va promosso a test ufficiale quando:

- protegge un contratto pubblico o interno stabile;
- ha un output atteso chiaro;
- non dipende troppo da timing o condizioni locali;
- ha gia' trovato un bug o protegge un bug risolto;
- e' utile a evitare regressioni.

Se lo scenario riguarda il contratto esterno strutturato, preferire un golden
test JSONL. Se riguarda un contratto interno tra moduli, preferire un test C
mirato. Se riguarda diagnostica storica o leggibilita' umana, puo' restare un
test sui log testuali.
