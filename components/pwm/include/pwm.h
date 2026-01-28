#pragma once
#include "esp_err.h"

esp_err_t pwm_init(void);
esp_err_t pwm_set_duty(int channel, int duty_percent);
