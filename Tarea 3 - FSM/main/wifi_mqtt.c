#include "wifi_mqtt.h"
#include "app_config.h"
#include "fsm_led.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>
#include <ctype.h>

static const char *TAG = "wifi_mqtt";

static esp_mqtt_client_handle_t s_mqtt;
static volatile bool s_wifi_up = false;
static volatile bool s_mqtt_up = false;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_up = false;
        ESP_LOGW(TAG, "WiFi desconectado, reintentando...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_wifi_up = true;
        ESP_LOGI(TAG, "WiFi conectado");
    }
}

/* Traduce el payload del topic .../set ("ON","OFF","TOGGLE", sin
 * distinguir mayúsculas/minúsculas) a un comando de la FSM */
static void handle_set_command(const char *payload, int len)
{
    char buf[16] = {0};
    if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, payload, len);
    for (int i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)buf[i]);

    if (strcmp(buf, "ON") == 0) {
        fsm_led_request(FSM_CMD_ON);
    } else if (strcmp(buf, "OFF") == 0) {
        fsm_led_request(FSM_CMD_OFF);
    } else if (strcmp(buf, "TOGGLE") == 0) {
        fsm_led_request(FSM_CMD_TOGGLE);
    } else {
        ESP_LOGW(TAG, "comando MQTT no reconocido: '%s'", buf);
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;

    switch (id) {
        case MQTT_EVENT_CONNECTED:
            s_mqtt_up = true;
            ESP_LOGI(TAG, "MQTT conectado, suscribiendo a %s", MQTT_TOPIC_SET);
            esp_mqtt_client_subscribe(s_mqtt, MQTT_TOPIC_SET, 1);
            /* publica el estado actual al reconectar, para que la app
             * del cel siempre muestre el valor real al abrir */
            wifi_mqtt_publish_state(fsm_led_get_state() == FSM_STATE_ON);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_up = false;
            ESP_LOGW(TAG, "MQTT desconectado");
            break;
        case MQTT_EVENT_DATA:
            if (event->topic_len == (int)strlen(MQTT_TOPIC_SET) &&
                strncmp(event->topic, MQTT_TOPIC_SET, event->topic_len) == 0) {
                handle_set_command(event->data, event->data_len);
            }
            break;
        default:
            break;
    }
}

esp_err_t wifi_mqtt_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.last_will.topic = MQTT_TOPIC_STATE,
        .session.last_will.msg = "OFFLINE",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };
    s_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt);

    ESP_LOGI(TAG, "wifi_mqtt inicializado, conectando a %s / %s", WIFI_SSID, MQTT_BROKER_URI);
    return ESP_OK;
}

bool wifi_mqtt_is_connected(void) { return s_wifi_up && s_mqtt_up; }

void wifi_mqtt_publish_state(bool on)
{
    if (!s_mqtt_up) return;
    const char *payload = on ? "ON" : "OFF";
    esp_mqtt_client_publish(s_mqtt, MQTT_TOPIC_STATE, payload, 0, 1, true); // retained
}
