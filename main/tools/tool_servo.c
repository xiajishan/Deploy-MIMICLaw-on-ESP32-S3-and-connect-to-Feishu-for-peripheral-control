#include "tools/tool_servo.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "cJSON.h"

#define TAG "tool_servo"

#define SERVO_PWM_GPIO_1 GPIO_NUM_38
#define SERVO_PWM_GPIO_2 GPIO_NUM_46

#define SERVO_PWM_FREQ_HZ 50
#define SERVO_PWM_PERIOD_US 20000

#define SERVO_PULSE_MIN_US 500
#define SERVO_PULSE_MAX_US 2500

#define SERVO_ANGLE_MIN 0
#define SERVO_ANGLE_MAX 180

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL_1 LEDC_CHANNEL_0
#define LEDC_CHANNEL_2 LEDC_CHANNEL_1
#define LEDC_DUTY_RES LEDC_TIMER_14_BIT
#define LEDC_MAX_DUTY 16383

static bool s_initialized = false;

static uint32_t angle_to_duty(int angle)
{
    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    int pulse_us = SERVO_PULSE_MIN_US + 
                   (angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / SERVO_ANGLE_MAX;
    
    uint32_t duty = (pulse_us * LEDC_MAX_DUTY) / SERVO_PWM_PERIOD_US;
    
    return duty;
}

static void set_servo_duty(int channel, uint32_t duty)
{
    ledc_channel_t ledc_channel = (channel == 1) ? LEDC_CHANNEL_1 : LEDC_CHANNEL_2;
    ledc_set_duty(LEDC_MODE, ledc_channel, duty);
    ledc_update_duty(LEDC_MODE, ledc_channel);
    
    int gpio_num = (channel == 1) ? SERVO_PWM_GPIO_1 : SERVO_PWM_GPIO_2;
    ESP_LOGI(TAG, "Set servo ch%d GPIO%d duty=%" PRIu32, channel, gpio_num, duty);
}

esp_err_t tool_servo_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing servo tool");

    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t ch1_cfg = {
        .gpio_num = SERVO_PWM_GPIO_1,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&ch1_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel 1 config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ledc_channel_config_t ch2_cfg = {
        .gpio_num = SERVO_PWM_GPIO_2,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_2,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&ch2_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel 2 config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    set_servo_duty(1, angle_to_duty(90));
    set_servo_duty(2, angle_to_duty(90));

    s_initialized = true;
    ESP_LOGI(TAG, "Servo initialized: GPIO%d (ch1), GPIO%d (ch2), 0-180 degrees",
             SERVO_PWM_GPIO_1, SERVO_PWM_GPIO_2);

    return ESP_OK;
}

esp_err_t tool_servo_stop_sweep(int channel)
{
    (void)channel;
    return ESP_OK;
}

esp_err_t tool_servo_control_execute(const char *input_json, char *output, size_t output_size)
{
    if (!s_initialized) {
        esp_err_t ret = tool_servo_init();
        if (ret != ESP_OK) {
            snprintf(output, output_size, "{\"error\":\"servo init failed\"}");
            return ret;
        }
    }

    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "{\"error\":\"invalid JSON\"}");
        return ESP_ERR_INVALID_ARG;
    }

    int channel = 1;
    int angle = 90;

    cJSON *channel_json = cJSON_GetObjectItem(root, "channel");
    if (channel_json && cJSON_IsNumber(channel_json)) {
        channel = channel_json->valueint;
    }

    cJSON *angle_json = cJSON_GetObjectItem(root, "angle");
    if (angle_json && cJSON_IsNumber(angle_json)) {
        angle = angle_json->valueint;
    }

    cJSON_Delete(root);

    if (channel < 1 || channel > 2) {
        snprintf(output, output_size, "{\"error\":\"invalid channel (1-2)\"}");
        return ESP_ERR_INVALID_ARG;
    }

    if (angle < SERVO_ANGLE_MIN) angle = SERVO_ANGLE_MIN;
    if (angle > SERVO_ANGLE_MAX) angle = SERVO_ANGLE_MAX;

    uint32_t duty = angle_to_duty(angle);
    set_servo_duty(channel, duty);

    int gpio_num = (channel == 1) ? SERVO_PWM_GPIO_1 : SERVO_PWM_GPIO_2;
    int pulse_us = SERVO_PULSE_MIN_US + (angle * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)) / SERVO_ANGLE_MAX;

    ESP_LOGI(TAG, "servo: ch=%d, angle=%d, pulse=%dus, GPIO=%d",
             channel, angle, pulse_us, gpio_num);

    snprintf(output, output_size,
             "{\"status\":\"ok\",\"channel\":%d,\"angle\":%d,\"pulse_us\":%d,\"gpio\":%d}",
             channel, angle, pulse_us, gpio_num);

    return ESP_OK;
}
