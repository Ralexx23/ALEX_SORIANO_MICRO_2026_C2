idf_component_register(
    SRCS
        "app_main.c"
        "fsm_led.c"
        "wifi_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_wifi
        esp_event
        nvs_flash
        mqtt
)
