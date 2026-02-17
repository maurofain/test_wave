# Cloud API — Piano di implementazione (integrazione HTTP)

Data: 2026-02-12
Autore: GitHub Copilot

## Sommario esecutivo ✅
Aggiungere un componente `cloud_api` che gestisca autenticazione, keepalive, invio di pagamenti/servizi/attività e una coda locale persistente con retry/backoff. Obiettivo: rendere il dispositivo capace di comunicare in modo affidabile e sicuro con il server descritto in `docs/servizi_http.md`.

---

## 1) Requisiti funzionali (estratto)
- Autenticazione: POST `/api/login` (password = MD5(MMyyyydd + serial)).
- Keepalive periodico: POST `/api/keepalive`.
- Invio transazioni: `/api/payment`, `/api/paymentoffline`, `/api/serviceused`, `/api/deviceactivity`.
- Download risorse su richiesta: `/api/getconfig`, `/api/getimages`, `/api/getfirmware`, `/api/gettranslations`.
- Coda persistente per upload offline + retry con backoff.
- Interfaccia web per stato/diagnostica (queue size, last sync, forzature).

## 2) Architettura proposta
- Nuovo componente: `components/cloud_api/`
  - `cloud_api.h` / `cloud_api.c`
  - Task worker che processa la coda
  - Persistenza coda: SPIFFS (JSONL) con compaction
  - HTTP client: `esp_http_client` (HTTPS)
- Config estesa: `device_config.server` (url, enabled, serial, retry_policy,...)
- UI: estendere `web_ui` con pannello Server / Queue
- Integrazione: `main/tasks.c` avvia worker cloud_api

## 3) Flussi principali
- Avvio
  - Aspetta NTP; calcola password MD5; esegue POST `/api/login`; salva token in RAM/NVS
- Keepalive
  - Invio periodico; sul 401 => riesegui login; sul fallimento rete => log + retry
- Invio transazioni
  - Se rete disponibile => POST immediato
  - Se non raggiungibile => enqueue (persistente)
- Coda
  - Oggetti con fields: local_id, endpoint, payload, attempts, next_retry_ts
  - Retry esponenziale; max attempts → mark failed

## 4) Gestione errori & idempotenza
- 2xx → remove dalla coda
- 4xx → drop (salvo 429 → retry)
- 5xx / network → retry
- Includere `client_request_id` per idempotenza

## 5) Sicurezza
- HTTPS obbligatorio; supporto per certificate pinning (opzionale)
- Usare header `Date` (NTP time)
- Non esporre token su UI

## 6) API mappate (spec → implementare)
- Auth: `POST /api/login` (body: {serial, password}) → access_token
- Keepalive: `POST /api/keepalive` (device status)
- Payment/paymentoffline: `POST /api/payment`, `POST /api/paymentoffline`
- Service used: `POST /api/serviceused`
- Device activity: `POST /api/deviceactivity`
- Resource fetch: `POST /api/getconfig`, `POST /api/getimages`, `POST /api/getfirmware`, `POST /api/gettranslations`

> **Base server (for testing):** http://195.231.69.227:5556/
>
> **Full endpoint URLs (use these in requests)**
> - POST http://195.231.69.227:5556/api/login  — auth (body: {serial, password})
> - POST http://195.231.69.227:5556/api/keepalive  — device keepalive/status
> - POST http://195.231.69.227:5556/api/payment  — payment (online)
> - POST http://195.231.69.227:5556/api/paymentoffline  — payment offline
> - POST http://195.231.69.227:5556/api/serviceused  — service usage
> - POST http://195.231.69.227:5556/api/deviceactivity  — activity status
> - POST http://195.231.69.227:5556/api/getconfig  — request configuration
> - POST http://195.231.69.227:5556/api/getimages  — request images
> - POST http://195.231.69.227:5556/api/getfirmware  — request firmware
> - POST http://195.231.69.227:5556/api/gettranslations  — request translations/files
>
> Include sempre negli header `Date` (NTP time) e `Authorization: Bearer <token>` dopo il login. Se necessario inviare `client_request_id` per idempotenza.



## 7) Modifiche al codice (high-level)
- Nuovi file:
  - `components/cloud_api/cloud_api.c`, `cloud_api.h`
  - Tests: `components/cloud_api/test/*`
- Modifiche:
  - `components/device_config` → aggiungere `server` settings
  - `components/web_ui/web_ui.c` → pannello Server/Queue, pulsanti manuali
  - `main/tasks.c` → avviare `cloud_api` worker (keepalive)
- Documentazione: `docs/servizi_http_integration.md`

## 8) Persistence & queue details
- Formato: JSONL file append-only + periodic compaction
- Operazioni atomiche: write temp + rename
- Entry example:
```json
{ "id_local": "uuid", "endpoint": "/api/payment", "payload": {...}, "attempts":0, "next_retry": 0 }
```

## 9) Policy retry
- Backoff esponenziale: base 2s → *2 → max 1h
- Max attempts: 6 (configurabile)
- 429 → rispettare Retry-After header se presente

## 10) UI / diagnostica (web_ui)
- Stato connessione
- Last login / last keepalive (timestamp)
- Queue size / last queued item
- Pulsanti: Force-send queue, Clear failed, Manual login

## 11) Acceptance criteria (test di accettazione)
- Login automatico e persistenza token funzionante
- Keepalive inviato secondo intervallo configurato
- Pagamenti in assenza di rete vengono messi in coda e successivamente inviati
- Retry/backoff e marcatura failed applicati correttamente
- UI mostra stato e permette azioni manuali

## 12) Roadmap e stima (iterativa)
1. Scaffolding componente + config + UI placeholder — 0.5–1d
2. Auth + keepalive (NTP gating) — 1d
3. Persistent queue + invio payments/serviceused — 2d
4. Retry/backoff + error handling + idempotency — 1d
5. UI status + manual actions — 0.5d
6. Tests + docs + QA — 1d

_Tempo stimato totale: ~6–7 giorni uomo (dipende test/integrazione server)._ 

## 13) Test plan essenziale
- Unit: md5 calc, queue append/pop, serialize/deserialize
- Integration: mock server endpoints (simulate 200/5xx/429)
- Manual: simulate offline → queue → reconnect → verify sends

---

### Prossimi passi (se vuoi procedere ora)
- Creo gli issue/tasks dettagliati (file → funzione da modificare) e scaffoldo il componente `cloud_api` con interfacce base.
- Oppure procedo direttamente con l'implementazione del punto 1 (scaffolding).

Seleziona: `crea backlog` | `scaffold component` | `implementa auth/keepalive`