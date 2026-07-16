#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "joystick.h"
#include "mpu6050.h"
#include "buttons.h"

esp_err_t wifi_mqtt_init(void);

bool wifi_mqtt_is_wifi_connected(void);
bool wifi_mqtt_is_mqtt_connected(void);

/* Publica un snapshot de telemetría como JSON en <TOPIC_BASE>/telemetry.
 * No bloquea si no hay conexión (simplemente no hace nada). */
void wifi_mqtt_publish_telemetry(const joystick_data_t *joy, const imu_data_t *imu,
                                  const button_state_t *btn);
