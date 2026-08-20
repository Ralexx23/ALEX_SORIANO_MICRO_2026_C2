idf_component_register(SRCS "Gate_Control.c" "fsm.c" "motor.c" "actuator.c" "ssd1306.c" "display.c" "inputs.c" "rgb.c" "buzzer.c" "wifi_manager.c" "mqtt_manager.c" "runtime_config.c"
                        INCLUDE_DIRS "."
                        REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt json driver esp_timer)
                        