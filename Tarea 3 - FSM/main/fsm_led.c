#include "fsm_led.h"
#include "pin_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "fsm_led";

static QueueHandle_t s_cmd_queue;
static volatile fsm_state_t s_state = FSM_STATE_OFF;
static void (*s_on_state_changed)(fsm_state_t) = NULL;

#define BUTTON_POLL_MS       10
#define BUTTON_DEBOUNCE_CNT  3   // 3*10ms = 30ms estable

/* ===================== LED ===================== */

static void led_set(bool on)
{
    gpio_set_level(PIN_LED, on ? 1 : 0);
}

/* ===================== FSM (2 estados) =====================
 * OFF --(ON o TOGGLE)--> ON
 * ON  --(OFF o TOGGLE)--> OFF
 * Es literalmente una máquina de 2 estados: cualquier comando que no
 * mantenga el estado actual dispara la transición.
 */
static void fsm_task(void *arg)
{
    (void)arg;
    fsm_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            fsm_state_t next = s_state;

            switch (s_state) {
                case FSM_STATE_OFF:
                    if (cmd == FSM_CMD_ON || cmd == FSM_CMD_TOGGLE) next = FSM_STATE_ON;
                    break;
                case FSM_STATE_ON:
                    if (cmd == FSM_CMD_OFF || cmd == FSM_CMD_TOGGLE) next = FSM_STATE_OFF;
                    break;
            }

            if (next != s_state) {
                s_state = next;
                led_set(s_state == FSM_STATE_ON);
                ESP_LOGI(TAG, "estado -> %s", s_state == FSM_STATE_ON ? "ON" : "OFF");
                if (s_on_state_changed) s_on_state_changed(s_state);
            }
        }
    }
}

/* ===================== Botón (debounce por polling) ===================== */

static void button_task(void *arg)
{
    (void)arg;
    bool last_raw = true;   // pull-up: reposo = HIGH (no presionado)
    bool stable = true;
    uint8_t stable_count = 0;

    for (;;) {
        bool raw = gpio_get_level(PIN_BUTTON); // HIGH=suelto, LOW=presionado

        if (raw == last_raw) {
            if (stable_count < 255) stable_count++;
        } else {
            stable_count = 0;
            last_raw = raw;
        }

        if (stable_count == BUTTON_DEBOUNCE_CNT && stable != raw) {
            stable = raw;
            if (stable == false) { // flanco a presionado
                fsm_led_request(FSM_CMD_TOGGLE);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

/* ===================== API pública ===================== */

void fsm_led_request(fsm_cmd_t cmd)
{
    if (s_cmd_queue) {
        xQueueSend(s_cmd_queue, &cmd, 0);
    }
}

fsm_state_t fsm_led_get_state(void)
{
    return s_state;
}

void fsm_led_init(void (*on_state_changed)(fsm_state_t new_state))
{
    s_on_state_changed = on_state_changed;

    gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << PIN_LED,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    led_set(false);

    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << PIN_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    s_cmd_queue = xQueueCreate(8, sizeof(fsm_cmd_t));

    xTaskCreate(fsm_task, "fsm_task", 2048, NULL, 6, NULL);
    xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "FSM+LED+boton listos (LED=GPIO%d, BTN=GPIO%d)", PIN_LED, PIN_BUTTON);
}
