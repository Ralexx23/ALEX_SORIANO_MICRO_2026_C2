#pragma once
#include "esp_err.h"
#include "joystick.h"
#include "mpu6050.h"
#include "buttons.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

esp_err_t display_init(void);

/* Dibuja la pantalla completa: crosshair de cada joystick, indicador
 * gráfico de inclinación (giroscopio), valores numéricos (dígitos estilo
 * 7-segmentos) del acelerómetro, estado de todos los botones, e iconos
 * de estado de WiFi/MQTT/calibración. Requisitos 5 y 8. */
void display_render(const joystick_data_t *joy, const imu_data_t *imu,
                     const button_state_t *btn, bool calibrating,
                     bool wifi_ok, bool mqtt_ok);
