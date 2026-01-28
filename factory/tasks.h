#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
    void *arg;
    TaskHandle_t handle;
} task_param_t;

void tasks_start_all(void);
void tasks_load_config(const char *path);
