idf_component_register(SRCS "fsm_switch.c"
                    INCLUDE_DIRS "."
                    REQUIRES mqtt esp_driver_gpio esp_wifi esp_event nvs_flash esp_netif)