# COMPILE_FLAGS

Questo documento elenca i `#define` (e i `CONFIG_*` da `sdkconfig`) che influenzano la compilazione o i percorsi compilati nel progetto.

## Regola di manutenzione

Quando viene aggiunto, rimosso o modificato un flag di compilazione, aggiornare questo file nella stessa modifica.

## Flag DNA_* — posizione e gestione

Tutti i flag `DNA_*` (DoNotActivate) sono definiti in **`CMakeLists.txt` radice**
tramite `add_compile_definitions(...)` e sono propagati dal build system a
**tutti** i componenti e a `main/` come flag `-D` del compilatore.

Per attivare il mockup di un modulo: modificare il valore del flag corrispondente
da `0` a `1` nel blocco `add_compile_definitions()` di `CMakeLists.txt`.

Non occorre modificare `main/app_version.h`, né i `CMakeLists.txt` dei singoli
componenti.

## Flag custom progetto

### `COMPILE_APP`
- **File:** `main/app_version.h`
- **Valori:** `1` = build APP, `0` = build FACTORY
- **Impatto:** abilita/disabilita porzioni di codice con `#if COMPILE_APP` (es. `main/main.c`, `main/init.c`) e determina `COMPILE_MODE_LABEL` / `COMPILE_LOG_PREFIX`.

### `FORCE_VIDEO_DISABLED`
- **File:** `main/init.c`, `components/web_ui/web_ui.c`
- **Valori:** `1` disabilita forzatamente la parte video, `0` comportamento da configurazione runtime.
- **Impatto:** inibisce codice e/o comportamenti relativi a display/LVGL/touch e gestione web della parte video.

### `WEB_UI_PAGE_SOURCE`
- **File:** `CMakeLists.txt`, `components/web_ui/webpages.c`
- **Valori:**
  - `1` = pagine da SPIFFS (`/spiffs/www`)
  - `2` = pagine da SD (`/sdcard/www`)
- **Impatto:** seleziona la sorgente esterna delle pagine Web UI. Gli handler HTML leggono solo da filesystem (`/spiffs/www` o `/sdcard/www`) tramite `webpages_send_external_or_error(...)`.
- **Nota operativa:** il valore è definito tramite `F_WEB_UI_PAGE_SOURCE` nel `CMakeLists.txt` radice; per testare le pagine su SPIFFS impostare `F_WEB_UI_PAGE_SOURCE=1`.

### `WEB_UI_EXPORT_ON_BOOT`
- **File:** `CMakeLists.txt`, `components/web_ui/webpages.c`
- **Valori:** `0` = export seed disabilitato, `1` = export seed abilitato
- **Impatto:** quando `WEB_UI_PAGE_SOURCE` è esterno (`1` o `2`), all'avvio Web UI crea (se mancanti) pagine seed `.html` nella cartella source (`/spiffs/www` oppure `/sdcard/www`). Non sovrascrive file esistenti.

### `MINIMAL_I2C_EEPROM_BOOT`
- **File:** `main/app_version.h`, `main/main.c`
- **Valori:** `1` attiva boot minimale diagnostico, `0` boot normale.
- **Impatto:** subito dopo `init_i2c_and_io_expander()` in `app_main`, esegue solo:
  - `eeprom_24lc16_init()`
  - lettura EEPROM dei primi 16 byte (`0x0000..0x000F`)
  - log del dump HEX
  - stop intenzionale in loop (non prosegue verso `init_run_factory()` e non avvia i task)

### `BOOT_COUNTER_RESET_DELAYED`
- **File:** `main/main.c`
- **Valori:** `1` reset contatore reboot posticipato, `0` immediato.
- **Impatto:** cambia il flusso compilato relativo alla finestra di stabilizzazione prima di `init_mark_boot_completed()`.

### `BOOT_COUNTER_STABLE_WINDOW_MS`
- **File:** `main/main.c`
- **Impatto:** costante compile-time per la finestra di stabilizzazione del boot counter (se `BOOT_COUNTER_RESET_DELAYED=1`).

### `DNA_SERVER_POST`
- **File:** `main/init.c`, `components/http_services/http_services.c`
- **Valori:** `1` inibisce i POST outbound al server, `0` comportamento normale.
- **Impatto:**
  - in `main/init.c`: salta il preboot crash-send (`try_send_pending_crash_record`)
  - in `http_services.c`: blocca il forwarding POST remoto (`remote_post`)

### `DNA_LVGL_COUNTDOWN_BG`
- **File:** `components/lvgl_panel/lvgl_panel.c` (definito in `CMakeLists.txt` radice)
- **Valori:** `0` modalità barra verticale (default), `1` modalità sfondo countdown pieno
- **Impatto:**
  - `0`: visualizza la barra verticale a destra dell'area counter
  - `1`: rimuove la barra e usa un riempimento dell'intera area counter che decresce con il tempo residuo
  - in entrambi i casi, colore dinamico: verde oltre 30%, rosso a 30% o meno

### `DEBUG_FORCE_CRASH_AT_BOOT`
- **File:** `main/main.c`
- **Valori:** `1` forza crash intenzionale a inizio boot, `0` disattivo.
- **Impatto:** dopo `error_log_init()` scrive marker nel log e forza un fault di memoria (panic) in `app_main()` per validare crash-log e boot-guard.

### `HTTP_SERVICES_LOG_TO_UI`
- **File:** `components/http_services/http_services.c`
- **Valori:** definito/non definito
- **Impatto:** include/esclude il logging esteso verso `web_ui_add_log` nei percorsi HTTP services.

### `DNA_SHT40`
- **File:** `components/sht40/sht40.c`
- **Valori:** `1` = mockup attivo (nessun hardware I2C/SHT40), `0` = driver reale
- **Impatto:** quando impostato a `1`, le tre funzioni pubbliche del modulo
  (`sht40_init`, `sht40_read`, `sht40_is_ready`) vengono sostituite da versioni
  fittizie. `sht40_init` segna il sensore come pronto; `sht40_read` restituisce
  sempre T=25.0°C e H=50.0% senza accedere all'I2C.

### `DNA_IO_EXPANDER`
- **File:** `main/app_version.h`, `components/io_expander/io_expander.c`
- **Valori:** `1` = mockup attivo (nessun hardware I2C/FXL6408), `0` = driver reale
- **Impatto:** quando impostato a `1`, nessun bus I2C viene toccato; le variabili
  globali `io_output_state` / `io_input_state` vengono aggiornate localmente come
  farebbe il driver reale. `io_input_state` inizia a `0xFF` (tutti i pin in alto,
  come con pull-up attivi, incluso GPIO3). Utile per testare la logica applicativa
  che legge i pin senza hardware collegato.
  Per attivare, decommentare in `components/io_expander/CMakeLists.txt`:
  `target_compile_definitions(__idf_io_expander PRIVATE DNA_IO_EXPANDER=1)`

### `DNA_LED_STRIP`
- **File:** `main/app_version.h`, `components/led/led.c`
- **Valori:** `1` = mockup attivo (nessun hardware RMT/WS2812), `0` = driver reale
- **Impatto:** quando impostato a `1`, tutte le funzioni pubbliche del modulo LED
  vengono sostituite da versioni fittizie. `led_init` simula il conteggio LED da
  `CONFIG_APP_WS2812_LEDS`; `led_fill_color/set_pixel/breathe/rainbow/fade_in/fade_out`
  restituiscono ESP_OK senza toccare il bus RMT. `led_get_handle()` ritorna NULL.
  Per attivare, decommentare in `components/led/CMakeLists.txt`:
  `target_compile_definitions(__idf_led PRIVATE DNA_LED_STRIP=1)`

### `DNA_CCTALK`
- **File:** `main/app_version.h`, `components/cctalk/cctalk_driver.c`, `components/cctalk/cctalk.c`
- **Valori:** `1` = mockup attivo (nessuna UART CCtalk reale), `0` = driver reale
- **Impatto:** quando impostato a `0`, il driver CCtalk usa pin dedicati `GPIO20/GPIO21`
  su `CONFIG_APP_RS232_UART_PORT` con default seriale **9600, N, 8, 1**.
  Quando impostato a `1`, il driver viene simulato e non tocca hardware UART.

### `DNA_SD_CARD`
- **File:** `main/app_version.h`, `components/sd_card/sd_card.c`
- **Valori:** `1` = mockup attivo (nessun hardware reale), `0` = driver SD reale
- **Impatto:** quando impostato a `1`, tutte le funzioni pubbliche del modulo SD
  vengono sostituite da versioni fittizie che simulano una scheda da 32 MB con
  4 file di esempio (`mock1.txt`, `mock2.log`, `mock3.bin`, `mock4.cfg`).
  Le API coinvolte coprono **tutto** l'accesso filesystem SD:
  `sd_card_mount/unmount/read_file/write_file/list_dir/format`,
  `sd_card_fopen/fclose/fwrite/fread/fflush`,
  `sd_card_opendir/readdir/closedir/stat`,
  `sd_card_get_total_size/used_size/is_mounted/is_present`.
  I chiamanti (`init.c`, `error_log.c`, `web_ui_file_manager.c`) non devono
  apportare modifiche: tutto l'I/O SD avviene esclusivamente tramite le API
  del componente `sd_card`.


## Flag `CONFIG_*` usati nel codice applicativo

Questi simboli arrivano da `sdkconfig`/Kconfig e abilitano percorsi compilati con `#if/#ifdef`.

### Rete / init
- `CONFIG_APP_ETH_ENABLED`
- `CONFIG_APP_ETH_MDC_GPIO`
- `CONFIG_APP_ETH_MDIO_GPIO`
- `CONFIG_APP_ETH_PHY_ADDR`
- `CONFIG_APP_ETH_PHY_RST_GPIO`
- `CONFIG_APP_MDB_RX_GPIO`
- `CONFIG_BSP_I2C_NUM`

### Web UI / servizi
- `CONFIG_APP_HTTP_PORT`
- `CONFIG_USB_OTG_SUPPORTED`
- `CONFIG_ESP_MAIN_TASK_STACK_SIZE` (stack del task `main`, critico durante init/registrazione handler)
- `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`

### LVGL file/decoder logo boot
- `CONFIG_LV_USE_FS_STDIO`
- `CONFIG_LV_FS_STDIO_LETTER`
- `CONFIG_LV_FS_STDIO_PATH`
- `CONFIG_LV_USE_TJPGD`

Impatto runtime associato:
- abilita in LVGL il driver file `stdio` (mount VFS già disponibile su `/spiffs`), quindi percorsi come `S:/spiffs/logo.jpg`.
- abilita il decoder JPEG TJPGD usato dalla splash screen per mostrare `logo.jpg` dalla prima pagina.

### Crash dump / diagnostica
- `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`
- `CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF`

Impatto runtime associato:
- dopo mount SD, se esiste un core dump in partizione flash, viene trasferito direttamente su `/sdcard` con nome timestampato e metadati (mode/version/date);
- il core dump in partizione viene cancellato solo dopo scrittura SD completata correttamente;
- se SD non è montata o non ha spazio sufficiente, il trasferimento viene saltato e il core dump resta in partizione (nessuna saturazione SPIFFS).

### TAG: Serial ports
- `CONFIG_APP_MDB_UART_PORT`
- `CONFIG_APP_RS232_UART_PORT`
- `CONFIG_APP_RS485_UART_PORT`

Impatto runtime associato:
- RS232/RS485/MDB usano rispettivamente i `CONFIG_APP_*_UART_PORT` e la mappatura pin da `sdkconfig`.
- CCTALK usa UART `CONFIG_APP_RS232_UART_PORT` con pin dedicati `GPIO20/GPIO21` e default `9600, N, 8, 1`.

### Test API Web UI
- `CONFIG_APP_MDB_UART_PORT`
- `CONFIG_APP_RS232_UART_PORT`
- `CONFIG_APP_RS485_UART_PORT`

### Mappatura pin seriali (compile-time)
- `CONFIG_APP_RS232_TX_GPIO`
- `CONFIG_APP_RS232_RX_GPIO`
- `CONFIG_APP_RS485_TX_GPIO`
- `CONFIG_APP_RS485_RX_GPIO`
- `CONFIG_APP_RS485_DE_GPIO`
- `CONFIG_APP_MDB_TX_GPIO`
- `CONFIG_APP_MDB_RX_GPIO`

### USB CDC scanner
- `CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST`
- `CONFIG_USB_CDC_SCANNER_VID`
- `CONFIG_USB_CDC_SCANNER_PID`
- `CONFIG_USB_CDC_SCANNER_DUAL_PID`
- `CONFIG_USB_OTG_SUPPORTED`

## Note

- Questo documento copre i flag che influenzano la compilazione nel codice applicativo del progetto (`main/` e componenti custom usati in questa codebase).
- I flag interni a componenti esterni/managed possono avere logiche aggiuntive non elencate qui.
