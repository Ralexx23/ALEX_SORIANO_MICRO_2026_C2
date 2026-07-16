#include "wifi_mqtt.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "wifi_mqtt";

static EventGroupHandle_t s_evt;
#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

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
        xEventGroupClearBits(s_evt, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi desconectado, reintentando...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_wifi_up = true;
        xEventGroupSetBits(s_evt, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi conectado, IP obtenida");
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)data;
    switch (id) {
        case MQTT_EVENT_CONNECTED:
            s_mqtt_up = true;
            xEventGroupSetBits(s_evt, MQTT_CONNECTED_BIT);
            ESP_LOGI(TAG, "MQTT conectado");
            esp_mqtt_client_publish(s_mqtt, MQTT_TOPIC_BASE "/status", "online", 0, 1, true);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_up = false;
            xEventGroupClearBits(s_evt, MQTT_CONNECTED_BIT);
            ESP_LOGW(TAG, "MQTT desconectado");
            break;
        default:
            break;
    }
}

esp_err_t wifi_mqtt_init(void)
{
    s_evt = xEventGroupCreate();

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
        .session.last_will.topic = MQTT_TOPIC_BASE "/status",
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };
    s_mqtt = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt);

    ESP_LOGI(TAG, "wifi_mqtt inicializado, conectando a %s / %s", WIFI_SSID, MQTT_BROKER_URI);
    return ESP_OK;
}

bool wifi_mqtt_is_wifi_connected(void) { return s_wifi_up; }
bool wifi_mqtt_is_mqtt_connected(void) { return s_mqtt_up; }

void wifi_mqtt_publish_telemetry(const joystick_data_t *joy, const imu_data_t *imu,
                                  const button_state_t *btn)
{
    if (!s_mqtt_up) return;

    char payload[320];
    int n = snprintf(payload, sizeof(payload),
        "{"
        "\"joy\":{\"lx\":%d,\"ly\":%d,\"rx\":%d,\"ry\":%d},"
        "\"imu\":{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,"
        "\"gx\":%.1f,\"gy\":%.1f,\"gz\":%.1f,"
        "\"roll\":%.1f,\"pitch\":%.1f},"
        "\"btn\":{\"tl1\":%d,\"tl2\":%d,\"tr1\":%d,\"tr2\":%d,"
        "\"a\":%d,\"b\":%d,\"x\":%d,\"y\":%d,\"topl\":%d,\"topr\":%d}"
        "}",
        joy->lx, joy->ly, joy->rx, joy->ry,
        imu->accel_x_g, imu->accel_y_g, imu->accel_z_g,
        imu->gyro_x_dps, imu->gyro_y_dps, imu->gyro_z_dps,
        imu->roll_deg, imu->pitch_deg,
        btn->trig_l1, btn->trig_l2, btn->trig_r1, btn->trig_r2,
        btn->front_a, btn->front_b, btn->front_x, btn->front_y,
        btn->top_l, btn->top_r);

    if (n > 0 && n < (int)sizeof(payload)) {
        esp_mqtt_client_publish(s_mqtt, MQTT_TOPIC_BASE "/telemetry", payload, n, 0, false);
    }
}
