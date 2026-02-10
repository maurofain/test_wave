#include "remote_logging.h"
#include "device_config.h"
#include "web_ui.h"
#include <esp_log.h>
#include <esp_netif.h>
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <netinet/in.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#define TAG "REMOTE_LOG"
#define LOG_QUEUE_SIZE 50
#define LOG_MESSAGE_MAX_LEN 512
#define LOG_TASK_STACK_SIZE 4096
#define LOG_TASK_PRIORITY 5

// Struttura per i messaggi di log
typedef struct {
    char level[8];
    char tag[32];
    char message[LOG_MESSAGE_MAX_LEN];
    time_t timestamp;
} log_message_t;

// Variabili globali
static QueueHandle_t log_queue = NULL;
static TaskHandle_t log_task_handle = NULL;
static int sock_fd = -1;
static bool initialized = false;
static vprintf_like_t original_vprintf = NULL; // Per salvare la funzione originale

/**
 * @brief Converte il livello ESP_LOG in stringa
 */
static const char* esp_log_level_to_string(esp_log_level_t level)
{
    switch (level) {
        case ESP_LOG_NONE:    return "NONE";
        case ESP_LOG_ERROR:   return "ERROR";
        case ESP_LOG_WARN:    return "WARN";
        case ESP_LOG_INFO:    return "INFO";
        case ESP_LOG_DEBUG:   return "DEBUG";
        case ESP_LOG_VERBOSE: return "VERBOSE";
        default:              return "UNKNOWN";
    }
}

/**
 * @brief Funzione di logging personalizzata che intercetta tutti i log ESP-IDF
 */
static int custom_vprintf(const char *fmt, va_list args)
{
    // Prima chiama la funzione originale per output su console/uart
    int result = original_vprintf(fmt, args);

    // Se il remote logging è abilitato, invia anche al server remoto
    if (remote_logging_is_enabled()) {
        char buffer[LOG_MESSAGE_MAX_LEN];
        vsnprintf(buffer, sizeof(buffer), fmt, args);

        // Rimuovi newline finale se presente
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }

        // Invia al server remoto (usa livello INFO come default)
        remote_logging_send("INFO", "ESP_LOG", buffer);
    }

    return result;
}

/**
 * @brief Task per l'invio dei log al server remoto
 */
static void log_sender_task(void *pvParameters)
{
    log_message_t log_msg;
    device_config_t *cfg = device_config_get();

    ESP_LOGI(TAG, "Task di invio log avviato");

    while (true) {
        // Ricevi messaggio dalla coda
        if (xQueueReceive(log_queue, &log_msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // Verifica se il logging remoto è ancora abilitato
        cfg = device_config_get();
        if (!cfg->remote_log.enabled) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Salva sempre localmente nel web UI per visualizzazione diretta
        web_ui_add_log(log_msg.level, log_msg.tag, log_msg.message);

        // Invia anche remotamente se configurato (IP non vuoto)
        if (cfg->remote_log.enabled && strlen(cfg->remote_log.server_ip) > 0 && strcmp(cfg->remote_log.server_ip, "localhost") != 0 && strcmp(cfg->remote_log.server_ip, "127.0.0.1") != 0) {
        
        // Crea il messaggio formattato per l'invio remoto
        char formatted_msg[LOG_MESSAGE_MAX_LEN + 128];
        struct tm timeinfo;
        localtime_r(&log_msg.timestamp, &timeinfo);

        // Formato: [YYYY-MM-DD HH:MM:SS] LEVEL TAG: MESSAGE
        strftime(formatted_msg, sizeof(formatted_msg),
                "[%Y-%m-%d %H:%M:%S] ", &timeinfo);

        size_t len = strlen(formatted_msg);
        snprintf(formatted_msg + len, sizeof(formatted_msg) - len,
                "%s %s: %s\n", log_msg.level, log_msg.tag, log_msg.message);
        
        if (cfg->remote_log.use_udp) {
            // UDP
            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(cfg->remote_log.server_port);
            
            // Usa broadcast o IP specifico
            if (cfg->remote_log.use_broadcast) {
                server_addr.sin_addr.s_addr = INADDR_BROADCAST;
                ESP_LOGD(TAG, "Invio in broadcast UDP alla porta %d", cfg->remote_log.server_port);
            } else {
                inet_pton(AF_INET, cfg->remote_log.server_ip, &server_addr.sin_addr);
                ESP_LOGD(TAG, "Invio UDP a %s:%d", cfg->remote_log.server_ip, cfg->remote_log.server_port);
            }

            if (sendto(sock_fd, formatted_msg, strlen(formatted_msg), 0,
                      (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                ESP_LOGW(TAG, "Errore invio UDP: %s", strerror(errno));
            }
        } else {
            // TCP - per ora non implementato, usa UDP
            ESP_LOGW(TAG, "TCP non ancora implementato, usando UDP");
            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(cfg->remote_log.server_port);
            inet_pton(AF_INET, cfg->remote_log.server_ip, &server_addr.sin_addr);

            if (sendto(sock_fd, formatted_msg, strlen(formatted_msg), 0,
                      (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                ESP_LOGW(TAG, "Errore invio TCP: %s", strerror(errno));
            }
        }
        } // Fine blocco invio remoto
    }
}

/**
 * @brief Inizializza il socket UDP
 */
static esp_err_t init_socket(void)
{
    device_config_t *cfg = device_config_get();

    if (cfg->remote_log.use_udp) {
        // Socket UDP
        sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock_fd < 0) {
            ESP_LOGE(TAG, "Errore creazione socket UDP: %s", strerror(errno));
            return ESP_FAIL;
        }

        // Abilita broadcast se necessario
        int broadcast = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
            ESP_LOGW(TAG, "Impossibile abilitare broadcast sul socket: %s", strerror(errno));
        }

        // Imposta timeout per non bloccare
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000; // 100ms timeout
        setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        ESP_LOGI(TAG, "Socket UDP inizializzato per %s:%d",
                cfg->remote_log.server_ip, cfg->remote_log.server_port);
    } else {
        // TCP - per ora usa UDP
        ESP_LOGW(TAG, "TCP non implementato, usando UDP");
        sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock_fd < 0) {
            ESP_LOGE(TAG, "Errore creazione socket TCP: %s", strerror(errno));
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t remote_logging_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    device_config_t *cfg = device_config_get();
    if (!cfg->remote_log.enabled) {
        ESP_LOGI(TAG, "Logging remoto disabilitato");
        return ESP_OK;
    }

    // Crea la coda per i messaggi
    log_queue = xQueueCreate(LOG_QUEUE_SIZE, sizeof(log_message_t));
    if (log_queue == NULL) {
        ESP_LOGE(TAG, "Errore creazione coda log");
        return ESP_FAIL;
    }

    // Inizializza il socket
    if (init_socket() != ESP_OK) {
        return ESP_FAIL;
    }

    // Crea il task di invio
    if (xTaskCreate(log_sender_task, "log_sender", LOG_TASK_STACK_SIZE,
                   NULL, LOG_TASK_PRIORITY, &log_task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Errore creazione task log");
        vQueueDelete(log_queue);
        close(sock_fd);
        return ESP_FAIL;
    }

    // Installa la funzione di logging personalizzata per intercettare tutti i log ESP-IDF
    original_vprintf = esp_log_set_vprintf(custom_vprintf);

    initialized = true;
    ESP_LOGI(TAG, "Logging remoto inizializzato - intercettazione log ESP-IDF attiva");
    return ESP_OK;
}

esp_err_t remote_logging_send(const char *level, const char *tag, const char *message)
{
    if (!initialized || !remote_logging_is_enabled()) {
        return ESP_OK; // Non è un errore se disabilitato
    }

    log_message_t log_msg;
    strncpy(log_msg.level, level, sizeof(log_msg.level) - 1);
    strncpy(log_msg.tag, tag, sizeof(log_msg.tag) - 1);
    strncpy(log_msg.message, message, sizeof(log_msg.message) - 1);
    log_msg.timestamp = time(NULL);

    if (xQueueSend(log_queue, &log_msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Coda log piena, messaggio perso");
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool remote_logging_is_enabled(void)
{
    device_config_t *cfg = device_config_get();
    return cfg && cfg->remote_log.enabled && initialized;
}

void remote_logging_stop(void)
{
    if (!initialized) {
        return;
    }

    // Ripristina la funzione di logging originale
    if (original_vprintf) {
        esp_log_set_vprintf(original_vprintf);
        original_vprintf = NULL;
    }

    if (log_task_handle) {
        vTaskDelete(log_task_handle);
        log_task_handle = NULL;
    }

    if (log_queue) {
        vQueueDelete(log_queue);
        log_queue = NULL;
    }

    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }

    initialized = false;
    ESP_LOGI(TAG, "Logging remoto fermato");
}