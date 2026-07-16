#include "fsm_led.h"
#include "wifi_mqtt.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "app_main";

/* Se llama desde fsm_led.c cada vez que el estado cambia, sin importar
 * si el cambio lo disparó el botón físico o un comando MQTT recibido
 * desde el celular. Así el topic .../state siempre refleja la verdad,
 * sin duplicar lógica ni crear bucles de retroalimentación. */
static void on_fsm_state_changed(fsm_state_t new_state)
{
    wifi_mqtt_publish_state(new_state == FSM_STATE_ON);
}

void app_main(void)
{
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    fsm_led_init(on_fsm_state_changed);
    wifi_mqtt_init();

    ESP_LOGI(TAG, "listo: boton fisico o MQTT (%s) -> LED", "fsm_led/1/set");
}
