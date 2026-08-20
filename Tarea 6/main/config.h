#ifndef CONFIG_H
#define CONFIG_H

// Pin de prueba (botón BOOT integrado en la placa)
#define PIN_BOOT_BUTTON     GPIO_NUM_0

#define PIN_MOTOR_RELAY_OPEN   GPIO_NUM_4
#define PIN_MOTOR_RELAY_CLOSE  GPIO_NUM_5
#define MOTOR_RELAY_ACTIVE_LEVEL  1   // 1 = activo en HIGH, 0 = activo en LOW (cambia aqui si tu modulo es al reves)
#define MOTOR_DIRECTION_SWITCH_DELAY_MS  300  // pausa de seguridad al cambiar de sentido

#define PIN_CALIB_SELECT GPIO_NUM_47

#define PIN_LCD_SDA      GPIO_NUM_8
#define PIN_LCD_SCL      GPIO_NUM_9
#define LCD_I2C_ADDR     0x3C
#define LCD_I2C_FREQ_HZ  400000

#define PIN_LIMIT_OPEN   GPIO_NUM_1
#define PIN_LIMIT_CLOSE  GPIO_NUM_2
#define PIN_PHOTOCELL    GPIO_NUM_38
#define INPUT_POLL_INTERVAL_MS   20   // cada cuanto se lee el hardware
#define INPUT_DEBOUNCE_SAMPLES   3    // lecturas estables consecutivas para confirmar

#define PIN_BUZZER       GPIO_NUM_16
#define PIN_RGB_WS2812   GPIO_NUM_48
#define RGB_BLINK_INTERVAL_MS    300
#define BUZZER_BEEP_INTERVAL_MS  300

#define MQTT_BROKER_URI   "mqtt://test.mosquitto.org:1883"
#define DEVICE_ID         "gate01"

#define TRAVEL_TIMEOUT_DEFAULT_MS   15000

#endif