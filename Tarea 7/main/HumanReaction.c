#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fsm.h"
#include "button.h"
#include "rgb_led.h"
#include "oled.h"
#include "buzzer.h"
#include "wifi.h"
#include "mqtt.h"
#include "config.h"
#include "esp_log.h"

void app_main(void) {
    rgb_led_init();
    oled_init();
    buzzer_init();
    fsm_init();
    fsm_start_task();
    button_init();
    wifi_init_sta();
    mqtt_app_start();

    oled_show_mode_idle(fsm_get_current_mode());

    ESP_LOGI("MAIN", "Sistema iniciado. Presiona y manten PB1 para iniciar ronda.");
}