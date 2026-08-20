#include "fsm.h"
#include "config.h"
#include "rgb_led.h"
#include "oled.h"
#include "buzzer.h"
#include "mqtt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"

static const char *TAG = "FSM";

static QueueHandle_t fsm_queue = NULL;
static fsm_state_t current_state = STATE_IDLE_M1;
static game_mode_t current_mode = GAME_MODE_REACTION;
static fault_action_t fault_action = DEFAULT_FAULT_ACTION;
static mash_error_action_t mash_error_action = DEFAULT_MASH_ERROR_ACTION;

#define FSM_QUEUE_LEN   10

/* ---------------- Modo 1: timers y timestamps ---------------- */
static esp_timer_handle_t random_delay_timer = NULL;
static int64_t t_led_on_us = 0;
static int64_t t_release_us = 0;
static result_m1_t last_result_m1 = {0};

/* ---------------- Modo 2: timers, config y estado de ronda ---------------- */
static esp_timer_handle_t countdown_timer = NULL;
static esp_timer_handle_t mash_window_timer = NULL;
static int countdown_remaining = 0;
static uint32_t current_mash_window_ms = MASH_WINDOW_DEFAULT_MS;

static int32_t mash_count = 0;
static int last_button_pressed = -1;
static int64_t last_press_time_us = 0;
static int64_t sum_intervals_us = 0;
static int32_t interval_count = 0;
static result_m2_t last_result_m2 = {0};

static int64_t result_m2_pb1_time_us = -1;
static int64_t result_m2_pb2_time_us = -1;

/* ---------------- Utilidades ---------------- */
static const char *state_to_str(fsm_state_t s) {
    switch (s) {
        case STATE_IDLE_M1:              return "IDLE_M1";
        case STATE_WAIT_PB1_HOLD:        return "WAIT_PB1_HOLD";
        case STATE_RANDOM_DELAY:         return "RANDOM_DELAY";
        case STATE_LED_ON_WAIT_RELEASE:  return "LED_ON_WAIT_RELEASE";
        case STATE_WAIT_PB2_PRESS:       return "WAIT_PB2_PRESS";
        case STATE_RESULT_M1:            return "RESULT_M1";
        case STATE_FAULT_EARLY_RELEASE:  return "FAULT_EARLY_RELEASE";
        case STATE_IDLE_M2:              return "IDLE_M2";
        case STATE_COUNTDOWN_M2:         return "COUNTDOWN_M2";
        case STATE_RUNNING_M2:           return "RUNNING_M2";
        case STATE_RESULT_M2:            return "RESULT_M2";
        default:                         return "UNKNOWN";
    }
}

bool fsm_is_round_active(void) {
    switch (current_state) {
        case STATE_WAIT_PB1_HOLD:
        case STATE_RANDOM_DELAY:
        case STATE_LED_ON_WAIT_RELEASE:
        case STATE_WAIT_PB2_PRESS:
        case STATE_COUNTDOWN_M2:
        case STATE_RUNNING_M2:
            return true;
        default:
            return false;
    }
}

game_mode_t fsm_get_current_mode(void) { return current_mode; }
result_m1_t fsm_get_last_result_m1(void) { return last_result_m1; }
result_m2_t fsm_get_last_result_m2(void) { return last_result_m2; }

/* ================= MODO 1: Reaction Timer ================= */

static void random_delay_callback(void *arg) {
    fsm_event_t evt = { .type = EVT_TIMER_EXPIRED, .param = 0 };
    fsm_post_event(evt);
}

static void start_random_delay(void) {
    uint32_t range = REACTION_DELAY_MAX_MS - REACTION_DELAY_MIN_MS;
    uint32_t delay_ms = REACTION_DELAY_MIN_MS + (esp_random() % range);

    esp_timer_create_args_t args = { .callback = random_delay_callback, .name = "random_delay" };

    if (random_delay_timer) {
        esp_timer_delete(random_delay_timer);
        random_delay_timer = NULL;
    }
    esp_timer_create(&args, &random_delay_timer);
    esp_timer_start_once(random_delay_timer, (uint64_t)delay_ms * 1000ULL);

    ESP_LOGI(TAG, "Delay aleatorio iniciado: %lu ms", (unsigned long)delay_ms);
}

static void cancel_random_delay(void) {
    if (random_delay_timer) {
        esp_timer_stop(random_delay_timer);
        esp_timer_delete(random_delay_timer);
        random_delay_timer = NULL;
    }
}

static void m1_start_round(void) {
    current_state = STATE_WAIT_PB1_HOLD;
    oled_show_wait_hold();
    buzzer_beep(BUZZER_BEEP_PB1_HOLD_MS);
    ESP_LOGI(TAG, "Ronda M1 iniciada, mantenga PB1 presionado...");
}

static void m1_handle_early_release(void) {
    cancel_random_delay();
    rgb_led_off();

    ESP_LOGW(TAG, "Fault: PB1 soltado antes de tiempo");
    buzzer_beep_double(BUZZER_BEEP_FAULT_MS, BUZZER_BEEP_FAULT_GAP_MS);

    if (fault_action == FAULT_ACTION_PENALIZE) {
        last_result_m1.reaction_ms = -1;
        last_result_m1.second_press_ms = -1;
        last_result_m1.fault = true;
        current_state = STATE_RESULT_M1;
        oled_show_result_m1(last_result_m1);
        mqtt_publish_result_m1(last_result_m1);
        ESP_LOGI(TAG, "Resultado penalizado publicado (fault)");
        current_state = STATE_IDLE_M1;
    } else {
        current_state = STATE_IDLE_M1;
        oled_show_fault();
        ESP_LOGI(TAG, "Ronda reiniciada, presione PB1 de nuevo");
    }
}

static void m1_handle_timer_expired(void) {
    if (current_state != STATE_WAIT_PB1_HOLD) return;

    t_led_on_us = esp_timer_get_time();
    rgb_led_set(0, 255, 0);
    oled_show_react_now();
    buzzer_beep(BUZZER_BEEP_LED_ON_MS);
    current_state = STATE_LED_ON_WAIT_RELEASE;
    ESP_LOGI(TAG, "LED encendido. Suelte PB1 ahora!");
}

static void m1_handle_pb1_release(void) {
    int64_t now = esp_timer_get_time();
    t_release_us = now;
    rgb_led_off();

    int32_t reaction_ms = (int32_t)((now - t_led_on_us) / 1000);
    last_result_m1.reaction_ms = reaction_ms;
    last_result_m1.fault = false;

    current_state = STATE_WAIT_PB2_PRESS;
    oled_show_wait_second();
    ESP_LOGI(TAG, "Tiempo de reaccion: %ld ms. Presione el segundo boton (PB2)...", (long)reaction_ms);
}

static void m1_handle_pb2_press(void) {
    int64_t now = esp_timer_get_time();
    int32_t second_ms = (int32_t)((now - t_release_us) / 1000);
    last_result_m1.second_press_ms = second_ms;

    current_state = STATE_RESULT_M1;
    oled_show_result_m1(last_result_m1);
    buzzer_beep(BUZZER_BEEP_RESULT_MS);
    mqtt_publish_result_m1(last_result_m1);
    ESP_LOGI(TAG, "RESULTADO M1 -> reaccion: %ld ms | segundo boton: %ld ms",
             (long)last_result_m1.reaction_ms, (long)second_ms);

    current_state = STATE_IDLE_M1;
}

/* ================= MODO 2: Mash Test ================= */

static void countdown_tick_callback(void *arg) {
    fsm_event_t evt = { .type = EVT_COUNTDOWN_TICK, .param = 0 };
    fsm_post_event(evt);
}

static void mash_window_callback(void *arg) {
    fsm_event_t evt = { .type = EVT_MASH_WINDOW_EXPIRED, .param = 0 };
    fsm_post_event(evt);
}

static void cancel_countdown_timer(void) {
    if (countdown_timer) {
        esp_timer_stop(countdown_timer);
        esp_timer_delete(countdown_timer);
        countdown_timer = NULL;
    }
}

static void cancel_mash_window_timer(void) {
    if (mash_window_timer) {
        esp_timer_stop(mash_window_timer);
        esp_timer_delete(mash_window_timer);
        mash_window_timer = NULL;
    }
}

static void m2_start_running(void) {
    mash_count = 0;
    last_button_pressed = -1;
    sum_intervals_us = 0;
    interval_count = 0;
    last_press_time_us = 0;

    current_state = STATE_RUNNING_M2;
    oled_show_mash_running(mash_count);

    esp_timer_create_args_t args = { .callback = mash_window_callback, .name = "mash_window" };
    cancel_mash_window_timer();
    esp_timer_create(&args, &mash_window_timer);
    esp_timer_start_once(mash_window_timer, (uint64_t)current_mash_window_ms * 1000ULL);

    ESP_LOGI(TAG, "Mash Test iniciado, ventana: %lu ms", (unsigned long)current_mash_window_ms);
}

static void m2_start_countdown(void) {
    countdown_remaining = MASH_COUNTDOWN_SECONDS;
    current_state = STATE_COUNTDOWN_M2;
    oled_show_countdown_m2(countdown_remaining);
    buzzer_beep(BUZZER_BEEP_SHORT_MS);

    esp_timer_create_args_t args = { .callback = countdown_tick_callback, .name = "countdown" };
    cancel_countdown_timer();
    esp_timer_create(&args, &countdown_timer);
    esp_timer_start_periodic(countdown_timer, 1000000ULL);

    ESP_LOGI(TAG, "Countdown Mash Test iniciado");
}

static void m2_handle_countdown_tick(void) {
    countdown_remaining--;
    if (countdown_remaining > 0) {
        oled_show_countdown_m2(countdown_remaining);
        buzzer_beep(BUZZER_BEEP_SHORT_MS);
    } else {
        cancel_countdown_timer();
        m2_start_running();
    }
}

static void m2_handle_press(int btn) {
    if (last_button_pressed == btn) {
        switch (mash_error_action) {
            case MASH_ERROR_IGNORE:
                ESP_LOGW(TAG, "Press repetido ignorado (boton %d)", btn);
                break;
            case MASH_ERROR_PENALIZE_VISIBLE:
                ESP_LOGW(TAG, "Press repetido - penalizacion visible (boton %d)", btn);
                oled_show_mash_error(mash_count);
                break;
            case MASH_ERROR_RESET_COUNT:
                ESP_LOGW(TAG, "Press repetido - conteo reiniciado a 0");
                mash_count = 0;
                sum_intervals_us = 0;
                interval_count = 0;
                last_button_pressed = -1;
                oled_show_mash_running(mash_count);
                break;
        }
        return;
    }

    int64_t now = esp_timer_get_time();
    if (last_button_pressed != -1) {
        sum_intervals_us += (now - last_press_time_us);
        interval_count++;
    }
    last_press_time_us = now;
    last_button_pressed = btn;
    mash_count++;

    oled_show_mash_running(mash_count);
}

static void m2_handle_window_expired(void) {
    int32_t avg_ms = 0;
    if (interval_count > 0) {
        avg_ms = (int32_t)((sum_intervals_us / interval_count) / 1000);
    }

    last_result_m2.total_presses = mash_count;
    last_result_m2.avg_interval_ms = avg_ms;
    last_result_m2.window_ms = (int32_t)current_mash_window_ms;

    current_state = STATE_RESULT_M2;
    oled_show_result_m2(last_result_m2);
    buzzer_beep(BUZZER_BEEP_RESULT_MS);
    mqtt_publish_result_m2(last_result_m2);

    result_m2_pb1_time_us = -1;
    result_m2_pb2_time_us = -1;

    ESP_LOGI(TAG, "RESULTADO M2 -> total: %ld | promedio: %ld ms | ventana: %lu ms",
             (long)mash_count, (long)avg_ms, (unsigned long)current_mash_window_ms);
}

static void m2_handle_result_press(int btn) {
    int64_t now = esp_timer_get_time();

    if (btn == 0) {
        result_m2_pb1_time_us = now;
    } else {
        result_m2_pb2_time_us = now;
    }

    if (result_m2_pb1_time_us > 0 && result_m2_pb2_time_us > 0) {
        int64_t diff_ms = llabs(result_m2_pb1_time_us - result_m2_pb2_time_us) / 1000;
        if (diff_ms <= SIMULTANEOUS_PRESS_WINDOW_MS) {
            ESP_LOGI(TAG, "Presion simultanea detectada (diff %lld ms) -> reiniciando Mash Test",
                     (long long)diff_ms);
            result_m2_pb1_time_us = -1;
            result_m2_pb2_time_us = -1;
            m2_start_countdown();
        }
    }
}

/* ================= Manejo de modo (BOOT / MQTT) ================= */

static void enter_idle_for_mode(void) {
    current_state = (current_mode == GAME_MODE_REACTION) ? STATE_IDLE_M1 : STATE_IDLE_M2;
}

static void handle_boot_press(void) {
    if (fsm_is_round_active()) {
        ESP_LOGW(TAG, "BOOT ignorado: ronda activa (%s)", state_to_str(current_state));
        return;
    }
    current_mode = (current_mode == GAME_MODE_REACTION) ? GAME_MODE_MASH : GAME_MODE_REACTION;
    enter_idle_for_mode();
    oled_show_mode_idle(current_mode);
    buzzer_beep(BUZZER_BEEP_MODE_MS);
    mqtt_publish_mode_state(current_mode);
    ESP_LOGI(TAG, "Cambio de modo -> %s | estado -> %s",
             current_mode == GAME_MODE_REACTION ? "REACTION" : "MASH",
             state_to_str(current_state));
}

static void handle_mqtt_mode_set(int32_t new_mode) {
    if (fsm_is_round_active()) {
        ESP_LOGW(TAG, "MQTT mode_set ignorado: ronda activa");
        return;
    }
    if (new_mode != GAME_MODE_REACTION && new_mode != GAME_MODE_MASH) {
        ESP_LOGW(TAG, "Modo invalido recibido por MQTT: %ld", (long)new_mode);
        return;
    }
    current_mode = (game_mode_t)new_mode;
    enter_idle_for_mode();
    oled_show_mode_idle(current_mode);
    buzzer_beep(BUZZER_BEEP_MODE_MS);
    mqtt_publish_mode_state(current_mode);
    ESP_LOGI(TAG, "MQTT cambio de modo -> %s", state_to_str(current_state));
}

/* ================= Manejo de config remota (MQTT) ================= */

static void handle_set_fault_action(int32_t val) {
    if (val != FAULT_ACTION_RESTART && val != FAULT_ACTION_PENALIZE) {
        ESP_LOGW(TAG, "Valor invalido para fault_action: %ld", (long)val);
        return;
    }
    fault_action = (fault_action_t)val;
    ESP_LOGI(TAG, "MQTT config -> fault_action = %ld", (long)val);
}

static void handle_set_mash_error_action(int32_t val) {
    if (val != MASH_ERROR_IGNORE && val != MASH_ERROR_PENALIZE_VISIBLE && val != MASH_ERROR_RESET_COUNT) {
        ESP_LOGW(TAG, "Valor invalido para mash_error_action: %ld", (long)val);
        return;
    }
    mash_error_action = (mash_error_action_t)val;
    ESP_LOGI(TAG, "MQTT config -> mash_error_action = %ld", (long)val);
}

static void handle_set_mash_window(int32_t val) {
    if (val <= 0) {
        ESP_LOGW(TAG, "Valor invalido para mash_window_ms: %ld", (long)val);
        return;
    }
    current_mash_window_ms = (uint32_t)val;
    ESP_LOGI(TAG, "MQTT config -> mash_window_ms = %lu", (unsigned long)current_mash_window_ms);
}

/* ================= Dispatcher principal ================= */

static void dispatch_event(fsm_event_t evt) {
    ESP_LOGI(TAG, "Evento %d recibido en estado %s", evt.type, state_to_str(current_state));

    if (evt.type == EVT_BOOT_PRESS) {
        handle_boot_press();
        return;
    }
    if (evt.type == EVT_MQTT_MODE_SET) {
        handle_mqtt_mode_set(evt.param);
        return;
    }
    if (evt.type == EVT_MQTT_SET_FAULT_ACTION) {
        handle_set_fault_action(evt.param);
        return;
    }
    if (evt.type == EVT_MQTT_SET_MASH_ERROR_ACTION) {
        handle_set_mash_error_action(evt.param);
        return;
    }
    if (evt.type == EVT_MQTT_SET_MASH_WINDOW) {
        handle_set_mash_window(evt.param);
        return;
    }

    switch (current_state) {
        case STATE_IDLE_M1:
            if (evt.type == EVT_PB1_PRESS) {
                m1_start_round();
                start_random_delay();
                oled_show_get_ready();
                current_state = STATE_WAIT_PB1_HOLD;
            }
            break;

        case STATE_WAIT_PB1_HOLD:
            if (evt.type == EVT_PB1_RELEASE) {
                m1_handle_early_release();
            } else if (evt.type == EVT_TIMER_EXPIRED) {
                m1_handle_timer_expired();
            }
            break;

        case STATE_LED_ON_WAIT_RELEASE:
            if (evt.type == EVT_PB1_RELEASE) {
                m1_handle_pb1_release();
            }
            break;

        case STATE_WAIT_PB2_PRESS:
            if (evt.type == EVT_PB2_PRESS) {
                m1_handle_pb2_press();
            }
            break;

        case STATE_IDLE_M2:
            if (evt.type == EVT_PB1_PRESS || evt.type == EVT_PB2_PRESS) {
                m2_start_countdown();
            }
            break;

        case STATE_COUNTDOWN_M2:
            if (evt.type == EVT_COUNTDOWN_TICK) {
                m2_handle_countdown_tick();
            }
            break;

        case STATE_RUNNING_M2:
            if (evt.type == EVT_PB1_PRESS) {
                m2_handle_press(0);
            } else if (evt.type == EVT_PB2_PRESS) {
                m2_handle_press(1);
            } else if (evt.type == EVT_MASH_WINDOW_EXPIRED) {
                m2_handle_window_expired();
            }
            break;

        case STATE_RESULT_M2:
            if (evt.type == EVT_PB1_PRESS) {
                m2_handle_result_press(0);
            } else if (evt.type == EVT_PB2_PRESS) {
                m2_handle_result_press(1);
            }
            break;

        default:
            break;
    }
}

static void fsm_task(void *arg) {
    fsm_event_t evt;
    ESP_LOGI(TAG, "FSM task iniciado, estado inicial: %s", state_to_str(current_state));
    for (;;) {
        if (xQueueReceive(fsm_queue, &evt, portMAX_DELAY) == pdTRUE) {
            dispatch_event(evt);
        }
    }
}

void fsm_init(void) {
    fsm_queue = xQueueCreate(FSM_QUEUE_LEN, sizeof(fsm_event_t));
    current_state = STATE_IDLE_M1;
    current_mode = GAME_MODE_REACTION;
    fault_action = DEFAULT_FAULT_ACTION;
    mash_error_action = DEFAULT_MASH_ERROR_ACTION;
    current_mash_window_ms = MASH_WINDOW_DEFAULT_MS;
}

void fsm_start_task(void) {
    xTaskCreate(fsm_task, "fsm_task", 4096, NULL, 10, NULL);
}

bool fsm_post_event(fsm_event_t evt) {
    if (fsm_queue == NULL) return false;
    return xQueueSend(fsm_queue, &evt, pdMS_TO_TICKS(10)) == pdTRUE;
}