#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "MOTOR_S3";

#define PWM_GPIO       GPIO_NUM_25
#define PWM_FREQUENCY  20000
#define PWM_CHANNEL    LEDC_CHANNEL_0
#define PWM_MODE       LEDC_LOW_SPEED_MODE
#define PWM_TIMER      LEDC_TIMER_0
#define PWM_RES        LEDC_TIMER_10_BIT

#define RPM_MAX 32670
#define RPM_MIN 600

const gpio_num_t filas[] = {GPIO_NUM_19, GPIO_NUM_18, GPIO_NUM_5, GPIO_NUM_17};
const gpio_num_t columnas[] = {GPIO_NUM_16, GPIO_NUM_4, GPIO_NUM_2, GPIO_NUM_15};

const char key_map[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

float set_rpm = 0;

void pwm_init(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_RES,
        .freq_hz          = PWM_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t channel_conf = {
        .gpio_num   = PWM_GPIO,
        .speed_mode = PWM_MODE,
        .channel    = PWM_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_conf));
}

float rpm_to_percent(float rpm)
{
    if (rpm < RPM_MIN) rpm = RPM_MIN;
    if (rpm > RPM_MAX) rpm = RPM_MAX;

    return ((rpm - RPM_MIN) / (RPM_MAX - RPM_MIN)) * 100.0;
}

void speed_motor_percent(float percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    uint32_t duty = (percent / 100.0) * ((1 << 10) - 1);

    ESP_ERROR_CHECK(ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(PWM_MODE, PWM_CHANNEL));
}

void configure_keyboard(void)
{
    for (int i = 0; i < 4; i++)
    {
        gpio_reset_pin(filas[i]);
        gpio_set_direction(filas[i], GPIO_MODE_OUTPUT);
        gpio_set_level(filas[i], 1);
    }

    for (int i = 0; i < 4; i++)
    {
        gpio_reset_pin(columnas[i]);
        gpio_set_direction(columnas[i], GPIO_MODE_INPUT);
        gpio_set_pull_mode(columnas[i], GPIO_PULLDOWN_ONLY);
    }
}

char scan_keyboard(void)
{
    for (int row = 0; row < 4; row++)
    {
        for (int i = 0; i < 4; i++)
            gpio_set_level(filas[i], i == row);

        vTaskDelay(pdMS_TO_TICKS(2));

        for (int col = 0; col < 4; col++)
        {
            if (gpio_get_level(columnas[col]))
            {
                vTaskDelay(pdMS_TO_TICKS(5));
                while (gpio_get_level(columnas[col]))
                    vTaskDelay(pdMS_TO_TICKS(5));

                return key_map[row][col];
            }
        }
    }
    return '\0';
}

float read_num(void)
{
    char num[10];
    int idx = 0;
    num[0] = '\0';

    printf("Ingrese RPM y presione #:\n");

    while (1)
    {
        char key = scan_keyboard();

        if (isdigit(key) && idx < 9)
        {
            num[idx++] = key;
            num[idx] = '\0';
            printf("%c", key);
        }
        else if (key == '#')
        {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }

    printf("\n");
    return strlen(num) ? atof(num) : 0;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando sistema en ESP32-S3...");

    configure_keyboard();
    pwm_init();

    set_rpm = read_num();

    float percent = rpm_to_percent(set_rpm);
    speed_motor_percent(percent);

    ESP_LOGI(TAG, "Motor ajustado a %.0f RPM", set_rpm);

    while (1)
    {
        char key = scan_keyboard();

        if (key == 'B')
        {
            set_rpm = read_num();
            percent = rpm_to_percent(set_rpm);
            speed_motor_percent(percent);
            ESP_LOGI(TAG, "Nueva RPM: %.0f", set_rpm);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
