#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "config.h"
#include "fsm.h"
#include "motor.h"
#include "actuator.h"
#include "display.h"
#include "inputs.h"
#include "rgb.h"
#include "buzzer.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "runtime_config.h"

static const char *TAG = "MAIN";

static void gpio_setup(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_BOOT_BUTTON) | (1ULL << PIN_CALIB_SELECT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);
}

static void display_refresh_cb(void *arg)
{
    display_update(fsm_get_state(), motor_get_status_str(), inputs_is_obstacle_active());
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    runtime_config_init();
    wifi_manager_init();

    gpio_setup();
    fsm_init();
    motor_init();
    display_init();
    inputs_init();
    mqtt_manager_init();
    rgb_init();
    buzzer_init();
    vTaskDelay(pdMS_TO_TICKS(10)); // deja estabilizar los pull-ups
    fsm_resolve_boot_state(inputs_limit_open_active(), inputs_limit_close_active());
    actuator_apply_state(fsm_get_state());

    const esp_timer_create_args_t display_timer_args = {
        .callback = &display_refresh_cb,
        .name = "display_refresh"
    };
    esp_timer_handle_t display_timer;
    esp_timer_create(&display_timer_args, &display_timer);
    esp_timer_start_periodic(display_timer, 300000); // cada 300ms

    bool last_button = true; // pull-up: true = no presionado
    bool last_calib = true;  // pull-up: true = no activado

    while (1) {
        bool button = gpio_get_level(PIN_BOOT_BUTTON);
        bool calib  = gpio_get_level(PIN_CALIB_SELECT);

        // --- Selector de calibracion ---
        if (last_calib && !calib) { // flanco de bajada = activado
            fsm_handle_event(EVENT_CALIB_TOGGLE);
            actuator_apply_state(fsm_get_state());
        }
        last_calib = calib;

        // --- Boton BOOT (simulacion de comandos) ---
        if (last_button && !button) { // flanco de bajada = presionado
            gate_state_t s = fsm_get_state();
            if (s == GATE_STATE_CLOSED) {
                fsm_handle_event(EVENT_CMD_OPEN);
                actuator_apply_state(fsm_get_state());
            } else if (s == GATE_STATE_OPEN) {
                if (inputs_is_obstacle_active()) {
                    ESP_LOGW(TAG, "CLOSE bloqueado: obstaculo presente");
                } else {
                    fsm_handle_event(EVENT_CMD_CLOSE);
                    actuator_apply_state(fsm_get_state());
                }
            } else if (s == GATE_STATE_OPENING || s == GATE_STATE_CLOSING) {
                fsm_handle_event(EVENT_CMD_STOP);
                actuator_apply_state(fsm_get_state());
            } else if (s == GATE_STATE_STOPPED) {
                fsm_handle_event(EVENT_CMD_OPEN);
                actuator_apply_state(fsm_get_state());
            } else if (s == GATE_STATE_FAULT) {
                fsm_handle_reset(inputs_limit_open_active(), inputs_limit_close_active());
                actuator_apply_state(fsm_get_state());
            }
        }
        last_button = button;

        vTaskDelay(pdMS_TO_TICKS(50)); // debounce simple
    }
}