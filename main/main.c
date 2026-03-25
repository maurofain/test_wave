#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "init.h"
#include "tasks.h"
#include "lvgl_panel.h"
#include "lvgl_panel_pages.h"
#include "cctalk.h"
#include "modbus_relay.h"
#include "app_version.h"
#include "device_config.h"
#include "error_log.h"
#include "eeprom_24lc16.h"
#include "web_ui_programs.h"
/// @brief Abilita l'invio dei log a un server remoto via HTTP per debug.
bool send_http_log = false;
/// @brief Abilita il dump dei log CCTalk in seriale per debug.
bool dump_cctalk_log = false;
/// @brief Abilita log API (GET/POST) temporaneo
bool enable_api_log = false;

static const char *TAG = "APP";
#define LOG_CTX_PREFIX "[" COMPILE_LOG_PREFIX "]"

/*
 * Policy azzeramento contatore reboot consecutivi.
 * 0 = reset immediato dopo tasks_start_all()
 * 1 = reset posticipato dopo finestra di stabilità
 */
#define BOOT_COUNTER_RESET_DELAYED 1
#define BOOT_COUNTER_STABLE_WINDOW_MS (10000)

/*
 * Debug: forza un crash subito al boot per validare il flusso crash-log/boot-guard.
 * 0 = disattivo, 1 = attivo
 */
#define DEBUG_FORCE_CRASH_AT_BOOT 0

/*
 * Policy log runtime iniziale: minima verbosità al boot.
 *
 * Regola: livello globale = ERROR all'avvio.
 * In seguito il livello può essere rialzato dinamicamente via codice/API.
 */

/**
 * @brief Applica la politica di log del boot.
 *
 * Questa funzione si occupa di configurare e attivare la politica di log
 * relativa al boot del sistema. La politica di log definisce come vengono
 * gestiti i log generati durante il processo di avvio.
 *
 * @return Niente.
 */
static void apply_boot_log_policy(void)
{
#if COMPILE_APP
    esp_log_level_set("*", ESP_LOG_INFO);
#else
    esp_log_level_set("*", ESP_LOG_ERROR);
#endif

    /* Stack USB/CDC: forza ERROR al boot per evitare flood */
    esp_log_level_set("usb", ESP_LOG_ERROR);
    esp_log_level_set("USB", ESP_LOG_ERROR);
    esp_log_level_set("usb_host", ESP_LOG_ERROR);
    esp_log_level_set("cdc_acm", ESP_LOG_ERROR);
}

/*
 * Dopo il boot, abilitiamo INFO per vedere lo stato operativo,
 * mantenendo USB/CDC più silenziosi per evitare flood.
 */

/**
 * @brief Applica la politica di registrazione dei log post-boot.
 *
 * Questa funzione si occupa di configurare e attivare la politica di registrazione dei log
 * che vengono generati dopo il completamento del processo di avvio del sistema.
 *
 * @return Niente.
 */
static void apply_post_boot_log_policy(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("usb", ESP_LOG_WARN);
    esp_log_level_set("USB", ESP_LOG_WARN);
    esp_log_level_set("usb_host", ESP_LOG_WARN);
    esp_log_level_set("cdc_acm", ESP_LOG_WARN);
    /* httpd: ECONNRESET (104) e ENOBUFS (113) sono transitori (browser refresh),
     * non indicano errori applicativi — sopprimi il flood di W/E al livello ERROR */
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_uri",  ESP_LOG_ERROR);
    esp_log_level_set("httpd",      ESP_LOG_ERROR);
}


/**
 * @brief Potenzialmente forza un crash all'avvio del sistema.
 *
 * Questa funzione controlla se è necessario forzare un crash all'avvio del sistema.
 * Se la condizione di forzatura del crash è soddisfatta, il sistema verrà forzato a crashare.
 *
 * @return Nessun valore di ritorno.
 */
static void maybe_force_crash_at_boot(void)
{
#if DEBUG_FORCE_CRASH_AT_BOOT
    char debug_msg[192];
    snprintf(debug_msg,
             sizeof(debug_msg),
             "[DEBUG_FORCE_CRASH_AT_BOOT] trigger requested | mode=%s version=%s date=%s\n",
             COMPILE_MODE_LABEL,
             APP_VERSION,
             APP_DATE);
    error_log_write_msg(debug_msg);

    ESP_LOGE(TAG,
             LOG_CTX_PREFIX " DEBUG_FORCE_CRASH_AT_BOOT attivo: crash intenzionale in corso | mode=%s version=%s date=%s",
             COMPILE_MODE_LABEL,
             APP_VERSION,
             APP_DATE);

    snprintf(debug_msg,
             sizeof(debug_msg),
             "[DEBUG_FORCE_CRASH_AT_BOOT] forcing panic fault now | mode=%s version=%s date=%s\n",
             COMPILE_MODE_LABEL,
             APP_VERSION,
             APP_DATE);
    error_log_write_msg(debug_msg);

    error_log_write_msg("[DEBUG_FORCE_CRASH_AT_BOOT] stack/backtrace completo non viene scritto in error_*.log; verra' salvato nel coredump su SD al boot successivo\n");

    esp_err_t mark_ret = init_mark_forced_crash_request();
    if (mark_ret != ESP_OK)
    {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " impossibile registrare marker force_crash: %s", esp_err_to_name(mark_ret));
    }

    vTaskDelay(pdMS_TO_TICKS(150));
    volatile uint32_t *bad_ptr = (volatile uint32_t *)0x0;
    *bad_ptr = 0xDEADCAFE;
#endif
}


/**
 * @brief Invia la sequenza di inizializzazione CCTalk al dispositivo gettoniera.
 * 
 * Verifica che l'interfaccia CCTalk sia abilitata nella configurazione e invia
 * una sequenza di 4 comandi di inizializzazione al dispositivo (indirizzo 0x02):
 * 1. Address Poll (verifica presenza dispositivo)
 * 2. Modify Inhibit Status (abilita tutti i 16 canali)
 * 3. Modify Master Inhibit std (abilita accettazione globale)
 * 4. Request Inhibit Status (verifica stato inibizioni)
 * 
 * @return void Non restituisce alcun valore.
 */
void main_cctalk_send_initialization_sequence(void)
{
    device_config_t *cfg = device_config_get();
    if (!cfg || !cfg->sensors.cctalk_enabled) {
        ESP_LOGD(TAG, LOG_CTX_PREFIX " [M] Sequenza init CCTalk skip (disabilitato da config)");
        return;
    }

    const uint8_t dest_addr = 0x02;
    const uint32_t timeout_ms = 1000;
    
    ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Inizio sequenza di inizializzazione CCTalk...");
    
    // Comando 1: Address Poll (header 254 / 0xFE)
    // Verifica che la gettoniera sia presente e risponda
    if (cctalk_address_poll(dest_addr, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd1 - Address Poll: OK");
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd1 - Address Poll: FAIL");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // Comando 2: Modify Inhibit Status (header 231 / 0xE7)
    // Abilita tutti i 16 canali (mask 0xFF 0xFF)
    if (cctalk_modify_inhibit_status(dest_addr, 0xFF, 0xFF, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd2 - Modify Inhibit Status (all channels): OK");
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd2 - Modify Inhibit Status: FAIL");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // Comando 3: Modify Master Inhibit std (header 228 / 0xE4)
    // Abilita accettazione globale delle monete
    if (cctalk_modify_master_inhibit_std(dest_addr, true, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd3 - Modify Master Inhibit (accept enabled): OK");
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd3 - Modify Master Inhibit: FAIL");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // Comando 4: Request Inhibit Status (header 230 / 0xE6)
    // Legge e verifica lo stato corrente delle inibizioni
    uint8_t mask_low = 0, mask_high = 0;
    if (cctalk_request_inhibit_status(dest_addr, &mask_low, &mask_high, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd4 - Request Inhibit Status: OK (mask=0x%02X%02X)", mask_high, mask_low);
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd4 - Request Inhibit Status: FAIL");
    }

    ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Sequenza CCTalk completata");
}

/* Background task used to perform the blocking initialization sequence so
 * UI code can continue without waiting for the CCTalk timeouts. */
static void main_cctalk_init_task(void *pv)
{
    (void)pv;
    main_cctalk_send_initialization_sequence();
    vTaskDelete(NULL);
}

void main_cctalk_send_initialization_sequence_async(void)
{
    device_config_t *cfg = device_config_get();
    if (!cfg || !cfg->sensors.cctalk_enabled) {
        ESP_LOGD(TAG, LOG_CTX_PREFIX " [M] Sequenza init CCTalk async skip (disabilitato da config)");
        return;
    }

    BaseType_t r = xTaskCreate(main_cctalk_init_task, "cctalk_init", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    if (r != pdPASS) {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Creazione task sequenza CCTalk fallita");
    }
}

/* Background task used to start the CCTalk acceptor without blocking UI */
static void main_cctalk_start_acceptor_task(void *pv)
{
    (void)pv;
    esp_err_t start_err = cctalk_driver_start_acceptor();
    if (start_err != ESP_OK) {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " cctalk_driver_start_acceptor failed: %s", esp_err_to_name(start_err));
    } else {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " cctalk_driver_start_acceptor OK");
    }
    vTaskDelete(NULL);
}

/**
 * @brief Esegue un probe Modbus al boot: init + lettura porte configurate.
 *
 * @param cfg Configurazione device corrente.
 * @return ESP_OK se il probe è valido oppure se Modbus è disabilitato.
 */
static esp_err_t main_boot_probe_modbus(const device_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    bool modbus_enabled = (cfg->sensors.rs485_enabled && cfg->modbus.enabled);
    if (!modbus_enabled) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Probe Modbus skip (disabilitato da config)");
        return ESP_OK;
    }

    esp_err_t init_err = modbus_relay_init();
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, LOG_CTX_PREFIX " [M] Probe Modbus init fallita: %s", esp_err_to_name(init_err));
        return init_err;
    }

    uint8_t bit_buf[MODBUS_RELAY_MAX_BYTES] = {0};

    uint16_t relay_count = cfg->modbus.relay_count;
    if (relay_count > MODBUS_RELAY_MAX_POINTS) {
        relay_count = MODBUS_RELAY_MAX_POINTS;
    }
    if (relay_count > 0) {
        esp_err_t relay_err = modbus_relay_read_coils(cfg->modbus.slave_id,
                                                      cfg->modbus.relay_start,
                                                      relay_count,
                                                      bit_buf,
                                                      sizeof(bit_buf));
        if (relay_err != ESP_OK) {
            ESP_LOGE(TAG, LOG_CTX_PREFIX " [M] Probe Modbus lettura relè fallita: %s", esp_err_to_name(relay_err));
            return relay_err;
        }
    }

    uint16_t input_count = cfg->modbus.input_count;
    if (input_count > MODBUS_RELAY_MAX_POINTS) {
        input_count = MODBUS_RELAY_MAX_POINTS;
    }
    if (input_count > 0) {
        esp_err_t input_err = modbus_relay_read_discrete_inputs(cfg->modbus.slave_id,
                                                                 cfg->modbus.input_start,
                                                                 input_count,
                                                                 bit_buf,
                                                                 sizeof(bit_buf));
        if (input_err != ESP_OK) {
            ESP_LOGE(TAG, LOG_CTX_PREFIX " [M] Probe Modbus lettura ingressi fallita: %s", esp_err_to_name(input_err));
            return input_err;
        }
    }

    ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Probe Modbus completato (slave=%u, relays=%u, inputs=%u)",
             (unsigned)cfg->modbus.slave_id,
             (unsigned)relay_count,
             (unsigned)input_count);
    return ESP_OK;
}

/* Asynchronous wrapper to start cctalk acceptor after boot UI is ready */
void main_cctalk_start_acceptor_async(void)
{
    device_config_t *cfg = device_config_get();
    if (!cfg || !cfg->sensors.cctalk_enabled) {
        ESP_LOGD(TAG, LOG_CTX_PREFIX " [M] start acceptor async skip (disabilitato da config)");
        return;
    }

    BaseType_t r = xTaskCreate(main_cctalk_start_acceptor_task, "cctalk_start", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    if (r != pdPASS) {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Creazione task start acceptor CCTalk fallita");
    }
}


/**
 * @brief Funzione principale dell'applicazione.
 * 
 * Questa funzione è l'entry point dell'applicazione e viene chiamata automaticamente
 * dal sistema operativo al boot.
 * 
 * @return void Non restituisce alcun valore.
 */
void app_main(void)
{
    apply_boot_log_policy();
    esp_log_level_set("INIT", ESP_LOG_INFO);
#if !defined(DNA_ERROR_DUMP) || (DNA_ERROR_DUMP == 0)
    // inizializza logging errori su SD (dopo montare la SD)
    error_log_init();
#endif

    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "  MODO COMPILE: %s", COMPILE_MODE_LABEL);
    ESP_LOGI(TAG, "  MODO RUNNING: %s", device_config_get_running_app_name());
    ESP_LOGI(TAG, "  App Test Wave - Versione: %s", APP_VERSION);
    ESP_LOGI(TAG, "  Data Compilazione: %s", APP_DATE);
    ESP_LOGI(TAG, "==========================================");

    // Inizializzazione I2C e I/O Expander
    ESP_LOGI(TAG, "Inizializzazione I2C e I/O Expander");
    init_i2c_and_io_expander();

#if MINIMAL_I2C_EEPROM_BOOT
    ESP_LOGW(TAG, LOG_CTX_PREFIX " [MINBOOT] Modalita' minimale attiva: eseguo solo init EEPROM + read primi 16 byte");
    esp_err_t eep_init_ret = eeprom_24lc16_init();
    if (eep_init_ret != ESP_OK) {
        ESP_LOGE(TAG, LOG_CTX_PREFIX " [MINBOOT] EEPROM init fallita: %s", esp_err_to_name(eep_init_ret));
    } else {
        uint8_t probe[16] = {0};
        esp_err_t eep_read_ret = eeprom_24lc16_read(0x0000, probe, sizeof(probe));
        if (eep_read_ret != ESP_OK) {
            ESP_LOGE(TAG, LOG_CTX_PREFIX " [MINBOOT] EEPROM read 0x0000..0x000F fallita: %s", esp_err_to_name(eep_read_ret));
        } else {
            char hex_line[3 * 16 + 1] = {0};
            char *p = hex_line;
            for (size_t i = 0; i < sizeof(probe); i++) {
                int n = snprintf(p, (size_t)(hex_line + sizeof(hex_line) - p), "%02X%s", probe[i], (i + 1 < sizeof(probe)) ? " " : "");
                if (n <= 0) {
                    break;
                }
                p += n;
            }
            ESP_LOGI(TAG, LOG_CTX_PREFIX " [MINBOOT] EEPROM probe [0x0000..0x000F]: %s", hex_line);
        }
    }

    ESP_LOGW(TAG, LOG_CTX_PREFIX " [MINBOOT] Sequenza volutamente fermata dopo init I2C/IO-Expander/EEPROM");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    esp_err_t init_ret = init_run_factory();
    if (init_ret == ESP_ERR_INVALID_STATE && init_is_error_lock_active())
    {
        uint32_t reboots = init_get_consecutive_reboots();
        ESP_LOGE(TAG, LOG_CTX_PREFIX " ERROR_LOCK attivo: avvio task inibito (reboot consecutivi=%lu)",
                 (unsigned long)reboots);
        /* Mostra schermata "Fuori servizio" se il display è disponibile */
        if (init_run_display_only() == ESP_OK) {
            lvgl_panel_show_out_of_service(reboots);
        }
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_ERROR_CHECK(init_ret);

    maybe_force_crash_at_boot();

    /*
     * Riapplica la policy dopo le init: alcuni componenti possono alterare
     * i livelli durante l'avvio.
     */
    apply_post_boot_log_policy();

    tasks_load_config("/spiffs/tasks.json");
    web_ui_program_table_init();
    tasks_start_all();

    esp_err_t boot_done_ret = init_mark_boot_completed();
    if (boot_done_ret != ESP_OK)
    {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " impossibile azzerare contatore reboot consecutivi: %s", esp_err_to_name(boot_done_ret));
    }

    if (BOOT_COUNTER_RESET_DELAYED)
    {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " reset contatore reboot posticipato di %d ms", BOOT_COUNTER_STABLE_WINDOW_MS);
        const uint32_t step_ms = 5000;
        uint32_t remaining_ms = BOOT_COUNTER_STABLE_WINDOW_MS;
        while (remaining_ms > 0) {
            uint32_t this_step = (remaining_ms > step_ms) ? step_ms : remaining_ms;
            ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] finestra stabilita': attendo ancora %lu ms (heap=%lu dram=%lu)",
                     (unsigned long)remaining_ms,
                     (unsigned long)esp_get_free_heap_size(),
                     (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
            vTaskDelay(pdMS_TO_TICKS(this_step));
            remaining_ms -= this_step;
        }
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] finestra stabilita' completata (%d ms)", BOOT_COUNTER_STABLE_WINDOW_MS);
    }

    device_config_t *cfg = device_config_get();

    bool enter_out_of_service = false;
    agn_id_t out_of_service_agent = AGN_ID_NONE;
    const char *out_of_service_key = NULL;
    const char *out_of_service_fallback = NULL;
    esp_err_t cctalk_ret = ESP_OK;

    if (cfg) {
        esp_err_t modbus_probe_err = main_boot_probe_modbus(cfg);
        if (modbus_probe_err != ESP_OK) {
            enter_out_of_service = true;
            out_of_service_agent = AGN_ID_RS485;
            out_of_service_key = "out_of_service_reason_modbus";
            out_of_service_fallback = "Errore ModBus";
        }
    }

    if (!enter_out_of_service) {
        if (cfg && cfg->sensors.cctalk_enabled) {
            cctalk_ret = cctalk_driver_init();
            if (cctalk_ret != ESP_OK) {
                ESP_LOGW(TAG,
                         LOG_CTX_PREFIX " init CCTALK prima uscita logo fallita: %s",
                         esp_err_to_name(cctalk_ret));
            } else {
                ESP_LOGI(TAG, LOG_CTX_PREFIX " init CCTALK completata prima uscita logo");
            }
        } else {
            ESP_LOGI(TAG, LOG_CTX_PREFIX " init CCTALK pre-logo skip (disabilitato da config)");
        }

        if (cfg) {
            bool cctalk_ko = (!cfg->sensors.cctalk_enabled) || (cctalk_ret != ESP_OK);
            bool scanner_ko = !cfg->scanner.enabled;
            if (cctalk_ko && scanner_ko) {
                enter_out_of_service = true;
                out_of_service_agent = AGN_ID_CCTALK;
                out_of_service_key = "out_of_service_reason_credit_systems";
                out_of_service_fallback = "Nessun sistema di acquisizione credito attivo";
                ESP_LOGE(TAG, LOG_CTX_PREFIX " [M] Nessun sistema credito attivo (CCTalk e Scanner KO)");
            }
        }
    }

    if (enter_out_of_service) {
        ESP_LOGE(TAG,
                 LOG_CTX_PREFIX " [M] Avvio in stato FUORI SERVIZIO richiesto (agent=%d, reason=%s)",
                 (int)out_of_service_agent,
                 out_of_service_key ? out_of_service_key : "n/a");
        tasks_request_out_of_service(out_of_service_agent,
                                     out_of_service_key,
                                     out_of_service_fallback);
    }

    if (cfg && cfg->sensors.cctalk_enabled) {
        main_cctalk_start_acceptor_async();
    }

    if (cfg && cfg->display.enabled) {
        /* [M] Check if ADS is enabled before loading images and showing slideshow */
        if (cfg->display.ads_enabled) {
            lvgl_page_ads_preload_images();
            lvgl_panel_show_ads_page();  /* [M] usa lock + preload imagini fuori lock */
            ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] finestra stabilita' chiusa: slideshow pubblicitario attivato");
        } else {
            ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] ADS disabilitato: salto caricamento immagini e slideshow");
            // Mostra direttamente la pagina principale se ADS è disabilitato
            lvgl_panel_show();  // [C] Inizializza il sistema i18n LVGL prima di mostrare la pagina
            lvgl_panel_show_main_page();
        }
    } else {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] display disabilitato: salto slideshow");
    }

    if (!tasks_start_named("fsm")) {
        ESP_LOGE(TAG, LOG_CTX_PREFIX " impossibile avviare task FSM dopo bootstrap UI");
    }
    ESP_LOGI(TAG, LOG_CTX_PREFIX " App pronta: endpoint HTTP /status e /ota disponibili");

    // Loop principale: segnala attività periodicamente
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Attesa
        ESP_LOGI(TAG, LOG_CTX_PREFIX " Device in funzione ✓");
    }
}
