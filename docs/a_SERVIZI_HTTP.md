**api\login - POST**

Richiesta token per chiamate successive 

## Aggiornamento implementazione (2026-03-26)

- Gli endpoint `POST /api/*` di integrazione cloud sono registrati nel componente `http_services` (`components/http_services/http_services.c`).
- Gli endpoint file manager remoti (`/api/remote/files/*`) sono registrati nel componente `web_ui` (`components/web_ui/web_ui.c`), non in `http_services`.
- Flusso applicativo attivo:
  - lettura scanner -> `http_services_getcustomers(...)`
  - avvio programma (`FSM` `CREDIT->RUNNING`) -> `http_services_payment(...)`
  - nessuna `payment` su cambio programma (`PROGRAM_SWITCH`).
- Login remoto usa il `serial` runtime e ricalcola sempre la password come `MD5(serial + MMyyyydd)` usando la data UTC corrente del device; il campo password in configurazione resta solo per compatibilità UI/debug.
- Se `DNA_SERVER_POST=1`, i POST remoti sono disabilitati (debug cloud).

## Endpoint remoti file manager (local device)

Questi endpoint eseguono sul device le stesse azioni della pagina `/files` e sono utili per client remoti (script, integrazioni, tool esterni).

- `GET /api/remote/files/list?storage=spiffs|sdcard`
  - Risposta: JSON con `storage` e array `files` (`name`, `size`).

- `POST /api/remote/files/upload?storage=spiffs|sdcard&name=<filename>`
  - Header: `Content-Type: application/octet-stream`
  - Body: contenuto binario del file.
  - Risposta: testo `ok` o errore.

- `POST /api/remote/files/delete`
  - Header: `Content-Type: application/json`
  - Body: `{"storage":"spiffs|sdcard","name":"<filename>"}`
  - Risposta: testo `ok` o errore.

- `GET /api/remote/files/download?storage=spiffs|sdcard&name=<filename>`
  - Risposta: stream binario con header `Content-Disposition` per download.

Note:
- `name` accetta solo filename semplici (niente path separator `/` o `\\`).
- `storage` supportato: `spiffs`, `sd`, `sdcard`.

## Schema di autenticazione — ottenimento dell'access_token 🔐

- Header richiesti
  - `Content-Type: application/json`
  - `Date: <ISO8601 con microsecondi e offset>` (es. `2026-01-23T13:25:13.218763+01:00`)

- Corpo (POST /api/login)
  - JSON: `{"serial":"<serial>","password":"<md5>"}`
  - `password` è ricalcolata dal firmware come `MD5(serial + MMyyyydd)` usando la data UTC corrente del device.

- Risposta
  - 200 OK con JSON: `{ "access_token": "<JWT>" }`
  - Il `access_token` va usato nelle richieste successive con header `Authorization: Bearer <access_token>`.

- Note implementative
  - Il server può rispondere con `Transfer-Encoding: chunked` oppure con `Content-Length` valido; il client deve raccogliere i dati dall'evento `HTTP_EVENT_ON_DATA` o leggere il buffer interno dopo `esp_http_client_perform()`.
  - Inviare sempre `Date` nel formato ISO usato dal firmware; il server può validare la data/time-drift.
  - Per evitare compressione, il client può usare `Accept-Encoding: identity`.

- Esempi
  - cURL (login):

    curl -H 'Content-Type: application/json' \
         -H 'Date: 2026-01-23T13:25:13.218763+01:00' \
         -d '{"serial":"AD-34-DFG-333","password":"c1ef6429c5e0f753ff24a114de6ee7d4"}' \
         http://195.231.69.227:5556/api/login

  - cURL (richiesta protetta):

    curl -H 'Authorization: Bearer <access_token>' http://195.231.69.227:5556/api/getconfig

```
> POST /api/login HTTP/1.1
> Host: 195.231.69.227:5556
> Content-Type: application/json
> Date: 2026-01-23T13:25:13.218763+01:00
> User-Agent: insomnia/12.1.0
> Accept: */*
> Content-Length: 83

| {
|     "serial":"AD-34-DFG-333",
|     "password":"c1ef6429c5e0f753ff24a114de6ee7d4"
| }

```



Body

{

"serial":"AD-34-DFG-333",

"password":"23c23b7278ef4a13a799ab0704e0f8c7"

}



Nell’header aggiungere parametro **Date** con la data del terminale



Password = MD5(serial + MMyyyydd)



|          | **Tipo** | **Note**                           |
| -------- | -------- | ---------------------------------- |
| serial   | string   | seriale                            |
| password | string   | password 		crittografata md5 |




Restituisce

{

"access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOjEsIm5hbWUiOiJBRC0zNC1ERkctMzMzIiwiZXhwIjoxNzY0OTg5ODAxfQ.EaUjton4BvqSoYZjAvs4nGP_1B3yUP29SfzZXSGC0Nw"

}





 


Invio keepalive terminali 

Body

{

  "status": "OK",

  "inputstates": "00010001",

  "outputstates":"00000000",

  "temperature": 2000,

  "humidity": 4700,

  "subdevices":[{"code":"QRC","status":"NOT FOUND"},{"code":"PRN1","status":"OK"}]

}





|              | **Tipo** | **Note**                                       |
| ------------ | -------- | ---------------------------------------------- |
| status       | string   | ok 		warning 		alarm 		setup |
| inputstates  | string   |                                                |
| outputstates | string   |                                                |
| temperature  | int      |                                                |
| humidity     | int      |                                                |
| subdevices   | list     |                                                |
| code         | string   | codice 		dispositivo                     |
| status       | string   | Stati 		dispositivo                      |




Restituisce

|            | **Tipo**          | **Note**                             |
| ---------- | ----------------- | ------------------------------------ |
| iserror    | bool              | true 		se si verificano errori |
| codeerror  | int               | codice 		errore                |
| deserror   | string            | descrizione 		errore           |
| datetime   | date 		time | data 		ora GMT                 |
| activities | list              |                                      |
| activityid | int               | identificativo 		attività      |
| code       | string            | codice 		attività              |
| parameters | string            | parametri 		aggiuntivi         |





 



/* POST /api/deviceactivity - Device activity information */
static esp_err_t api_deviceactivity_post(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/deviceactivity");



Invio da parte del terminale dello stato di esecuzione delle attività 
|            | **Tipo** | **Note**                                             |
| ---------- | -------- | ---------------------------------------------------- |
| activityid | int      | Riferimento 		identificativo attività eseguita |
| status     | string   | Stato 		dell’esecuzione                        |




Restituisce

|           | **Tipo** | **Note**                             |
| --------- | -------- | ------------------------------------ |
| iserror   | bool     | true 		se si verificano errori |
| codeerror | int      | codice 		errore                |
| deserror  | string   | descrizione 		errore           |





 

**api\getimages - POST**

Richiesta immagini

Restituisce

|           | **Tipo**             | **Note**                             |
| --------- | -------------------- | ------------------------------------ |
| iserror   | bool                 | true 		se si verificano errori |
| codeerror | int                  | codice 		errore                |
| deserror  | string               | descrizione 		errore           |
| files     | array 		string | lista 		nomi file da scaricare |
| server    | string               | server 		ftp                   |
| user      | string               | user 		ftp                     |
| password  | string               | password 		ftp                 |
| path      | string               | percorso 		server ftp          |




**api\getconfig - POST**

Richiesta configurazione

Restituisce

|           | **Tipo**             | **Note**                             |
| --------- | -------------------- | ------------------------------------ |
| iserror   | bool                 | true 		se si verificano errori |
| codeerror | int                  | codice 		errore                |
| deserror  | string               | descrizione 		errore           |
| files     | array 		string | lista 		nomi file da scaricare |
| server    | string               | server 		ftp                   |
| user      | string               | user 		ftp                     |
| password  | string               | password 		ftp                 |
| path      | string               | percorso 		server ftp          |




**api\gettranslations - POST**

Richiesta file traduzioni

Restituisce

|           | **Tipo**             | **Note**                             |
| --------- | -------------------- | ------------------------------------ |
| iserror   | bool                 | true 		se si verificano errori |
| codeerror | int                  | codice 		errore                |
| deserror  | string               | descrizione 		errore           |
| files     | array 		string | lista 		nomi file da scaricare |
| server    | string               | server 		ftp                   |
| user      | string               | user 		ftp                     |
| password  | string               | password 		ftp                 |
| path      | string               | percorso 		server ftp          |




**api\getfirmware - POST**

Richiesta file firmware

Restituisce

|           | **Tipo**             | **Note**                             |
| --------- | -------------------- | ------------------------------------ |
| iserror   | bool                 | true 		se si verificano errori |
| codeerror | int                  | codice 		errore                |
| deserror  | string               | descrizione 		errore           |
| files     | array 		string | lista 		nomi file da scaricare |
| server    | string               | server 		ftp                   |
| user      | string               | user 		ftp                     |
| password  | string               | password 		ftp                 |
| path      | string               | percorso 		server ftp          |

**api\payment – POST**   			

Invio registrazione pagamenti online

Body

|              | **Tipo**          | **Note**                                                     |
| ------------ | ----------------- | ------------------------------------------------------------ |
| customer     | list              |                                                              |
| code         | string            | codice 		cliente                                       |
| telephone    | string            |                                                              |
| email        | string            |                                                              |
| name         | string            |                                                              |
| surname      | string            |                                                              |
| datetime     | date 		time | data 		ora GMT                                         |
| amount       | int               |                                                              |
| recharge     | bool              | indica 		se si tratta di una ricarica borsellino       |
| paymenttype  | string            | codice 		tipo pagamento (`CASH`, `CREC`, `BCOIN`, `COIN`, `SATI`, `VOUC`, `CASHL`) |
| paymentdata  | string            | dati 		aggiuntivi relativi al pagamento                |
| cashreturned | list              |                                                              |
| value        | int               | valore 		banconota\moneta                              |
| quantity     | int               | quantità                                                     |
| position     | int               | posizione                                                    |
| cashentered  | list              |                                                              |
| value        | int               | valore 		banconota\moneta                              |
| quantity     | int               | quantità                                                     |
| position     | int               | posizione                                                    |
| services     | list              | elenco 		servizi acquistati                            |
| code         | string            | codice 		servizio (`CWASH`, `CGATE`, `CLANE`, `CCLEAN`) |
| amount       | int               | importo 		servizio                                     |
| quantity     | int               | quantità                                                     |
| used         | bool              | utilizzato                                                   |




Restituisce

|           | **Tipo**          | **Note**                                      |
| --------- | ----------------- | --------------------------------------------- |
| iserror   | bool              | true 		se si verificano errori          |
| codeerror | int               | codice 		errore                         |
| deserror  | string            | descrizione 		errore                    |
| datetime  | date 		time | data 		ora GMT                          |
| paymentid | int               | identificativo 		del pagamento inserito |





 

**api\serviceused – POST**   			

Invio registrazione utilizzo servizi

Body

|          | **Tipo**          | **Note**                          |
| -------- | ----------------- | --------------------------------- |
| code     | string            | codice 		cliente            |
| datetime | date 		time | data 		ora gmt              |
| services | list              | elenco 		servizi acquistati |
| code     | string            | codice 		servizio           |
| amount   | int               | importo 		servizio          |
| quantity | int               | quantità                          |




Restituisce

|           | **Tipo**          | **Note**                             |
| --------- | ----------------- | ------------------------------------ |
| iserror   | bool              | true 		se si verificano errori |
| codeerror | int               | codice 		errore                |
| deserror  | string            | descrizione 		errore           |
| datetime  | date 		time | data 		ora GMT                 |

**api\paymentoffline – POST**   			

Invio registrazione pagamenti offline

Body

|              | **Tipo**          | **Note**                                                     |
| ------------ | ----------------- | ------------------------------------------------------------ |
| customer     | list              |                                                              |
| code         | string            | codice 		cliente                                       |
| telephone    | string            |                                                              |
| email        | string            |                                                              |
| name         | string            |                                                              |
| surname      | string            |                                                              |
| datetime     | date 		time | data 		ora GMT                                         |
| amount       | int               |                                                              |
| recharge     | bool              | indica 		se si tratta di una ricarica borsellino       |
| paymenttype  | string            | codice 		tipo pagamento (`CASH`, `CREC`, `BCOIN`, `COIN`, `SATI`, `VOUC`, `CASHL`) |
| paymentdata  | string            | dati 		aggiuntivi relativi al pagamento                |
| cashreturned | list              |                                                              |
| value        | int               | valore 		banconota\moneta                              |
| quantity     | int               | quantità                                                     |
| position     | int               | posizione                                                    |
| cashentered  | list              |                                                              |
| value        | int               | valore 		banconota\moneta                              |
| quantity     | int               | quantità                                                     |
| position     | int               | posizione                                                    |
| services     | list              | elenco 		servizi acquistati                            |
| code         | string            | codice 		servizio (`CWASH`, `CGATE`, `CLANE`, `CCLEAN`) |

Codici servizi usati nel payload `payment/services[].code` e `serviceused/services[].code`:

- `CWASH`: credito per car wash
- `CGATE`: credito usufruito per gate
- `CLANE`: credito usufruito per pista
- `CCLEAN`: credito usufruito per aspiratore

Codici `paymenttype` usati nel payload `payment`:

- `CASH`: contanti
- `CREC`: credit card
- `BCOIN`: bit coin
- `COIN`: gettoni
- `SATI`: Satispay
- `VOUC`: vaucher
- `CASHL`: cash less tessera o gettone
| amount       | int               | importo 		servizio                                     |
| quantity     | int               | quantità                                                     |
| used         | bool              | utilizzato                                                   |




Restituisce

|           | **Tipo**          | **Note**                                      |
| --------- | ----------------- | --------------------------------------------- |
| iserror   | bool              | true 		se si verificano errori          |
| codeerror | int               | codice 		errore                         |
| deserror  | string            | descrizione 		errore                    |
| datetime  | date 		time | data 		ora GMT                          |
| paymentid | int               | identificativo 		del pagamento inserito |





 

**api\getcustomers – POST**   			

richiesta dati cliente

Body

|           | **Tipo** | **Note**                                                    |
| --------- | -------- | ----------------------------------------------------------- |
| Code      | String   | Codice 		cliente – se  *  restituisce elenco completo |
| Telephone | String   |                                                             |




Restituisce

|           | **Tipo** | **Note**                                       |
| --------- | -------- | ---------------------------------------------- |
| iserror   | bool     | true 		se si verificano errori           |
| codeerror | int      | codice 		errore                          |
| deserror  | string   | descrizione 		errore                     |
| customers | list     |                                                |
| valid     | bool     |                                                |
| code      | string   | codice 		cliente                         |
| telephone | string   |                                                |
| email     | string   |                                                |
| name      | string   |                                                |
| surname   | string   |                                                |
| amount    | int      | Importo 		disponibile                    |
| new       | bool     | true: 		esiste ma non su questo impianto |




**api\getoperators – POST**   			

richiesta dati operatori

Body

|      | **Tipo** | **Note**                                                     |
| ---- | -------- | ------------------------------------------------------------ |
| Code | String   | Codice 		operatore – se  *  restituisce elenco completo |




Restituisce

|            | **Tipo** | **Note**                             |
| ---------- | -------- | ------------------------------------ |
| iserror    | bool     | true 		se si verificano errori |
| codeerror  | int      | codice 		errore                |
| deserror   | string   | descrizione 		errore           |
| operator   | list     |                                      |
| valid      | bool     |                                      |
| code       | string   | codice 		operatore             |
| username   | string   |                                      |
| pin        | string   |                                      |
| password   | string   |                                      |
| opeativity | string   |                                      |





 




**api\activity– POST**   			

Invio registrazione attività locali

Body

|             | **Tipo**          | **Note**                                                     |
| ----------- | ----------------- | ------------------------------------------------------------ |
| service     | string            | codice 		servizio                                      |
| opetator    | string            | codice 		operatore eventuale                           |
| datetime    | date 		time | data 		ora GMT                                         |
| activity    | string            | codice 		attività (inizio /fine autorizzato/non autorizzato) |
| subactivity | string            | codice 		sotto attività                                |




Restituisce

|           | **Tipo** | **Note**                             |
| --------- | -------- | ------------------------------------ |
| iserror   | bool     | true 		se si verificano errori |
| codeerror | int      | codice 		errore                |
| deserror  | string   | descrizione 		errore           |





 




**CODICE CLIENTE**

ULTIMI QUATTRO CARATTERI DEL SERIALE DEL TERMINALE   

GIORNI DALL’INIZIO DELL’ANNO + 100 

ULTIMI TRE CARATTERI DELL’ ANNO 

NUMERO DI SECONDI DALLA MEZZANOTTE





 





 