#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"

#define FSM_EVENT_TEXT_MAX_LEN (64U)

typedef enum {
    FSM_STATE_IDLE = 0,
    FSM_STATE_CREDIT,
    FSM_STATE_RUNNING,
    FSM_STATE_PAUSED,
} fsm_state_t;

typedef enum {
    FSM_EVENT_NONE = 0,
    FSM_EVENT_USER_ACTIVITY,
    FSM_EVENT_PAYMENT_ACCEPTED,
    FSM_EVENT_PROGRAM_SELECTED,
    FSM_EVENT_PROGRAM_STOP,
    FSM_EVENT_PROGRAM_PAUSE,
    FSM_EVENT_PROGRAM_RESUME,
    FSM_EVENT_TIMEOUT,
    FSM_EVENT_CREDIT_ENDED,
} fsm_event_t;

typedef enum {
    FSM_INPUT_EVENT_NONE = 0,
    FSM_INPUT_EVENT_USER_ACTIVITY,
    FSM_INPUT_EVENT_TOUCH,
    FSM_INPUT_EVENT_KEY,
    FSM_INPUT_EVENT_COIN,
    FSM_INPUT_EVENT_TOKEN,
    FSM_INPUT_EVENT_CARD_CREDIT,
    FSM_INPUT_EVENT_QR_CREDIT,
    FSM_INPUT_EVENT_QR_SCANNED,
    FSM_INPUT_EVENT_PROGRAM_SELECTED,
    FSM_INPUT_EVENT_PROGRAM_STOP,
    FSM_INPUT_EVENT_PROGRAM_PAUSE_TOGGLE,
    FSM_INPUT_EVENT_CREDIT_ENDED,
} fsm_input_event_type_t;

typedef struct {
    fsm_input_event_type_t type;
    uint32_t timestamp_ms;
    int32_t value_i32;
    uint32_t value_u32;
    uint32_t aux_u32;
    char text[FSM_EVENT_TEXT_MAX_LEN];
} fsm_input_event_t;

typedef struct {
    fsm_state_t state;
    int32_t credit_cents;
    bool program_running;
    uint32_t running_elapsed_ms;
    uint32_t pause_elapsed_ms;
    uint32_t pause_max_ms;
    bool pause_limit_reached;
    uint32_t running_target_ms;
    int32_t running_price_units;
    char running_program_name[FSM_EVENT_TEXT_MAX_LEN];
    uint32_t inactivity_ms;
    uint32_t splash_screen_time_ms;
} fsm_ctx_t;

void fsm_init(fsm_ctx_t *ctx);
bool fsm_handle_event(fsm_ctx_t *ctx, fsm_event_t event);
bool fsm_handle_input_event(fsm_ctx_t *ctx, const fsm_input_event_t *event);
bool fsm_tick(fsm_ctx_t *ctx, uint32_t elapsed_ms);
const char *fsm_state_to_string(fsm_state_t state);

bool fsm_event_queue_init(size_t queue_len);
bool fsm_event_publish(const fsm_input_event_t *event, TickType_t timeout_ticks);
bool fsm_event_publish_from_isr(const fsm_input_event_t *event, BaseType_t *task_woken);
bool fsm_event_receive(fsm_input_event_t *event, TickType_t timeout_ticks);
bool fsm_publish_simple_event(fsm_input_event_type_t type, int32_t value_i32, const char *text, TickType_t timeout_ticks);
size_t fsm_pending_messages_copy(char out[][FSM_EVENT_TEXT_MAX_LEN], size_t max_count);
void fsm_append_message(const char *message);
void fsm_runtime_publish(const fsm_ctx_t *ctx);
bool fsm_runtime_snapshot(fsm_ctx_t *out_ctx);
