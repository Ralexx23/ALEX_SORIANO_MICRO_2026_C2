#include "button.h"
#include "config.h"
#include "fsm.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "BUTTON";

typedef enum {
    BTN_PB1 = 0,
    BTN_PB2,
    BTN_BOOT,
    BTN_COUNT
} button_id_t;

typedef struct {
    gpio_num_t gpio;
    button_id_t id;
    int last_level;   // 1 = suelto (pull-up), 0 = presionado
} button_info_t;

static button_info_t buttons[BTN_COUNT] = {
    [BTN_PB1]  = { .gpio = PIN_PB1,  .id = BTN_PB1,  .last_level = 1 },
    [BTN_PB2]  = { .gpio = PIN_PB2,  .id = BTN_PB2,  .last_level = 1 },
    [BTN_BOOT] = { .gpio = PIN_BOOT, .id = BTN_BOOT, .last_level = 1 },
};

#define BUTTON_ISR_QUEUE_LEN 20

static QueueHandle_t button_isr_queue = NULL;

// ---------------- ISR (mínima, ISR-safe) ----------------
static void IRAM_ATTR button_isr_handler(void *arg) {
    button_id_t id = (button_id_t)(uintptr_t)arg;
    BaseType_t higher_prio_woken = pdFALSE;
    xQueueSendFromISR(button_isr_queue, &id, &higher_prio_woken);
    if (higher_prio_woken) {
        portYIELD_FROM_ISR();
    }
}

// ---------------- Task de debounce + generación de eventos ----------------
static void post_button_event(button_id_t id, bool pressed) {
    fsm_event_t evt = {0};

    switch (id) {
        case BTN_PB1:
            evt.type = pressed ? EVT_PB1_PRESS : EVT_PB1_RELEASE;
            break;
        case BTN_PB2:
            evt.type = pressed ? EVT_PB2_PRESS : EVT_PB2_RELEASE;
            break;
        case BTN_BOOT:
            if (!pressed) return;   // BOOT solo nos interesa el press
            evt.type = EVT_BOOT_PRESS;
            break;
        default:
            return;
    }

    ESP_LOGI(TAG, "Boton %d -> %s", id, pressed ? "PRESIONADO" : "SOLTADO");
    fsm_post_event(evt);
}

static void button_task(void *arg) {
    button_id_t id;
    for (;;) {
        if (xQueueReceive(button_isr_queue, &id, portMAX_DELAY) == pdTRUE) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));

            int level = gpio_get_level(buttons[id].gpio);

            if (level != buttons[id].last_level) {
                buttons[id].last_level = level;
                bool pressed = (level == 0);  // activo en bajo
                post_button_event(id, pressed);
            }
            // si el nivel es igual al anterior -> fue rebote, se ignora
        }
    }
}

// ---------------- Configuración de GPIOs ----------------
static void configure_button_gpio(gpio_num_t pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    gpio_config(&io_conf);
}

void button_init(void) {
    button_isr_queue = xQueueCreate(BUTTON_ISR_QUEUE_LEN, sizeof(button_id_t));

    configure_button_gpio(PIN_PB1);
    configure_button_gpio(PIN_PB2);
    configure_button_gpio(PIN_BOOT);

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Fallo al instalar ISR service: %s", esp_err_to_name(err));
        return;
    }

    gpio_isr_handler_add(PIN_PB1,  button_isr_handler, (void *)(uintptr_t)BTN_PB1);
    gpio_isr_handler_add(PIN_PB2,  button_isr_handler, (void *)(uintptr_t)BTN_PB2);
    gpio_isr_handler_add(PIN_BOOT, button_isr_handler, (void *)(uintptr_t)BTN_BOOT);

    xTaskCreate(button_task, "button_task", 3072, NULL, 9, NULL);

    ESP_LOGI(TAG, "Botones inicializados (PB1=%d, PB2=%d, BOOT=%d)",
             PIN_PB1, PIN_PB2, PIN_BOOT);
}