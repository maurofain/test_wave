# LVGL_PAGES

Mappa aggiornata delle pagine LVGL e dei punti di ingresso usati nel progetto.

## Pagine disponibili (API pubbliche)

Definite in `components/lvgl_panel/include/lvgl_panel_pages.h`:

- `lvgl_page_main_show()`
- `lvgl_page_main_refresh_texts()`
- `lvgl_page_main_deactivate()`
- `lvgl_page_language_show()`
- `lvgl_page_language_2_show(return_cb)`
- `lvgl_page_language_2_deactivate()`
- `lvgl_page_out_of_service_show(reboots)`
- `lvgl_page_out_of_service_show_message(message_key, fallback)`
- `lvgl_page_out_of_service_show_reason(reason_key, reason_fallback, agent_name)`
- `lvgl_page_ads_show()`
- `lvgl_page_ads_deactivate()`
- `lvgl_page_ads_preload_images()`
- `lvgl_page_ads_unload_images()`
- `lvgl_page_ads_set_error_message(msg)`

## Entry point pannello (`lvgl_panel.c`)

Le chiamate principali che visualizzano pagine sono:

- `lvgl_panel_show()`
  - inizializza i18n runtime LVGL
  - mostra pagina boot logo (`lvgl_panel_show_boot_logo()`)
- `lvgl_panel_show_main_page()` -> `lvgl_page_main_show()`
- `lvgl_panel_show_ads_page()` -> `lvgl_page_ads_show()`
- `lvgl_panel_show_language_select()` -> `lvgl_page_language_2_show(lvgl_page_main_show)`
- `lvgl_panel_show_out_of_service(...)` -> `lvgl_page_out_of_service_show(...)`
- `lvgl_panel_show_out_of_service_message(...)` -> `lvgl_page_out_of_service_show_message(...)`
- `lvgl_panel_show_out_of_service_reason(...)` -> `lvgl_page_out_of_service_show_reason(...)`

## Comportamento pagina programmi (main)

Implementata in `components/lvgl_panel/lvgl_page_programmi.c`.

`lvgl_page_main_show()`:

- riabilita i dispositivi pagamento lato pannello (`panel_enable_payment_devices_on_programs_open()`)
- inizializza la mappa digital I/O pulsanti
- invia sequenza init CCTalk asincrona
- ferma ADS/language timer e rimuove chrome precedente
- ricostruisce UI (status/header/griglia pulsanti)
- aggancia chrome (flag lingua)

### Nome programma usato dagli eventi

Il nome inviato negli eventi FSM (`event.text`) viene dalla tabella programmi runtime (`entry->name`), derivata da:

- runtime: `/spiffs/programs.json`
- seed: `data/programs.json`

Campi sorgente nome: `name_it`, `name_en`, `name_fr`, `name_de`, `name_es`.

## Comportamento pagina ADS

Implementata in `components/lvgl_panel/lvgl_page_ads.c`.

- slideshow immagini con timer rotazione
- preload immagini (`img*.jpg`) da SPIFFS/SD e decode in PSRAM
- pulsante uscita verso pagina programmi
- integrazione con chrome superiore

## Selezione lingua

- pagina consigliata: `lvgl_page_language_2_show(return_cb)`
- da pannello viene usata con fallback a `lvgl_page_main_show`
- la lingua runtime viene impostata da `lvgl_panel_set_runtime_language(...)`
- refresh testi globale: `lvgl_panel_refresh_texts()`

## Out Of Service (OOS)

Le pagine OOS sono dedicate e separate dalla main page:

- semplice con reboot counter: `lvgl_page_out_of_service_show(reboots)`
- con messaggio chiave/fallback: `lvgl_page_out_of_service_show_message(...)`
- con motivo+agente: `lvgl_page_out_of_service_show_reason(...)`

Queste schermate sono usate quando il sistema entra in `FSM_STATE_OUT_OF_SERVICE`.

## Relazione con FSM test mode

La FSM supporta `FSM_STATE_LVGL_PAGES_TEST` (`ACTION_ID_LVGL_TEST_ENTER/EXIT`) per test pagine.
In questa modalità gli eventi applicativi normali sono limitati/ignorati finché non avviene `EXIT`.

## Note operative

- Prima di `lv_obj_clean(...)` viene rimosso il chrome e resettato l'indev per evitare callback su oggetti invalidi.
- Le operazioni pagina sono protette da `bsp_display_lock(...)` nel wrapper `lvgl_panel_*`.
- Il refresh lingua runtime pannello (`user_language`) è separato dalla lingua backend web (`backend_language`).
