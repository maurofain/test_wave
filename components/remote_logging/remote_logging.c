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
#define LOG_QUEUE_SIZE 50  // default queue length; reduced dynamically if heap low
#define LOG_MESSAGE_MAX_LEN 512
#define LOG_TASK_STACK_SIZE 12288  /* increased to avoid stack smash when handling large log messages */
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
// Helper to log heap statistics at various points
static void dump_heap(const char *ctx)
{
    size_t internal = heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    size_t dma = heap_caps_get_free_size(MALLOC_CAP_DMA);
    size_t spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "[HEAP] %s - internal=%u, dma=%u, spiram=%u",
             ctx, (unsigned)internal, (unsigned)dma, (unsigned)spiram);
}
static TaskHandle_t log_task_handle = NULL;
static int sock_fd = -1;
static bool initialized = false;
static vprintf_like_t original_vprintf = NULL; // Per salvare la funzione originale
static volatile bool s_in_custom_vprintf = false;

// Per limitare i log di errore UDP
static int udp_error_count = 0;
static TickType_t last_udp_error_time = 0;

/**
 * @brief Converte il livello ESP_LOG in stringa
 */
/**
 * @brief Converte un livello ESP_LOG in stringa
 *
 * Funzione di utilità usata internamente; non esportata.
 */
__attribute__((unused)) static const char* esp_log_level_to_string(esp_log_level_t level)
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
/**
 * @brief Funzione di logging personalizzata interceptando esp_log
 *
 * Questa routine viene installata con esp_log_set_vprintf() e inoltra i
 * messaggi al logger originale. Se il modulo è inizializzato inoltra anche
 * i messaggi formattati alla coda di log per la trasmissione remota.
 */
static int custom_vprintf(const char *fmt, va_list args)
{
    /* Copia args per il formatting locale: original_vprintf potrebbe consumare la
       va_list e quindi non possiamo riutilizzarla. */
    char buffer[LOG_MESSAGE_MAX_LEN] = {0};
    va_list ap;
    va_copy(ap, args);
    vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    /* Chiama la funzione originale per l'output su console/uart */
    int result = original_vprintf ? original_vprintf(fmt, args) : vprintf(fmt, args);

    if (!initialized || log_queue == NULL) {
        return result;
    }

    /* Evita ricorsione quando il task sender genera log interni. */
    if (log_task_handle != NULL && xTaskGetCurrentTaskHandle() == log_task_handle) {
        return result;
    }

    /* Guard globale anti-ricorsione (es. coda piena, errori interni, ecc.). */
    if (s_in_custom_vprintf) {
        return result;
    }

    /* Invia al server remoto usando la copia già formattata */
    if (remote_logging_is_enabled()) {
        s_in_custom_vprintf = true;
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        remote_logging_send("INFO", "ESP_LOG", buffer);
        s_in_custom_vprintf = false;
    }

    return result;
}

/**
 * @brief Task per l'invio dei log al server remoto
 */
/**
 * @brief Task che invia i messaggi della coda al server remoto
 *
 * Estrae dalla coda log_message_t, copia i messaggi nella UI locale e
 * invia i pacchetti UDP in broadcast se abilitato. Limita lo spam di errori
 * tramite un contatore temporizzato.
 */
static void log_sender_task(void *pvParameters)
{
    log_message_t log_msg;
    device_config_t *cfg = device_config_get();

    while (true) {
        // Ricevi messaggio dalla coda
        if (xQueueReceive(log_queue, &log_msg, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // Verifica configurazione
        cfg = device_config_get();

        // Salva sempre localmente nel web UI per visualizzazione diretta
        web_ui_add_log(log_msg.level, log_msg.tag, log_msg.message);

        // Invia in broadcast UDP se abilitato
        if (cfg->remote_log.use_broadcast) {
        
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
        
        // Invia in broadcast UDP se abilitato
        if (cfg->remote_log.use_broadcast) {
            struct sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(cfg->remote_log.server_port);
            server_addr.sin_addr.s_addr = INADDR_BROADCAST;
            
            if (sendto(sock_fd, formatted_msg, strlen(formatted_msg), 0,
                      (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
                // Limita i log di errore per evitare spam
                udp_error_count++;
                TickType_t now = xTaskGetTickCount();
                
                // Logga solo ogni 10 errori o ogni 30 secondi
                if (udp_error_count >= 10 || 
                    (now - last_udp_error_time) > pdMS_TO_TICKS(30000)) {
                    udp_error_count = 0; // Reset contatore
                    last_udp_error_time = now;
                }
            } else {
                // Reset contatore se invio riuscito
                if (udp_error_count > 0) {
                    udp_error_count = 0;
                }
            }
        }
        } // Fine blocco invio remoto
    }
}

/**
 * @brief Inizializza il socket UDP
 */
/**
 * @brief Inizializza il socket UDP per il broadcast dei log
 *
 * Restituisce ESP_OK se il socket è stato creato e configurato
 * correttamente; in caso contrario ritorna ESP_FAIL.
 */
static esp_err_t init_socket(void)
{
    device_config_t *cfg = device_config_get();

    // Reset del contatore errori UDP quando si reinizializza il socket
    udp_error_count = 0;
    last_udp_error_time = 0;

    // Crea sempre socket UDP per broadcast
    sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_fd < 0) {
        ESP_LOGE(TAG, "Errore creazione socket UDP: %s", strerror(errno));
        return ESP_FAIL;
    }

    // Abilita broadcast
    int broadcast = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        ESP_LOGW(TAG, "Impossibile abilitare broadcast sul socket: %s", strerror(errno));
    }

    // Imposta timeout per non bloccare
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms timeout
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    ESP_LOGI(TAG, "Socket UDP inizializzato per broadcast alla porta %d",
            cfg->remote_log.server_port);

    return ESP_OK;
}

/**
 * @brief Avvia il modulo di logging remoto
 *
 * Questo è il punto d'ingresso pubblico. Viene creata una coda di messaggi
 * (ridimensionata automaticamente se necessario), un socket UDP per il
 * broadcast ed un task di invio. Inoltre sostituisce il vprintf di ESP‑IDF
 * per intercettare automaticamente tutti i log.
 *
 * La funzione è idempotente: se il modulo è già inizializzato ritorna ESP_OK
 * immediatamente.
 *
 * @return ESP_OK se il modulo è pronto, ESP_FAIL in caso di errori critici
 *         (ad esempio impossibilità di creare la coda e allocare socket).
 */
esp_err_t remote_logging_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    // Crea sempre la coda per i messaggi (logging locale sempre attivo)
    dump_heap("before queue alloc");
    int queue_len = LOG_QUEUE_SIZE;
    while (queue_len > 0) {
        log_queue = xQueueCreate(queue_len, sizeof(log_message_t));
        if (log_queue != NULL) break;
        ESP_LOGW(TAG, "queue alloc (%d) fallita, heap dopo fallimento:", queue_len);
        dump_heap("after failed alloc");
        queue_len /= 2; // dimezza e riprova
    }
    if (log_queue == NULL) {
        ESP_LOGE(TAG, "Errore creazione coda log: memoria insufficiente, logging locale disabilitato");
        /* Non è critico: continueremo comunque a funzionare senza coda.  Il
           custom_vprintf e le funzioni di invio verificano log_queue==NULL
           e semplicemente ignorano i messaggi in uscita.  Ritorniamo ESP_OK
           così il chiamante può proseguire senza abort() e non blocchiamo il
           boot per un problema di heap momentaneo. */
        initialized = true; /* evita che la funzione venga ripetuta */
        return ESP_OK;
    }
    dump_heap("after queue alloc");

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

/**
 * @brief Mette un messaggio nella coda per l'invio remoto
 *
 * Questa API è utilizzata internamente da custom_vprintf ma può essere
 * chiamata direttamente da altri moduli per inviare messaggi personalizzati.
 * Se la coda non esiste o il modulo non è inizializzato la chiamata è un no‑op.
 *
 * @param level  livello del log (es. "INFO")
 * @param tag    tag associato al log
 * @param message testo del log
 * @return ESP_OK se il messaggio è stato accodato, ESP_FAIL in caso di errore
 */
esp_err_t remote_logging_send(const char *level, const char *tag, const char *message)
{
    if (!initialized || log_queue == NULL) {
        return ESP_OK; // Non è un errore se non inizializzato
    }

    log_message_t log_msg;
    strncpy(log_msg.level, level, sizeof(log_msg.level) - 1);
    log_msg.level[sizeof(log_msg.level) - 1] = '\0';
    strncpy(log_msg.tag, tag, sizeof(log_msg.tag) - 1);
    log_msg.tag[sizeof(log_msg.tag) - 1] = '\0';
    strncpy(log_msg.message, message, sizeof(log_msg.message) - 1);
    log_msg.message[sizeof(log_msg.message) - 1] = '\0';
    log_msg.timestamp = time(NULL);

    if (xQueueSend(log_queue, &log_msg, 0) != pdTRUE) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool remote_logging_is_enabled(void)
{
    // Il logging locale è sempre attivo quando il componente è inizializzato
    return initialized;
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
