# http_services

## Scopo
Questo documento descrive le chiamate previste dal file `servizi2.docx`, lo stato di implementazione nel componente `components/http_services`, e le strutture C usate per gestire le risposte.

## Endpoints previsti (spec)
- `POST /api/login`
- `POST /api/keepalive`
- `POST /api/deviceactivity`
- `POST /api/getimages`
- `POST /api/getconfig`
- `POST /api/gettranslations`
- `POST /api/getfirmware`
- `POST /api/payment`
- `POST /api/serviceused`
- `POST /api/paymentoffline`
- `POST /api/getcustomers`
- `POST /api/getoperators`
- `POST /api/activity` (presente nel documento sorgente come endpoint locale attività)

## Note protocollo
- Header richiesto: `Authorization: Bearer <token>` (dopo login).
- Header richiesto: `Date` con data/ora terminale.
- Login: password calcolata come `MD5(serial + MMyyyydd)` usando la data UTC corrente del device.

## Mappatura implementazione attuale
- Proxy remoto implementato per tutte le route sopra.
- Aggiunto anche handler locale esplicito per `POST /api/deviceactivity`.
- È mantenuta la route `POST /api/activity` per compatibilità con specifiche precedenti.
- Disponibili chiamate C dirette per integrazione task/FSM:
  - `http_services_getcustomers(code, telephone, out)`
  - `http_services_payment(customer, amount, service_code, out)`
- Flusso runtime attivo in `main/tasks.c`:
  - evento scanner QR -> `getcustomers`
  - cache ultimo customer valido
  - transizione FSM `CREDIT -> RUNNING` su `PROGRAM_SELECTED` -> chiamata `payment`
  - nessuna chiamata `payment` su `PROGRAM_SWITCH`.

## Stato corrente (2026-03-26)

- Handler registrati dal componente `http_services`: `POST /api/login`, `POST /api/keepalive`, `POST /api/getimages`, `POST /api/getconfig`, `POST /api/gettranslations`, `POST /api/getfirmware`, `POST /api/payment`, `POST /api/serviceused`, `POST /api/paymentoffline`, `POST /api/getcustomers`, `POST /api/getoperators`, `POST /api/activity`, `POST /api/deviceactivity`.
- Le chiamate `getimages/getconfig/gettranslations/getfirmware` eseguono anche sync FTP locale su SPIFFS se la risposta remota è OK.
- Le API C dirette `http_services_getcustomers(...)` e `http_services_payment(...)` forzano login remoto (`http_services_login_if_needed(true)`) e includono gestione retry su `401/403` per `getcustomers`.
- Login remoto: usa il `serial` runtime in `device_config.server` e ricalcola automaticamente la password MD5 prima di ogni `POST /api/login`.
- In presenza del flag `DNA_SERVER_POST=1` i POST remoti sono inibiti (modalità debug/simulazione cloud).

## Strutture C di risposta
Definite in `components/http_services/include/http_services.h`.

```c
typedef struct {
    bool iserror;
    int32_t codeerror;
    char deserror[128];
} http_services_common_response_t;

typedef struct {
    int32_t activityid;
    char code[64];
    char parameters[256];
} http_services_activity_t;

typedef struct {
    http_services_common_response_t common;
    char datetime[40];
    size_t activity_count;
    http_services_activity_t activities[16];
} http_services_keepalive_response_t;

typedef struct {
    http_services_common_response_t common;
    size_t file_count;
    char files[32][64];
    char server[128];
    char user[64];
    char password[64];
    char path[128];
} http_services_filelist_response_t;

typedef struct {
    http_services_common_response_t common;
    char datetime[40];
    int32_t paymentid;
} http_services_payment_response_t;

typedef struct {
    http_services_common_response_t common;
    char datetime[40];
} http_services_serviceused_response_t;

typedef struct {
    bool valid;
    char code[64];
    char telephone[64];
    char email[128];
    char name[64];
    char surname[64];
    int32_t amount;
    bool is_new;
} http_services_customer_t;

typedef struct {
    http_services_common_response_t common;
    size_t customer_count;
    http_services_customer_t customers[32];
} http_services_getcustomers_response_t;

typedef struct {
    bool valid;
    char code[64];
    char username[64];
    char pin[64];
    char password[64];
    char operativity[64];
} http_services_operator_t;

typedef struct {
    http_services_common_response_t common;
    size_t operator_count;
    http_services_operator_t operators[32];
} http_services_getoperators_response_t;
```

## Dettaglio chiamate e payload

### 1) `POST /api/login`
Richiede token per chiamate successive.

Body richiesto:
```json
{
  "serial": "AD-34-DFG-333",
  "password": "<md5(serial+MMyyyydd)>"
}
```

Response attesa:
```json
{
  "access_token": "<token>"
}
```

### 2) `POST /api/keepalive`
Invio heartbeat terminale e stato periferiche.

Body richiesto (schema):
```json
{
  "status": "OK|WARNING|ALARM|SETUP",
  "inputstates": "00010001",
  "outputstates": "00000000",
  "temperature": 2000,
  "humidity": 4700,
  "subdevices": [
    {"code": "QRC", "status": "NOT FOUND"},
    {"code": "PRN1", "status": "OK"}
  ]
}
```

### Aggiornamento payload `payment` / `paymentoffline`

- `paymenttype` viene inviato come stringa e non piu come intero.
- I valori supportati sono: `CASH`, `CREC`, `BCOIN`, `COIN`, `SATI`, `VOUC`, `CASHL`.
- `services[].code` usa un enum chiuso con questi valori: `CWASH`, `CGATE`, `CLANE`, `CCLEAN`.
- `datetime` nel body `payment` deve essere serializzato in UTC ISO8601 con suffisso `Z` (esempio `2026-04-14T11:05:12.717598Z`); il backend rifiuta il formato con offset esplicito `+00:00`.
- Nel runtime attuale la FSM propaga il tipo pagamento verso `http_services`; il codice servizio viene normalizzato verso i codici del server.

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": "",
  "datetime": "...",
  "activities": [
    {"activityid": 1, "code": "...", "parameters": "..."}
  ]
}
```

### 3) `POST /api/deviceactivity`
Invio attività eseguita dal terminale.

Body (schema):
```json
{
  "activityid": 1,
  "status": "OK"
}
```

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": ""
}
```

### 4) `POST /api/getimages`
### 5) `POST /api/getconfig`
### 6) `POST /api/gettranslations`
### 7) `POST /api/getfirmware`
Stesso schema risposta file-list + credenziali server.

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": "",
  "files": ["file1", "file2"],
  "server": "...",
  "user": "...",
  "password": "...",
  "path": "..."
}
```

### 8) `POST /api/payment`
### 9) `POST /api/paymentoffline`
Invio pagamento online/offline.

Body include cliente, importi, contanti inseriti/resto, servizi acquistati.

Nota implementativa attuale (`http_services_payment`):
- `paymenttype` impostato a `CASH`
- `datetime` inviato in formato UTC ISO8601 con suffisso `Z`
- `paymentdata` impostato a stringa vuota
- `cashentered` valorizzato con una voce (`value=amount`, `quantity=1`, `position=99`) se `amount > 0`
- `services[0]` valorizzato con `code=service_code`, `amount`, `quantity=1`, `used=true`, `recharge=false`.

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": "",
  "datetime": "...",
  "paymentid": 1234
}
```

### 10) `POST /api/serviceused`
Invio utilizzo servizi.

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": "",
  "datetime": "..."
}
```

### 11) `POST /api/getcustomers`
Richiesta dati clienti (`Code` specifico o `*`).

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": "",
  "customers": [
    {
      "valid": true,
      "code": "...",
      "telephone": "...",
      "email": "...",
      "name": "...",
      "surname": "...",
      "amount": 0,
      "new": false
    }
  ]
}
```

### 12) `POST /api/getoperators`
Richiesta dati operatori (`Code` specifico o `*`).

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": "",
  "operators": [
    {
      "valid": true,
      "code": "...",
      "username": "...",
      "pin": "...",
      "password": "...",
      "operativity": "..."
    }
  ]
}
```

### 13) `POST /api/activity`
Invio attività locali (compatibilità).

Body (schema):
```json
{
  "service": "...",
  "operator": "...",
  "datetime": "...",
  "activity": "...",
  "subactivity": "..."
}
```

Response (schema):
```json
{
  "iserror": false,
  "codeerror": 0,
  "deserror": ""
}
```

## Nota su payload stringa generica (`deviceactivity`)
Per eventi critici (es. crash pre-boot) è previsto l'invio tramite `deviceactivity` usando il campo stringa generico nel payload; va verificato il limite massimo lato server e applicato troncamento controllato lato device se necessario.

## File coinvolti
- `components/http_services/include/http_services.h`
- `components/http_services/http_services.c`
- `main/tasks.c` (trigger runtime `getcustomers`/`payment`)
- `docs/http_services.md`
