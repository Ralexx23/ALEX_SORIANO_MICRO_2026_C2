#include "motor.h"
#include "driver/gpio.h"
#include "config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "MOTOR";
static char motor_status[17] = "DETENIDO";

static inline void relay_set(gpio_num_t pin, bool energized)
{
    // Si tu relay es activo en LOW, MOTOR_RELAY_ACTIVE_LEVEL = 0 invierte esto automaticamente
    gpio_set_level(pin, energized ? MOTOR_RELAY_ACTIVE_LEVEL : !MOTOR_RELAY_ACTIVE_LEVEL);
}

const char *motor_get_status_str(void)
{
    return motor_status;
}

void motor_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_MOTOR_RELAY_OPEN) | (1ULL << PIN_MOTOR_RELAY_CLOSE),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);

    relay_set(PIN_MOTOR_RELAY_OPEN, false);
    relay_set(PIN_MOTOR_RELAY_CLOSE, false);

    ESP_LOGI(TAG, "Motor inicializado (control por reles)");
}

void motor_open(void)
{
    // Interlock: primero apaga el rele contrario si estaba activo
    relay_set(PIN_MOTOR_RELAY_CLOSE, false);
    vTaskDelay(pdMS_TO_TICKS(MOTOR_DIRECTION_SWITCH_DELAY_MS));

    relay_set(PIN_MOTOR_RELAY_OPEN, true);
    snprintf(motor_status, sizeof(motor_status), "ABRIENDO");
    ESP_LOGI(TAG, "Motor abriendo (rele OPEN activado)");
}

void motor_close(void)
{
    relay_set(PIN_MOTOR_RELAY_OPEN, false);
    vTaskDelay(pdMS_TO_TICKS(MOTOR_DIRECTION_SWITCH_DELAY_MS));

    relay_set(PIN_MOTOR_RELAY_CLOSE, true);
    snprintf(motor_status, sizeof(motor_status), "CERRANDO");
    ESP_LOGI(TAG, "Motor cerrando (rele CLOSE activado)");
}

void motor_stop(void)
{
    relay_set(PIN_MOTOR_RELAY_OPEN, false);
    relay_set(PIN_MOTOR_RELAY_CLOSE, false);
    snprintf(motor_status, sizeof(motor_status), "DETENIDO");
    ESP_LOGI(TAG, "Motor detenido (ambos reles apagados)");
}