#pragma once

/* ===== EDITA ESTO con tus datos ===== */
#define WIFI_SSID          "TU_WIFI"
#define WIFI_PASSWORD      "TU_PASSWORD"

/* Broker MQTT. Puede ser uno local (mosquitto en tu PC/RaspberryPi en
 * la misma red) o uno público de pruebas como broker.hivemq.com.
 * Formato: "mqtt://usuario:pass@host:puerto" o "mqtt://host:1883" */
#define MQTT_BROKER_URI    "mqtt://192.168.1.100:1883"

/* Prefijo de topics. Se publica en:
 *   <TOPIC_BASE>/telemetry   -> JSON con joystick+imu+botones (cada ~100ms)
 *   <TOPIC_BASE>/status      -> "online" / "offline" (LWT) */
#define MQTT_TOPIC_BASE    "rc_controller/1"
