#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t wifi_mqtt_init(void);

bool wifi_mqtt_is_connected(void);

/* Publica el estado actual (retained) en MQTT_TOPIC_STATE.
 * Se llama cada vez que la FSM cambia de estado, sin importar si el
 * cambio vino del botón físico o de un comando MQTT previo. */
void wifi_mqtt_publish_state(bool on);
