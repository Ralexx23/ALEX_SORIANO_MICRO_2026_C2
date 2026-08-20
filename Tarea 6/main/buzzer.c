#include "buzzer.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "BUZZER";
static gate_state_t last_state = GATE_STATE_INIT;
static bool beep_phase = false;

static void apply(void)
{
    bool on = (last_state == GATE_STATE_FAULT) && beep_phase;
    gpio_set_level(PIN_BUZZER, on ? 1 : 0);
}

static void beep_timer_cb(void *arg)
{
    beep_phase = !beep_phase;
    apply();
}

void buzzer_set_state(gate_state_t state)
{
    last_state = state;
    if (state != GATE_STATE_FAULT) {
        beep_phase = false;
    }
    apply();
}

void buzzer_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_BUZZER, 0);

    const esp_timer_create_args_t timer_args = {
        .callback = &beep_timer_cb,
        .name = "buzzer_beep"
    };
    esp_timer_handle_t beep_timer;
    esp_timer_create(&timer_args, &beep_timer);
    esp_timer_start_periodic(beep_timer, BUZZER_BEEP_INTERVAL_MS * 1000);

    ESP_LOGI(TAG, "Buzzer inicializado");
}