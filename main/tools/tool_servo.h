#pragma once

#include "esp_err.h"
#include <stddef.h>

esp_err_t tool_servo_init(void);

esp_err_t tool_servo_control_execute(const char *input_json, char *output, size_t output_size);

esp_err_t tool_servo_stop_sweep(int channel);
