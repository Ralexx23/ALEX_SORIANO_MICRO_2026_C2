idf_component_register(
    SRCS
        "HumanReaction.c"
        "fsm.c"
        "button.c"
        "rgb_led.c"
        "oled.c"
        "buzzer.c"
        "wifi.c"
        "mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES driver u8g2
    PRIV_REQUIRES esp_timer esp_wifi esp_netif esp_event nvs_flash mqtt json
)