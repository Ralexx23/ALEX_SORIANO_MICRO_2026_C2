#pragma once

/* ===== EDITA ESTO con tus datos ===== */
#define WIFI_SSID          "TU_WIFI"
#define WIFI_PASSWORD      "TU_PASSWORD"

/* Broker MQTT: local (Mosquitto en tu PC) o público de pruebas */
#define MQTT_BROKER_URI    "mqtt://192.168.1.100:1883"

/* Topics:
 *   <BASE>/state  -> el ESP32 publica "ON"/"OFF" (retained) cada vez
 *                    que el estado cambia, sin importar el origen
 *   <BASE>/set    -> cualquiera (botón físico O la app del cel)
 *                    publica "ON" / "OFF" / "TOGGLE" aquí para pedir
 *                    un cambio de estado
 */
#define MQTT_TOPIC_STATE   "fsm_led/1/state"
#define MQTT_TOPIC_SET     "fsm_led/1/set"
