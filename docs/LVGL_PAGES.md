Ecco i punti in cui il codice carica esplicitamente una pagina LVGL:

1. **Boot/logo & pannello wrapper** – [lvgl_panel_show()](cci:1://file:///home/mauro/1P/MicroHard/test_wave/components/lvgl_panel/lvgl_panel.c:78:0-87:1) richiama [lvgl_panel_show_boot_logo()](cci:1://file:///home/mauro/1P/MicroHard/test_wave/components/lvgl_panel/lvgl_panel.c:90:0-106:1), che prende il lock del display e invoca [panel_show_boot_logo_screen()](cci:1://file:///home/mauro/1P/MicroHard/test_wave/components/lvgl_panel/lvgl_panel.c:41:0-75:1) per pulire lo schermo e piazzare logo o fallback testuale.@components/lvgl_panel/lvgl_panel.c#49-108

2. **Pagina principale programmi** – `lvgl_page_main_show()` pulisce lo schermo, costruisce header/status/griglia pulsanti, sincronizza le traduzioni e avvia i timer di aggiornamento del pannello.@components/lvgl_panel/lvgl_page_main.c#1018-1055

3. **Schermata pubblicitaria/slideshow** – `lvgl_page_ads_show()` cancella lo schermo, prepara container & image view, carica immagini e avvia il timer di rotazione; viene richiamata anche al boot ([main.c](cci:7://file:///home/mauro/1P/MicroHard/test_wave/main/main.c:0:0-0:0)) e quando scade il timeout crediti.@components/lvgl_panel/lvgl_page_ads.c#163-220 @main/main.c#316-334

4. **Selezione lingua** – [lvgl_panel_show_language_select()](cci:1://file:///home/mauro/1P/MicroHard/test_wave/components/lvgl_panel/lvgl_panel.c:126:0-153:1) acquisisce il lock display e richiama `lvgl_page_language_show()`, che pulisce lo schermo, imposta lo sfondo e crea i bottoni delle lingue.@components/lvgl_panel/lvgl_panel.c#118-137 @components/lvgl_panel/lvgl_page_language.c#151-170

5. **Fuori servizio** – [lvgl_panel_show_out_of_service()](cci:1://file:///home/mauro/1P/MicroHard/test_wave/components/lvgl_panel/lvgl_panel.c:161:0-187:1) (con lock) chiama `lvgl_page_out_of_service_show()` per mostrare icona di warning e testo con conteggio reboot.@components/lvgl_panel/lvgl_panel.c#168-187 @components/lvgl_panel/lvgl_page_out_of_service.c#20-55

6. **Pagina programmi v2** – `lvgl_page_programs_v2_show()` pulisce lo schermo e costruisce header, griglia e footer alternativi; usata quando serve la nuova UI programmi.@components/lvgl_panel/lvgl_page_programs_v2.c#157-174

7. **Ritorno ad altre pagine** – alcune schermate richiamano direttamente la pagina principale (es. `on_ad_touch()` e `panel_apply_selected_language_async()` chiamano `lvgl_page_main_show()` dopo aver completato le rispettive azioni).@components/lvgl_panel/lvgl_page_ads.c#142-154 @components/lvgl_panel/lvgl_page_language.c#105-120