#include "inputs.h"
#include "config.h"
#include "fsm.h"
#include "actuator.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "runtime_config.h"
#include "mqtt_manager.h"

static const char *TAG = "INPUTS";

typedef struct {
    gpio_num_t pin;
    bool stable_active;      // estado confirmado (true = switch activado)
    bool candidate_active;   // ultima lectura cruda distinta a stable_active
    uint8_t debounce_count;
} debounced_input_t;

static debounced_input_t limit_open  = { .pin = PIN_LIMIT_OPEN };
static debounced_input_t limit_close = { .pin = PIN_LIMIT_CLOSE };
static debounced_input_t photocell   = { .pin = PIN_PHOTOCELL };
static esp_timer_handle_t poll_timer;
static bool prev_both_active = false;
static bool stopped_for_obstacle = false;
static bool stopped_was_opening  = false;

bool inputs_is_obstacle_active(void)  { return photocell.stable_active; }
bool inputs_limit_open_active(void)   { return limit_open.stable_active; }
bool inputs_limit_close_active(void)  { return limit_close.stable_active; }

static void update_debounced_input(debounced_input_t *in)
{
    bool raw_active = (gpio_get_level(in->pin) == 0); // activo en bajo, por pull-up

    if (raw_active == in->candidate_active) {
        if (in->debounce_count < INPUT_DEBOUNCE_SAMPLES) {
            in->debounce_count++;
        }
    } else {
        in->candidate_active = raw_active;
        in->debounce_count = 1;
    }

    if (in->debounce_count >= INPUT_DEBOUNCE_SAMPLES) {
        in->stable_active = in->candidate_active;
    }
}

static void poll_cb(void *arg)
{
    bool prev_open  = limit_open.stable_active;
    bool prev_close = limit_close.stable_active;
    bool prev_photo = photocell.stable_active;

    update_debounced_input(&limit_open);
    update_debounced_input(&limit_close);
    update_debounced_input(&photocell);

    // Falla dura: los dos limit switch no pueden estar activos a la vez
    bool both_active = limit_open.stable_active && limit_close.stable_active;
    if (both_active) {
        if (!prev_both_active) {
            ESP_LOGW(TAG, "Ambos limit switch activos -> FAULT");
            mqtt_publish_fault("LIMITS_CONFLICT");
            fsm_handle_event(EVENT_BOTH_LIMITS_ACTIVE);
            actuator_apply_state(fsm_get_state());
        }
        prev_both_active = true;
        return;
    }
    prev_both_active = false;

    if (limit_open.stable_active && !prev_open) {
        ESP_LOGI(TAG, "Limit switch OPEN activado");
        mqtt_publish_limits(true, limit_close.stable_active);
        fsm_handle_event(EVENT_LIMIT_OPEN_REACHED);
        actuator_apply_state(fsm_get_state());
    }

    if (limit_close.stable_active && !prev_close) {
        ESP_LOGI(TAG, "Limit switch CLOSE activado");
        mqtt_publish_limits(limit_open.stable_active, true);
        fsm_handle_event(EVENT_LIMIT_CLOSE_REACHED);
        actuator_apply_state(fsm_get_state());
    }

    if (photocell.stable_active && !prev_photo) {
        ESP_LOGW(TAG, "Fotocelda: obstaculo detectado");
        gate_state_t s = fsm_get_state();

        if (s == GATE_STATE_CLOSED) {
            ESP_LOGW(TAG, "Obstaculo con porton CLOSED -> FAULT");
            mqtt_publish_fault("OBSTACLE_WHILE_CLOSED");
            fsm_handle_event(EVENT_OBSTACLE_FAULT);
            actuator_apply_state(fsm_get_state());
        } else if (s == GATE_STATE_OPENING || s == GATE_STATE_CLOSING) {
            stopped_was_opening = (s == GATE_STATE_OPENING);
            obstacle_behavior_t behavior = runtime_config_get_obstacle_behavior();

            fsm_handle_event(EVENT_OBSTACLE_DETECTED); // -> STOPPED

            if (behavior == OBSTACLE_STOP_AND_REVERSE) {
                if (stopped_was_opening) fsm_handle_event(EVENT_CMD_CLOSE);
                else                     fsm_handle_event(EVENT_CMD_OPEN);
            } else if (behavior == OBSTACLE_STOP_AND_RESUME) {
                stopped_for_obstacle = true;
            }
            actuator_apply_state(fsm_get_state());
        }
        // si esta en OPEN, no hacemos nada aqui: el bloqueo del comando CLOSE
        // se maneja en main.c y mqtt_manager.c
        mqtt_publish_obstacle(true);

    } else if (!photocell.stable_active && prev_photo) {
        ESP_LOGI(TAG, "Fotocelda: camino despejado");
        if (stopped_for_obstacle && fsm_get_state() == GATE_STATE_STOPPED) {
            stopped_for_obstacle = false;
            if (stopped_was_opening) fsm_handle_event(EVENT_CMD_OPEN);
            else                     fsm_handle_event(EVENT_CMD_CLOSE);
            actuator_apply_state(fsm_get_state());
        }
        mqtt_publish_obstacle(false);
    }
}

void inputs_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_LIMIT_OPEN) | (1ULL << PIN_LIMIT_CLOSE) | (1ULL << PIN_PHOTOCELL),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);

    const esp_timer_create_args_t timer_args = {
        .callback = &poll_cb,
        .name = "input_poll"
    };
    esp_timer_create(&timer_args, &poll_timer);
    esp_timer_start_periodic(poll_timer, INPUT_POLL_INTERVAL_MS * 1000);

    ESP_LOGI(TAG, "Entradas inicializadas, polling cada %d ms", INPUT_POLL_INTERVAL_MS);
}