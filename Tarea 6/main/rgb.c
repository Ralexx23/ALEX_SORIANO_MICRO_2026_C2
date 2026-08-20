#include "rgb.h"
#include "config.h"
#include "led_strip.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "RGB";
static led_strip_handle_t strip;
static gate_state_t last_state = GATE_STATE_INIT;
static bool blink_phase = false;

static void set_color(uint8_t r, uint8_t g, uint8_t b)
{
    led_strip_set_pixel(strip, 0, r, g, b);
    led_strip_refresh(strip);
}

static void render(void)
{
    switch (last_state) {
        case GATE_STATE_CLOSED:
            set_color(255, 0, 0);
            break;
        case GATE_STATE_OPEN:
            set_color(0, 255, 0);
            break;
        case GATE_STATE_OPENING:
        case GATE_STATE_CLOSING:
            blink_phase ? set_color(255, 255, 0) : set_color(0, 0, 0);
            break;
        case GATE_STATE_STOPPED:
            blink_phase ? set_color(255, 0, 0) : set_color(0, 255, 0);
            break;
        case GATE_STATE_FAULT:
            set_color(255, 255, 0); // fijo, sin parpadeo
            break;
        case GATE_STATE_CALIBRATION:
            set_color(0, 0, 255);
            break;
        default: // INIT
            set_color(0, 0, 0);
            break;
    }
}

static void blink_timer_cb(void *arg)
{
    blink_phase = !blink_phase;
    render();
}

void rgb_set_state(gate_state_t state)
{
    last_state = state;
    render();
}

void rgb_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = PIN_RGB_WS2812,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    led_strip_clear(strip); // apaga el blanco inicial que veias

    const esp_timer_create_args_t timer_args = {
        .callback = &blink_timer_cb,
        .name = "rgb_blink"
    };
    esp_timer_handle_t blink_timer;
    esp_timer_create(&timer_args, &blink_timer);
    esp_timer_start_periodic(blink_timer, RGB_BLINK_INTERVAL_MS * 1000);

    ESP_LOGI(TAG, "RGB inicializado (WS2812 via RMT)");
}