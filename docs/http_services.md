# Documentazione: `http_services.c`

## Scopo
Questo documento descrive l'implementazione e le funzioni presenti in `components/http_services/http_services.c` — un insieme di handler HTTP locali utilizzati per emulare/implementare le API riportate in `docs/servizi_http.md` e per i test locali del dispositivo.

---

## Panoramica architetturale 🔧
- Server HTTP: usa `esp_http_server` (ESP‑IDF).
- JSON: `cJSON` per parsing e serializzazione.
- Comportamento: endpoint `POST /api/*` con autenticazione basica via header `Authorization: Bearer <token>` e header `Date` richiesto.
- Token: memorizzato in RAM in `g_auth_token[256]` (implementazione di test: token == `serial`).
- Nota: molte route sono stub/implementazioni di test che restituiscono risposte di esempio (vedi TODO per hardening).

---

## Variabili globali
- `char g_auth_token[256]` — token corrente emesso da `/api/login` (stringa NUL-terminated).
- `static int s_next_payment_id` — contatore incrementale usato per `payment`/`paymentoffline` (inizia da 1000).
- `static int s_next_activity_id` — contatore per `activity` (inizia da 1).

---

## Funzioni principali (descrizione dettagliata)

### static void md5_hex(const unsigned char *input, size_t ilen, char *out_hex, size_t out_hex_len)
- Scopo: calcola l'MD5 e scrive la rappresentazione esadecimale in minuscolo (terminata NUL).
- Uso: verifica password in `api_login_post` (password = MD5(MMYYYYdd + serial)).
- Note: `out_hex_len` deve essere >= 33 (32 + '\0').

---

### static char *read_request_body(httpd_req_t *req)
- Firma: restituisce un buffer NUL-terminated allocato con `malloc` (caller deve `free`).
- Funzionalità: legge esattamente `req->content_len` byte tramite `httpd_req_recv` in loop.
- Errori: ritorna `NULL` su body vuoto o errore di ricezione.

---

### static void format_date_mm_yyyy_dd(char *out, size_t len)
- Scopo: restituisce data formattata come `MMYYYYdd` (usata per la generazione della password MD5 attesa).
- Limite: basata su `localtime_r()` (ora usata per generare la stringa attesa dal login).

---

### static void format_full_datetime(char *out, size_t len)
- Scopo: produce timestamp ISO UTC del tipo `YYYY-MM-DDTHH:MM:SS.000Z`.
- Nota: definita ma non usata in ogni punto (warning non bloccante).

---

### static esp_err_t api_login_post(httpd_req_t *req)
- Endpoint: `POST /api/login`
- Body JSON atteso: `{ "serial":"...", "password":"<md5>" }`
  - `password` deve essere MD5(MMYYYYdd + serial) (device date)
- Comportamento:
  - verifica JSON e campi;
  - calcola MD5 atteso e confronta con `password` fornita;
  - su successo: imposta `g_auth_token = serial` (token semplice per test) e risponde `{"iserror":false,"access_token":"<token>"}`;
  - su fallimento: risponde error JSON con `iserror:true` e `codeerror` appropriato.

---

### static esp_err_t api_keepalive_post(httpd_req_t *req)
- Endpoint: `POST /api/keepalive`
- Requisiti: `g_auth_token` deve essere impostato; l'handler ritorna 401 se non autorizzato.
- Risposta: JSON con `timestamp` (UTC ISO) e `access_token` (token attuale).

---

### static bool require_date_and_auth(httpd_req_t *req)
- Scopo: helper centrale per validare le intestazioni richieste dagli endpoint protetti.
- Controlli effettuati:
  - presenza dell'header `Date` (solo presenza; non viene validato il formato);
  - presenza e formato di `Authorization: Bearer <token>`;
  - confronto `token == g_auth_token`.
- Implementazione: usa `httpd_req_get_hdr_value_len()` e `httpd_req_get_hdr_value_str()` per leggere gli header in modo sicuro (alloca buffer temporanei e li libera).
- Restituisce: `true` se ok, `false` e invio di errore JSON in caso contrario.

---

### Endpoints di test / implementazioni di esempio
Per tutti gli endpoint protetti si chiama prima `require_date_and_auth()`.

- `POST /api/getimages` → ritorna elenco file + credenziali FTP di esempio.
- `POST /api/getconfig` → ritorna file di configurazione + FTP di esempio.
- `POST /api/gettranslations` → ritorna file di traduzioni + FTP.
- `POST /api/getfirmware` → ritorna file firmware + FTP.

- `POST /api/payment` → legge body (non persistente), restituisce `{"iserror":false,"datetime":"...","paymentid":<id>}`; `paymentid` incrementale atomico.
- `POST /api/serviceused` → restituisce `{"iserror":false,"datetime":"..."}`.
- `POST /api/paymentoffline` → simile a `payment` (id incrementale).

- `POST /api/getcustomers`
  - Body (opzionale): `{ "Code":"<code>|*" }` (nota: campo case-sensitive come implementato);
  - Risposta: array `customers` con esempio di record o entry vuota con `new:true`.

- `POST /api/getoperators` → ritorna lista operatori di test.

- `POST /api/activity` → registra/ritorna `activityid` incrementale e `datetime`.

- `esp_err_t http_services_register_handlers(httpd_handle_t server)`
  - Registra tutti gli handler `POST /api/*` con `httpd_register_uri_handler()`.

---

## Formati JSON comuni
- Errore standard: `{ "iserror": true, "codeerror": <http-like-code>, "deserror": "reason" }`
- Login OK: `{ "iserror": false, "access_token": "<token>" }`
- Keepalive OK: `{ "timestamp": "...Z", "access_token": "<token>" }`

---

## Esempi rapidi (curl)
- Login (ottieni token):

```bash
curl -s --http1.1 -X POST http://<IP>/api/login \
  -H 'Content-Type: application/json' \
  -d '{"serial":"SN123","password":"<md5_of_MMYYYYddSN123>"}'
# => {"iserror":false,"access_token":"SN123"}
```

- Keepalive / endpoint protetti (esempio `payment`):

```bash
curl -s --http1.1 -X POST http://<IP>/api/payment \
  -H 'Authorization: Bearer SN123' \
  -H 'Date: Sat, 15 Feb 2026 12:00:00 GMT' \
  -H 'Content-Type: application/json' \
  -d '{"amount":100}'
# => {"iserror":false,"datetime":"2026-02-15T12:00:00.000Z","paymentid":1000}
```

---

## Limitazioni note & suggerimenti di miglioramento ⚠️
- Token corrente è "debole" (uguale a `serial`) e viene memorizzato in RAM: sostituire con JWT/HMAC o token firmati + scadenza.
- `Date` header: attualmente solo presenza richiesta; consigliato validare skew (NTP) per prevenire replay.
- Mancano controlli approfonditi sull'input JSON (range, tipi, dimensione) — aggiungere validazione e limiti.
- `g_auth_token` non è protetto da mutex: se si prevede concorrenza reale, usare locking o una struttura thread-safe.
- Implementare persistenza per token/credenziali se necessario (NVS/Flash).
- Aggiungere test automatici (unit/integration) per tutte le route.

---

## TODO / roadmap
1. Sostituire token test con JWT + verifica server-side.
2. Validare `Date` (window ±N secondi) e richiedere NTP sync.
3. Sanitizzare e validare tutte le richieste in ingresso.
4. Aggiungere test end-to-end (ci + test device emulato).

---

## Riferimenti
- Implementazione: `components/http_services/http_services.c`
- Spec / API: `docs/servizi_http.md`, `docs/cloud_api_implementation_plan.md`

---

Autore: GitHub Copilot — generato il 2026-02-15
