#include "buzzer.h"
#include "config.h"

#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BUZZER";
static esp_timer_handle_t off_timer = NULL;

static void buzzer_off_callback(void *arg) {
    gpio_set_level(PIN_BUZZER, 0);
}

void buzzer_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_BUZZER, 0);

    esp_timer_create_args_t args = { .callback = buzzer_off_callback, .name = "buzzer_off" };
    esp_timer_create(&args, &off_timer);

    ESP_LOGI(TAG, "Buzzer inicializado en GPIO %d", PIN_BUZZER);
}

void buzzer_beep(uint32_t duration_ms) {
    if (!off_timer) return;

    esp_timer_stop(off_timer);  // por si habia un beep en curso, lo re-dispara
    gpio_set_level(PIN_BUZZER, 1);
    esp_timer_start_once(off_timer, (uint64_t)duration_ms * 1000ULL);
}

void buzzer_beep_double(uint32_t duration_ms, uint32_t gap_ms) {
    buzzer_beep(duration_ms);
    vTaskDelay(pdMS_TO_TICKS(duration_ms + gap_ms));
    buzzer_beep(duration_ms);
    vTaskDelay(pdMS_TO_TICKS(duration_ms + gap_ms));
    buzzer_beep(duration_ms);
}