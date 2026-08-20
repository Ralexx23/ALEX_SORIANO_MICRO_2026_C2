#include "rgb_led.h"
#include "config.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "RGB_LED";
static led_strip_handle_t strip = NULL;

void rgb_led_init(void) {
    led_strip_config_t strip_config = {
    .strip_gpio_num = PIN_RGB_LED,
    .max_leds = 1,
    .led_model = LED_MODEL_WS2812,
    .led_pixel_format = LED_PIXEL_FORMAT_GRB, 
    .flags.invert_out = false,
};

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al inicializar led_strip: %s", esp_err_to_name(err));
        return;
    }

    rgb_led_off();
    ESP_LOGI(TAG, "RGB LED inicializado en GPIO %d", PIN_RGB_LED);
}

void rgb_led_set(uint8_t r, uint8_t g, uint8_t b) {
    if (!strip) return;
    led_strip_set_pixel(strip, 0, r, g, b);
    led_strip_refresh(strip);
}

void rgb_led_off(void) {
    if (!strip) return;
    led_strip_clear(strip);
}