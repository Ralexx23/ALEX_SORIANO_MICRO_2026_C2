#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "led_strip.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include <string.h>

static const char *TAG = "FSM_SWITCH";

#define LED_GPIO    48
#define BOTON_GPIO  0

typedef enum {
    ESTADO_OFF = 0,
    ESTADO_ON
} estado_t;

typedef enum {
    EVENTO_TOGGLE = 0
} evento_t;

static estado_t estado_actual = ESTADO_OFF;
static led_strip_handle_t led_strip;
static QueueHandle_t cola_eventos_boton;

static const char* nombre_estado(estado_t e)
{
    return (e == ESTADO_ON) ? "ON" : "OFF";
}

static void led_iniciar(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);
}

static void led_actualizar(estado_t estado)
{
    if (estado == ESTADO_ON) {
        led_strip_set_pixel(led_strip, 0, 0, 20, 0);
    } else {
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
    }
    led_strip_refresh(led_strip);
}

// Forward declaration: le avisamos al compilador que esta funcion existe,
// aunque su implementacion completa este mas abajo en el archivo
static void mqtt_publicar_estado(void);

static void fsm_manejar_evento(evento_t evento)
{
    estado_t estado_anterior = estado_actual;

    switch (estado_actual) {
        case ESTADO_OFF:
            if (evento == EVENTO_TOGGLE) estado_actual = ESTADO_ON;
            break;
        case ESTADO_ON:
            if (evento == EVENTO_TOGGLE) estado_actual = ESTADO_OFF;
            break;
    }

    if (estado_anterior != estado_actual) {
        ESP_LOGI(TAG, "Transicion: %s -> %s", nombre_estado(estado_anterior), nombre_estado(estado_actual));
        led_actualizar(estado_actual);
        mqtt_publicar_estado(); // <-- nuevo: avisa por MQTT tambien cuando cambia por boton
    }
}

// --- Manejo del botón ---

// ISR: se ejecuta en cuanto el pin cambia. Debe ser MUY corta y rápida.
static void IRAM_ATTR boton_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    // Solo mandamos una señal a una cola; el trabajo pesado se hace afuera
    xQueueSendFromISR(cola_eventos_boton, &gpio_num, NULL);
}

// Tarea que procesa las señales del botón, con debounce
static void tarea_boton(void *arg)
{
    uint32_t gpio_num;

    while (1) {
        if (xQueueReceive(cola_eventos_boton, &gpio_num, portMAX_DELAY)) {

            // No aceptar más interrupciones mientras se procesa esta pulsación
            gpio_intr_disable(gpio_num);

            // Descarta posibles eventos repetidos que quedaron en la cola
            xQueueReset(cola_eventos_boton);

            // Espera breve para estabilizar el contacto del botón
            vTaskDelay(pdMS_TO_TICKS(30));

            // Si después de 30 ms sigue LOW, fue una pulsación real
            if (gpio_get_level(gpio_num) == 0) {
                fsm_manejar_evento(EVENTO_TOGGLE);

                // Espera a que el usuario suelte el botón
                while (gpio_get_level(gpio_num) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                // Pequeña espera adicional para filtrar rebote al soltar
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            // Queda listo para detectar la próxima pulsación
            gpio_intr_enable(gpio_num);
        }
    }
}

static void boton_iniciar(void)
{
    gpio_config_t config_boton = {
        .pin_bit_mask = (1ULL << BOTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // dispara en flanco de bajada (al presionar)
    };
    gpio_config(&config_boton);

    cola_eventos_boton = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOTON_GPIO, boton_isr_handler, (void*) BOTON_GPIO);

    xTaskCreate(tarea_boton, "tarea_boton", 4096, NULL, 10, NULL);
}

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONECTADO_BIT BIT0

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Desconectado del WiFi, reintentando...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONECTADO_BIT);
    }
}

static void wifi_iniciar(void)
{
    wifi_event_group = xEventGroupCreate();

    // 1. NVS: necesario para que el WiFi guarde su configuracion interna
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Red y eventos
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Inicializar el driver WiFi con configuracion por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. Suscribirse a los eventos que nos interesan
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // 5. Configurar SSID y password (vienen de menuconfig)
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Conectando a WiFi: %s ...", CONFIG_WIFI_SSID);

    // 6. Esperar (bloqueante) hasta que se confirme la conexion
    xEventGroupWaitBits(wifi_event_group, WIFI_CONECTADO_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

#define MQTT_BROKER_URI     "mqtt://test.mosquitto.org:1883"
#define MQTT_TOPIC_ESTADO   "alex_fsm_switch/estado"
#define MQTT_TOPIC_COMANDO  "alex_fsm_switch/comando"

static esp_mqtt_client_handle_t mqtt_client;

// Publica el estado actual en el topic correspondiente
static void mqtt_publicar_estado(void)
{
    const char *msg = (estado_actual == ESTADO_ON) ? "ON" : "OFF";
    esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_ESTADO, msg, 0, 1, 0);
    ESP_LOGI(TAG, "Publicado estado: %s", msg);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado al broker");
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_COMANDO, 1);
            mqtt_publicar_estado(); // publicamos el estado inicial al conectar
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "Mensaje recibido en topic: %.*s, data: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);

            // Si llega "TOGGLE" al topic de comando, disparamos el evento
            if (strncmp(event->data, "TOGGLE", event->data_len) == 0) {
                fsm_manejar_evento(EVENTO_TOGGLE);
                mqtt_publicar_estado(); // avisamos el nuevo estado
            }
            break;

        default:
            break;
    }
}

static void mqtt_iniciar(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void app_main(void)
{
    led_iniciar();
    led_actualizar(estado_actual);
    boton_iniciar();

    wifi_iniciar();
    mqtt_iniciar();  // <-- nuevo

    ESP_LOGI(TAG, "FSM iniciada. Estado inicial: %s. Presiona BOOT para cambiar de estado.", nombre_estado(estado_actual));
}