**api\login - POST**

Richiesta token per chiamate successive 

Body

{

"serial":"AD-34-DFG-333",

"password":"23c23b7278ef4a13a799ab0704e0f8c7"

}



Nell’header aggiungere parametro **Date** con la data del terminale



Password = MD5(MMyyyydd + serial)



|          | **Tipo** | **Note**                           |
| -------- | -------- | ---------------------------------- |
| serial   | string   | seriale                            |
| password | string   | password 		crittografata md5 |


 

Restituisce

{

"access_token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOjEsIm5hbWUiOiJBRC0zNC1ERkctMzMzIiwiZXhwIjoxNzY0OTg5ODAxfQ.EaUjton4BvqSoYZjAvs4nGP_1B3yUP29SfzZXSGC0Nw"

}


 


 

**api\keepalive - POST**

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


 


 


 

**api\deviceactivity - POST**

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
| paymenttype  | int               | codice 		tipo pagamento(elettronico/contanti/promozioni/test) |
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
| code         | string            | codice 		servizio                                      |
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
| paymenttype  | int               | codice 		tipo pagamento(elettronico/contanti/promozioni/test) |
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
| code         | string            | codice 		servizio                                      |
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


 


 


 


 