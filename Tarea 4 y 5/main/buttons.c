#include "buttons.h"
#include "pin_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "buttons";

#define POLL_PERIOD_MS      10
#define DEBOUNCE_STABLE_CNT 3   // 3*10ms = 30ms estable para confirmar cambio

typedef struct {
    gpio_num_t pin;
    bool stable_state;      // estado debounceado actual (true = presionado)
    bool last_raw;
    uint8_t stable_count;
} btn_ch_t;

enum { B_TL1, B_TL2, B_TR1, B_TR2, B_FA, B_FB, B_FX, B_FY, B_TOPL, B_TOPR, B_COUNT };

static btn_ch_t s_ch[B_COUNT];
static SemaphoreHandle_t s_mutex;
static volatile bool s_cal_triggered = false;
static int64_t s_top_hold_start_us = -1;

static void configure_input(gpio_num_t pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

static void poll_task(void *arg)
{
    (void)arg;
    for (;;) {
        for (int i = 0; i < B_COUNT; i++) {
            bool raw_pressed = (gpio_get_level(s_ch[i].pin) == 0); // activo en LOW
            if (raw_pressed == s_ch[i].last_raw) {
                if (s_ch[i].stable_count < 255) s_ch[i].stable_count++;
            } else {
                s_ch[i].stable_count = 0;
                s_ch[i].last_raw = raw_pressed;
            }
            if (s_ch[i].stable_count >= DEBOUNCE_STABLE_CNT) {
                s_ch[i].stable_state = raw_pressed;
            }
        }

        /* Combo de calibración: BTN_TOP_L + BTN_TOP_R sostenidos 3s */
        bool both_top = s_ch[B_TOPL].stable_state && s_ch[B_TOPR].stable_state;
        if (both_top) {
            if (s_top_hold_start_us < 0) {
                s_top_hold_start_us = esp_timer_get_time();
            } else if (!s_cal_triggered &&
                       (esp_timer_get_time() - s_top_hold_start_us) >= (int64_t)CALIBRATION_HOLD_MS * 1000) {
                s_cal_triggered = true;
                ESP_LOGI(TAG, "combo de calibracion detectado (3s)");
            }
        } else {
            s_top_hold_start_us = -1;
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
    }
}

esp_err_t buttons_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    gpio_num_t pins[B_COUNT] = {
        PIN_BTN_TRIG_L1, PIN_BTN_TRIG_L2, PIN_BTN_TRIG_R1, PIN_BTN_TRIG_R2,
        PIN_BTN_FRONT_A, PIN_BTN_FRONT_B, PIN_BTN_FRONT_X, PIN_BTN_FRONT_Y,
        PIN_BTN_TOP_L, PIN_BTN_TOP_R
    };
    for (int i = 0; i < B_COUNT; i++) {
        s_ch[i].pin = pins[i];
        configure_input(pins[i]);
    }

    xTaskCreate(poll_task, "btn_poll", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "botones listos (%d canales)", B_COUNT);
    return ESP_OK;
}

void buttons_read(button_state_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    out->trig_l1 = s_ch[B_TL1].stable_state;
    out->trig_l2 = s_ch[B_TL2].stable_state;
    out->trig_r1 = s_ch[B_TR1].stable_state;
    out->trig_r2 = s_ch[B_TR2].stable_state;
    out->front_a = s_ch[B_FA].stable_state;
    out->front_b = s_ch[B_FB].stable_state;
    out->front_x = s_ch[B_FX].stable_state;
    out->front_y = s_ch[B_FY].stable_state;
    out->top_l   = s_ch[B_TOPL].stable_state;
    out->top_r   = s_ch[B_TOPR].stable_state;
    xSemaphoreGive(s_mutex);
}

bool buttons_calibration_triggered(void)
{
    if (s_cal_triggered) {
        s_cal_triggered = false;
        return true;
    }
    return false;
}
