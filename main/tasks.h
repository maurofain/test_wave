#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_err.h"
#include "fsm.h"
#include "digital_io.h"

#define TASKS_ERR_BASE 0x7700
#define TASKS_ERR_FSM_QUEUE_NOT_READY (TASKS_ERR_BASE + 1)
#define TASKS_ERR_PROGRAM_DISABLED (TASKS_ERR_BASE + 2)
#define TASKS_ERR_FSM_SNAPSHOT_UNAVAILABLE (TASKS_ERR_BASE + 3)
#define TASKS_ERR_PROGRAM_STATE_CONFLICT (TASKS_ERR_BASE + 4)
#define TASKS_ERR_PROGRAM_CREDIT_INSUFFICIENT (TASKS_ERR_BASE + 5)

typedef enum {
    TASK_STATE_RUN = 0,
    TASK_STATE_IDLE,
    TASK_STATE_PAUSE,
} task_state_t;

typedef struct {
    const char *name;
    task_state_t state;
    UBaseType_t priority;
    BaseType_t core_id;
    TickType_t period_ticks;
    TaskFunction_t task_fn;
    uint32_t stack_words;
    uint32_t stack_caps;   /* MALLOC_CAP_INTERNAL o MALLOC_CAP_SPIRAM */
    void *arg;
    TaskHandle_t handle;
} task_param_t;

void tasks_start_all(void);
bool tasks_start_named(const char *name);
void tasks_load_config(const char *path);
void tasks_apply_n_run(void);
void tasks_set_touchscreen_handle(void *handle);
float tasks_get_temperature(void);
float tasks_get_humidity(void);
bool tasks_publish_key_event(void);
bool tasks_publish_card_credit_event(int32_t vcd_amount_cents, const char *source_tag);
esp_err_t tasks_publish_program_button_action(uint8_t program_id, agn_id_t sender);
esp_err_t tasks_digital_io_set_output_via_agent(uint8_t output_id, bool value, TickType_t timeout_ticks);
esp_err_t tasks_digital_io_get_output_via_agent(uint8_t output_id, bool *out_value, TickType_t timeout_ticks);
esp_err_t tasks_digital_io_get_input_via_agent(uint8_t input_id, bool *out_value, TickType_t timeout_ticks);
esp_err_t tasks_digital_io_get_snapshot_via_agent(digital_io_snapshot_t *out_snapshot, TickType_t timeout_ticks);
const char *tasks_err_to_name(esp_err_t err);
void tasks_suspend_peripherals_for_lvgl_test(void);
void tasks_resume_peripherals_after_lvgl_test(void);
