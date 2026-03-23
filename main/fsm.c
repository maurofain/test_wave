#include "fsm.h"
#include "tasks.h"
#include "device_config.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "FSM_MB";

/* Abilita il logging dettagliato delle operazioni sulla mailbox (ENQUEUE/DEQUEUE).
 * Di default disabilitato: abilitare per debug della coda messaggi. */
#define LOG_QUEUE

#define FSM_DEFAULT_SPLASH_TIMEOUT_MS (30000U)
#define FSM_DEFAULT_PAUSE_MAX_MS (60000U)
#define FSM_DEFAULT_EVENT_QUEUE_LEN (32U)
#define FSM_DEFAULT_AD_ROTATION_MS (30000U)
#define FSM_DEFAULT_CREDIT_RESET_TIMEOUT_MS (300000U)
#define FSM_CENTS_PER_CREDIT (100)
#define FSM_MAILBOX_TTL_MS (1100U)

/* mailbox-based event bus replaces the old FreeRTOS queue */
static SemaphoreHandle_t s_fsm_pending_lock = NULL;
static SemaphoreHandle_t s_fsm_runtime_lock = NULL;
static fsm_ctx_t s_fsm_runtime_ctx = {0};
static bool s_fsm_runtime_valid = false;
static char s_fsm_pending_msgs[FSM_DEFAULT_EVENT_QUEUE_LEN][FSM_EVENT_TEXT_MAX_LEN] = {{0}};
static size_t s_fsm_pending_head = 0;
static size_t s_fsm_pending_count = 0;

#define FSM_MAILBOX_SIZE 32

typedef struct {
    fsm_input_event_t ev;
    uint32_t to_mask;
    uint32_t enqueue_timestamp_ms;
} mailbox_slot_t;

static mailbox_slot_t s_mailbox[FSM_MAILBOX_SIZE];
static size_t s_mb_head = 0;
static size_t s_mb_tail = 0;
static SemaphoreHandle_t s_mb_mutex = NULL;
/* #7 fix: counting semaphore — segnala ai receiver che c'è almeno un
 * messaggio in coda, eliminando il polling ogni 1ms. */
static SemaphoreHandle_t s_mb_signal = NULL;

static uint32_t to_mask_from_event(const fsm_input_event_t *e);
static const char *fsm_input_event_type_to_string(fsm_input_event_type_t type);
static void fsm_reset_runtime_locked(fsm_ctx_t *ctx);
static void fsm_prepare_open_session(fsm_ctx_t *ctx, fsm_session_source_t source);
static void fsm_prepare_virtual_locked_session(fsm_ctx_t *ctx, fsm_session_source_t source);
static bool fsm_try_charge_program_cycle(fsm_ctx_t *ctx, int32_t cost);
static bool fsm_try_autorenew_running_program(fsm_ctx_t *ctx);
static void fsm_add_credit_from_cents(fsm_ctx_t *ctx,
                                      int32_t amount_cents,
                                      bool is_ecd,
                                      const char *source_tag);
static size_t fsm_mailbox_drop_expired_locked(uint32_t now_ms);

static size_t fsm_mailbox_drop_expired_locked(uint32_t now_ms)
{
    size_t dropped = 0;

    for (size_t i = s_mb_head; i != s_mb_tail; i = (i + 1) % FSM_MAILBOX_SIZE) {
        if (s_mailbox[i].to_mask == 0U) {
            continue;
        }

        uint32_t age_ms = now_ms - s_mailbox[i].enqueue_timestamp_ms;
        if (age_ms > FSM_MAILBOX_TTL_MS) {
#ifdef LOG_QUEUE
            ESP_LOGW(TAG,
                     "DROP_EXPIRED[%zu] type=%s action=%d from=%d age_ms=%lu",
                     i,
                     fsm_input_event_type_to_string(s_mailbox[i].ev.type),
                     (int)s_mailbox[i].ev.action,
                     (int)s_mailbox[i].ev.from,
                     (unsigned long)age_ms);
#endif
            s_mailbox[i].to_mask = 0U;
            dropped++;
        }
    }

    while (s_mb_head != s_mb_tail && s_mailbox[s_mb_head].to_mask == 0U) {
        s_mb_head = (s_mb_head + 1) % FSM_MAILBOX_SIZE;
    }

    return dropped;
}

static void fsm_add_credit_from_cents(fsm_ctx_t *ctx,
                                      int32_t amount_cents,
                                      bool is_ecd,
                                      const char *source_tag)
{
    if (!ctx || amount_cents <= 0) {
        return;
    }

    int32_t *bucket_credits = is_ecd ? &ctx->ecd_coins : &ctx->vcd_coins;
    int32_t *bucket_residual = is_ecd ? &ctx->ecd_cents_residual : &ctx->vcd_cents_residual;
    const char *bucket_name = is_ecd ? "ecd" : "vcd";

    int32_t total_cents = *bucket_residual + amount_cents;
    int32_t credits_add = total_cents / FSM_CENTS_PER_CREDIT;
    int32_t cents_residual = total_cents % FSM_CENTS_PER_CREDIT;

    *bucket_residual = cents_residual;
    if (credits_add > 0) {
        *bucket_credits += credits_add;
        ctx->credit_cents += credits_add;
    }

    ESP_LOGI(TAG,
             "[M] [ADD_CREDIT] src=%s type=%s in_cents=%ld add_credits=%ld residual_cents=%ld ecd=%ld vcd=%ld credit=%ld",
             source_tag ? source_tag : "payment",
             bucket_name,
             (long)amount_cents,
             (long)credits_add,
             (long)cents_residual,
             (long)ctx->ecd_coins,
             (long)ctx->vcd_coins,
             (long)ctx->credit_cents);
}

static void fsm_reset_runtime_locked(fsm_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }
    ctx->program_running = false;
    ctx->running_elapsed_ms = 0;
    ctx->pause_elapsed_ms = 0;
    ctx->pause_limit_reached = false;
    ctx->running_target_ms = 0;
    ctx->running_price_units = 0;
    memset(ctx->running_program_name, 0, sizeof(ctx->running_program_name));
    ctx->inactivity_ms = 0;
    ctx->stop_after_cycle_requested = false;
    ctx->pre_fine_ciclo_active = false;
}

static void fsm_prepare_open_session(fsm_ctx_t *ctx, fsm_session_source_t source)
{
    if (!ctx) {
        return;
    }
    ctx->session_source = source;
    ctx->session_mode = FSM_SESSION_MODE_OPEN_PAYMENTS;
    ctx->allow_additional_payments = true;
    if (ctx->state == FSM_STATE_IDLE || ctx->state == FSM_STATE_ADS) {
        ctx->state = FSM_STATE_CREDIT;
        ctx->inactivity_ms = 0;
    }
}

static void fsm_prepare_virtual_locked_session(fsm_ctx_t *ctx, fsm_session_source_t source)
{
    if (!ctx) {
        return;
    }
    ctx->session_source = source;
    ctx->session_mode = FSM_SESSION_MODE_VIRTUAL_LOCKED;
    ctx->allow_additional_payments = false;
    if (ctx->state == FSM_STATE_IDLE || ctx->state == FSM_STATE_ADS) {
        ctx->state = FSM_STATE_CREDIT;
        ctx->inactivity_ms = 0;
    }
}

static bool fsm_try_charge_program_cycle(fsm_ctx_t *ctx, int32_t cost)
{
    if (!ctx || cost <= 0 || ctx->credit_cents < cost) {
        return false;
    }
    int32_t f_ecd = (ctx->ecd_coins >= cost) ? cost : ctx->ecd_coins;
    int32_t f_vcd = cost - f_ecd;
    if (f_vcd > ctx->vcd_coins) {
        return false;
    }
    ctx->ecd_coins    -= f_ecd;
    ctx->vcd_coins    -= f_vcd;
    ctx->ecd_used     += f_ecd;
    ctx->vcd_used     += f_vcd;
    ctx->credit_cents -= cost;
    ESP_LOGI(TAG, "[M] [USE_CREDIT] cost=%ld from_ecd=%ld from_vcd=%ld ecd_rem=%ld vcd_rem=%ld credit=%ld",
             (long)cost, (long)f_ecd, (long)f_vcd,
             (long)ctx->ecd_coins, (long)ctx->vcd_coins, (long)ctx->credit_cents);
    return true;
}

static bool fsm_try_autorenew_running_program(fsm_ctx_t *ctx)
{
    if (!ctx || ctx->running_price_units <= 0) {
        return false;
    }

    if (ctx->stop_after_cycle_requested) {
        fsm_append_message("Stop a fine ciclo richiesto: rinnovo automatico disabilitato");
        return false;
    }

    /* Se il saldo reale (ECD+VCD) è esaurito, blocca il rinnovo automatico. */
    if ((ctx->ecd_coins + ctx->vcd_coins) <= 0) {
        ctx->credit_cents = 0;
        return false;
    }

    if (!fsm_try_charge_program_cycle(ctx, ctx->running_price_units)) {
        return false;
    }
    ctx->running_elapsed_ms = 0;
    ctx->pause_elapsed_ms = 0;
    ctx->pause_limit_reached = false;
    ctx->program_running = true;
    ctx->pre_fine_ciclo_active = false;
    fsm_append_message("Rinnovo automatico programma");
    return true;
}


/**
 * @brief Converte un evento di input in una maschera.
 *
 * Questa funzione prende un evento di input e lo converte in una maschera
 * utilizzabile per la gestione degli stati di un automa finito.
 *
 * @param [in] e Puntatore all'evento di input da convertire.
 * @return Maschera corrispondente all'evento di input.
 */
static uint32_t to_mask_from_event(const fsm_input_event_t *e)
{
    uint32_t mask = 0;
    if (!e) {
        return 0;
    }
    for (int i = 0; i < 10; ++i) {
        if (e->to[i] != AGN_ID_NONE) {
            mask |= (1u << e->to[i]);
        } else {
            break;
        }
    }
    if (mask == 0) {
        /* backwards compatibility: deliver to FSM when no recipient given */
        mask = (1u << AGN_ID_FSM);
    }
    return mask;
}

static bool fsm_publish_control_event(fsm_event_t ev)
{
    fsm_input_event_t event = {
        .from = AGN_ID_WEB_UI,
        .to = {AGN_ID_FSM},
        .action = (ev == FSM_EVENT_ENTER_LVGL_TEST) ? ACTION_ID_LVGL_TEST_ENTER : ACTION_ID_LVGL_TEST_EXIT,
        .type = FSM_INPUT_EVENT_NONE,
    };
    event.value_i32 = (int32_t)ev;
    return fsm_event_publish(&event, pdMS_TO_TICKS(20));
}

bool fsm_enter_lvgl_pages_test(void)
{
    fsm_ctx_t snap = {0};
    if (fsm_runtime_snapshot(&snap) && snap.state == FSM_STATE_LVGL_PAGES_TEST) {
        return true;
    }
    return fsm_publish_control_event(FSM_EVENT_ENTER_LVGL_TEST);
}

bool fsm_exit_lvgl_pages_test(void)
{
    return fsm_publish_control_event(FSM_EVENT_EXIT_LVGL_TEST);
}

static const char *fsm_input_event_type_to_string(fsm_input_event_type_t type)
{
    switch (type) {
        case FSM_INPUT_EVENT_USER_ACTIVITY: return "user_activity";
        case FSM_INPUT_EVENT_TOUCH: return "touch";
        case FSM_INPUT_EVENT_KEY: return "key";
        case FSM_INPUT_EVENT_COIN: return "coin";
        case FSM_INPUT_EVENT_TOKEN: return "token";
        case FSM_INPUT_EVENT_CARD_CREDIT: return "card_credit";
        case FSM_INPUT_EVENT_QR_CREDIT: return "qr_credit";
        case FSM_INPUT_EVENT_QR_SCANNED: return "qr_scanned";
        case FSM_INPUT_EVENT_PROGRAM_SELECTED: return "program_selected";
        case FSM_INPUT_EVENT_PROGRAM_STOP: return "program_stop";
        case FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE: return "program_pause_toggle";
        case FSM_INPUT_EVENT_CREDIT_ENDED: return "credit_ended";
        case FSM_INPUT_EVENT_PROGRAM_SWITCH: return "program_switch";
        case FSM_INPUT_EVENT_NONE:
        default:
            return "none";
    }
}


/**
 * @brief Aggiunge un messaggio alla coda di messaggi pendenti in modalità locked.
 * 
 * @param [in] message Puntatore al messaggio da aggiungere. Deve essere diverso da NULL.
 * @return Nessun valore di ritorno.
 */
static void fsm_pending_push_text_locked(const char *message)
{
    if (!message) {
        return;
    }

    size_t idx = (s_fsm_pending_head + s_fsm_pending_count) % FSM_DEFAULT_EVENT_QUEUE_LEN;
    char *dst = s_fsm_pending_msgs[idx];
    snprintf(dst, FSM_EVENT_TEXT_MAX_LEN, "%s", message);

    if (s_fsm_pending_count < FSM_DEFAULT_EVENT_QUEUE_LEN) {
        s_fsm_pending_count++;
    } else {
        s_fsm_pending_head = (s_fsm_pending_head + 1) % FSM_DEFAULT_EVENT_QUEUE_LEN;
    }
}


/**
 * @brief Inserisce un evento di input nella coda di eventi pendenti.
 * 
 * @param [in] event Puntatore all'evento di input da inserire.
 * @return void Nessun valore di ritorno.
 */
static void fsm_pending_push(const fsm_input_event_t *event)
{
    if (!s_fsm_pending_lock || !event) {
        return;
    }

    if (xSemaphoreTake(s_fsm_pending_lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    char msg[FSM_EVENT_TEXT_MAX_LEN] = {0};
    snprintf(msg, FSM_EVENT_TEXT_MAX_LEN, "%.20s v=%ld %.28s",
             fsm_input_event_type_to_string(event->type),
             (long)event->value_i32,
             event->text[0] ? event->text : "");
    fsm_pending_push_text_locked(msg);

    xSemaphoreGive(s_fsm_pending_lock);
}


/**
 * @brief Inizializza il contesto del finite state machine.
 * 
 * @param [in/out] ctx Puntatore al contesto del finite state machine da inizializzare.
 * @return Nessun valore di ritorno.
 * 
 * Questa funzione inizializza il contesto del finite state machine utilizzando il puntatore fornito.
 * Se il puntatore è nullo, la funzione non ha alcun effetto.
 */
void fsm_init(fsm_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    ctx->state = FSM_STATE_IDLE;
    ctx->session_source = FSM_SESSION_SOURCE_NONE;
    ctx->session_mode = FSM_SESSION_MODE_NONE;
    ctx->credit_cents = 0;
    ctx->ecd_coins = 0;
    ctx->vcd_coins = 0;
    ctx->ecd_used  = 0;
    ctx->vcd_used  = 0;
    ctx->ecd_cents_residual = 0;
    ctx->vcd_cents_residual = 0;
    ctx->program_running = false;
    ctx->running_elapsed_ms = 0;
    ctx->pause_elapsed_ms = 0;
    ctx->pause_max_ms = FSM_DEFAULT_PAUSE_MAX_MS;
    ctx->pause_limit_reached = false;
    ctx->running_target_ms = 0;
    ctx->running_price_units = 0;
    memset(ctx->running_program_name, 0, sizeof(ctx->running_program_name));
    ctx->inactivity_ms = 0;
    ctx->splash_screen_time_ms = FSM_DEFAULT_SPLASH_TIMEOUT_MS;
    ctx->ads_rotation_ms = FSM_DEFAULT_AD_ROTATION_MS;
    ctx->credit_reset_timeout_ms = FSM_DEFAULT_CREDIT_RESET_TIMEOUT_MS;
    ctx->ads_enabled = true;
    ctx->allow_additional_payments = false;
    ctx->stop_after_cycle_requested = false;
    ctx->pre_fine_ciclo_active = false;
}


/**
 * @brief Gestisce un evento per lo stato finito.
 * 
 * @param [in] ctx Contesto del finite state machine.
 * @param [in] event Evento da gestire.
 * @return true Se l'evento è stato gestito con successo.
 * @return false Se l'evento non è stato gestito o se il contesto è nullo.
 */
bool fsm_handle_event(fsm_ctx_t *ctx, fsm_event_t event)
{
    if (!ctx || event == FSM_EVENT_NONE) {
        return false;
    }

    fsm_state_t prev = ctx->state;

    if (event == FSM_EVENT_ENTER_LVGL_TEST) {
        if (ctx->state != FSM_STATE_LVGL_PAGES_TEST) {
            tasks_suspend_peripherals_for_lvgl_test();
            ctx->state = FSM_STATE_LVGL_PAGES_TEST;
            ctx->program_running = false;
        }
        return prev != ctx->state;
    }

    if (event == FSM_EVENT_EXIT_LVGL_TEST) {
        if (ctx->state == FSM_STATE_LVGL_PAGES_TEST) {
            tasks_resume_peripherals_after_lvgl_test();
            ctx->state = FSM_STATE_IDLE;
        }
        return prev != ctx->state;
    }

    switch (ctx->state) {
        case FSM_STATE_IDLE:
        case FSM_STATE_ADS:
            if (event == FSM_EVENT_USER_ACTIVITY || event == FSM_EVENT_PAYMENT_ACCEPTED) {
                ctx->state = FSM_STATE_CREDIT;
                fsm_reset_runtime_locked(ctx);
                ctx->inactivity_ms = 0;
            }
            break;

        case FSM_STATE_CREDIT:
            if (event == FSM_EVENT_PROGRAM_SELECTED) {
                ctx->state = FSM_STATE_RUNNING;
                ctx->program_running = true;
                ctx->running_elapsed_ms = 0;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->inactivity_ms = 0;
            } else if (event == FSM_EVENT_USER_ACTIVITY) {
                ctx->inactivity_ms = 0;
            } else if (event == FSM_EVENT_TIMEOUT) {
                /* Timeout scelta programma:
                 * trattiene ECD, azzera VCD e torna in IDLE. */
                int32_t retained_ecd = (ctx->ecd_coins > 0) ? ctx->ecd_coins : 0;
                fsm_reset_runtime_locked(ctx);
                ctx->ecd_coins = retained_ecd;
                ctx->vcd_coins = 0;
                ctx->credit_cents = retained_ecd;
                ctx->ecd_used = 0;
                ctx->vcd_used = 0;
                ctx->ecd_cents_residual = 0;
                ctx->vcd_cents_residual = 0;
                ctx->session_mode = FSM_SESSION_MODE_NONE;
                ctx->session_source = FSM_SESSION_SOURCE_NONE;
                ctx->allow_additional_payments = false;
                ctx->state = FSM_STATE_IDLE;
                ctx->inactivity_ms = 0;
            } else if (event == FSM_EVENT_PAYMENT_ACCEPTED) {
                ctx->inactivity_ms = 0;
            }
            break;

        case FSM_STATE_RUNNING:
            if (event == FSM_EVENT_PROGRAM_PAUSE) {
                ctx->state = FSM_STATE_PAUSED;
                ctx->program_running = false;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->inactivity_ms = 0;
            } else if (event == FSM_EVENT_PROGRAM_STOP || event == FSM_EVENT_CREDIT_ENDED) {
                fsm_reset_runtime_locked(ctx);
                ctx->session_mode = FSM_SESSION_MODE_NONE;
                ctx->session_source = FSM_SESSION_SOURCE_NONE;
                ctx->allow_additional_payments = false;
                ctx->state = FSM_STATE_IDLE;
                ctx->inactivity_ms = 0;
            }
            break;

        case FSM_STATE_PAUSED:
            if (event == FSM_EVENT_PROGRAM_RESUME) {
                ctx->state = FSM_STATE_RUNNING;
                ctx->program_running = true;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->inactivity_ms = 0;
            } else if (event == FSM_EVENT_PROGRAM_STOP || event == FSM_EVENT_CREDIT_ENDED) {
                fsm_reset_runtime_locked(ctx);
                ctx->session_mode = FSM_SESSION_MODE_NONE;
                ctx->session_source = FSM_SESSION_SOURCE_NONE;
                ctx->allow_additional_payments = false;
                ctx->state = FSM_STATE_IDLE;
                ctx->inactivity_ms = 0;
            }
            break;

        case FSM_STATE_LVGL_PAGES_TEST:
            /* In modalità test ignoriamo gli eventi normali; le transizioni
             * sono gestite da ENTER/EXIT sopra. */
            break;

        default:
            break;
    }

    return prev != ctx->state;
}


/**
 * @brief Gestisce un evento di input per la macchina a stati finiti.
 * 
 * @param [in] ctx Puntatore al contesto della macchina a stati finiti.
 * @param [in] event Puntatore all'evento di input da gestire.
 * @return true Se l'evento è stato gestito con successo.
 * @return false Se l'evento non è stato gestito o se i parametri sono invalidi.
 */
bool fsm_handle_input_event(fsm_ctx_t *ctx, const fsm_input_event_t *event)
{
    if (!ctx || !event) {
        return false;
    }

    /* #12 fix: bridge action_id → fsm_input_event_type per i nuovi produttori
     * che impostano .action invece di (o in aggiunta a) .type.
     * Se .type è già impostato, ha priorità; .action viene usato solo come
     * fallback, permettendo la migrazione graduale senza rompere i caller
     * esistenti. */
    fsm_input_event_type_t etype = event->type;
    if (etype == FSM_INPUT_EVENT_NONE && event->action != ACTION_ID_NONE) {
        switch (event->action) {
            case ACTION_ID_USER_ACTIVITY:          etype = FSM_INPUT_EVENT_USER_ACTIVITY;        break;
            case ACTION_ID_PAYMENT_ACCEPTED:       etype = FSM_INPUT_EVENT_COIN;                 break;
            case ACTION_ID_PROGRAM_SELECTED:       etype = FSM_INPUT_EVENT_PROGRAM_SELECTED;     break;
            case ACTION_ID_PROGRAM_STOP:           etype = FSM_INPUT_EVENT_PROGRAM_STOP;         break;
            case ACTION_ID_PROGRAM_PAUSE_TOGGLE:   etype = FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE; break;
            case ACTION_ID_CREDIT_ENDED:           etype = FSM_INPUT_EVENT_CREDIT_ENDED;         break;
            case ACTION_ID_LVGL_TEST_ENTER:        return fsm_handle_event(ctx, FSM_EVENT_ENTER_LVGL_TEST);
            case ACTION_ID_LVGL_TEST_EXIT:         return fsm_handle_event(ctx, FSM_EVENT_EXIT_LVGL_TEST);
            default: break;
        }
    }

    switch (etype) {
        case FSM_INPUT_EVENT_USER_ACTIVITY:
        case FSM_INPUT_EVENT_TOUCH:
            fsm_prepare_open_session(ctx, FSM_SESSION_SOURCE_TOUCH);
            return fsm_handle_event(ctx, FSM_EVENT_USER_ACTIVITY);

        case FSM_INPUT_EVENT_KEY:
            fsm_prepare_open_session(ctx, FSM_SESSION_SOURCE_KEY);
            return fsm_handle_event(ctx, FSM_EVENT_USER_ACTIVITY);

        case FSM_INPUT_EVENT_QR_SCANNED:
            return fsm_handle_event(ctx, FSM_EVENT_USER_ACTIVITY);

        case FSM_INPUT_EVENT_COIN:
            if (!ctx->allow_additional_payments && ctx->session_mode == FSM_SESSION_MODE_VIRTUAL_LOCKED) {
                fsm_append_message("Pagamento aggiuntivo non consentito");
                return false;
            }
            fsm_prepare_open_session(ctx, FSM_SESSION_SOURCE_COIN);
            if (event->value_i32 > 0) {
                fsm_add_credit_from_cents(ctx,
                                          event->value_i32,
                                          true,
                                          (event->text[0] != '\0') ? event->text : "coin");
            }
            return fsm_handle_event(ctx, FSM_EVENT_PAYMENT_ACCEPTED);

        case FSM_INPUT_EVENT_TOKEN:
            if (!ctx->allow_additional_payments && ctx->session_mode == FSM_SESSION_MODE_VIRTUAL_LOCKED) {
                fsm_append_message("Pagamento aggiuntivo non consentito");
                return false;
            }
            fsm_prepare_open_session(ctx, FSM_SESSION_SOURCE_COIN);
            if (event->value_i32 > 0) {
                fsm_add_credit_from_cents(ctx,
                                          event->value_i32,
                                          true,
                                          (event->text[0] != '\0') ? event->text : "token");
            }
            return fsm_handle_event(ctx, FSM_EVENT_PAYMENT_ACCEPTED);

        case FSM_INPUT_EVENT_QR_CREDIT:
            if (ctx->session_mode != FSM_SESSION_MODE_NONE &&
                ctx->session_mode != FSM_SESSION_MODE_VIRTUAL_LOCKED) {
                fsm_append_message("Pagamento aggiuntivo non consentito");
                return false;
            }
            fsm_prepare_virtual_locked_session(ctx, FSM_SESSION_SOURCE_QR);
            if (event->value_i32 > 0) {
                fsm_add_credit_from_cents(ctx,
                                          event->value_i32,
                                          false,
                                          (event->text[0] != '\0') ? event->text : "qr_vcd");
            }
            return fsm_handle_event(ctx, FSM_EVENT_PAYMENT_ACCEPTED);

        case FSM_INPUT_EVENT_CARD_CREDIT: {
            if (ctx->session_mode != FSM_SESSION_MODE_NONE &&
                ctx->session_mode != FSM_SESSION_MODE_VIRTUAL_LOCKED) {
                fsm_append_message("Pagamento aggiuntivo non consentito");
                return false;
            }
            fsm_prepare_virtual_locked_session(ctx, FSM_SESSION_SOURCE_CARD);
            if (event->value_i32 > 0) {
                fsm_add_credit_from_cents(ctx,
                                          event->value_i32,
                                          false,
                                          (event->text[0] != '\0') ? event->text : "card_vcd");
            }
            return fsm_handle_event(ctx, FSM_EVENT_PAYMENT_ACCEPTED);
        }

        case FSM_INPUT_EVENT_PROGRAM_SELECTED:
            if (ctx->state != FSM_STATE_CREDIT) {
                return false;
            }

            int32_t prev_credit_cents = ctx->credit_cents;
            int32_t prev_ecd_coins = ctx->ecd_coins;
            int32_t prev_vcd_coins = ctx->vcd_coins;
            int32_t prev_ecd_used = ctx->ecd_used;
            int32_t prev_vcd_used = ctx->vcd_used;
            uint32_t prev_pause_max_ms = ctx->pause_max_ms;
            uint32_t prev_running_target_ms = ctx->running_target_ms;
            int32_t prev_running_price_units = ctx->running_price_units;
            char prev_running_program_name[FSM_EVENT_TEXT_MAX_LEN] = {0};
            snprintf(prev_running_program_name,
                     sizeof(prev_running_program_name),
                     "%s",
                     ctx->running_program_name);

            if (ctx->credit_cents <= 0) {
                fsm_append_message("Credito a zero: selezione programma non consentita");
                return false;
            }

            if (event->value_u32 > 0) {
                ctx->pause_max_ms = event->value_u32;
            }

            if (event->aux_u32 > 0) {
                ctx->running_target_ms = event->aux_u32;
            }

            ctx->running_price_units = (event->value_i32 > 0) ? event->value_i32 : 0;
            snprintf(ctx->running_program_name, sizeof(ctx->running_program_name), "%s", event->text[0] ? event->text : "programma");

            bool charged = false;
            if (ctx->running_price_units > 0) {
                if (ctx->credit_cents < ctx->running_price_units) {
                    fsm_append_message("Credito insufficiente per avvio programma");
                    return false;
                }
                {
                    if (!fsm_try_charge_program_cycle(ctx, ctx->running_price_units)) {
                        fsm_append_message("Credito insufficiente per avvio programma");
                        return false;
                    }
                    charged = true;
                }
            }

            if (!fsm_handle_event(ctx, FSM_EVENT_PROGRAM_SELECTED)) {
                if (charged) {
                    ctx->credit_cents = prev_credit_cents;
                    ctx->ecd_coins = prev_ecd_coins;
                    ctx->vcd_coins = prev_vcd_coins;
                    ctx->ecd_used = prev_ecd_used;
                    ctx->vcd_used = prev_vcd_used;
                }
                ctx->pause_max_ms = prev_pause_max_ms;
                ctx->running_target_ms = prev_running_target_ms;
                ctx->running_price_units = prev_running_price_units;
                snprintf(ctx->running_program_name,
                         sizeof(ctx->running_program_name),
                         "%s",
                         prev_running_program_name);
                fsm_append_message("Avvio programma fallito: credito ripristinato");
                return false;
            }

            return true;

        case FSM_INPUT_EVENT_PROGRAM_STOP:
            if (event->aux_u32 == 1U &&
                (ctx->state == FSM_STATE_RUNNING || ctx->state == FSM_STATE_PAUSED)) {
                ctx->stop_after_cycle_requested = true;
                fsm_append_message("Stop a fine ciclo richiesto");
                if (ctx->state == FSM_STATE_PAUSED) {
                    ESP_LOGI(TAG, "[M] Stop a fine ciclo confermato in pausa: ripresa countdown");
                    return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_RESUME);
                }
                return true;
            }
            return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_STOP);

        case FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE:
            if (ctx->state == FSM_STATE_RUNNING) {
                return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_PAUSE);
            }
            if (ctx->state == FSM_STATE_PAUSED) {
                return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_RESUME);
            }
            break;

        case FSM_INPUT_EVENT_CREDIT_ENDED:
            ctx->credit_cents = 0;
            ctx->ecd_coins = 0;
            ctx->vcd_coins = 0;
            ctx->ecd_used  = 0;
            ctx->vcd_used  = 0;
            ctx->ecd_cents_residual = 0;
            ctx->vcd_cents_residual = 0;
            return fsm_handle_event(ctx, FSM_EVENT_CREDIT_ENDED);

        case FSM_INPUT_EVENT_PROGRAM_SWITCH: {
            /* cambio programma a macchina running/paused:
             * scala il tempo residuo al rateo del nuovo programma;
             * se era in pausa, riprende l'esecuzione */
            if (ctx->state != FSM_STATE_RUNNING && ctx->state != FSM_STATE_PAUSED) {
                return false;
            }
            bool was_paused = (ctx->state == FSM_STATE_PAUSED);
            uint32_t new_dur_ms  = event->aux_u32;   /* duration_sec * 1000 nuovo prg */
            uint32_t old_dur_ms  = ctx->running_target_ms;
            uint32_t elapsed     = ctx->running_elapsed_ms;
            uint32_t rem_old_ms  = (old_dur_ms > elapsed) ? (old_dur_ms - elapsed) : 0;
            /* scala per rateo: rem_new = rem_old * (new_dur/new_price) / (old_dur/old_price)
             * = rem_old * new_dur * old_price / (old_dur * new_price) */
            int32_t old_price = (ctx->running_price_units > 0) ? ctx->running_price_units : 1;
            int32_t new_price = (event->value_i32 > 0) ? event->value_i32 : 1;
            uint32_t rem_new_ms  = (old_dur_ms > 0 && new_dur_ms > 0)
                                   ? (uint32_t)((uint64_t)rem_old_ms * new_dur_ms * (uint32_t)old_price
                                                / ((uint64_t)old_dur_ms * (uint32_t)new_price))
                                   : new_dur_ms;
            /* log prima di sovrascrivere il nome vecchio */
            ESP_LOGI(TAG, "[SWITCH_PRG] %.20s/%ldc/%lus -> %.20s/%ldc/%lus (rem %lus->%lus)",
                     ctx->running_program_name[0] ? ctx->running_program_name : "prg",
                     (long)old_price, (unsigned long)(old_dur_ms / 1000),
                     event->text[0] ? event->text : "prg",
                     (long)new_price, (unsigned long)(new_dur_ms / 1000),
                     (unsigned long)(rem_old_ms / 1000),
                     (unsigned long)(rem_new_ms / 1000));
            ctx->running_target_ms    = elapsed + rem_new_ms;
            ctx->running_price_units  = (event->value_i32 > 0) ? event->value_i32 : ctx->running_price_units;
            if (event->value_u32 > 0) ctx->pause_max_ms = event->value_u32;
            snprintf(ctx->running_program_name, sizeof(ctx->running_program_name),
                     "%s", event->text[0] ? event->text : "programma");

            if (was_paused) {
                ctx->state = FSM_STATE_RUNNING;
                ctx->program_running = true;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->inactivity_ms = 0;
                return true;
            }

            return false; /* nessun cambio di stato */
        }

        case FSM_INPUT_EVENT_NONE:
        default:
            break;
    }

    return false;
}


/**
 * @brief Esegue un tick del finite state machine (FSM).
 * 
 * Questa funzione aggiorna lo stato dell'FSM in base al tempo trascorso.
 * 
 * @param [in] ctx Puntatore al contesto dell'FSM.
 * @param [in] elapsed_ms Numero di millisecondi trascorsi dall'ultimo tick.
 * @return true Se lo stato dell'FSM è stato aggiornato con successo.
 * @return false Se il contesto è nullo o l'aggiornamento non è stato possibile.
 */
bool fsm_tick(fsm_ctx_t *ctx, uint32_t elapsed_ms)
{
    if (!ctx) {
        return false;
    }

    ctx->inactivity_ms += elapsed_ms;
    /* #11 fix: cap inactivity_ms per evitare overflow uint32_t dopo ~49gg
     * in stato IDLE; il tetto è leggermente sopra il timeout splash così
     * la logica di timeout continua a funzionare correttamente. */
    if (ctx->inactivity_ms > ctx->splash_screen_time_ms + 5000U) {
        ctx->inactivity_ms = ctx->splash_screen_time_ms + 5000U;
    }

    if (ctx->state == FSM_STATE_RUNNING) {
        ctx->running_elapsed_ms += elapsed_ms;
        
        // Logica PreFineCiclo: attiva la segnalazione quando si raggiunge la percentuale configurata
        if (!ctx->pre_fine_ciclo_active && ctx->running_target_ms > 0) {
            device_config_t *cfg = device_config_get();
            if (cfg && cfg->timeouts.pre_fine_ciclo_percent > 0 && cfg->timeouts.pre_fine_ciclo_percent < 100) {
                uint32_t pre_fine_threshold_ms = (ctx->running_target_ms * cfg->timeouts.pre_fine_ciclo_percent) / 100;
                if (ctx->running_elapsed_ms >= pre_fine_threshold_ms) {
                    ctx->pre_fine_ciclo_active = true;
                    // Pubblica evento PreFineCiclo per notificare altri componenti (LED, display, etc.)
                    fsm_input_event_t pre_fine_event = {
                        .from = AGN_ID_FSM,
                        .to = {AGN_ID_LED, AGN_ID_LVGL, AGN_ID_WEB_UI},
                        .action = ACTION_ID_PROGRAM_PREFINE_CYCLO,
                        .type = FSM_INPUT_EVENT_NONE,
                        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
                        .value_i32 = 0,
                        .value_u32 = 0,
                        .aux_u32 = 0,
                        .data_ptr = NULL,
                        .text = {0}
                    };
                    fsm_event_publish(&pre_fine_event, pdMS_TO_TICKS(100));
                    fsm_append_message("PreFineCiclo: soglia raggiunta");
                    ESP_LOGI(TAG, "[M] PreFineCiclo attivo - %u%% raggiunta (%lu ms / %lu ms)", 
                             cfg->timeouts.pre_fine_ciclo_percent, 
                             (unsigned long)ctx->running_elapsed_ms, 
                             (unsigned long)ctx->running_target_ms);
                }
            }
        }
        
        if (ctx->running_target_ms > 0 && ctx->running_elapsed_ms >= ctx->running_target_ms) {
            if (fsm_try_autorenew_running_program(ctx)) {
                return false;
            }

            /* Quando il credito ECD+VCD è finito, interrompi il ciclo automatico
             * e torna in IDLE. */
            if ((ctx->ecd_coins + ctx->vcd_coins) <= 0) {
                fsm_append_message("Credito esaurito: stop auto-riavvio programma");
                ESP_LOGI(TAG, "[M] Credito ECD+VCD azzerato: stop auto-riavvio e ritorno a IDLE");

                ctx->credit_cents = 0;
                fsm_reset_runtime_locked(ctx);
                ctx->session_mode = FSM_SESSION_MODE_NONE;
                ctx->session_source = FSM_SESSION_SOURCE_NONE;
                ctx->allow_additional_payments = false;
                ctx->state = FSM_STATE_IDLE;
                ctx->inactivity_ms = 0;
                return true;
            }

            return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_STOP);
        }
    } else if (ctx->state == FSM_STATE_PAUSED) {
        if (!ctx->pause_limit_reached) {
            ctx->pause_elapsed_ms += elapsed_ms;
            if (ctx->pause_elapsed_ms >= ctx->pause_max_ms) {
                ctx->pause_limit_reached = true;
                fsm_append_message("Pausa scaduta: ripresa automatica programma");
                ESP_LOGI(TAG, "[M] Timeout pausa raggiunto: ripresa countdown");
                return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_RESUME);
            }
        }
    }

    if (ctx->state == FSM_STATE_CREDIT && ctx->inactivity_ms >= ctx->splash_screen_time_ms) {
        return fsm_handle_event(ctx, FSM_EVENT_TIMEOUT);
    }

    return false;
}

const char *fsm_state_to_string(fsm_state_t state)
{
    switch (state) {
        case FSM_STATE_IDLE:
            return "idle";
        case FSM_STATE_ADS:
            return "ads";
        case FSM_STATE_CREDIT:
            return "credit";
        case FSM_STATE_RUNNING:
            return "running";
        case FSM_STATE_PAUSED:
            return "paused";
        default:
            return "unknown";
    }
}


/**
 * @brief Inizializza la coda degli eventi del finite state machine.
 *
 * @param queue_len Dimensione massima della coda degli eventi.
 * @return true se l'inizializzazione è stata completata con successo, false altrimenti.
 */
bool fsm_event_queue_init(size_t queue_len)
{
    /* mailbox size is fixed, ignore parameter */
    (void)queue_len;

    static bool inited = false;
    if (inited) {
        return true;
    }

    /* #9 fix: crea tutti i semafori PRIMA della sezione critica.
     * xSemaphoreCreate* usa l'heap allocator che acquisisce internamente un
     * lock FreeRTOS — annidarlo dentro portENTER_CRITICAL su RISC-V
     * multi-core può causare assert o deadlock. */
    SemaphoreHandle_t pre_mb_mutex  = xSemaphoreCreateMutex();
    SemaphoreHandle_t pre_mb_signal = xSemaphoreCreateCounting(FSM_MAILBOX_SIZE, 0);
    SemaphoreHandle_t pre_pend_lock = xSemaphoreCreateMutex();
    SemaphoreHandle_t pre_runt_lock = xSemaphoreCreateMutex();

    if (!pre_mb_mutex || !pre_mb_signal || !pre_pend_lock || !pre_runt_lock) {
        if (pre_mb_mutex)  vSemaphoreDelete(pre_mb_mutex);
        if (pre_mb_signal) vSemaphoreDelete(pre_mb_signal);
        if (pre_pend_lock) vSemaphoreDelete(pre_pend_lock);
        if (pre_runt_lock) vSemaphoreDelete(pre_runt_lock);
        ESP_LOGE(TAG, "Allocazione semafori fallita");
        return false;
    }

    /* use a dedicated mux for startup; we can't rely on taskENTER_CRITICAL()
     * without an argument on RISCV multi‑core builds. */
    static portMUX_TYPE init_mux = portMUX_INITIALIZER_UNLOCKED;
    portENTER_CRITICAL(&init_mux);
    if (inited) {
        portEXIT_CRITICAL(&init_mux);
        /* già inizializzato da un altro core: libera i semafori extra */
        vSemaphoreDelete(pre_mb_mutex);
        vSemaphoreDelete(pre_mb_signal);
        vSemaphoreDelete(pre_pend_lock);
        vSemaphoreDelete(pre_runt_lock);
        return true;
    }

    s_mb_mutex         = pre_mb_mutex;
    s_mb_signal        = pre_mb_signal;
    s_fsm_pending_lock = pre_pend_lock;
    s_fsm_runtime_lock = pre_runt_lock;

    s_mb_head = s_mb_tail = 0;
    memset(s_mailbox, 0, sizeof(s_mailbox));

    inited = true;
    portEXIT_CRITICAL(&init_mux);
    ESP_LOGI(TAG, "Mailbox inizializzata (size=%d)", FSM_MAILBOX_SIZE);
    return true;
}


/**
 * @brief Pubblica un evento all'interno del gestore di stati.
 * 
 * Questa funzione pubblica un evento all'interno del gestore di stati e attende fino a un massimo di timeout_ticks per un segnale di completamento.
 * 
 * @param [in] event Puntatore all'evento da pubblicare.
 * @param [in] timeout_ticks Numero di ticks di timeout per l'attesa del completamento.
 * @return true se l'evento è stato pubblicato con successo, false altrimenti.
 */
bool fsm_event_publish(const fsm_input_event_t *event, TickType_t timeout_ticks)
{
    if (!s_mb_mutex || !event) {
        return false;
    }

    /* take the mailbox mutex with the caller-supplied timeout. the old
     * implementation tried to implement its own polling/timeout loop; that
     * was chewing CPU time during bursts and made the callers harder to
     * reason about. the native FreeRTOS API handles the wait for us.
     */
    if (xSemaphoreTake(s_mb_mutex, timeout_ticks) != pdTRUE) {
        return false;
    }

    bool ok = false;
    size_t next = (s_mb_tail + 1) % FSM_MAILBOX_SIZE;
    if (next != s_mb_head) {
        size_t slot = s_mb_tail;
        s_mailbox[slot].ev = *event;
        s_mailbox[slot].to_mask = to_mask_from_event(event);
        s_mailbox[slot].enqueue_timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
        s_mb_tail = next;
        ok = true;
#ifdef LOG_QUEUE
        ESP_LOGI(TAG, "ENQUEUE[%zu] type=%s action=%d from=%d to_mask=0x%08lx",
                 slot, fsm_input_event_type_to_string(event->type),
                 (int)event->action, (int)event->from,
                 (unsigned long)s_mailbox[slot].to_mask);
#endif
    } else {
#ifdef LOG_QUEUE
        ESP_LOGW(TAG, "ENQUEUE FULL type=%s from=%d",
                 fsm_input_event_type_to_string(event->type), (int)event->from);
#endif
    }

    xSemaphoreGive(s_mb_mutex);

    if (ok) {
        fsm_pending_push(event);
        /* #7 fix: segnala ai receiver che c'è un nuovo messaggio */
        xSemaphoreGive(s_mb_signal);
    }
    return ok;
}


/**
 * @brief Pubblica un evento all'interno di un contesto ISR.
 * 
 * Questa funzione viene utilizzata per pubblicare un evento all'interno di un contesto di Service Routine Interrupt (ISR).
 * 
 * @param [in] event Puntatore all'evento da pubblicare.
 * @param [out] task_woken Puntatore alla variabile che indica se è stata attivata una task.
 * @return true se l'evento è stato pubblicato con successo, false altrimenti.
 */
bool fsm_event_publish_from_isr(const fsm_input_event_t *event, BaseType_t *task_woken)
{
    if (!s_mb_mutex || !event) {
        return false;
    }

    BaseType_t higher = pdFALSE;
    if (xSemaphoreTakeFromISR(s_mb_mutex, &higher) == pdTRUE) {
        size_t next = (s_mb_tail + 1) % FSM_MAILBOX_SIZE;
        if (next != s_mb_head) {
            s_mailbox[s_mb_tail].ev = *event;
            s_mailbox[s_mb_tail].to_mask = to_mask_from_event(event);
            s_mailbox[s_mb_tail].enqueue_timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCountFromISR());
            s_mb_tail = next;
            xSemaphoreGiveFromISR(s_mb_mutex, &higher);
            /* BUG4 fix: task_woken va letto DOPO xSemaphoreGiveFromISR */
            if (task_woken) *task_woken = higher;
            /* BUG3 fix: NON chiamare fsm_pending_push() da ISR */
            /* #7 fix: sblocca receiver in attesa sul signal semaforo */
            if (s_mb_signal) {
                BaseType_t sig_woken = pdFALSE;
                xSemaphoreGiveFromISR(s_mb_signal, &sig_woken);
                if (sig_woken == pdTRUE && task_woken) *task_woken = pdTRUE;
            }
            return true;
        }
        xSemaphoreGiveFromISR(s_mb_mutex, &higher);
    }
    return false;
}


/**
 * @brief Gestisce l'arrivo di un evento all'interno del sistema di gestione degli stati.
 * 
 * Questa funzione riceve un evento e lo invia al destinatario specificato. Se il destinatario
 * non è valido o se non ci sono risorse disponibili, la funzione restituisce false.
 * 
 * @param [in] event Puntatore all'evento da inviare.
 * @param [in] receiver_id ID dell'agente che riceverà l'evento.
 * @param [in] timeout_ticks Numero di ticks di timeout per l'attesa.
 * @return true se l'evento è stato inviato con successo, false altrimenti.
 */
bool fsm_event_receive(fsm_input_event_t *event, agn_id_t receiver_id, TickType_t timeout_ticks)
{
    /* #8 fix: receiver_id parametrico — qualunque agent può ricevere messaggi
     * dalla mailbox, non solo AGN_ID_FSM.
     *
     * FIX consegna multi-destinatario:
     * - prima scandiamo SEMPRE la mailbox sotto mutex per cercare messaggi
     *   destinati al receiver.
     * - solo se non troviamo nulla attendiamo sul semaforo di segnalazione.
     *
     * Questo evita che un task "sbagliato" consumi il token di segnale e lasci
     * bloccato il destinatario reale (effetto osservato: eventi QR vecchi
     * processati in ritardo quando arriva un nuovo evento touch/program).
     */
    if (!s_mb_mutex || !event) {
        return false;
    }

    uint32_t mybit = (1u << (uint32_t)receiver_id);
    TickType_t start_tick = xTaskGetTickCount();
    TickType_t poll_delay = pdMS_TO_TICKS(2);
    TickType_t mutex_wait = pdMS_TO_TICKS(10);
    if (poll_delay == 0) {
        poll_delay = 1;
    }
    if (mutex_wait == 0) {
        mutex_wait = 1;
    }

    while (true) {
        size_t dropped = 0;
        if (xSemaphoreTake(s_mb_mutex, mutex_wait) == pdTRUE) {
            uint32_t now_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
            dropped = fsm_mailbox_drop_expired_locked(now_ms);

            for (size_t i = s_mb_head; i != s_mb_tail; i = (i + 1) % FSM_MAILBOX_SIZE) {
                if (s_mailbox[i].to_mask & mybit) {
                    bool slot_fully_consumed = false;
                    *event = s_mailbox[i].ev;
#ifdef LOG_QUEUE
                    ESP_LOGI(TAG, "DEQUEUE[%zu] type=%s action=%d from=%d receiver=%d",
                             i, fsm_input_event_type_to_string(event->type),
                             (int)event->action, (int)event->from, (int)receiver_id);
#endif
                    s_mailbox[i].to_mask &= ~mybit;
                    slot_fully_consumed = (s_mailbox[i].to_mask == 0);

                    if (slot_fully_consumed) {
                        while (s_mb_head != s_mb_tail && s_mailbox[s_mb_head].to_mask == 0) {
                            s_mb_head = (s_mb_head + 1) % FSM_MAILBOX_SIZE;
                        }
                    }

                    xSemaphoreGive(s_mb_mutex);

                    if (slot_fully_consumed) {
                        (void)xSemaphoreTake(s_mb_signal, 0);
                    }

                    for (size_t d = 0; d < dropped; ++d) {
                        if (xSemaphoreTake(s_mb_signal, 0) != pdTRUE) {
                            break;
                        }
                    }
                    return true;
                }
            }
            xSemaphoreGive(s_mb_mutex);

            for (size_t d = 0; d < dropped; ++d) {
                if (xSemaphoreTake(s_mb_signal, 0) != pdTRUE) {
                    break;
                }
            }
        }

        if (timeout_ticks == 0) {
            return false;
        }

        if (timeout_ticks == portMAX_DELAY) {
            if (s_mb_signal) {
                (void)xSemaphoreTake(s_mb_signal, poll_delay);
            } else {
                vTaskDelay(poll_delay);
            }
            continue;
        }

        TickType_t elapsed = xTaskGetTickCount() - start_tick;
        if (elapsed >= timeout_ticks) {
            return false;
        }

        TickType_t remaining = timeout_ticks - elapsed;
        TickType_t wait_slice = (remaining > poll_delay) ? poll_delay : remaining;
        if (wait_slice > 0) {
            vTaskDelay(wait_slice);
        } else {
            taskYIELD();
        }
    }
}


/**
 * @brief Pubblica un evento semplice nella macchina a stati finiti.
 *
 * Questa funzione pubblica un evento semplice nella macchina a stati finiti (FSM),
 * che può essere utilizzato per gestire vari tipi di eventi.
 *
 * @param type [in] Tipo dell'evento da pubblicare.
 * @param value_i32 [in] Valore intero associato all'evento.
 * @param text [in] Testo associato all'evento.
 * @param timeout_ticks [in] Numero di ticks di timeout per l'evento.
 *
 * @return true se l'evento è stato pubblicato con successo, false altrimenti.
 */
bool fsm_publish_simple_event(fsm_input_event_type_t type, int32_t value_i32, const char *text, TickType_t timeout_ticks)
{
    /* helper for legacy callers; populate mailbox fields, defaulting to FSM */
    fsm_input_event_t event = {
        .from = AGN_ID_NONE,
        .to = {AGN_ID_FSM}, /* deliver to FSM by default */
        .action = ACTION_ID_NONE,

        .type = type,
        .timestamp_ms = (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()),
        .value_i32 = value_i32,
        .value_u32 = 0,
        .aux_u32 = 0,
        .text = {0},
    };

    if (text) {
        strncpy(event.text, text, sizeof(event.text) - 1);
        event.text[sizeof(event.text) - 1] = '\0';
    }

    return fsm_event_publish(&event, timeout_ticks);
}


/**
 * @brief Aggiunge un messaggio alla coda di messaggi in attesa.
 * 
 * @param [in] message Il messaggio da aggiungere. Deve essere una stringa non vuota.
 * @return void Non restituisce alcun valore.
 */
void fsm_append_message(const char *message)
{
    if (!s_fsm_pending_lock || !message || message[0] == '\0') {
        return;
    }

    if (xSemaphoreTake(s_fsm_pending_lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    fsm_pending_push_text_locked(message);
    xSemaphoreGive(s_fsm_pending_lock);
}


/**
 * @brief Copia i messaggi pendenti in un buffer di output.
 * 
 * @param out Buffer di output dove verranno copiati i messaggi pendenti. Ogni messaggio deve avere una lunghezza massima di FSM_EVENT_TEXT_MAX_LEN.
 * @param max_count Numero massimo di messaggi da copiare.
 * @return size_t Numero di messaggi effettivamente copiati.
 * 
 * @note La funzione copia i messaggi pendenti in un buffer di output fornito. Se il buffer è NULL o il numero massimo di messaggi è zero, la funzione restituirà 0.
 * @note La funzione utilizza una lock per proteggere l'accesso ai messaggi pendenti. Se la lock non è disponibile, la funzione restituirà 0.
 */
size_t fsm_pending_messages_copy(char out[][FSM_EVENT_TEXT_MAX_LEN], size_t max_count)
{
    if (!out || max_count == 0 || !s_fsm_pending_lock) {
        return 0;
    }

    if (xSemaphoreTake(s_fsm_pending_lock, pdMS_TO_TICKS(20)) != pdTRUE) {
        return 0;
    }

    size_t n = (s_fsm_pending_count < max_count) ? s_fsm_pending_count : max_count;
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (s_fsm_pending_head + i) % FSM_DEFAULT_EVENT_QUEUE_LEN;
        strncpy(out[i], s_fsm_pending_msgs[idx], FSM_EVENT_TEXT_MAX_LEN - 1);
        out[i][FSM_EVENT_TEXT_MAX_LEN - 1] = '\0';
    }

    xSemaphoreGive(s_fsm_pending_lock);
    return n;
}


/**
 * @brief Pubblica lo stato corrente del contesto del gestore di stati.
 * 
 * @param [in] ctx Puntatore al contesto del gestore di stati.
 * @return void Nessun valore di ritorno.
 * 
 * Questa funzione pubblica lo stato corrente del contesto del gestore di stati.
 * Se il contesto o il lock del gestore di stati non sono validi, la funzione non fa nulla.
 */
void fsm_runtime_publish(const fsm_ctx_t *ctx)
{
    if (!ctx || !s_fsm_runtime_lock) {
        return;
    }

    if (xSemaphoreTake(s_fsm_runtime_lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return;
    }

    s_fsm_runtime_ctx = *ctx;
    s_fsm_runtime_valid = true;
    xSemaphoreGive(s_fsm_runtime_lock);
}


/**
 * @brief Cattura uno snapshot del contesto di runtime del finite state machine.
 * 
 * @param [out] out_ctx Puntatore al contesto di output dove verrà salvato lo stato corrente del finite state machine.
 * @return true Se lo snapshot è stato catturato con successo.
 * @return false Se il contesto di output è nullo o se la lock del runtime del finite state machine non è valida.
 */
bool fsm_runtime_snapshot(fsm_ctx_t *out_ctx)
{
    if (!out_ctx || !s_fsm_runtime_lock) {
        return false;
    }

    if (xSemaphoreTake(s_fsm_runtime_lock, pdMS_TO_TICKS(10)) != pdTRUE) {
        return false;
    }

    bool valid = s_fsm_runtime_valid;
    if (valid) {
        *out_ctx = s_fsm_runtime_ctx;
    }

    xSemaphoreGive(s_fsm_runtime_lock);
    return valid;
}
