# COMPILE_FLAGS

Questo documento elenca i `#define` (e i `CONFIG_*` da `sdkconfig`) che influenzano la compilazione o i percorsi compilati nel progetto.

## Regola di manutenzione

Quando viene aggiunto, rimosso o modificato un flag di compilazione, aggiornare questo file nella stessa modifica.

## Flag custom progetto

### `COMPILE_APP`
- **File:** `app_version.h`, `main/app_version.h`
- **Valori:** `1` = build APP, `0` = build FACTORY
- **Impatto:** abilita/disabilita porzioni di codice con `#if COMPILE_APP` (es. `main/main.c`, `main/init.c`) e determina `COMPILE_MODE_LABEL` / `COMPILE_LOG_PREFIX`.

### `FORCE_VIDEO_DISABLED`
- **File:** `main/init.c`, `components/web_ui/web_ui.c`
- **Valori:** `1` disabilita forzatamente la parte video, `0` comportamento da configurazione runtime.
- **Impatto:** inibisce codice e/o comportamenti relativi a display/LVGL/touch e gestione web della parte video.

### `BOOT_COUNTER_RESET_DELAYED`
- **File:** `main/main.c`
- **Valori:** `1` reset contatore reboot posticipato, `0` immediato.
- **Impatto:** cambia il flusso compilato relativo alla finestra di stabilizzazione prima di `init_mark_boot_completed()`.

### `BOOT_COUNTER_STABLE_WINDOW_MS`
- **File:** `main/main.c`
- **Impatto:** costante compile-time per la finestra di stabilizzazione del boot counter (se `BOOT_COUNTER_RESET_DELAYED=1`).

### `DEBUG_DISABLE_SERVER_POST`
- **File:** `main/init.c`, `components/http_services/http_services.c`
- **Valori:** `1` inibisce i POST outbound al server, `0` comportamento normale.
- **Impatto:**
  - in `main/init.c`: salta il preboot crash-send (`try_send_pending_crash_record`)
  - in `http_services.c`: blocca il forwarding POST remoto (`remote_post`)

### `DEBUG_FORCE_CRASH_AT_BOOT`
- **File:** `main/main.c`
- **Valori:** `1` forza crash intenzionale a inizio boot, `0` disattivo.
- **Impatto:** dopo `error_log_init()` scrive marker nel log e forza un fault di memoria (panic) in `app_main()` per validare crash-log e boot-guard.

### `HTTP_SERVICES_LOG_TO_UI`
- **File:** `components/http_services/http_services.c`
- **Valori:** definito/non definito
- **Impatto:** include/esclude il logging esteso verso `web_ui_add_log` nei percorsi HTTP services.

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

### Crash dump / diagnostica
- `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`
- `CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF`

Impatto runtime associato:
- dopo mount SD, se esiste un core dump in partizione flash, viene trasferito direttamente su `/sdcard` con nome timestampato e metadati (mode/version/date);
- il core dump in partizione viene cancellato solo dopo scrittura SD completata correttamente;
- se SD non è montata o non ha spazio sufficiente, il trasferimento viene saltato e il core dump resta in partizione (nessuna saturazione SPIFFS).

### Test API Web UI
- `CONFIG_APP_MDB_UART_PORT`
- `CONFIG_APP_RS232_UART_PORT`
- `CONFIG_APP_RS485_UART_PORT`

### USB CDC scanner
- `CONFIG_USB_CDC_SCANNER_USE_CDC_ACM_HOST`
- `CONFIG_USB_CDC_SCANNER_VID`
- `CONFIG_USB_CDC_SCANNER_PID`
- `CONFIG_USB_CDC_SCANNER_DUAL_PID`
- `CONFIG_USB_OTG_SUPPORTED`

## Note

- Questo documento copre i flag che influenzano la compilazione nel codice applicativo del progetto (`main/` e componenti custom usati in questa codebase).
- I flag interni a componenti esterni/managed possono avere logiche aggiuntive non elencate qui.
