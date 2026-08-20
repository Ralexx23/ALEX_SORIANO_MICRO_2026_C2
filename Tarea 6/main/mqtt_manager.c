#include "mqtt_manager.h"
#include "config.h"
#include "fsm.h"
#include "actuator.h"
#include "runtime_config.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include "inputs.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t client;

static char topic_status[48];
static char topic_limits[48];
static char topic_obstacle[48];
static char topic_fault[48];
static char topic_availability[48];
static char topic_cmd_action[48];
static char topic_cmd_config[48];
static char topic_cmd_calibration[48];

static void build_topics(void)
{
    snprintf(topic_status, sizeof(topic_status), "gate/%s/state/status", DEVICE_ID);
    snprintf(topic_limits, sizeof(topic_limits), "gate/%s/state/limits", DEVICE_ID);
    snprintf(topic_obstacle, sizeof(topic_obstacle), "gate/%s/state/obstacle", DEVICE_ID);
    snprintf(topic_fault, sizeof(topic_fault), "gate/%s/state/fault", DEVICE_ID);
    snprintf(topic_availability, sizeof(topic_availability), "gate/%s/availability", DEVICE_ID);
    snprintf(topic_cmd_action, sizeof(topic_cmd_action), "gate/%s/cmd/action", DEVICE_ID);
    snprintf(topic_cmd_config, sizeof(topic_cmd_config), "gate/%s/cmd/config", DEVICE_ID);
    snprintf(topic_cmd_calibration, sizeof(topic_cmd_calibration), "gate/%s/cmd/calibration", DEVICE_ID);
}

static void handle_cmd_action(const char *payload, int len)
{
    gate_state_t s = fsm_get_state();
    if (strncmp(payload, "OPEN", len) == 0 && (s == GATE_STATE_CLOSED || s == GATE_STATE_STOPPED)) {
        fsm_handle_event(EVENT_CMD_OPEN);
    } else if (strncmp(payload, "CLOSE", len) == 0 && (s == GATE_STATE_OPEN || s == GATE_STATE_STOPPED)) {
        if (inputs_is_obstacle_active()) {
            ESP_LOGW(TAG, "CLOSE bloqueado via MQTT: obstaculo presente");
            return;
        }
        fsm_handle_event(EVENT_CMD_CLOSE);
    } else if (strncmp(payload, "STOP", len) == 0 && (s == GATE_STATE_OPENING || s == GATE_STATE_CLOSING)) {
        fsm_handle_event(EVENT_CMD_STOP);
    } else if (strncmp(payload, "RESET", len) == 0 && s == GATE_STATE_FAULT) {
        fsm_handle_reset(inputs_limit_open_active(), inputs_limit_close_active());
    } else {
        ESP_LOGW(TAG, "Comando ignorado (no valido para el estado actual)");
        return;
    }
    actuator_apply_state(fsm_get_state());
}

static void handle_cmd_config(const char *payload, int len)
{
    cJSON *json = cJSON_ParseWithLength(payload, len);
    if (!json) {
        ESP_LOGW(TAG, "JSON invalido en cmd/config");
        return;
    }
    cJSON *behavior_item = cJSON_GetObjectItem(json, "obstacle_behavior");
    if (cJSON_IsString(behavior_item)) {
        obstacle_behavior_t b = runtime_config_obstacle_behavior_from_str(behavior_item->valuestring);
        runtime_config_set_obstacle_behavior(b);
        ESP_LOGI(TAG, "obstacle_behavior actualizado via MQTT: %s", behavior_item->valuestring);
    }
    cJSON_Delete(json);
}

static void handle_cmd_calibration(const char *payload, int len)
{
    gate_state_t s = fsm_get_state();
    if (strncmp(payload, "START", len) == 0 && s != GATE_STATE_CALIBRATION) {
        fsm_handle_event(EVENT_CALIB_TOGGLE);
        actuator_apply_state(fsm_get_state());
    } else if (strncmp(payload, "STOP", len) == 0 && s == GATE_STATE_CALIBRATION) {
        fsm_handle_event(EVENT_CALIB_TOGGLE);
        actuator_apply_state(fsm_get_state());
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado al broker MQTT");
            esp_mqtt_client_publish(client, topic_availability, "online", 0, 1, true);
            esp_mqtt_client_subscribe(client, topic_cmd_action, 1);
            esp_mqtt_client_subscribe(client, topic_cmd_config, 1);
            esp_mqtt_client_subscribe(client, topic_cmd_calibration, 1);
            break;

        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, topic_cmd_action, event->topic_len) == 0) {
                handle_cmd_action(event->data, event->data_len);
            } else if (strncmp(event->topic, topic_cmd_config, event->topic_len) == 0) {
                handle_cmd_config(event->data, event->data_len);
            } else if (strncmp(event->topic, topic_cmd_calibration, event->topic_len) == 0) {
                handle_cmd_calibration(event->data, event->data_len);
            }
            break;

        default:
            break;
    }
}

void mqtt_publish_status(gate_state_t state)
{
    esp_mqtt_client_publish(client, topic_status, fsm_state_to_str(state), 0, 1, true);
}

void mqtt_publish_limits(bool open_active, bool close_active)
{
    char payload[48];
    snprintf(payload, sizeof(payload), "{\"open\":%s,\"closed\":%s}",
              open_active ? "true" : "false", close_active ? "true" : "false");
    esp_mqtt_client_publish(client, topic_limits, payload, 0, 0, false);
}

void mqtt_publish_obstacle(bool active)
{
    esp_mqtt_client_publish(client, topic_obstacle, active ? "true" : "false", 0, 0, true);
}

void mqtt_publish_fault(const char *reason)
{
    esp_mqtt_client_publish(client, topic_fault, reason, 0, 1, true);
}

void mqtt_manager_init(void)
{
    build_topics();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.protocol_ver = MQTT_PROTOCOL_V_5,
        .session.last_will.topic = topic_availability,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 7,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "MQTT inicializado, conectando a %s", MQTT_BROKER_URI);
}