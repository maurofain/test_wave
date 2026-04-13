#include "init.h"
#include "app_version.h"
#include "aux_gpio.h"
#include "device_config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_core_dump.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_sntp.h"
#include "esp_spiffs.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "led_test.h"
#include "led_strip.h"
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "mdb.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "tasks.h"
#include "cctalk.h"
#include <dirent.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "device_activity.h"
#include "digital_io.h"
#include "eeprom_24lc16.h"
#include "error_log.h"
#include "fsm.h" // needed for fsm_event_queue_init()
#include "http_services.h"
#include "io_expander.h"
#include "lvgl.h"
#include "lvgl_panel.h"
#include "lvgl_page_chrome.h"
#include "periph_i2c.h"
#include "pwm.h"
#include "remote_logging.h"
#include "rs232.h"
#include "rs485.h"
#include "sd_card.h"
#include "sdkconfig.h"
#include "serial_test.h"
#include "sht40.h"
#include "web_ui.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "INIT";
#define LOG_CTX_PREFIX "[M]"
#define LVGL_TASK_STACK_SAFE_BYTES 16384
/* Debug: inibisce i POST verso server cloud durante il bootstrap */
#ifndef DNA_SERVER_POST
#define DNA_SERVER_POST 0
#endif

/*
 * Forzatura temporanea: disabilita SEMPRE la parte video (LVGL + LCD + touch +
 * backlight).
 *
 * Impostare a 1 per modalità headless forzata, indipendentemente da /config.
 * Impostare a 0 per comportamento normale basato su cfg->display.enabled.
 */
#define FORCE_VIDEO_DISABLED 0

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

    if (cctalk_address_poll(dest_addr, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd1 - Address Poll: OK");
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd1 - Address Poll: FAIL");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    if (cctalk_modify_inhibit_status(dest_addr, 0xFF, 0xFF, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd2 - Modify Inhibit Status (all channels): OK");
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd2 - Modify Inhibit Status: FAIL");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    if (cctalk_modify_master_inhibit_std(dest_addr, true, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd3 - Modify Master Inhibit (accept enabled): OK");
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd3 - Modify Master Inhibit: FAIL");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t mask_low = 0, mask_high = 0;
    if (cctalk_request_inhibit_status(dest_addr, &mask_low, &mask_high, timeout_ms)) {
        ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Cmd4 - Request Inhibit Status: OK (mask=0x%02X%02X)", mask_high, mask_low);
    } else {
        ESP_LOGW(TAG, LOG_CTX_PREFIX " [M] Cmd4 - Request Inhibit Status: FAIL");
    }

    ESP_LOGI(TAG, LOG_CTX_PREFIX " [M] Sequenza CCTalk completata");
}

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

static esp_netif_t *s_netif_ap;
static esp_netif_t *s_netif_sta;
static esp_netif_t *s_netif_eth;
static esp_eth_handle_t s_eth_handle;
static esp_lcd_touch_handle_t s_touch_handle;
static bool s_error_lock_active = false;
static uint32_t s_consecutive_reboots = 0;
static bool s_display_ready = false;
static bool s_http_services_initial_token_done = false;

static init_agent_status_t s_agent_status_table[] = {
    {AGN_ID_NONE, 0, INIT_AGENT_ERR_NONE},
    {AGN_ID_FSM, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_WEB_UI, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_TOUCH, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_TOKEN, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_AUX_GPIO, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_IO_EXPANDER, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_PWM1, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_PWM2, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_LED, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_SHT40, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_CCTALK, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_RS232, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_RS485, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_MDB, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_USB_CDC_SCANNER, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_USB_HOST, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_SD_CARD, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_EEPROM, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_REMOTE_LOGGING, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_HTTP_SERVICES, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_DEVICE_CONFIG, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_DEVICE_ACTIVITY, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_ERROR_LOG, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_LVGL, 1, INIT_AGENT_ERR_NOT_EVALUATED},
    {AGN_ID_WAVESHARE_LCD, 1, INIT_AGENT_ERR_NOT_EVALUATED},
};

/**
 * @brief Inizializza il contatore degli errori di aggiornamento dello stato
 * dell'agente.
 *
 * Questa funzione si occupa di inizializzare il contatore degli errori
 * associato all'aggiornamento dello stato dell'agente.
 *
 * @param [in/out] Non applicabile per questa funzione.
 * @return Non applicabile per questa funzione.
 */
static void init_agent_status_update_error_counter(void)
{
  int32_t error_count = 0;
  const size_t count =
      sizeof(s_agent_status_table) / sizeof(s_agent_status_table[0]);
  for (size_t i = 1; i < count; ++i)
  {
    if (s_agent_status_table[i].state == 0)
    {
      error_count++;
    }
  }
  s_agent_status_table[0].state = error_count;
  s_agent_status_table[0].error_code = INIT_AGENT_ERR_NONE;
}

/**
 * @brief Resetta i valori di default dello stato dell'agente.
 *
 * Questa funzione reimposta tutti i valori dello stato dell'agente ai loro
 * valori di default.
 *
 * @return Niente.
 */
void init_agent_status_reset_defaults(void)
{
  const size_t count =
      sizeof(s_agent_status_table) / sizeof(s_agent_status_table[0]);
  for (size_t i = 1; i < count; ++i)
  {
    s_agent_status_table[i].state = 1;
    s_agent_status_table[i].error_code = INIT_AGENT_ERR_NOT_EVALUATED;
  }
  s_agent_status_table[0].state = 0;
  s_agent_status_table[0].error_code = INIT_AGENT_ERR_NONE;
}

/**
 * @brief Inizializza lo stato dell'agente.
 *
 * @param [in] agn_value Valore dell'agente.
 * @param [in] state Stato dell'agente.
 * @param [in] error_code Codice di errore dell'agente.
 * @return Nessun valore di ritorno.
 */
void init_agent_status_set(int32_t agn_value, int32_t state,
                           init_agent_error_code_t error_code)
{
  const size_t count =
      sizeof(s_agent_status_table) / sizeof(s_agent_status_table[0]);
  for (size_t i = 1; i < count; ++i)
  {
    if (s_agent_status_table[i].agn_value == agn_value)
    {
      s_agent_status_table[i].state = state;
      s_agent_status_table[i].error_code = (int32_t)error_code;
      init_agent_status_update_error_counter();
      return;
    }
  }
}

const init_agent_status_t *init_agent_status_get_table(size_t *out_count)
{
  if (out_count)
  {
    *out_count = sizeof(s_agent_status_table) / sizeof(s_agent_status_table[0]);
  }
  return s_agent_status_table;
}

const char *init_agent_error_code_text(init_agent_error_code_t code)
{
  switch (code)
  {
  case INIT_AGENT_ERR_NONE:
    return "none";
  case INIT_AGENT_ERR_NOT_EVALUATED:
    return "not_evaluated";
  case INIT_AGENT_ERR_DISABLED_BY_CONFIG:
    return "disabled_by_config";
  case INIT_AGENT_ERR_INIT_FAILED:
    return "init_failed";
  case INIT_AGENT_ERR_DEPENDENCY_FAILED:
    return "dependency_failed";
  case INIT_AGENT_ERR_NETWORK_NO_IP:
    return "network_no_ip";
  case INIT_AGENT_ERR_REMOTE_LOGIN_FAILED:
    return "remote_login_failed";
  case INIT_AGENT_ERR_RUNTIME_FAILED:
    return "runtime_failed";
  case INIT_AGENT_ERR_NOT_AVAILABLE:
    return "not_available";
  default:
    return "unknown";
  }
}

#define BOOT_GUARD_NAMESPACE "boot_guard"
#define BOOT_GUARD_KEY_CONSEC "consecutive"
#define BOOT_GUARD_KEY_CRASH_PENDING "crash_pending"
#define BOOT_GUARD_KEY_CRASH_REASON "crash_reason"
#define BOOT_GUARD_KEY_FORCE_CRASH "force_crash"
#define BOOT_GUARD_REBOOT_LIMIT 3

#define COREDUMP_MIN_FREE_MARGIN_BYTES (64 * 1024)

/**
 * @brief Verifica se il motivo del reset è un motivo di reset non pulito.
 *
 * Questa funzione verifica se il motivo del reset fornito è uno dei motivi di
 * reset considerati non puliti.
 *
 * @param reason Il motivo del reset da verificare.
 * @return true se il motivo del reset è un motivo di reset non pulito, false
 * altrimenti.
 */
static bool is_unclean_reset_reason(esp_reset_reason_t reason)
{
  switch (reason)
  {
  case ESP_RST_PANIC:
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT:
  case ESP_RST_BROWNOUT:
  case ESP_RST_UNKNOWN:
    return true;
  default:
    return false;
  }
}

/**
 * @brief Aggiorna il guardiano del riavvio del boot.
 *
 * Questa funzione aggiorna il guardiano del riavvio del boot per garantire che
 * il dispositivo non venga riavviato in modo non controllato.
 *
 * @return esp_err_t
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione non riuscita.
 */
static esp_err_t update_boot_reboot_guard(void)
{
  nvs_handle_t handle;
  esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] Apertura NVS boot guard fallita: %s",
             esp_err_to_name(ret));
    return ret;
  }

  uint32_t consecutive = 0;
  uint32_t force_crash = 0;
  ret = nvs_get_u32(handle, BOOT_GUARD_KEY_CONSEC, &consecutive);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_LOGE(TAG, "[M] Lettura counter reboot fallita: %s",
             esp_err_to_name(ret));
    nvs_close(handle);
    return ret;
  }

  ret = nvs_get_u32(handle, BOOT_GUARD_KEY_FORCE_CRASH, &force_crash);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
  {
    ESP_LOGE(TAG, "[M] Lettura marker force_crash fallita: %s",
             esp_err_to_name(ret));
    nvs_close(handle);
    return ret;
  }

  esp_reset_reason_t reason = esp_reset_reason();
  if (is_unclean_reset_reason(reason) || (force_crash == 1))
  {
    consecutive++;
  }
  else
  {
    consecutive = 0;
  }

  ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CONSEC, consecutive);
  if (ret == ESP_OK && force_crash == 1)
  {
    ret = nvs_set_u32(handle, BOOT_GUARD_KEY_FORCE_CRASH, 0);
  }
  if (ret == ESP_OK)
  {
    ret = nvs_commit(handle);
  }
  nvs_close(handle);

  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] Salvataggio counter reboot fallito: %s",
             esp_err_to_name(ret));
    return ret;
  }

  s_consecutive_reboots = consecutive;
  s_error_lock_active = (consecutive >= BOOT_GUARD_REBOOT_LIMIT);

  ESP_LOGW(TAG,
           "[M] boot_guard: reset_reason=%d force_crash=%lu "
           "consecutive_reboots=%lu limit=%d",
           (int)reason, (unsigned long)force_crash,
           (unsigned long)s_consecutive_reboots, BOOT_GUARD_REBOOT_LIMIT);

  if (s_error_lock_active)
  {
    ESP_LOGE(TAG, "[M] ERROR_LOCK attivo: troppi reboot consecutivi");
  }

  return ESP_OK;
}

/**
 * @brief Aggiorna il record di crash in sospeso.
 *
 * Questa funzione aggiorna il record di crash in sospeso nel sistema.
 *
 * @return esp_err_t
 * @retval ESP_OK Se l'operazione ha successo.
 * @retval ESP_FAIL Se l'operazione ha fallito.
 */
static esp_err_t update_crash_pending_record(void)
{
  esp_reset_reason_t reason = esp_reset_reason();
  if (!is_unclean_reset_reason(reason))
  {
    return ESP_OK;
  }

  nvs_handle_t handle;
  esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] [C] apertura NVS crash record fallita: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CRASH_PENDING, 1);
  if (ret == ESP_OK)
  {
    ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CRASH_REASON, (uint32_t)reason);
  }
  if (ret == ESP_OK)
  {
    ret = nvs_commit(handle);
  }
  nvs_close(handle);

  if (ret == ESP_OK)
  {
    ESP_LOGW(TAG, "[M] [C] crash pending registrato: reason=%d", (int)reason);
  }
  return ret;
}

#if !DNA_SERVER_POST

/**
 * @brief Costruisce l'URL per l'attività del dispositivo.
 *
 * Questa funzione prende una URL di base e la modifica per includere l'endpoint
 * relativo all'attività del dispositivo. Il risultato viene memorizzato nella
 * stringa di output fornita.
 *
 * @param [out] out Puntatore alla stringa di output dove verrà memorizzato
 * l'URL completo.
 * @param out_len Lunghezza massima della stringa di output.
 * @param [in] base_url URL di base da cui viene costruito l'URL finale.
 *
 * @return Niente.
 */
static void build_deviceactivity_url(char *out, size_t out_len,
                                     const char *base_url)
{
  if (!out || out_len == 0)
  {
    return;
  }
  out[0] = '\0';
  if (!base_url || base_url[0] == '\0')
  {
    return;
  }

  size_t bl = strlen(base_url);
  if (bl > 0 && base_url[bl - 1] == '/')
  {
    snprintf(out, out_len, "%sapi/deviceactivity", base_url);
  }
  else
  {
    snprintf(out, out_len, "%s/api/deviceactivity", base_url);
  }
}
#endif

/**
 * @brief Tenta di inviare un record di crash in sospeso.
 *
 * Questa funzione si occupa di inviare un record di crash che è in sospeso.
 * Se l'invio ha successo, la funzione restituirà ESP_OK. In caso di errore,
 * verrà restituito un codice d'errore specifico.
 *
 * @return esp_err_t - Codice di errore che indica il successo o la causa
 * dell'errore.
 */
static esp_err_t try_send_pending_crash_record(void)
{
#if DNA_SERVER_POST
  ESP_LOGW(TAG, "[M] [C] preboot crash send disabilitato da DNA_SERVER_POST");
  return ESP_OK;
#else
  nvs_handle_t handle;
  esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] [C] apertura NVS preboot send fallita: %s",
             esp_err_to_name(ret));
    return ret;
  }

  uint32_t pending = 0;
  uint32_t crash_reason = 0;
  if (nvs_get_u32(handle, BOOT_GUARD_KEY_CRASH_PENDING, &pending) != ESP_OK ||
      pending == 0)
  {
    nvs_close(handle);
    return ESP_OK;
  }

  (void)nvs_get_u32(handle, BOOT_GUARD_KEY_CRASH_REASON, &crash_reason);
  nvs_close(handle);

  device_config_t *cfg = device_config_get();
  if (!cfg || !cfg->server.enabled || cfg->server.url[0] == '\0')
  {
    ESP_LOGW(TAG, "[M] [C] preboot crash send saltato: server non configurato");
    return ESP_OK;
  }

  char url[192];
  build_deviceactivity_url(url, sizeof(url), cfg->server.url);
  if (url[0] == '\0')
  {
    ESP_LOGW(TAG, "[M] [C] preboot crash send saltato: URL non valida");
    return ESP_OK;
  }

  char body[320];
  snprintf(body, sizeof(body),
           "{\"activityid\":999,\"status\":\"CRASH\",\"serial\":\"%s\","
           "\"reason\":%lu,\"reboots\":%lu}",
           cfg->server.serial, (unsigned long)crash_reason,
           (unsigned long)s_consecutive_reboots);

  esp_http_client_config_t http_cfg = {
      .url = url,
      .method = HTTP_METHOD_POST,
      .timeout_ms = 3000,
  };

  esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
  if (!client)
  {
    ESP_LOGE(TAG, "[M] [C] preboot crash send: client init fallita");
    return ESP_FAIL;
  }

  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_header(client, "Connection", "close");
  esp_http_client_set_header(client, "Accept-Encoding", "identity");
  esp_http_client_set_post_field(client, body, (int)strlen(body));

  esp_log_level_t prev_http_client_level = esp_log_level_get("HTTP_CLIENT");
  esp_log_level_t prev_esp_http_client_level = esp_log_level_get("esp_http_client");
  esp_log_level_set("HTTP_CLIENT", ESP_LOG_NONE);
  esp_log_level_set("esp_http_client", ESP_LOG_NONE);
  esp_err_t http_ret = esp_http_client_perform(client);
  esp_log_level_set("HTTP_CLIENT", prev_http_client_level);
  esp_log_level_set("esp_http_client", prev_esp_http_client_level);
  int status = esp_http_client_get_status_code(client);
  esp_http_client_cleanup(client);

  if (http_ret != ESP_OK && status >= 200 && status < 300)
  {
    ESP_LOGW(TAG,
             "[M] [C] preboot crash send: risposta 2xx con body parziale (%s), tratto come successo",
             esp_err_to_name(http_ret));
    http_ret = ESP_OK;
  }

  if (http_ret == ESP_OK && status >= 200 && status < 300)
  {
    ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK)
    {
      ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CRASH_PENDING, 0);
      if (ret == ESP_OK)
      {
        ret = nvs_commit(handle);
      }
      nvs_close(handle);
    }
    if (ret == ESP_OK)
    {
      ESP_LOGI(TAG,
               "[M] [C] preboot crash send OK (status=%d), pending cleared",
               status);
    }
    return ret;
  }

  if (http_ret == ESP_ERR_HTTP_CONNECT)
  {
    ESP_LOGD(TAG, "[M] [C] preboot crash send fallito per rete non pronta, "
                  "riprovo dopo (pending mantenuto)");
  }
  else
  {
    ESP_LOGW(TAG,
             "[M] [C] preboot crash send fallito (err=%s status=%d), pending "
             "mantenuto",
             esp_err_to_name(http_ret), status);
  }
  return ESP_OK;
#endif
}

/**
 * @brief Controlla se la scheda SD ha spazio libero sufficiente.
 *
 * @param bytes_needed Numero di byte necessari.
 * @return true Se la scheda SD ha spazio libero sufficiente.
 * @return false Se la scheda SD non ha spazio libero sufficiente.
 */
static bool has_sd_free_space(size_t bytes_needed)
{
  if (!sd_card_is_mounted())
  {
    return false;
  }

  uint64_t total_kb = sd_card_get_total_size();
  uint64_t used_kb = sd_card_get_used_size();
  if (total_kb <= used_kb)
  {
    return false;
  }

  uint64_t free_bytes = (total_kb - used_kb) * 1024ULL;
  uint64_t required =
      (uint64_t)bytes_needed + (uint64_t)COREDUMP_MIN_FREE_MARGIN_BYTES;
  return free_bytes > required;
}

/**
 * @brief Trasferisce il coredump in memoria flash al dispositivo SD.
 *
 * Questa funzione trasferisce il coredump, memorizzato in memoria flash, al
 * dispositivo SD. Il trasferimento avviene in modo asincrono e viene gestito
 * dal sistema operativo.
 *
 * @return esp_err_t
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 * - Altri errori specifici: errore generico.
 */
static esp_err_t transfer_coredump_flash_to_sd(void)
{
  const esp_partition_t *core_part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
  if (!core_part)
  {
    ESP_LOGI(
        TAG,
        "[M] [C] partizione coredump assente: skip trasferimento coredump");
    return ESP_OK;
  }

  uint32_t raw_size = 0;
  esp_err_t err = esp_partition_read(core_part, 0, &raw_size, sizeof(raw_size));
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "[M] [C] lettura header coredump fallita: %s",
             esp_err_to_name(err));
    return ESP_OK;
  }

  /* Partizione vergine/non usata: nessun coredump disponibile */
  if (raw_size == 0xFFFFFFFF || raw_size == 0)
  {
    return ESP_OK;
  }

  /* Header invalido (tipico dopo cambio layout): pulizia silenziosa e skip */
  if (raw_size < sizeof(uint32_t) || raw_size > core_part->size)
  {
    ESP_LOGW(TAG,
             "[M] [C] header coredump invalido (size=%lu, part_size=%lu): "
             "pulizia partizione",
             (unsigned long)raw_size, (unsigned long)core_part->size);
    (void)esp_partition_erase_range(core_part, 0, core_part->size);
    return ESP_OK;
  }

  size_t image_addr = 0;
  size_t image_size = 0;
  err = esp_core_dump_image_get(&image_addr, &image_size);
  if (err == ESP_ERR_NOT_FOUND)
  {
    return ESP_OK;
  }
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "[M] [C] core dump non disponibile: %s",
             esp_err_to_name(err));
    return ESP_OK;
  }

  if (!sd_card_is_mounted())
  {
    ESP_LOGW(TAG, "[M] [C] SD non montata: coredump mantenuto in partizione");
    return ESP_OK;
  }

  if (!has_sd_free_space(image_size))
  {
    ESP_LOGW(TAG, "[M] [C] spazio SD insufficiente per coredump (%u byte)",
             (unsigned)image_size);
    return ESP_OK;
  }

  if (image_addr < core_part->address)
  {
    ESP_LOGW(TAG, "[M] [C] indirizzo coredump non valido");
    return ESP_OK;
  }

  size_t part_offset = image_addr - core_part->address;
  if ((part_offset + image_size) > core_part->size)
  {
    ESP_LOGW(TAG, "[M] [C] size coredump oltre i limiti partizione");
    return ESP_OK;
  }

  time_t now = time(NULL);
  struct tm tm_now = {0};
  localtime_r(&now, &tm_now);

  char dst_path[192];
  snprintf(dst_path, sizeof(dst_path),
           "/sdcard/coredump_%04d%02d%02d_%02d%02d%02d_%s_v%s.elf",
           tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
           tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, COMPILE_MODE_LABEL,
           APP_VERSION);

  FILE *out = sd_card_fopen(dst_path, "wb");
  if (!out)
  {
    ESP_LOGE(TAG, "[M] [C] creazione file coredump su SD fallita");
    return ESP_OK;
  }

  uint8_t buf[1024];
  size_t written = 0;
  while (written < image_size)
  {
    size_t chunk = image_size - written;
    if (chunk > sizeof(buf))
    {
      chunk = sizeof(buf);
    }

    err = esp_partition_read(core_part, part_offset + written, buf, chunk);
    if (err != ESP_OK)
    {
      sd_card_fclose(out);
      remove(dst_path);
      ESP_LOGE(TAG, "[M] [C] lettura coredump da flash fallita: %s",
               esp_err_to_name(err));
      return ESP_OK;
    }

    if (sd_card_fwrite(out, buf, chunk) != chunk)
    {
      sd_card_fclose(out);
      remove(dst_path);
      ESP_LOGE(TAG, "[M] [C] copia coredump flash->SD fallita");
      return ESP_OK;
    }

    written += chunk;
  }
  sd_card_fclose(out);

  char report_path[192];
  snprintf(report_path, sizeof(report_path),
           "/sdcard/coredump_%04d%02d%02d_%02d%02d%02d_%s_v%s.txt",
           tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday,
           tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, COMPILE_MODE_LABEL,
           APP_VERSION);

  FILE *report = sd_card_fopen(report_path, "w");
  if (report)
  {
    char report_buf[512];
    int report_len;
    report_len = snprintf(report_buf, sizeof(report_buf),
                          "Coredump trasferito su "
                          "SD\nmode=%s\nversion=%s\ndate=%s\nsource=flash_"
                          "coredump_partition\nsize=%u\n",
                          COMPILE_MODE_LABEL, APP_VERSION, APP_DATE,
                          (unsigned)image_size);
    if (report_len > 0)
      sd_card_fwrite(report, report_buf, (size_t)report_len);

    report_len =
        snprintf(report_buf, sizeof(report_buf),
                 "note=lo stack/backtrace completo e' nel file ELF coredump; "
                 "error_*.log contiene solo marker applicativi\n");
    if (report_len > 0)
      sd_card_fwrite(report, report_buf, (size_t)report_len);

#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
    char panic_reason[200] = {0};
    if (esp_core_dump_get_panic_reason(panic_reason, sizeof(panic_reason)) ==
        ESP_OK)
    {
      report_len = snprintf(report_buf, sizeof(report_buf), "panic_reason=%s\n",
                            panic_reason);
      if (report_len > 0)
        sd_card_fwrite(report, report_buf, (size_t)report_len);
    }
#endif

    sd_card_fclose(report);
  }

  err = esp_core_dump_image_erase();
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG,
             "[M] [C] coredump scritto su SD ma erase partizione fallita: %s",
             esp_err_to_name(err));
    return ESP_OK;
  }

  ESP_LOGW(TAG,
           "[M] [C] coredump trasferito su SD e cancellato da partizione: %s",
           dst_path);

  char errorlog_note[320];
  snprintf(errorlog_note, sizeof(errorlog_note),
           "[COREDUMP] stack/backtrace completo salvato in %s (size=%u). "
           "Questo error_*.log non contiene lo stack completo.\n",
           dst_path, (unsigned)image_size);
  error_log_write_msg(errorlog_note);

  return ESP_OK;
}

// -----------------------------------------------------------------------------
// Rete + HTTP + OTA (factory)
// -----------------------------------------------------------------------------

/**
 * @brief Lista tutte le entry presenti nel namespace NVS.
 *
 * Questa funzione elenca tutte le entry di configurazione presenti nel
 * namespace NVS. Non accetta parametri di input. Non restituisce alcun valore.
 */
static void nvs_list_entries(void)
{
  const char *namespaces[] = {"device_config", BOOT_GUARD_NAMESPACE};
  int total_count = 0;

  ESP_LOGI(TAG, "[M] === Elenco NVS Entries ===");

  for (size_t ns_i = 0; ns_i < (sizeof(namespaces) / sizeof(namespaces[0])); ++ns_i)
  {
    const char *ns = namespaces[ns_i];
    nvs_iterator_t it = NULL;
    esp_err_t ret = nvs_entry_find("nvs", ns, NVS_TYPE_ANY, &it);

    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
      ESP_LOGI(TAG, "[M] namespace '%s': nessuna entry", ns);
      continue;
    }

    if (ret != ESP_OK)
    {
      ESP_LOGW(TAG, "[M] Avviso lettura NVS namespace '%s': %s", ns,
               esp_err_to_name(ret));
      continue;
    }

    int ns_count = 0;
    while (it != NULL)
    {
      nvs_entry_info_t info;
      nvs_entry_info(it, &info);
      ns_count++;
      total_count++;
      ESP_LOGI(TAG, "[M]   [%s:%d] chiave='%s', tipo=%d", ns, ns_count,
               info.key, info.type);
      ret = nvs_entry_next(&it);
      if (ret != ESP_OK)
      {
        break;
      }
    }
  }

  ESP_LOGI(TAG, "[M] === Totale entries NVS note: %d ===", total_count);
}

/**
 * @brief Inizializza il sistema NVS (Non-Volatile Storage).
 *
 * Questa funzione inizializza il sistema NVS, che è utilizzato per la
 * memorizzazione non volatili dei dati persistenti. Questo è fondamentale per
 * la gestione dei parametri di configurazione e dei dati di stato
 * dell'applicazione.
 *
 * @return esp_err_t
 *         - ESP_OK: Inizializzazione avvenuta con successo.
 *         - ESP_FAIL: Inizializzazione fallita.
 */
static esp_err_t init_nvs(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_RETURN_ON_ERROR(ret, TAG, "Inizializzazione NVS fallita");

  // Mostra l'elenco dei file/entries nella NVS
  nvs_list_entries();

  return ESP_OK;
}

/**
 * @brief Inizializza il sistema di file SPIFFS.
 *
 * Questa funzione inizializza il sistema di file SPIFFS (SPI Flash File System)
 * sul dispositivo. Questo è necessario per utilizzare il file system SPIFFS per
 * la memorizzazione dei dati.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
static esp_err_t init_spiffs(void)
{
  esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = "storage",
      .max_files = 5,
      .format_if_mount_failed = false,
  };
  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] Montaggio SPIFFS fallito: %s", esp_err_to_name(ret));
    return ret;
  }
  size_t total = 0, used = 0;
  if (esp_spiffs_info(conf.partition_label, &total, &used) == ESP_OK)
  {
    ESP_LOGI(TAG, "[M] SPIFFS montato: totale=%u, usato=%u", (unsigned)total,
             (unsigned)used);
  }

  // Elenca file in SPIFFS
  DIR *dir = opendir("/spiffs");
  if (dir)
  {
    ESP_LOGI(TAG, "[M] === Elenco file SPIFFS ===");
    struct dirent *entry;
    int file_count = 0;
    while ((entry = readdir(dir)) != NULL)
    {
      file_count++;
      char filepath[300];
      snprintf(filepath, sizeof(filepath), "/spiffs/%s", entry->d_name);
      struct stat st;
      if (stat(filepath, &st) == 0)
      {
        ESP_LOGI(TAG, "[M]   [%d] %s (%ld bytes)", file_count, entry->d_name,
                 st.st_size);
      }
      else
      {
        ESP_LOGI(TAG, "[M]   [%d] %s", file_count, entry->d_name);
      }
    }
    closedir(dir);
    ESP_LOGI(TAG, "[M] === Totale file: %d ===", file_count);
  }

  // Logga la dimensione di tasks.json
  FILE *f = fopen("/spiffs/tasks.json", "r");
  if (f)
  {
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fclose(f);
    ESP_LOGI(TAG, "[M] tasks.json presente: %ld byte", fsz);
  }
  else
  {
    ESP_LOGW(TAG, "[M] File tasks.json non trovato");
  }

  return ESP_OK;
}

/**
 * @brief Funzione che registra le partizioni.
 *
 * Questa funzione si occupa di registrare tutte le partizioni presenti nel
 * sistema.
 *
 * @return void Non restituisce alcun valore.
 */
static void log_partitions(void)
{
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *boot = esp_ota_get_boot_partition();
  ESP_LOGI(TAG, "[M] Partizione in esecuzione: %s (tipo %d, sottotipo %d)",
           running ? running->label : "?", running ? running->type : -1,
           running ? running->subtype : -1);
  ESP_LOGI(TAG, "[M] Partizione boot      : %s (tipo %d, sottotipo %d)",
           boot ? boot->label : "?", boot ? boot->type : -1,
           boot ? boot->subtype : -1);
}

/**
 * @brief Callback chiamata quando la sincronizzazione NTP è completata.
 *
 * Questa funzione viene invocata quando il sistema ha completato la
 * sincronizzazione NTP.
 *
 * @param tv Puntatore al tempo sincronizzato.
 * @return Nessun valore di ritorno.
 */
static inline void init_lvgl_status_log(const char *text)
{
  if (s_display_ready && text && text[0] != '\0')
  {
    device_config_t *cfg = device_config_get();
    if (cfg->display.enabled)
    {
      lvgl_panel_set_init_status(text);
    }
  }
}

/**
 * @brief Restituisce testo "abilitata"/"disabilitata" per i flag booleani.
 */
static const char *enabled_text(bool enabled)
{
  return enabled ? "abilitata" : "disabilitata";
}

/**
 * @brief Logga lo stato periferiche e funzionalità subito dopo il caricamento config.
 */
static void log_peripherals_from_config(const device_config_t *cfg)
{
  if (!cfg)
  {
    ESP_LOGW(TAG, "[M] Config non disponibile: impossibile loggare periferiche");
    return;
  }

  ESP_LOGI(TAG, "[M] ╔════════════════════════════════════════════════════════════════╗");
  ESP_LOGI(TAG, "[M] ║        STATO PERIFERICHE E FUNZIONALITÀ ABILITATE               ║");
  ESP_LOGI(TAG, "[M] ╚════════════════════════════════════════════════════════════════╝");
  
  /* Rete e comunicazione */
  ESP_LOGI(TAG, "[M] │ RETE E COMUNICAZIONE │");
  ESP_LOGI(TAG, "[M]  - ethernet:        %s", enabled_text(cfg->eth.enabled));
  ESP_LOGI(TAG, "[M]  - wifi_sta:        %s", enabled_text(cfg->wifi.sta_enabled));
  ESP_LOGI(TAG, "[M]  - ntp:             %s", enabled_text(cfg->ntp_enabled));
  ESP_LOGI(TAG, "[M]  - server remoto:   %s", enabled_text(cfg->server.enabled));
  ESP_LOGI(TAG, "[M]  - ftp:             %s", enabled_text(cfg->ftp.enabled));
  ESP_LOGI(TAG, "[M]  - log remoto:      %s", enabled_text(cfg->remote_log.use_broadcast));
  ESP_LOGI(TAG, "[M]  - log su SD:       %s", enabled_text(cfg->remote_log.write_to_sd));
  
  /* Display e interfaccia utente */
  ESP_LOGI(TAG, "[M] │ DISPLAY E INTERFACCIA UTENTE │");
  ESP_LOGI(TAG, "[M]  - display:        %s", enabled_text(cfg->display.enabled));
  ESP_LOGI(TAG, "[M]  - ads:            %s", enabled_text(cfg->display.ads_enabled));
  ESP_LOGI(TAG, "[M]  - audio:          %s", enabled_text(cfg->audio.enabled));
  
  /* Periferiche di input/output */
  ESP_LOGI(TAG, "[M] │ PERIFERICHE I/O │");
  ESP_LOGI(TAG, "[M]  - scanner_usb:    %s", enabled_text(cfg->scanner.enabled));
  ESP_LOGI(TAG, "[M]  - io_expander:    %s", enabled_text(cfg->sensors.io_expander_enabled));
  ESP_LOGI(TAG, "[M]  - rs232:          %s", enabled_text(cfg->sensors.rs232_enabled));
  ESP_LOGI(TAG, "[M]  - rs485:          %s", enabled_text(cfg->sensors.rs485_enabled));
  ESP_LOGI(TAG, "[M]  - mdb:            %s", enabled_text(cfg->sensors.mdb_enabled));
  ESP_LOGI(TAG, "[M]  - cctalk:         %s", enabled_text(cfg->sensors.cctalk_enabled));
  
  /* Sensori e attuatori */
  ESP_LOGI(TAG, "[M] │ SENSORI E ATTUATORI │");
  ESP_LOGI(TAG, "[M]  - temperatura:    %s", enabled_text(cfg->sensors.temperature_enabled));
  ESP_LOGI(TAG, "[M]  - led_strip:      %s", enabled_text(cfg->sensors.led_enabled));
  ESP_LOGI(TAG, "[M]  - pwm1:           %s", enabled_text(cfg->sensors.pwm1_enabled));
  ESP_LOGI(TAG, "[M]  - pwm2:           %s", enabled_text(cfg->sensors.pwm2_enabled));
  
  /* Storage e memoria */
  ESP_LOGI(TAG, "[M] │ STORAGE │");
  ESP_LOGI(TAG, "[M]  - sd_card:        %s", enabled_text(cfg->sensors.sd_card_enabled));
  ESP_LOGI(TAG, "[M]  - eeprom:         %s", enabled_text(cfg->sensors.eeprom_enabled));
  
  ESP_LOGI(TAG, "[M] ╚════════════════════════════════════════════════════════════════╝");
}

/**
 * @brief Applica override display in base allo stato DIP1.
 *
 * Regola richiesta:
 * - DIP1 LOW (switch ON) => forza display disabilitato.
 * - DIP1 HIGH (switch OFF) => nessun forcing (usa stato da config).
 */
static void apply_display_override_from_dip1(device_config_t *cfg)
{
  if (!cfg)
  {
    return;
  }

  esp_err_t io_ret = io_expander_init();
  if (io_ret != ESP_OK)
  {
    ESP_LOGW(TAG,
             "[M] DIP1 override non disponibile: IO expander non pronto (%s)",
             esp_err_to_name(io_ret));
    return;
  }

  bool dip1_high = io_get_pin((int)(DIGITAL_IO_INPUT_DIP1 - 1U));
  if (!dip1_high)
  {
    cfg->display.enabled = false;
    ESP_LOGW(TAG,
             "[M] =========================================================");
    ESP_LOGW(TAG,
             "[M] MIPI DISPLAY FORCED DISALED (DIP1=ON)");
    ESP_LOGW(TAG,
             "[M] =========================================================");
  }
}

static bool is_leap_year(int year)
{
  return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

static int day_of_week(int year, int month, int day)
{
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) year -= 1;
  return (year + year/4 - year/100 + year/400 + t[month - 1] + day) % 7;
}

static int last_sunday_of_month(int year, int month)
{
  int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    days_in_month[1] = 29;
  }
  int last_day = days_in_month[month - 1];
  int dow = day_of_week(year, month, last_day);
  return last_day - dow;
}

static bool is_europe_dst(int year, int month, int day, int hour)
{
  if (month < 3 || month > 10) {
    return false;
  }
  if (month > 3 && month < 10) {
    return true;
  }
  if (month == 3) {
    int last_sunday = last_sunday_of_month(year, month);
    if (day < last_sunday) {
      return false;
    }
    if (day > last_sunday) {
      return true;
    }
    return hour >= 2;
  }
  if (month == 10) {
    int last_sunday = last_sunday_of_month(year, month);
    if (day < last_sunday) {
      return true;
    }
    if (day > last_sunday) {
      return false;
    }
    return hour < 3;
  }
  return false;
}

static void ntp_sync_callback(struct timeval *tv)
{
  device_config_t *cfg = device_config_get();
  int total_offset_hours = cfg->ntp.timezone_offset;
  int dst_offset = 0;

  if (cfg->ntp.use_dst) {
    time_t utc_time = tv->tv_sec;
    time_t local_time = utc_time + (time_t)cfg->ntp.timezone_offset * 3600;
    struct tm local_tm;
    gmtime_r(&local_time, &local_tm);
    if (is_europe_dst(local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday, local_tm.tm_hour)) {
      dst_offset = 1;
    }
  }

  if (cfg->ntp.timezone_offset != 0 || dst_offset != 0) {
    total_offset_hours += dst_offset;
    tv->tv_sec += (total_offset_hours * 3600);
    settimeofday(tv, NULL);
    ESP_LOGI(TAG, "[NTP] Offset del fuso orario applicato: %+d ore%s",
             total_offset_hours,
             dst_offset ? " (ora legale attiva)" : "");
  } else {
    settimeofday(tv, NULL);
    ESP_LOGI(TAG, "[NTP] Offset del fuso orario applicato: %d ore", total_offset_hours);
  }

  time_t now = time(NULL);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  ESP_LOGI(TAG,
           "[NTP] Time synchronized: %04d-%02d-%02d %02d:%02d:%02d (UTC%+d)",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
           total_offset_hours);

  // Quando la sincronizzazione NTP ha successo, aumenta l'intervallo a 1 ora
  // (3600000 ms) Evita tentativi continui quando l'ora è già corretta
  sntp_set_sync_interval(3600000);
  ESP_LOGI(TAG,
           "[NTP] Intervallo di sync impostato a 1 ora (ora sincronizzata)");
}

/* forward declarations */
static void init_sntp(void);

static bool s_sntp_initialized = false; /* [C] Flag: init_sntp() già chiamato */

/**
 * @brief Inizializza il servizio SNTP.
 *
 * Questa funzione configura e avvia il servizio SNTP per sincronizzare l'orario
 * del dispositivo con un server SNTP.
 *
 * @return Nessun valore di ritorno.
 */
static void init_sntp(void)
{
  if (s_sntp_initialized)
  {
    ESP_LOGI(TAG, "[NTP] SNTP già inizializzato, skip init");
    return;
  }

  device_config_t *cfg = device_config_get();

  ESP_LOGI(TAG, "[NTP] Initializing SNTP with servers: %s, %s",
           cfg->ntp.server1, cfg->ntp.server2);

  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, cfg->ntp.server1);
  esp_sntp_setservername(1, cfg->ntp.server2);

  // Imposta la callback da chiamare alla sincronizzazione NTP riuscita
  esp_sntp_set_time_sync_notification_cb(ntp_sync_callback);

  esp_sntp_init();

  // Imposta modalità e intervallo dopo init() (come nell'implementazione
  // originale)
  sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  sntp_set_sync_interval(300000); /* 5 min — tentativi iniziali frequenti */

  s_sntp_initialized = true;
  ESP_LOGI(
      TAG,
      "[NTP] SNTP inizializzato - la sincronizzazione avverrà in background");
}

/**
 * @brief Forza la sincronizzazione NTP manuale
 * @return ESP_OK se la sincronizzazione è iniziata, ESP_FAIL altrimenti
 */

/**
 * @brief Inizializza la sincronizzazione NTP.
 *
 * Questa funzione inizializza il servizio di sincronizzazione NTP per ottenere
 * l'ora corrente.
 *
 * @return esp_err_t
 *         - ESP_OK: operazione riuscita
 *         - ESP_FAIL: operazione fallita
 */
esp_err_t init_sync_ntp(void)
{
  device_config_t *cfg = device_config_get();

  if (!cfg->ntp_enabled)
  {
    ESP_LOGW(TAG, "[NTP] NTP is disabled in configuration");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "[NTP] Forcing NTP synchronization...");

  // Se SNTP non è mai stato inizializzato (es. ip_event non ricevuto),
  // inizializza ora
  if (!s_sntp_initialized)
  {
    ESP_LOGW(TAG, "[NTP] SNTP non ancora inizializzato — eseguo init");
    init_sntp();
    ESP_LOGI(TAG, "[NTP] Richiesta inviata — completamento in background");
    return ESP_OK;
  }

  // Controlla se già sincronizzato
  sntp_sync_status_t status = sntp_get_sync_status();
  if (status == SNTP_SYNC_STATUS_COMPLETED)
  {
    ESP_LOGI(TAG, "[NTP] Ora già sincronizzata, skip restart");
    return ESP_OK;
  }

  // SNTP già inizializzato: forza un nuovo tentativo
  esp_sntp_restart();

  ESP_LOGI(TAG,
           "[NTP] Richiesta di sincronizzazione inviata (status=%d) — "
           "completamento in background",
           (int)status);

  return ESP_OK;
}

/**
 * @brief Gestore degli eventi IP.
 *
 * Questa funzione viene chiamata quando si verifica un evento IP.
 *
 * @param arg Puntatore agli argomenti passati alla funzione.
 * @param event_base Base dell'evento.
 * @param event_id ID dell'evento.
 * @param event_data Dati dell'evento.
 */
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
  (void)arg;
  if ((event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) ||
      (event_base == IP_EVENT && event_id == IP_EVENT_ETH_GOT_IP))
  {
    if (event_id == IP_EVENT_STA_GOT_IP)
    {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      ESP_LOGI(TAG, "[M] STA Wi-Fi ha ottenuto IP: %s",
               ip4addr_ntoa((const ip4_addr_t *)&event->ip_info.ip));
    }
    else if (event_id == IP_EVENT_ETH_GOT_IP)
    {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      const esp_netif_ip_info_t *ip_info = &event->ip_info;
      ESP_LOGI(TAG, "[M] Ethernet ha ottenuto l'indirizzo IP");
      ESP_LOGI(TAG, "[M] ~~~~~~~~~~~");
      ESP_LOGI(TAG, "[M] ETH IP:" IPSTR, IP2STR(&ip_info->ip));
      ESP_LOGI(TAG, "[M] ETH MASCHERA:" IPSTR, IP2STR(&ip_info->netmask));
      ESP_LOGI(TAG, "[M] ETH GATEWAY:" IPSTR, IP2STR(&ip_info->gw));
      ESP_LOGI(TAG, "[M] ~~~~~~~~~~~");

      if (s_netif_eth)
      {
        struct netif *lwip_netif =
            (struct netif *)esp_netif_get_netif_impl(s_netif_eth);
        if (lwip_netif)
        {
          etharp_gratuitous(lwip_netif);
          ESP_LOGI(TAG, "[M] Gratuitous ARP inviato");
          ESP_LOGI(TAG, "[M] Post-ARP: ntp_enabled=%d free_heap=%u",
                   (int)device_config_get()->ntp_enabled,
                   (unsigned)esp_get_free_heap_size());
        }
        else
        {
          ESP_LOGW(TAG, "[M] Post-ARP: lwip_netif NULL (impossibile inviare "
                        "gratuitous ARP)");
        }
      }
      else
      {
        ESP_LOGW(TAG, "[M] Post-ARP: s_netif_eth NULL");
      }

      /* init_sync_ntp() usa solo esp_sntp_* non-bloccanti: sicuro nell'event
       * handler */
      if (device_config_get()->ntp_enabled)
      {
        ESP_LOGI(TAG, "[M] [NTP] Avvio SNTP (IP disponibile)");
        if (init_sync_ntp() != ESP_OK)
        {
          ESP_LOGW(TAG, "[M] [NTP] init_sync_ntp() fallito");
        }
      }
      else
      {
        ESP_LOGI(TAG, "[M] [NTP] NTP disabilitato da config");
      }

      if (bsp_display_lock(0))
      {
        lvgl_page_chrome_set_status_icon_state(0, true); /* Cloud = rete online */
        bsp_display_unlock();
      }
    }

    if (!s_http_services_initial_token_done)
    {
      s_http_services_initial_token_done = true;

      if (!http_services_is_remote_enabled())
      {
        init_agent_status_set(AGN_ID_HTTP_SERVICES, 1,
                              INIT_AGENT_ERR_DISABLED_BY_CONFIG);
        ESP_LOGI(TAG, "[M] [HTTP_SVC] Login iniziale token saltato: server "
                      "remoto disabilitato");
        return;
      }

      ESP_LOGI(TAG,
               "[M] [HTTP_SVC] Avvio login iniziale token (IP disponibile)");
      esp_err_t hs_sync_err = http_services_sync_runtime_state(true);
      if (hs_sync_err == ESP_OK && http_services_is_remote_online())
      {
        init_agent_status_set(AGN_ID_HTTP_SERVICES, 1, INIT_AGENT_ERR_NONE);
        ESP_LOGI(TAG, "[M] [HTTP_SVC] Login iniziale token completato");
      }
      else
      {
        init_agent_status_set(AGN_ID_HTTP_SERVICES, 0,
                              INIT_AGENT_ERR_REMOTE_LOGIN_FAILED);
        ESP_LOGW(TAG, "[M] [HTTP_SVC] Login iniziale token fallito: %s",
                 esp_err_to_name(hs_sync_err));
      }
    }
  }
}

/**
 * @brief Gestore degli eventi Ethernet.
 *
 * Questa funzione viene chiamata quando si verifica un evento Ethernet.
 *
 * @param arg Puntatore a un argomento specifico, utilizzato per passare
 * informazioni aggiuntive.
 * @param event_base Base dell'evento Ethernet.
 * @param event_id ID dell'evento Ethernet.
 * @param event_data Dati associati all'evento Ethernet.
 */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
  (void)arg;
  switch (event_id)
  {
  case ETHERNET_EVENT_CONNECTED:
  {
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
    ESP_LOGI(TAG, "[M] Collegamento Ethernet attivo");
    ESP_LOGI(TAG, "[M] Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
             mac_addr[5]);
  }
  break;
  case ETHERNET_EVENT_DISCONNECTED:
    ESP_LOGW(TAG, "[M] Collegamento Ethernet inattivo");
    if (bsp_display_lock(0))
    {
      lvgl_page_chrome_set_status_icon_state(0, false); /* Cloud = rete offline */
      bsp_display_unlock();
    }
    break;
  case ETHERNET_EVENT_START:
    ESP_LOGI(TAG, "[M] Driver Ethernet avviato");
    break;
  case ETHERNET_EVENT_STOP:
    ESP_LOGW(TAG, "[M] Driver Ethernet fermato");
    break;
  default:
    break;
  }
}

/**
 * @brief Inizializza il loop degli eventi.
 *
 * Questa funzione inizializza il loop degli eventi necessario per la gestione
 * degli eventi asincroni.
 *
 * @return esp_err_t
 *         - ESP_OK: inizializzazione avvenuta con successo.
 *         - ESP_FAIL: inizializzazione fallita.
 */
static esp_err_t init_event_loop(void)
{
  // Nota: esp_netif_init() e esp_event_loop_create_default() vengono chiamati
  // in start_ethernet() DOPO aver inizializzato il driver Ethernet, come
  // nell'esempio funzionante Inizializza sempre netif/event loop prima di
  // eventuali socket (HTTP server, etc.) per evitare assert lwIP "Invalid mbox"
  // se Ethernet è disabilitato/non avviato.
  esp_err_t ret = esp_netif_init();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
  {
    return ret;
  }

  ret = esp_event_loop_create_default();
  if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
  {
    return ret;
  }

  return ESP_OK;
}

/**
 * @brief Inizializza l'handle interno per l'ethernet.
 *
 * Questa funzione inizializza l'handle interno necessario per la gestione
 * dell'ethernet.
 *
 * @return esp_eth_handle_t Handle interno per l'ethernet.
 */
static esp_eth_handle_t eth_init_internal(void)
{
  esp_err_t ret __attribute__((unused)) = ESP_OK;
  // Inizializza le config comuni MAC e PHY ai valori di default
  // Nota: Il reset PHY viene gestito automaticamente dal driver quando
  // phy_config.reset_gpio_num è configurato
  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

  // Update PHY config based on board specific configuration
  phy_config.phy_addr = CONFIG_APP_ETH_PHY_ADDR;
  phy_config.reset_gpio_num = CONFIG_APP_ETH_PHY_RST_GPIO;

  // Init vendor specific MAC config to default
  eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  // Update vendor specific MAC config based on board configuration
  esp32_emac_config.smi_gpio.mdc_num = CONFIG_APP_ETH_MDC_GPIO;
  esp32_emac_config.smi_gpio.mdio_num = CONFIG_APP_ETH_MDIO_GPIO;

  // Crea istanza MAC Ethernet ESP32
  esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
  // Crea istanza PHY (IP101 per ESP32-P4 Module DEV KIT)
  esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
  // Inizializza il driver Ethernet ai valori di default e installalo
  esp_eth_handle_t eth_handle = NULL;
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
  ESP_GOTO_ON_FALSE(esp_eth_driver_install(&eth_config, &eth_handle) == ESP_OK,
                    ESP_FAIL, err, TAG,
                    "Installazione driver Ethernet fallita");

  return eth_handle;
err:
  if (eth_handle != NULL)
  {
    esp_eth_driver_uninstall(eth_handle);
  }
  if (mac != NULL)
  {
    mac->del(mac);
  }
  if (phy != NULL)
  {
    phy->del(phy);
  }
  return NULL;
}

/**
 * @brief Avvia la connessione Ethernet.
 *
 * Questa funzione inizializza e avvia la connessione Ethernet del dispositivo.
 *
 * @return esp_err_t
 *         - ESP_OK: Operazione riuscita.
 *         - ESP_FAIL: Operazione fallita.
 */
static esp_err_t start_ethernet(void)
{
#if CONFIG_APP_ETH_ENABLED
  esp_err_t ret = ESP_OK;

  // Check for GPIO conflicts
  if (CONFIG_APP_ETH_MDC_GPIO == CONFIG_APP_MDB_RX_GPIO)
  {
    ESP_LOGW(TAG,
             "[M] ATTENZIONE: Conflitto GPIO! ETH_MDC_GPIO (%d) e MDB_RX_GPIO "
             "(%d) usano lo stesso pin",
             CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_MDB_RX_GPIO);
  }

  ESP_LOGI(TAG,
           "[M] Inizializzazione Ethernet: MDC=%d, MDIO=%d, PHY_ADDR=%d, "
           "RST_GPIO=%d",
           CONFIG_APP_ETH_MDC_GPIO, CONFIG_APP_ETH_MDIO_GPIO,
           CONFIG_APP_ETH_PHY_ADDR, CONFIG_APP_ETH_PHY_RST_GPIO);

  // Inizializza il driver Ethernet ai valori di default e installalo
  esp_eth_handle_t eth_handle = eth_init_internal();
  if (!eth_handle)
  {
    ESP_LOGE(TAG, "[M] Installazione driver Ethernet fallita");
    return ESP_FAIL;
  }

  // Salva l'handle Ethernet per riferimento futuro
  s_eth_handle = eth_handle;

  // esp_netif/event loop sono inizializzati in init_event_loop(); qui non li
  // reinizializziamo.

  // Crea netif DOPO l'installazione del driver e l'inizializzazione di netif
  // (come nell'esempio funzionante)
  esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
  s_netif_eth = esp_netif_new(&netif_cfg);
  if (!s_netif_eth)
  {
    ESP_LOGE(TAG, "[M] Impossibile allocare Ethernet netif");
    esp_eth_driver_uninstall(eth_handle);
    return ESP_FAIL;
  }

  // Crea glue e aggancia netif
  esp_eth_netif_glue_handle_t glue = esp_eth_new_netif_glue(eth_handle);
  ESP_RETURN_ON_ERROR(esp_netif_attach(s_netif_eth, glue), TAG,
                      "Aggancio Netif fallito");

  // Allinea MAC del netif a quello del driver Ethernet
  uint8_t mac_addr[6] = {0};
  esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
  ESP_RETURN_ON_ERROR(esp_netif_set_mac(s_netif_eth, mac_addr), TAG,
                      "esp_netif_set_mac fallito");
  ESP_LOGI(TAG, "[M] Netif MAC impostato a %02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
           mac_addr[5]);

  // Registra i gestori di eventi DOPO la creazione del netif (come nel progetto
  // factory)
  ESP_RETURN_ON_ERROR(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID,
                                                 &eth_event_handler, NULL),
                      TAG, "registrazione ETH_EVENT fallita");
  ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                                 &ip_event_handler, NULL),
                      TAG, "registrazione IP_EVENT fallita");
  ESP_LOGI(TAG, "[M] EXIT from ip_event_handlers");
  // Applica configurazione IP statica se DHCP è disabilitato
  device_config_t *cfg = device_config_get();
  if (!cfg->eth.dhcp_enabled && strlen(cfg->eth.ip) > 0)
  {
    esp_netif_dhcpc_stop(s_netif_eth);
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = ipaddr_addr(cfg->eth.ip);
    ip_info.gw.addr = ipaddr_addr(cfg->eth.gateway);
    ip_info.netmask.addr = ipaddr_addr(cfg->eth.subnet);
    esp_netif_set_ip_info(s_netif_eth, &ip_info);

    // Imposta i server DNS quando si usa IP statico
    esp_netif_dns_info_t dns_info;
    if (strlen(cfg->eth.dns1) > 0)
    {
      dns_info.ip.u_addr.ip4.addr = ipaddr_addr(cfg->eth.dns1);
    }
    else
    {
      dns_info.ip.u_addr.ip4.addr =
          ipaddr_addr("8.8.8.8"); // Google DNS default
    }
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(s_netif_eth, ESP_NETIF_DNS_MAIN, &dns_info);

    if (strlen(cfg->eth.dns2) > 0)
    {
      dns_info.ip.u_addr.ip4.addr = ipaddr_addr(cfg->eth.dns2);
    }
    else
    {
      dns_info.ip.u_addr.ip4.addr =
          ipaddr_addr("8.8.4.4"); // Google DNS secondary default
    }
    esp_netif_set_dns_info(s_netif_eth, ESP_NETIF_DNS_BACKUP, &dns_info);

    ESP_LOGI(TAG, "[M] Ethernet IP statico: %s, DNS: %s, %s", cfg->eth.ip,
             strlen(cfg->eth.dns1) > 0 ? cfg->eth.dns1 : "8.8.8.8",
             strlen(cfg->eth.dns2) > 0 ? cfg->eth.dns2 : "8.8.4.4");
  }

  // Start Ethernet driver (come nel progetto factory)
  ret = esp_eth_start(eth_handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] Avvio Ethernet fallito: %s", esp_err_to_name(ret));
    esp_eth_del_netif_glue(glue);
    esp_eth_driver_uninstall(eth_handle);
    return ret;
  }

  ESP_LOGI(TAG, "[M] Ethernet avviato (indirizzo PHY %d, DHCP abilitato)",
           CONFIG_APP_ETH_PHY_ADDR);
#else
  ESP_LOGI(TAG, "[M] Ethernet disabilitato (CONFIG_APP_ETH_ENABLED=n)");
#endif
  return ESP_OK;
}

// -----------------------------------------------------------------------------

// Public API
// -----------------------------------------------------------------------------

/**
 * @brief Inizializza il display utilizzando la libreria LVGL in modalità
 * minima.
 *
 * Questa funzione configura il display per l'uso con la libreria LVGL in
 * modalità minima, preparando il sistema per l'uso di interfacce grafiche
 * minimaliste.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
static esp_err_t init_display_lvgl_minimal(void)
{
  device_config_t *device_cfg = device_config_get();
  if (!device_cfg || !device_cfg->display.enabled)
  {
    ESP_LOGI(TAG, "[M] Display disabilitato: skip init display/backlight/touch");
    s_touch_handle = NULL;
    tasks_set_touchscreen_handle(NULL);
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGI(TAG, "Heap before display init:");
  ESP_LOGI(
      TAG, "  INTERNAL free: %u",
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
  ESP_LOGI(TAG, "  DMA free: %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "  PSRAM free: %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  ESP_LOGI(
      TAG, "  SPIRAM caps alloc free (8bit): %u",
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  bsp_display_cfg_t cfg = {
      .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
      .buffer_size = BSP_LCD_DRAW_BUFF_SIZE / 2,
      .double_buffer = false,
      .flags =
          {
              .buff_dma = true,
              .buff_spiram = false,
              .sw_rotate = false,
          },
  };
  /* Compromesso stabilità/memoria: evitare pressione eccessiva su DRAM interna
   * (USB host). */
  cfg.lvgl_port_cfg.task_stack = 32768;

  lv_display_t *disp = bsp_display_start_with_config(&cfg);
  if (!disp)
  {
    return ESP_FAIL;
  }

  // Forza orientamento verticale (portrait)
  bsp_display_rotate(disp, LV_DISPLAY_ROTATION_0);

  ESP_LOGI(TAG, "Heap after LVGL init:");
  ESP_LOGI(
      TAG, "  INTERNAL free: %u",
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL));
  ESP_LOGI(TAG, "  DMA free: %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA));
  ESP_LOGI(TAG, "  PSRAM free: %u",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  ESP_LOGI(
      TAG, "  SPIRAM caps alloc free (8bit): %u",
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

  // Luminosità: non fatale — schermo visibile è prioritario rispetto alla
  // brightness corretta
  if (bsp_display_brightness_init() != ESP_OK)
  {
    ESP_LOGW(TAG,
             "[M] brightness_init fallita: backlight potrebbe restare fisso");
  }
  else
  {
    uint8_t brightness = device_cfg->display.lcd_brightness;
    if (brightness == 0)
    {
      ESP_LOGW(TAG, "[M] lcd_brightness=0 in config, forzo 80%%");
      brightness = 80;
    }
    if (bsp_display_brightness_set(brightness) != ESP_OK)
    {
      ESP_LOGW(TAG, "[M] brightness_set(%u) fallita", (unsigned)brightness);
    }
    ESP_LOGI(TAG, "[M] Luminosità display: %u%%", (unsigned)brightness);
  }

  // Log delle dimensioni
  lv_coord_t hor_res = lv_display_get_horizontal_resolution(disp);
  lv_coord_t ver_res = lv_display_get_vertical_resolution(disp);
  ESP_LOGI(TAG, "[M] Dimensioni display dopo rotazione: %dx%d", hor_res,
           ver_res);

  // Inizializza il touch (non fatale: il display funziona anche senza touch)
  bsp_touch_config_t touch_cfg = {
      .dummy = NULL,
  };
  esp_err_t touch_ret = bsp_touch_new(&touch_cfg, &s_touch_handle);
  if (touch_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "[M] Touch init fallita (%s): display operativo senza touch",
             esp_err_to_name(touch_ret));
    s_touch_handle = NULL;
  }
  else if (s_touch_handle)
  {
    ESP_LOGI(TAG, "[M] Touch handle inizializzato: %p", s_touch_handle);
    tasks_set_touchscreen_handle(s_touch_handle);
  }
  else
  {
    ESP_LOGW(TAG, "[M] Touch handle è NULL dopo init");
  }

  lvgl_panel_show();
  return ESP_OK;
}

/**
 * @brief Inizializza il display in modalità solo lettura.
 *
 * Questa funzione configura il display per la modalità solo lettura,
 * preparandolo per la visualizzazione di contenuti senza la possibilità di
 * modifiche.
 *
 * @return esp_err_t
 * - ESP_OK: Inizializzazione avvenuta con successo.
 * - ESP_FAIL: Inizializzazione fallita.
 */
esp_err_t init_run_display_only(void)
{
#if defined(DNA_LVGL) && (DNA_LVGL == 1)
  ESP_LOGI(TAG, "[M] init_run_display_only: DNA_LVGL mock attivo, display non "
                "disponibile");
  return ESP_ERR_NOT_SUPPORTED;
#else
  device_config_t *cfg = device_config_get();
  if (!cfg || !cfg->display.enabled)
  {
    ESP_LOGI(TAG, "[M] init_run_display_only: display disabilitato da config");
    return ESP_ERR_INVALID_STATE;
  }
  bsp_display_cfg_t cfg_disp = {
      .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
      .buffer_size = BSP_LCD_DRAW_BUFF_SIZE / 2,
      .double_buffer = false,
      .flags = {.buff_dma = true, .buff_spiram = false, .sw_rotate = false},
  };
  /* Allineato alla init principale: evita consumo DRAM eccessivo che impatta
   * USB host. */
  cfg_disp.lvgl_port_cfg.task_stack = 32768;
  lv_display_t *disp = bsp_display_start_with_config(&cfg_disp);
  if (!disp)
  {
    ESP_LOGE(TAG, "[M] init_run_display_only: bsp_display_start_with_config fallito");
    return ESP_FAIL;
  }
  bsp_display_rotate(disp, LV_DISPLAY_ROTATION_0);
  if (bsp_display_brightness_init() == ESP_OK)
  {
    bsp_display_brightness_set(cfg->display.lcd_brightness);
  }
  ESP_LOGI(TAG,
           "[M] init_run_display_only: display avviato per schermata errore");
  return ESP_OK;
#endif
}

/* forward declarations */
static void generic_i2c_diagnostic_scan(i2c_master_bus_handle_t bus,
                                        const char *bus_name,
                                        int timeout_ticks);

/**
 * @brief Inizializza il sistema di produzione in modalità factory.
 *
 * Questa funzione configura il sistema per l'operazione in modalità factory,
 * preparando tutti i componenti necessari per la produzione di massa.
 *
 * @return esp_err_t
 * - ESP_OK: Operazione completata con successo.
 * - ESP_FAIL: Operazione fallita.
 */
static void init_bsp_i2c_bus_with_log(void)
{
#if defined(CONFIG_BSP_I2C_NUM)
  ESP_LOGI(TAG,
           "[M] [I2C] Avvio init bus I2C BSP Monitor (port=%d SDA=%d SCL=%d)",
           BSP_I2C_NUM, BSP_I2C_SDA, BSP_I2C_SCL);
  esp_err_t i2c_ret = bsp_i2c_init();
  if (i2c_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "[M] [I2C] Init bus I2C BSP fallita: %s",
             esp_err_to_name(i2c_ret));
  }
  else
  {
    ESP_LOGI(TAG, "[M] [I2C] Bus I2C BSP inizializzato correttamente");
  }
#else
  ESP_LOGW(TAG, "[M] [I2C] CONFIG_BSP_I2C_NUM non definito: init bus I2C BSP "
                "non disponibile");
#endif
}

static esp_err_t init_periph_i2c_bus_with_log(void)
{
  ESP_LOGI(TAG,
           "[M] [IOX] Avvio init Periferiche su bus I2C (port=%d SDA=%d SCL=%d)",
           CONFIG_APP_I2C_PORT, CONFIG_APP_I2C_SDA_GPIO,
           CONFIG_APP_I2C_SCL_GPIO);
  esp_err_t periph_ret = periph_i2c_init();
  if (periph_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "[M] [I2C] Init bus periferiche fallita: %s",
             esp_err_to_name(periph_ret));
  }
  else
  {
    ESP_LOGI(TAG, "[M] [I2C] Bus periferiche pronto su GPIO%d/GPIO%d",
             CONFIG_APP_I2C_SCL_GPIO, CONFIG_APP_I2C_SDA_GPIO);
  }
  return periph_ret;
}

static void init_eeprom_on_periph_i2c(void)
{
  if (periph_i2c_get_handle() == NULL)
  {
    ESP_LOGW(TAG,
             "[M] EEPROM 24LC16 non inizializzata: bus periferiche non disponibile");
    init_agent_status_set(AGN_ID_EEPROM, 0, INIT_AGENT_ERR_DEPENDENCY_FAILED);
    return;
  }

  ESP_LOGI(TAG,
           "[M] Inizializzo EEPROM 24LC16 su periph_i2c (port=%d, GPIO%d SCL, "
           "GPIO%d SDA)",
           CONFIG_APP_I2C_PORT, CONFIG_APP_I2C_SCL_GPIO,
           CONFIG_APP_I2C_SDA_GPIO);
  esp_err_t eeprom_ret = eeprom_24lc16_init();
  if (eeprom_ret != ESP_OK)
  {
    ESP_LOGW(TAG, "[M] EEPROM init fallita: %s", esp_err_to_name(eeprom_ret));
    init_agent_status_set(AGN_ID_EEPROM, 0, INIT_AGENT_ERR_INIT_FAILED);
    return;
  }

  if (eeprom_24lc16_is_available())
  {
    ESP_LOGI(TAG, "[M] EEPROM 24LC16 pronta");
    init_agent_status_set(AGN_ID_EEPROM, 1, INIT_AGENT_ERR_NONE);
  }
  else
  {
    ESP_LOGW(TAG, "[M] EEPROM 24LC16 non rilevata sul bus periferiche");
    init_agent_status_set(AGN_ID_EEPROM, 1, INIT_AGENT_ERR_NOT_AVAILABLE);
  }
}

esp_err_t init_run_factory(void)
{
  init_agent_status_reset_defaults();
  s_http_services_initial_token_done = false;

  ESP_ERROR_CHECK(init_nvs());
  ESP_ERROR_CHECK(update_boot_reboot_guard());
  ESP_ERROR_CHECK(update_crash_pending_record());

  ESP_ERROR_CHECK(init_spiffs());
  log_partitions();
  ESP_ERROR_CHECK(init_event_loop());

  (void)init_periph_i2c_bus_with_log();

  // Lascia assestare i livelli elettrici (pull-up hardware/software) per
  // evitare che la FSM I2C si blocchi al primissimo start e dia "I2C software
  // timeout".
  vTaskDelay(pdMS_TO_TICKS(100));

  // Suppress I2C probe timeout logs to WARN level
  esp_log_level_set("i2c.master", ESP_LOG_WARN);

  // Scansione diagnostica bus periferiche (port 0, GPIO 26/27) - timeout
  // ridotto a 20 ticks (il prolungato mascherava timeout fisici)
  generic_i2c_diagnostic_scan(periph_i2c_get_handle(), "Periph I2C", 20);

  init_bsp_i2c_bus_with_log();
#if defined(CONFIG_BSP_I2C_NUM)
  // Scansione diagnostica bus BSP (port 1, GPIO 7/8) - timeout breve (20 ticks
  // = ~20ms)
  generic_i2c_diagnostic_scan(bsp_i2c_get_handle(), "BSP I2C", 20);
#endif

  /* Inizializzazione EEPROM separata dalla persistenza config,
   * utile per diagnostica e API test dedicate. */
  init_eeprom_on_periph_i2c();

  // Inizializza configurazione device PRIMA degli altri moduli applicativi
  ESP_ERROR_CHECK(device_config_init());
  init_agent_status_set(AGN_ID_DEVICE_CONFIG, 1, INIT_AGENT_ERR_NONE);
  apply_display_override_from_dip1(device_config_get());
  log_peripherals_from_config(device_config_get());

  // Inizializza Remote Logging il prima possibile per catturare i log pre-rete
#if !defined(DNA_REMOTE_LOGGING) || (DNA_REMOTE_LOGGING == 0)
  {
    esp_err_t rl_ret = remote_logging_init();
    if (rl_ret != ESP_OK)
    {
      ESP_LOGW(TAG,
               "[M] remote_logging_init early failed (%s), continuo senza "
               "remote logs",
               esp_err_to_name(rl_ret));
      init_agent_status_set(AGN_ID_REMOTE_LOGGING, 0,
                            INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      ESP_LOGI(TAG,
               "[M] remote_logging early init attivo (log pre-rete catturati)");
      init_agent_status_set(AGN_ID_REMOTE_LOGGING, 1, INIT_AGENT_ERR_NONE);
    }
  }
#else
  ESP_LOGI(TAG, "[M] remote_logging: disabilitato (DNA_REMOTE_LOGGING=1)");
  init_agent_status_set(AGN_ID_REMOTE_LOGGING, 1, INIT_AGENT_ERR_NOT_AVAILABLE);
#endif

  /* prepare the FSM mailbox early so that any code executing during
   * initialization (event handlers, helper tasks, etc.) can publish
   * events without races or drops. the call is idempotent thanks to the
   * improved fsm_event_queue_init above. this was the main cause of the
   * mysterious "boot slower/blocked" behaviour: some pieces attempted to
   * send an event before the daemon task had started and our old init
   * reentrant logic would reset the mailbox repeatedly.
   */
  if (!fsm_event_queue_init(0))
  {
    init_agent_status_set(AGN_ID_FSM, 0, INIT_AGENT_ERR_INIT_FAILED);
    return ESP_FAIL;
  }
  init_agent_status_set(AGN_ID_FSM, 1, INIT_AGENT_ERR_NONE);

  if (s_error_lock_active)
  {
    return ESP_ERR_INVALID_STATE;
  }

  // Inizializza GPIO ausiliari
  aux_gpio_init();
  init_agent_status_set(AGN_ID_AUX_GPIO, 1, INIT_AGENT_ERR_NONE);

  // Display + LVGL (minimal screen) - skip se headless
  device_config_t *cfg = device_config_get();

  if (cfg)
  {
    if (!cfg->scanner.enabled)
    {
      ESP_LOGW(TAG, "[M] Scanner QR disabilitato in config: forzo abilitazione all'avvio");
      cfg->scanner.enabled = true;
    }

    if (!cfg->sensors.cctalk_enabled)
    {
      ESP_LOGW(TAG, "[M] CCTALK disabilitato in config: forzo abilitazione all'avvio");
      cfg->sensors.cctalk_enabled = true;
    }
  }
#if 0
#if !COMPILE_APP
    /* In FACTORY il display deve rimanere disponibile anche con config salvata headless */
    if (!cfg->display.enabled) {
        ESP_LOGW(TAG, "[F] Display disabilitato in config: forzo abilitazione runtime in FACTORY");
    }
    cfg->display.enabled = true;
    if (cfg->display.lcd_brightness == 0) {
        cfg->display.lcd_brightness = 80;
        ESP_LOGI(TAG, "[F] Luminosità display a 0: imposto fallback runtime a 80%% in FACTORY");
    }
#endif
#endif

#if FORCE_VIDEO_DISABLED
  /* Override runtime: blocca ogni inizializzazione video in questa build */
  if (cfg->display.enabled)
  {
    ESP_LOGW(TAG,
             "[M] FORCE_VIDEO_DISABLED attivo: disabilito display da runtime");
  }
  cfg->display.enabled = false;
#endif

  s_display_ready = false;

  if (cfg->display.enabled)
  {
    esp_err_t disp_ret = init_display_lvgl_minimal();
    if (disp_ret != ESP_OK)
    {
      ESP_LOGW(TAG, "[M] Display/LVGL init failed: %s",
               esp_err_to_name(disp_ret));
      init_agent_status_set(AGN_ID_LVGL, 0, INIT_AGENT_ERR_INIT_FAILED);
      init_agent_status_set(AGN_ID_WAVESHARE_LCD, 0,
                            INIT_AGENT_ERR_INIT_FAILED);
      init_agent_status_set(AGN_ID_TOUCH, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      init_agent_status_set(AGN_ID_LVGL, 1, INIT_AGENT_ERR_NONE);
      init_agent_status_set(AGN_ID_WAVESHARE_LCD, 1, INIT_AGENT_ERR_NONE);
      if (s_touch_handle)
      {
        init_agent_status_set(AGN_ID_TOUCH, 1, INIT_AGENT_ERR_NONE);
      }
      else
      {
        init_agent_status_set(AGN_ID_TOUCH, 0, INIT_AGENT_ERR_INIT_FAILED);
      }
      s_display_ready = true;
      init_lvgl_status_log("Init: display e touch pronti");
    }
  }
  else
  {
    ESP_LOGI(TAG, "[M] Display disabilitato da config: salto init LVGL/display "
                  "(modalità headless)");
    s_touch_handle = NULL;
    tasks_set_touchscreen_handle(NULL);
    init_agent_status_set(AGN_ID_LVGL, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
    init_agent_status_set(AGN_ID_WAVESHARE_LCD, 1,
                          INIT_AGENT_ERR_DISABLED_BY_CONFIG);
    init_agent_status_set(AGN_ID_TOUCH, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (http_services_is_remote_enabled())
  {
    init_agent_status_set(AGN_ID_HTTP_SERVICES, 1,
                          INIT_AGENT_ERR_NETWORK_NO_IP);
  }
  else
  {
    init_agent_status_set(AGN_ID_HTTP_SERVICES, 1,
                          INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  // Ethernet - continua anche se fallisce
  init_lvgl_status_log("Init: inizializzazione rete");
  if (cfg->eth.enabled)
  {
    esp_err_t eth_ret =
        start_ethernet(); // TODO: aggiornare start_ethernet per usare cfg
    if (eth_ret != ESP_OK)
    {
      ESP_LOGW(TAG, "[M] Ethernet non disponibile, continuo senza");
    }
  }
  else
  {
    ESP_LOGI(TAG, "[M] Ethernet disabilitato da config");
  }

  // Invio crash record solo dopo che la rete è stata inizializzata
  ESP_ERROR_CHECK(try_send_pending_crash_record());

  // Inizializza monitoraggio seriale per i test
  serial_test_init();
  init_lvgl_status_log("Init: monitor seriale pronto");

  // Inizializza e avvia Web UI (Server + Handler)
  init_lvgl_status_log("Init: avvio Web UI");
  esp_err_t web_ret = web_ui_init();
  if (web_ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] web_ui_init fallita: %s", esp_err_to_name(web_ret));
    init_agent_status_set(AGN_ID_WEB_UI, 0, INIT_AGENT_ERR_INIT_FAILED);
    return web_ret;
  }
  init_agent_status_set(AGN_ID_WEB_UI, 1, INIT_AGENT_ERR_NONE);
  ESP_LOGI(TAG, "[M] Web UI avviata correttamente");
  init_lvgl_status_log("Init: Web UI avviata");

#if COMPILE_APP
  // Carica la tabella activity da SPIFFS (solo build APP)
  init_lvgl_status_log("Init: caricamento activity");
  esp_err_t activity_ret = device_activity_init();
  if (activity_ret != ESP_OK)
  {
    init_agent_status_set(AGN_ID_DEVICE_ACTIVITY, 0,
                          INIT_AGENT_ERR_INIT_FAILED);
    return activity_ret;
  }
  init_agent_status_set(AGN_ID_DEVICE_ACTIVITY, 1, INIT_AGENT_ERR_NONE);
#endif

  // Inizializzazioni condizionali basate su NVS
  if (cfg->display.enabled)
    lvgl_panel_set_init_status("Init: periferiche hardware");
  init_lvgl_status_log("Init: periferiche hardware");

  // Imposta flag abilitazione IO expander prima di qualsiasi operazione
  io_expander_set_config_enabled(cfg->sensors.io_expander_enabled);

  if (cfg->sensors.io_expander_enabled)
  {
    // La porta I2C è già inizializzata sopra (BSP o legacy), io_expander_init
    // la riutilizzerà
    esp_err_t exp_ret = io_expander_init();
    if (exp_ret != ESP_OK)
    {
      ESP_LOGW(TAG,
               "[M] I/O Expander non disponibile o errore (%s): proseguo senza "
               "bloccare l'esecuzione",
               esp_err_to_name(exp_ret));
      cfg->sensors.io_expander_enabled = false;
      init_agent_status_set(AGN_ID_IO_EXPANDER, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    if (exp_ret == ESP_OK)
    {
      init_agent_status_set(AGN_ID_IO_EXPANDER, 1, INIT_AGENT_ERR_NONE);
    }
  }
  else
  {
    ESP_LOGI(TAG, "I/O Expander disabilitato da config");
    init_agent_status_set(AGN_ID_IO_EXPANDER, 1,
                          INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (cfg->sensors.led_enabled)
  {
    esp_err_t led_ret = led_init();
    if (led_ret != ESP_OK)
    {
      ESP_LOGE(TAG, "[M] Inizializzazione LED fallita (%s): periferica disabilitata "
                    "a runtime",
               esp_err_to_name(led_ret));
      cfg->sensors.led_enabled = false;
      init_agent_status_set(AGN_ID_LED, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      init_agent_status_set(AGN_ID_LED, 1, INIT_AGENT_ERR_NONE);
      // Ferma eventuali task LED test rainbow attivi all'avvio
      led_test_stop();
      ESP_LOGI(TAG, "[M] LED test rainbow fermato all'avvio");
    }
  }
  else
  {
    ESP_LOGI(TAG, "LED Strip disabilitato da config");
    init_agent_status_set(AGN_ID_LED, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (cfg->sensors.rs232_enabled)
  {
    esp_err_t rs232_ret = rs232_init();
    if (rs232_ret != ESP_OK)
    {
      ESP_LOGE(TAG,
               "[M] Inizializzazione RS232 fallita (%s): periferica "
               "disabilitata a runtime",
               esp_err_to_name(rs232_ret));
      cfg->sensors.rs232_enabled = false;
      init_agent_status_set(AGN_ID_RS232, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      init_agent_status_set(AGN_ID_RS232, 1, INIT_AGENT_ERR_NONE);
    }
  }
  else
  {
    ESP_LOGI(TAG, "UART RS232 disabilitato da config");
    init_agent_status_set(AGN_ID_RS232, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (cfg->sensors.cctalk_enabled)
  {
    esp_err_t cctalk_ret = cctalk_driver_init();
    if (cctalk_ret != ESP_OK)
    {
      ESP_LOGW(TAG, "CCTALK driver non avviato: %s",
               esp_err_to_name(cctalk_ret));
      init_agent_status_set(AGN_ID_CCTALK, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      init_agent_status_set(AGN_ID_CCTALK, 1, INIT_AGENT_ERR_NONE);
    }
  }
  else
  {
    init_agent_status_set(AGN_ID_CCTALK, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (cfg->sensors.rs485_enabled)
  {
    if (cfg->modbus.enabled)
    {
      ESP_LOGI(
          TAG,
          "[M] RS485 gestita da Modbus: init UART differita al task rs485");
      init_agent_status_set(AGN_ID_RS485, 1, INIT_AGENT_ERR_NONE);
    }
    else
    {
      esp_err_t rs485_ret = rs485_init();
      if (rs485_ret != ESP_OK)
      {
        ESP_LOGE(TAG,
                 "[M] Inizializzazione RS485 fallita (%s): periferica "
                 "disabilitata a runtime",
                 esp_err_to_name(rs485_ret));
        cfg->sensors.rs485_enabled = false;
        init_agent_status_set(AGN_ID_RS485, 0, INIT_AGENT_ERR_INIT_FAILED);
      }
      else
      {
        init_agent_status_set(AGN_ID_RS485, 1, INIT_AGENT_ERR_NONE);
      }
    }
  }
  else
  {
    ESP_LOGI(TAG, "UART RS485 disabilitato da config");
    init_agent_status_set(AGN_ID_RS485, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (cfg->sensors.mdb_enabled)
  {
    /* mdb_init() inizializza solo l'hardware UART.
     * Il task di polling (mdb_engine) è avviato da tasks_start_all() via
     * s_tasks[]. */
    esp_err_t mdb_ret = mdb_init();

    if (mdb_ret != ESP_OK)
    {
      ESP_LOGE(TAG,
               "[M] Inizializzazione MDB fallita (%s): periferica disabilitata "
               "a runtime",
               esp_err_to_name(mdb_ret));
      cfg->sensors.mdb_enabled = false;
      init_agent_status_set(AGN_ID_MDB, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      init_agent_status_set(AGN_ID_MDB, 1, INIT_AGENT_ERR_NONE);
    }
  }
  else
  {
    ESP_LOGI(TAG, "MDB Engine disabilitato da config");
    init_agent_status_set(AGN_ID_MDB, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (cfg->sensors.pwm1_enabled || cfg->sensors.pwm2_enabled)
  {
    esp_err_t pwm_ret = pwm_init();
    if (pwm_ret != ESP_OK)
    {
      ESP_LOGE(TAG,
               "[M] Inizializzazione PWM fallita (%s): PWM1/PWM2 disabilitati "
               "a runtime",
               esp_err_to_name(pwm_ret));
      cfg->sensors.pwm1_enabled = false;
      cfg->sensors.pwm2_enabled = false;
      init_agent_status_set(AGN_ID_PWM1, 0, INIT_AGENT_ERR_INIT_FAILED);
      init_agent_status_set(AGN_ID_PWM2, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      init_agent_status_set(AGN_ID_PWM1, 1,
                            cfg->sensors.pwm1_enabled
                                ? INIT_AGENT_ERR_NONE
                                : INIT_AGENT_ERR_DISABLED_BY_CONFIG);
      init_agent_status_set(AGN_ID_PWM2, 1,
                            cfg->sensors.pwm2_enabled
                                ? INIT_AGENT_ERR_NONE
                                : INIT_AGENT_ERR_DISABLED_BY_CONFIG);
    }
  }
  else
  {
    ESP_LOGI(TAG, "PWM Hardware disabilitato da config");
    init_agent_status_set(AGN_ID_PWM1, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
    init_agent_status_set(AGN_ID_PWM2, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  if (cfg->sensors.temperature_enabled)
  {
    esp_err_t sht_ret = sht40_init();
    if (sht_ret != ESP_OK)
    {
      ESP_LOGE(TAG, "[M] Inizializzazione SHT40 fallita! Sensore temperatura "
                    "disabilitato a runtime");
      cfg->sensors.temperature_enabled = false;
      init_agent_status_set(AGN_ID_SHT40, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      init_agent_status_set(AGN_ID_SHT40, 1, INIT_AGENT_ERR_NONE);
    }
  }
  else
  {
    init_agent_status_set(AGN_ID_SHT40, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  /* Il task sd_monitor è avviato da tasks_start_all() via s_tasks[]
   * (sd_monitor_wrapper). sd_card_init_monitor() è mantenuta per
   * retrocompatibilità ma è ora una no-op. */

  if (cfg->sensors.sd_card_enabled)
  {
    esp_err_t sd_ret = sd_card_mount();
    if (sd_ret != ESP_OK)
    {
      ESP_LOGE(TAG,
               "[M] Inizializzazione SD Card fallita (%s): periferica "
               "disabilitata a runtime",
               esp_err_to_name(sd_ret));
      cfg->sensors.sd_card_enabled = false;
      init_agent_status_set(AGN_ID_SD_CARD, 0, INIT_AGENT_ERR_INIT_FAILED);
    }
    else
    {
      (void)transfer_coredump_flash_to_sd();
      init_agent_status_set(AGN_ID_SD_CARD, 1, INIT_AGENT_ERR_NONE);
    }
  }
  else
  {
    ESP_LOGI(TAG, "[M] SD Card disabilitata da config");
    init_agent_status_set(AGN_ID_SD_CARD, 1, INIT_AGENT_ERR_DISABLED_BY_CONFIG);
  }

  return ESP_OK;
}

/**
 * @brief Scansione diagnostica generica per qualsiasi bus I2C.
 *
 * Scandisce tutti gli indirizzi I2C sul bus fornito e registra i dispositivi
 * trovati. Esegue la scansione una sola volta per bus per evitare timeout
 * ripetuti.
 *
 * @param [in] bus Handle del bus I2C master da scansionare.
 * @param [in] bus_name Nome descrittivo del bus (es. "BSP I2C", "Periph I2C").
 * @param [in] timeout_ticks Timeout in ticks per ogni probe (20 ticks ~ 20ms).
 * @return Nessun valore di ritorno.
 */
static void generic_i2c_diagnostic_scan(i2c_master_bus_handle_t bus,
                                        const char *bus_name,
                                        int timeout_ticks)
{
  // Usa un flag statico per eseguire una sola volta (basato sul bus_name)
  static bool s_bsp_scanned = false;
  static bool s_periph_scanned = false;

  // Determina quale flag usare in base al bus_name
  bool *scan_flag =
      (strstr(bus_name, "BSP") != NULL) ? &s_bsp_scanned : &s_periph_scanned;

  if (*scan_flag)
  {
    return; // Già scansionato una volta
  }
  *scan_flag = true;

  if (bus == NULL)
  {
    ESP_LOGI(TAG, "[C][I2C-DIAG] bus %s non disponibile", bus_name);
    return;
  }

  ESP_LOGI(TAG, "[C][I2C-DIAG] scan I2C %s (indirizzo 0x03-0x77)", bus_name);
  int found = 0;
  for (uint8_t addr = 0x03; addr < 0x78; addr++)
  {
    esp_err_t probe_ret = i2c_master_probe(bus, addr, timeout_ticks);
    if (probe_ret == ESP_OK)
    {
      ESP_LOGI(TAG, "[C][I2C-DIAG] trovato device @0x%02X", addr);
      found++;
    }
  }

  if (found == 0)
  {
    ESP_LOGI(TAG, "[C][I2C-DIAG] Nessun device trovato su %s", bus_name);
  }
  else
  {
    ESP_LOGI(TAG, "[C][I2C-DIAG] totale device trovati: %d", found);
  }
}

/**
 * @brief Inizializza e restituisce un handle per la gestione di un LED strip
 * WS2812.
 *
 * @return led_strip_handle_t Handle per la gestione del LED strip WS2812.
 */
led_strip_handle_t init_get_ws2812_handle(void) { return led_get_handle(); }

/**
 * @brief Inizializza i puntatori ai diversi tipi di interfaccie di rete.
 *
 * Questa funzione inizializza i puntatori ai diversi tipi di interfaccie di
 * rete disponibili nel sistema, come le reti access point (AP), le reti di
 * accesso (STA) e le reti Ethernet (ETH).
 *
 * @param [out] ap Puntatore al puntatore all'interfaccia di rete AP.
 * @param [out] sta Puntatore al puntatore all'interfaccia di rete STA.
 * @param [out] eth Puntatore al puntatore all'interfaccia di rete Ethernet.
 *
 * @return Nessun valore di ritorno.
 */
void init_get_netifs(esp_netif_t **ap, esp_netif_t **sta, esp_netif_t **eth)
{
  if (ap)
    *ap = s_netif_ap;
  if (sta)
    *sta = s_netif_sta;
  if (eth)
    *eth = s_netif_eth;
}

/** @brief Inizializza il bus I2C e l'estenders I/O.
 *
 *  Questa funzione configura il bus I2C e l'estenders I/O per consentire la
 * comunicazione con dispositivi collegati.
 *
 *  @param [in] Nessun parametro di input.
 *  @return Nessun valore di ritorno.
 */
void init_i2c_and_io_expander(void)
{
  init_bsp_i2c_bus_with_log();
  (void)init_periph_i2c_bus_with_log();
  init_eeprom_on_periph_i2c();

  esp_err_t exp_ret = io_expander_init();
  if (exp_ret != ESP_OK)
  {
    ESP_LOGW(TAG,
             "[M] [IOX] I/O Expander non disponibile o errore (%s): proseguo "
             "senza bloccare l'esecuzione",
             esp_err_to_name(exp_ret));
    return;
  }
  ESP_LOGI(TAG, "[M] [IOX] I/O Expander inizializzato correttamente");

  ESP_LOGI(TAG, "************************************************************");
  ESP_LOGI(TAG, "[M] Build v%s | APP_DATE=%s | COMPILER_TS=%s %s", APP_VERSION,
           APP_DATE, __DATE__, __TIME__);
  ESP_LOGI(TAG, "************************************************************");

  // Log riepilogativo sezioni DNA mock attive
  {
    char dna[192] = "";
#if defined(DNA_SD_CARD) && (DNA_SD_CARD == 1)
    strcat(dna, " SD_CARD");
#endif
#if defined(DNA_IO_EXPANDER) && (DNA_IO_EXPANDER == 1)
    strcat(dna, " IO_EXPANDER");
#endif
#if defined(DNA_LED_STRIP) && (DNA_LED_STRIP == 1)
    strcat(dna, " LED_STRIP");
#endif
#if defined(DNA_SHT40) && (DNA_SHT40 == 1)
    strcat(dna, " SHT40");
#endif
#if defined(DNA_RS232) && (DNA_RS232 == 1)
    strcat(dna, " RS232");
#endif
#if defined(DNA_RS485) && (DNA_RS485 == 1)
    strcat(dna, " RS485");
#endif
#if defined(DNA_CCTALK) && (DNA_CCTALK == 1)
    strcat(dna, " CCTALK");
#endif
#if defined(DNA_PWM) && (DNA_PWM == 1)
    strcat(dna, " PWM");
#endif
#if defined(DNA_TOUCHSCREEN) && (DNA_TOUCHSCREEN == 1)
    strcat(dna, " TOUCHSCREEN");
#endif
#if defined(DNA_REMOTE_LOGGING) && (DNA_REMOTE_LOGGING == 1)
    strcat(dna, " REMOTE_LOGGING");
#endif
#if defined(DNA_ETHERNET) && (DNA_ETHERNET == 1)
    strcat(dna, " ETHERNET");
#endif
#if defined(DNA_WIFI) && (DNA_WIFI == 1)
    strcat(dna, " WIFI");
#endif
#if defined(DNA_LVGL) && (DNA_LVGL == 1)
    strcat(dna, " LVGL");
#endif
#if defined(DNA_GPIO) && (DNA_GPIO == 1)
    strcat(dna, " GPIO");
#endif
#if defined(DNA_MDB) && (DNA_MDB == 1)
    strcat(dna, " MDB");
#endif
#if defined(DNA_USB_SCANNER) && (DNA_USB_SCANNER == 1)
    strcat(dna, " SCANNER");
#endif
    if (dna[0] == '\0')
      strcat(dna, " (nessuna)");
    ESP_LOGI(TAG, "[M] Sezioni DNA mock attive:%s", dna);
  }
}

/**
 * @brief Controlla se l'errore di blocco è attivo.
 *
 * Questa funzione verifica lo stato del blocco di errore.
 *
 * @return true se il blocco di errore è attivo, false altrimenti.
 */
bool init_is_error_lock_active(void) { return s_error_lock_active; }

/**
 * @brief Inizializza la funzione per ottenere il conteggio di riavvii
 * consecutivi.
 *
 * @return uint32_t Il conteggio di riavvii consecutivi.
 */
uint32_t init_get_consecutive_reboots(void) { return s_consecutive_reboots; }

/**
 * @brief Inizializza la variabile di boot completato.
 *
 * Questa funzione imposta la variabile di stato per indicare che il boot del
 * sistema è stato completato.
 *
 * @return esp_err_t
 *         - ESP_OK: operazione riuscita
 *         - ESP_FAIL: operazione fallita
 */
esp_err_t init_mark_boot_completed(void)
{
  nvs_handle_t handle;
  esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] [C] apertura NVS boot_completed fallita: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ret = nvs_set_u32(handle, BOOT_GUARD_KEY_CONSEC, 0);
  if (ret == ESP_OK)
  {
    ret = nvs_commit(handle);
  }
  nvs_close(handle);

  if (ret == ESP_OK)
  {
    s_consecutive_reboots = 0;
    s_error_lock_active = false;
    ESP_LOGI(TAG,
             "[M] [C] boot completato: contatore reboot consecutivi azzerato");
  }

  return ret;
}

/**
 * @brief Inizializza una richiesta di forzato crash.
 *
 * Questa funzione inizializza una richiesta di forzato crash, preparando il
 * sistema per un crash imminente. Questo può essere utilizzato per testare la
 * gestione del crash o per altri scopi specifici.
 *
 * @return esp_err_t - Codice di errore che indica il successo o la fallita
 * dell'operazione.
 */
esp_err_t init_mark_forced_crash_request(void)
{
  nvs_handle_t handle;
  esp_err_t ret = nvs_open(BOOT_GUARD_NAMESPACE, NVS_READWRITE, &handle);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "[M] [C] apertura NVS force_crash fallita: %s",
             esp_err_to_name(ret));
    return ret;
  }

  ret = nvs_set_u32(handle, BOOT_GUARD_KEY_FORCE_CRASH, 1);
  if (ret == ESP_OK)
  {
    ret = nvs_commit(handle);
  }
  nvs_close(handle);

  if (ret == ESP_OK)
  {
    ESP_LOGW(TAG, "[M] [C] marker force_crash registrato");
  }
  return ret;
}
