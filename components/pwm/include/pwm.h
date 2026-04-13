#pragma once
#include "esp_err.h"
#include "hw_common.h"

esp_err_t pwm_init(void);
esp_err_t pwm_set_duty(int channel, int duty_percent);
int pwm_get_duty(int channel);
hw_component_status_t pwm_get_status(void);
