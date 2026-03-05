# Servire pagine HTTP dalla SD — Analisi e raccomandazioni

Data: 12 febbraio 2026

Questa pagina riassume vantaggi, svantaggi, considerazioni di performance e una proposta di implementazione per servire le pagine HTTP direttamente da file presenti sulla scheda SD del dispositivo.

## Sintesi
Servire le pagine web dalla SD offre benefici importanti per il workflow di sviluppo e per la manutenzione (deploy senza ricompilare, supporto ad asset di grandi dimensioni, sviluppo rapido), ma introduce vincoli operativi (dipendenza dalla SD all'avvio, latenza I/O, maggiore attenzione alla sicurezza). La soluzione più pratica è usare la SD per asset/statici non critici e mantenere nel firmware un piccolo set di pagine/endpoint di fallback.

---

## ✅ Vantaggi
- Aggiornamenti rapidi senza ricompilare/OTA: basta copiare file sulla SD.
- Sviluppo veloce: modifica HTML/CSS/JS direttamente sulla SD per debug immediato.
- Riduzione del firmware size (asset spostati fuori dal binario). 
- Supporto per asset grandi (video, immagini) senza occupare SPI flash o PSRAM.

## ⚠️ Svantaggi / rischi
- Affidabilità all'avvio: se la SD non è montata, il web server deve usare un fallback.
- Latenza I/O e prestazioni variabili rispetto a SPIFFS/flash.
- Maggior superficie di attacco: validazione dei path, permessi e upload sicuri.
- Bisogna gestire correttamente cache, compressione e content-type.

## Performance / affidabilità — raccomandazioni pratiche
- Conservare in firmware le pagine critiche (status, landing di emergenza) come fallback.
- Usare caching HTTP (Cache-Control) e servire file gzip precompressi (.gz) quando possibile.
- Verificare `sd_card_is_mounted()` prima di tentare l'accesso ai file sulla SD.
- Evitare letture sincrone lunghe nel thread della webserver: usare streaming/sendfile e buffer limitati.

## Sicurezza e manutenzione
- Proteggere da directory traversal (rifiutare percorsi contenenti `..`).
- Validare e sanificare i file upload (tipo MIME, dimensione, estensione).
- Implementare una logica di ‘version lock’ sulla SD (file `VERSION` o `manifest.json`) per verificare compatibilità app/web.
- Limitare gli endpoint che possono scrivere sulla SD (solo API admin autenticate).

## Strategia consigliata (pattern di implementazione)
1. Montare la SD all'avvio (già supportato nel progetto).
2. Cerca il file richiesto sotto `/sdcard/www/<path>` (o percorso configurabile).
3. Se il file è presente e leggibile → servirlo.
4. Altrimenti → servire la risorsa embedded nel firmware (fallback).
5. Supportare `.gz` precompressi: se `file.js.gz` esiste e l'header `Accept-Encoding` contiene `gzip`, servire il `.gz` con `Content-Encoding: gzip`.
6. Proteggere da path traversal, limitare mime types riconosciuti e gestire cache-control.

### Pseudocodice (handler concettuale)
```c
if (sd_card_is_mounted() && file_exists("/sdcard/www" + req_path)) {
  FILE *f = fopen(...);
  httpd_resp_set_type(req, content_type_for_path(req_path));
  httpd_resp_sendfile(req, f);
  fclose(f);
} else {
  // fallback embedded
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, embedded_index_html, strlen(embedded_index_html));
}
```

## File da spostare / priorità
- Alta priorità su SD: `index.html`, `config` page HTML/JS/CSS, file JS/CSS minificati, asset multimediali grandi.
- Tenere in firmware: API JSON (status, config save), pagine di fallback, endpoint di amministrazione critici.

## Export pagine embedded in HTML
Per rigenerare i file `.html` esterni a partire dalle costanti embedded (`WEBPAGE_*`) usare lo script:

```bash
python3 scripts/export_embedded_pages.py --output data/www
```

Output generato (default):
- `index.html`, `config.html`, `ota.html`
- `stats.html`, `tasks.html`, `httpservices.html`, `api.html`
- `files.html`, `logs.html`, `test.html`, `programs.html`, `emulator.html`

Opzioni utili:
- `--pages index.html,config.html` esporta solo alcune pagine
- `--config-read-only` aggiunge lo script read-only nella pagina config
- `--show-factory-password-section` / `--no-show-factory-password-section` imposta la variabile JS di sezione password factory

## Miglioramenti opzionali
- Endpoint admin per sincronizzare/validare la versione web sulla SD (`/api/admin/websync`).
- Script di deploy (pc → copia su SD) e validazione (checksum/manifest).
- Meccanismo di rollback automatico se la versione web sulla SD è corrotta.

---

## Conclusione — raccomandazione
Usare la SD per i file statici del sito è vantaggioso e semplifica lo sviluppo e gli aggiornamenti. Implementare sempre un fallback embedded per le pagine critiche e aggiungere controlli di sicurezza e caching per mitigare latenza e rischi.

---

Se vuoi, implemento subito il prototipo nel `web_ui` per: 
- servire `/` e `/config` da `/sdcard/www/` con fallback automatico; 
- supporto `.gz` e check `sd_card_is_mounted()`.

Rispondi con "Procedi" per creare la PR/patch. 
