#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

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
void tasks_load_config(const char *path);
void tasks_apply_n_run(void);
void tasks_set_touchscreen_handle(void *handle);
float tasks_get_temperature(void);
float tasks_get_humidity(void);
void tasks_suspend_peripherals_for_lvgl_test(void);
void tasks_resume_peripherals_after_lvgl_test(void);
