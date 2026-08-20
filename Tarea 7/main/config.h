#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"

/* ---------------- OLED / I2C ---------------- */
#define I2C_PORT_NUM         I2C_NUM_0
#define I2C_FREQ_HZ          400000
#define OLED_I2C_ADDR         0x3C

/* ---------------- PINES ---------------- */
#define PIN_PB1                 GPIO_NUM_4   // Botón 1 (reacción / mash)
#define PIN_PB2                 GPIO_NUM_5   // Botón 2 (segundo botón / mash)
#define PIN_BOOT                GPIO_NUM_0   // Boot button (cambio de modo)

#define PIN_RGB_LED             GPIO_NUM_48  // WS2812 integrado ESP32-S3

#define PIN_OLED_SDA            GPIO_NUM_8
#define PIN_OLED_SCL            GPIO_NUM_9

#define PIN_BUZZER              GPIO_NUM_10  // opcional, feedback sonoro

/* ---------------- TIMING ---------------- */
#define DEBOUNCE_TIME_MS        30

#define REACTION_DELAY_MIN_MS   1500
#define REACTION_DELAY_MAX_MS   5000

#define MASH_WINDOW_DEFAULT_MS  1000

/* ---------------- MQTT TOPICS ---------------- */
#define MQTT_TOPIC_MODE_SET         "reaction/mode/set"
#define MQTT_TOPIC_MODE_STATE       "reaction/mode/state"
#define MQTT_TOPIC_CFG_MASH_WINDOW  "reaction/config/mash_window_ms"
#define MQTT_TOPIC_CFG_FAULT_ACTION "reaction/config/early_release_action"
#define MQTT_TOPIC_RESULT_M1        "reaction/result/m1"
#define MQTT_TOPIC_RESULT_M2        "reaction/result/m2"
#define MQTT_TOPIC_STATUS           "reaction/status"
#define MQTT_TOPIC_CFG_MASH_ERROR   "reaction/config/mash_error_action"

#define MQTT_BROKER_URI          "mqtt://test.mosquitto.org"

/* ---------------- MASH TEST: acción ante error de alternancia ---------------- */
typedef enum {
    MASH_ERROR_IGNORE = 0,
    MASH_ERROR_PENALIZE_VISIBLE,
    MASH_ERROR_RESET_COUNT
} mash_error_action_t;

#define DEFAULT_MASH_ERROR_ACTION   MASH_ERROR_IGNORE

#define MASH_COUNTDOWN_SECONDS      3

/* ---------------- BUZZER ---------------- */
#define BUZZER_BEEP_SHORT_MS      80    // countdown ticks
#define BUZZER_BEEP_MODE_MS       150   // cambio de modo (BOOT)
#define BUZZER_BEEP_RESULT_MS     200   // pantalla de resultados

/* ---------------- BUZZER: Modo 1 ---------------- */
#define BUZZER_BEEP_PB1_HOLD_MS     60    // al presionar PB1 (inicio de espera)
#define BUZZER_BEEP_LED_ON_MS       100   // al encender el LED (señal de reaccionar)
#define BUZZER_BEEP_FAULT_MS        80    // cada pitido del doble beep de fallo
#define BUZZER_BEEP_FAULT_GAP_MS    80    // pausa entre los dos pitidos del fallo

/* ---------------- MASH TEST: reinicio simultaneo ---------------- */
#define SIMULTANEOUS_PRESS_WINDOW_MS   200

/* ---------------- WIFI ---------------- */
#define WIFI_USE_ENTERPRISE      0   // 0 = WPA2/WPA3 Personal (casa) | 1 = WPA2-Enterprise (uni)

#define WIFI_SSID                "ARSS"
#define WIFI_PASSWORD            "F4mili4s0rian0s3v3rin0"       // usado si WIFI_USE_ENTERPRISE = 0

#define WIFI_EAP_IDENTITY         "tu_usuario"              // usado si WIFI_USE_ENTERPRISE = 1
#define WIFI_EAP_USERNAME         "tu_usuario"
#define WIFI_EAP_PASSWORD         "tu_password_uni"

#define WIFI_MAX_RETRY            10

/* ---------------- DEFAULTS ---------------- */
typedef enum {
    FAULT_ACTION_RESTART = 0,
    FAULT_ACTION_PENALIZE
} fault_action_t;

#define DEFAULT_FAULT_ACTION    FAULT_ACTION_RESTART

#endif // CONFIG_H