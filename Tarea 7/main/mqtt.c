#include "mqtt.h"
#include "config.h"
#include "fsm.h"

#include "mqtt_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "MQTT";

static esp_mqtt_client_handle_t client = NULL;
static bool connected = false;

bool mqtt_is_connected(void) {
    return connected;
}

/* ---------------- Parsing de mensajes entrantes ---------------- */
static void handle_incoming(const char *topic, const char *data) {
    ESP_LOGI(TAG, "Mensaje recibido -> topic: %s | data: %s", topic, data);

    if (strcmp(topic, MQTT_TOPIC_MODE_SET) == 0) {
        int32_t mode = atoi(data);
        fsm_event_t evt = { .type = EVT_MQTT_MODE_SET, .param = mode };
        fsm_post_event(evt);

    } else if (strcmp(topic, MQTT_TOPIC_CFG_MASH_WINDOW) == 0) {
        int32_t window_ms = atoi(data);
        fsm_event_t evt = { .type = EVT_MQTT_SET_MASH_WINDOW, .param = window_ms };
        fsm_post_event(evt);

    } else if (strcmp(topic, MQTT_TOPIC_CFG_FAULT_ACTION) == 0) {
        int32_t val = -1;
        if (strcmp(data, "restart") == 0) val = 0;
        else if (strcmp(data, "penalize") == 0) val = 1;
        if (val >= 0) {
            fsm_event_t evt = { .type = EVT_MQTT_SET_FAULT_ACTION, .param = val };
            fsm_post_event(evt);
        } else {
            ESP_LOGW(TAG, "Valor invalido para fault_action: %s", data);
        }

    } else if (strcmp(topic, MQTT_TOPIC_CFG_MASH_ERROR) == 0) {
        int32_t val = -1;
        if (strcmp(data, "ignore") == 0) val = 0;
        else if (strcmp(data, "penalize") == 0) val = 1;
        else if (strcmp(data, "reset") == 0) val = 2;
        if (val >= 0) {
            fsm_event_t evt = { .type = EVT_MQTT_SET_MASH_ERROR_ACTION, .param = val };
            fsm_post_event(evt);
        } else {
            ESP_LOGW(TAG, "Valor invalido para mash_error_action: %s", data);
        }
    }
}

/* ---------------- Event handler del cliente MQTT ---------------- */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            connected = true;
            ESP_LOGI(TAG, "Conectado al broker MQTT");

            esp_mqtt_client_subscribe(client, MQTT_TOPIC_MODE_SET, 1);
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_CFG_MASH_WINDOW, 1);
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_CFG_FAULT_ACTION, 1);
            esp_mqtt_client_subscribe(client, MQTT_TOPIC_CFG_MASH_ERROR, 1);

            esp_mqtt_client_publish(client, MQTT_TOPIC_STATUS, "online", 0, 1, 1);
            mqtt_publish_mode_state(fsm_get_current_mode());
            break;

        case MQTT_EVENT_DISCONNECTED:
            connected = false;
            ESP_LOGW(TAG, "Desconectado del broker MQTT");
            break;

        case MQTT_EVENT_DATA: {
            char topic_buf[80];
            char data_buf[48];

            int tlen = event->topic_len < (int)sizeof(topic_buf) - 1 ? event->topic_len : (int)sizeof(topic_buf) - 1;
            int dlen = event->data_len < (int)sizeof(data_buf) - 1 ? event->data_len : (int)sizeof(data_buf) - 1;

            memcpy(topic_buf, event->topic, tlen);
            topic_buf[tlen] = '\0';
            memcpy(data_buf, event->data, dlen);
            data_buf[dlen] = '\0';

            handle_incoming(topic_buf, data_buf);
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error MQTT reportado");
            break;

        default:
            break;
    }
}

/* ---------------- Init ---------------- */
void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.last_will.topic = MQTT_TOPIC_STATUS,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 0,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    ESP_LOGI(TAG, "Cliente MQTT iniciado, broker: %s", MQTT_BROKER_URI);
}

/* ---------------- Publish de resultados ---------------- */
void mqtt_publish_result_m1(result_m1_t result) {
    if (!connected) {
        ESP_LOGW(TAG, "Resultado M1 no publicado: sin conexion MQTT");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "reaction_ms", result.reaction_ms);
    cJSON_AddNumberToObject(root, "second_press_ms", result.second_press_ms);
    cJSON_AddBoolToObject(root, "fault", result.fault);
    cJSON_AddNumberToObject(root, "uptime_ms", (double)(esp_timer_get_time() / 1000));

    char *payload = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(client, MQTT_TOPIC_RESULT_M1, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Publicado M1: %s", payload);

    free(payload);
    cJSON_Delete(root);
}

void mqtt_publish_result_m2(result_m2_t result) {
    if (!connected) {
        ESP_LOGW(TAG, "Resultado M2 no publicado: sin conexion MQTT");
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "total_presses", result.total_presses);
    cJSON_AddNumberToObject(root, "avg_interval_ms", result.avg_interval_ms);
    cJSON_AddNumberToObject(root, "window_ms", result.window_ms);
    cJSON_AddNumberToObject(root, "uptime_ms", (double)(esp_timer_get_time() / 1000));

    char *payload = cJSON_PrintUnformatted(root);
    esp_mqtt_client_publish(client, MQTT_TOPIC_RESULT_M2, payload, 0, 1, 0);
    ESP_LOGI(TAG, "Publicado M2: %s", payload);

    free(payload);
    cJSON_Delete(root);
}

void mqtt_publish_mode_state(game_mode_t mode) {
    if (!client) return;
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", (int)mode);
    esp_mqtt_client_publish(client, MQTT_TOPIC_MODE_STATE, buf, 0, 1, 1); // retained
}