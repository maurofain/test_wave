# Ipotesi di refactor localizzazione

## Obiettivo
Valutare un refactor del sistema di localizzazione delle pagine web per rendere più semplice:

- la manutenzione delle pagine
- la ricostruzione automatica del markup
- il collegamento tra stringhe UI e catalogo i18n
- la verifica di coerenza tra codice, pagine e file lingua

## Stato attuale osservato
Il sistema attuale usa record JSON di localizzazione nel formato:

```json
{
  "scope": 3,
  "key": 3,
  "section": 0,
  "text": "Config"
}
```

Dove:

- `scope` identifica il gruppo logico o la pagina
- `key` identifica la singola voce
- `section` ordina eventuali frammenti della stessa stringa
- `text` contiene il testo finale

Nel codice, il recupero avviene tipicamente tramite una API logica come:

```c
device_config_get_ui_text_scoped("nav", "config", "Config", out, sizeof(out));
```

che internamente risolve:

- `scope` testuale -> `scope_id` numerico
- `key` testuale -> `key_id` numerico

per poi eseguire il lookup finale numerico.

## Esempio reale discusso
Per il bottone/menu di configurazione della web UI:

- uso logico nel codice: `("nav", "config")`
- mapping numerico: `scope = 3`, `key = 3`
- record italiano: `{ "scope": 3, "key": 3, "section": 0, "text": "Config" }`

Quindi il riferimento reale non dovrebbe essere il testo visibile `Config` o `Configurazione`, ma la coppia identificativa della voce.

## Problema di manutenzione attuale
Nelle pagine web il legame tra markup e localizzazione non è sempre esplicito dentro l'HTML/JS.

Questo comporta:

- maggiore difficoltà nel capire quale stringa corrisponde a quale record i18n
- difficoltà nel ricostruire automaticamente le pagine
- maggiore dipendenza dalla conoscenza del codice C/JS che genera il contenuto
- possibilità di fallback euristici o logiche implicite meno robuste

## Ipotesi 1: placeholder espliciti nelle pagine
Una prima ipotesi è introdurre placeholder direttamente nel markup, ad esempio:

```text
{{i18n:3:3}}
```

oppure forme equivalenti.

### Vantaggi
- mapping esplicito tra pagina e catalogo i18n
- maggiore facilità di ricostruzione automatica delle pagine
- verifica automatica più semplice
- eliminazione di inferenze sul testo visibile
- allineamento con il runtime che già usa ID numerici

### Svantaggi
- minore leggibilità umana dei sorgenti
- code review meno immediata
- necessità di tooling di supporto per capire a colpo d'occhio cosa rappresentano gli ID

## Valutazione sulla forma numerica
La forma numerica è stata considerata valida, soprattutto perché coerente con il motore attuale.

Esempio:

```text
{{i18n:3:3}}
```

### Perché può essere efficiente
- evita la conversione runtime da nome logico a ID
- rende il template deterministico
- semplifica i controlli automatici di validazione
- si presta a preprocessori e generatori

### Limite principale
La forma numerica pura non è autoesplicativa senza una mappa o tool di supporto.

## Necessità di utility di supporto
Per rendere sostenibile un approccio con placeholder numerici, servirebbero utility dedicate per:

- espansione dei placeholder in forma leggibile
- reverse lookup `scope/key -> nome logico -> testo italiano`
- validazione dei placeholder esistenti
- verifica che tutte le lingue abbiano i record richiesti
- individuazione di chiavi usate ma mancanti
- individuazione di chiavi definite ma mai referenziate

## Ipotesi 2: preprocessore di authoring
È stata discussa un'ipotesi più evoluta: usare una sintassi di authoring più leggibile, per esempio:

```text
{{i18n:'nav':'Nuovo testo che non abbiamo mai usato'}}
```

con un preprocessore incaricato di:

- cercare se la voce esiste già nello scope indicato
- riusare l'ID se la voce è già presente
- creare un nuovo ID se la voce non esiste
- generare l'output finale numerico o comunque canonico

## Vantaggi del preprocessore
- scrittura delle pagine più semplice
- minore necessità di conoscere gli ID numerici a mano
- onboarding più facile
- inserimento rapido di nuove stringhe statiche
- possibilità di automatizzare il mantenimento del catalogo

## Rischio principale del preprocessore
Il rischio maggiore è usare il testo italiano come identità permanente della voce.

Se il testo usato in authoring cambia nel tempo, ad esempio:

- da `Configurazione avanzata`
- a `Configurazione esperta`

il preprocessore potrebbe erroneamente creare una nuova chiave, causando:

- duplicati semantici
- perdita di continuità nelle traduzioni
- instabilità del catalogo i18n

## Principio raccomandato
Il testo italiano può essere usato come:

- seed iniziale
- default
- suggerimento per la creazione

ma **non** dovrebbe diventare l'identità stabile della voce.

L'identità stabile dovrebbe restare:

- una chiave canonica
- oppure direttamente l'ID numerico

## Strategia consigliata
La soluzione ritenuta più robusta è una pipeline in più fasi.

### Fase 1: authoring leggibile
L'autore scrive una forma comoda, ad esempio:

```text
{{i18n:'nav':'Nuovo testo'}}
```

### Fase 2: risoluzione preprocessore
Il tool:

- cerca la voce nello scope corretto
- la riusa se già esiste
- crea una nuova voce se manca
- aggiorna catalogo e mapping in modo esplicito

### Fase 3: normalizzazione
Il sistema converge verso una forma canonica stabile, ad esempio:

```text
{{i18n:3:512}}
```

oppure verso una chiave simbolica stabile, successivamente convertita in ID numerico.

## Variante utile
Una possibile distinzione sintattica potrebbe separare:

- riferimento a voce già esistente
- richiesta di creazione nuova voce

Esempio concettuale:

```text
{{i18n:3:3}}
{{i18n-new:'nav':'Nuovo testo'}}
```

Questo eviterebbe ambiguità tra uso normale e creazione automatica.

## Raccomandazione architetturale
Dal confronto è emersa questa valutazione:

- usare placeholder espliciti nelle pagine sarebbe un miglioramento netto
- usare placeholder numerici è coerente con il runtime ed efficiente
- usare solo placeholder numerici senza tool di supporto riduce troppo la leggibilità
- un preprocessore di authoring è utile se il testo libero è trattato solo come seed e non come identità definitiva
- il catalogo i18n deve restare stabile nel tempo

## Conclusione
La direzione più promettente è:

- authoring umano più semplice
- mapping esplicito nelle pagine
- risoluzione automatica verso ID numerici
- utility di verifica e ispezione
- stabilizzazione delle chiavi/ID dopo la prima creazione

In sintesi:

- **numerico per il motore**
- **sintassi più leggibile per l'autore**
- **tooling forte per espansione, verifica e normalizzazione**
