#include "fsm.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>

#define FSM_DEFAULT_SPLASH_TIMEOUT_MS (30000U)
#define FSM_DEFAULT_PAUSE_MAX_MS (60000U)
#define FSM_DEFAULT_EVENT_QUEUE_LEN (32U)

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
} mailbox_slot_t;

static mailbox_slot_t s_mailbox[FSM_MAILBOX_SIZE];
static size_t s_mb_head = 0;
static size_t s_mb_tail = 0;
static SemaphoreHandle_t s_mb_mutex = NULL;

static uint32_t to_mask_from_event(const fsm_input_event_t *e);

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
        case FSM_INPUT_EVENT_NONE:
        default:
            return "none";
    }
}

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

void fsm_init(fsm_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    ctx->state = FSM_STATE_IDLE;
    ctx->credit_cents = 0;
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
}

bool fsm_handle_event(fsm_ctx_t *ctx, fsm_event_t event)
{
    if (!ctx || event == FSM_EVENT_NONE) {
        return false;
    }

    fsm_state_t prev = ctx->state;

    switch (ctx->state) {
        case FSM_STATE_IDLE:
            if (event == FSM_EVENT_USER_ACTIVITY || event == FSM_EVENT_PAYMENT_ACCEPTED) {
                ctx->state = FSM_STATE_CREDIT;
                ctx->running_elapsed_ms = 0;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->inactivity_ms = 0;
            }
            break;

        case FSM_STATE_CREDIT:
            if (event == FSM_EVENT_PROGRAM_SELECTED && ctx->credit_cents > 0) {
                ctx->state = FSM_STATE_RUNNING;
                ctx->program_running = true;
                ctx->running_elapsed_ms = 0;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->inactivity_ms = 0;
            } else if (event == FSM_EVENT_TIMEOUT) {
                ctx->state = FSM_STATE_IDLE;
                ctx->credit_cents = 0;
                ctx->program_running = false;
                ctx->running_elapsed_ms = 0;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->running_target_ms = 0;
                ctx->running_price_units = 0;
                memset(ctx->running_program_name, 0, sizeof(ctx->running_program_name));
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
                ctx->state = FSM_STATE_CREDIT;
                ctx->program_running = false;
                ctx->running_elapsed_ms = 0;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->running_target_ms = 0;
                ctx->running_price_units = 0;
                memset(ctx->running_program_name, 0, sizeof(ctx->running_program_name));
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
                ctx->state = FSM_STATE_CREDIT;
                ctx->program_running = false;
                ctx->running_elapsed_ms = 0;
                ctx->pause_elapsed_ms = 0;
                ctx->pause_limit_reached = false;
                ctx->running_target_ms = 0;
                ctx->running_price_units = 0;
                memset(ctx->running_program_name, 0, sizeof(ctx->running_program_name));
                ctx->inactivity_ms = 0;
            }
            break;

        default:
            break;
    }

    return prev != ctx->state;
}

bool fsm_handle_input_event(fsm_ctx_t *ctx, const fsm_input_event_t *event)
{
    if (!ctx || !event) {
        return false;
    }

    switch (event->type) {
        case FSM_INPUT_EVENT_USER_ACTIVITY:
        case FSM_INPUT_EVENT_TOUCH:
        case FSM_INPUT_EVENT_KEY:
        case FSM_INPUT_EVENT_QR_SCANNED:
            return fsm_handle_event(ctx, FSM_EVENT_USER_ACTIVITY);

        case FSM_INPUT_EVENT_COIN:
        case FSM_INPUT_EVENT_TOKEN:
        case FSM_INPUT_EVENT_CARD_CREDIT:
        case FSM_INPUT_EVENT_QR_CREDIT:
            if (event->value_i32 > 0) {
                ctx->credit_cents += event->value_i32;
            }
            return fsm_handle_event(ctx, FSM_EVENT_PAYMENT_ACCEPTED);

        case FSM_INPUT_EVENT_PROGRAM_SELECTED:
            if (ctx->state != FSM_STATE_CREDIT) {
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

            if (ctx->running_price_units > 0) {
                if (ctx->credit_cents < ctx->running_price_units) {
                    fsm_append_message("Credito insufficiente per avvio programma");
                    return false;
                }
                ctx->credit_cents -= ctx->running_price_units;
            }

            return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_SELECTED);

        case FSM_INPUT_EVENT_PROGRAM_STOP:
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
            return fsm_handle_event(ctx, FSM_EVENT_CREDIT_ENDED);

        case FSM_INPUT_EVENT_NONE:
        default:
            break;
    }

    return false;
}

bool fsm_tick(fsm_ctx_t *ctx, uint32_t elapsed_ms)
{
    if (!ctx) {
        return false;
    }

    ctx->inactivity_ms += elapsed_ms;

    if (ctx->state == FSM_STATE_RUNNING) {
        ctx->running_elapsed_ms += elapsed_ms;
        if (ctx->running_target_ms > 0 && ctx->running_elapsed_ms >= ctx->running_target_ms) {
            char msg[FSM_EVENT_TEXT_MAX_LEN] = {0};
            snprintf(msg, sizeof(msg), "Addebito %ld su %.24s",
                     (long)ctx->running_price_units,
                     ctx->running_program_name[0] ? ctx->running_program_name : "programma");
            fsm_append_message(msg);
            return fsm_handle_event(ctx, FSM_EVENT_PROGRAM_STOP);
        }
    } else if (ctx->state == FSM_STATE_PAUSED) {
        if (!ctx->pause_limit_reached) {
            ctx->pause_elapsed_ms += elapsed_ms;
            if (ctx->pause_elapsed_ms >= ctx->pause_max_ms) {
                ctx->pause_limit_reached = true;
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

bool fsm_event_queue_init(size_t queue_len)
{
    (void)queue_len; /* size is fixed by mailbox implementation */

    if (s_mb_mutex) {
        return true; /* already initialised */
    }

    s_mb_mutex = xSemaphoreCreateMutex();
    if (!s_mb_mutex) {
        return false;
    }

    s_mb_head = s_mb_tail = 0;
    memset(s_mailbox, 0, sizeof(s_mailbox));

    /* legacy locks still required for pending/runtime helpers */
    if (!s_fsm_pending_lock) {
        s_fsm_pending_lock = xSemaphoreCreateMutex();
        if (!s_fsm_pending_lock) {
            return false;
        }
    }

    if (!s_fsm_runtime_lock) {
        s_fsm_runtime_lock = xSemaphoreCreateMutex();
        if (!s_fsm_runtime_lock) {
            return false;
        }
    }

    return true;
}

bool fsm_event_publish(const fsm_input_event_t *event, TickType_t timeout_ticks)
{
    if (!s_mb_mutex || !event) {
        return false;
    }

    bool ok = false;
    TickType_t start = xTaskGetTickCount();
    while (true) {
        if (xSemaphoreTake(s_mb_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            size_t next = (s_mb_tail + 1) % FSM_MAILBOX_SIZE;
            if (next != s_mb_head) {
                s_mailbox[s_mb_tail].ev = *event;
                s_mailbox[s_mb_tail].to_mask = to_mask_from_event(event);
                s_mb_tail = next;
                ok = true;
            }
            xSemaphoreGive(s_mb_mutex);
            break;
        }
        if (timeout_ticks == 0) {
            break;
        }
        if ((xTaskGetTickCount() - start) >= timeout_ticks) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (ok) {
        fsm_pending_push(event);
    }
    return ok;
}

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
            s_mb_tail = next;
            if (task_woken) *task_woken = higher;
            xSemaphoreGiveFromISR(s_mb_mutex, &higher);
            fsm_pending_push(event);
            return true;
        }
        xSemaphoreGiveFromISR(s_mb_mutex, &higher);
    }
    return false;
}

bool fsm_event_receive(fsm_input_event_t *event, TickType_t timeout_ticks)
{
    if (!s_mb_mutex || !event) {
        return false;
    }

    TickType_t start = xTaskGetTickCount();
    uint32_t mybit = (1u << AGN_ID_FSM);

    while (true) {
        if (xSemaphoreTake(s_mb_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (size_t i = s_mb_head; i != s_mb_tail; i = (i + 1) % FSM_MAILBOX_SIZE) {
                if (s_mailbox[i].to_mask & mybit) {
                    *event = s_mailbox[i].ev;
                    s_mailbox[i].to_mask &= ~mybit;
                    if (s_mailbox[i].to_mask == 0) {
                        s_mb_head = (s_mb_head + 1) % FSM_MAILBOX_SIZE;
                    }
                    xSemaphoreGive(s_mb_mutex);
                    return true;
                }
            }
            xSemaphoreGive(s_mb_mutex);
        }
        if (timeout_ticks == 0) {
            break;
        }
        if ((xTaskGetTickCount() - start) >= timeout_ticks) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

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
