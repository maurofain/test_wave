#pragma once
#include "esp_err.h"

esp_err_t pwm_test_start(int channel);
esp_err_t pwm_test_stop(int channel);
esp_err_t pwm_test_set_param(int channel, uint32_t freq, uint32_t duty_percent);
