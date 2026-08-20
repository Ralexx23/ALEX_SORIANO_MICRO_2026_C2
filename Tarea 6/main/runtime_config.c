// runtime_config.c
#include "runtime_config.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "RUNTIME_CONFIG";
static obstacle_behavior_t obstacle_behavior = OBSTACLE_STOP_ONLY;

void runtime_config_init(void)
{
    obstacle_behavior = OBSTACLE_STOP_ONLY;
    ESP_LOGI(TAG, "Config inicial: obstacle_behavior=STOP_ONLY");
}

obstacle_behavior_t runtime_config_get_obstacle_behavior(void)
{
    return obstacle_behavior;
}

void runtime_config_set_obstacle_behavior(obstacle_behavior_t behavior)
{
    obstacle_behavior = behavior;
    ESP_LOGI(TAG, "obstacle_behavior actualizado a %d", behavior);
}

obstacle_behavior_t runtime_config_obstacle_behavior_from_str(const char *str)
{
    if (strcmp(str, "STOP_AND_RESUME") == 0)  return OBSTACLE_STOP_AND_RESUME;
    if (strcmp(str, "STOP_AND_REVERSE") == 0) return OBSTACLE_STOP_AND_REVERSE;
    return OBSTACLE_STOP_ONLY; // default si el string no matchea
}

static uint32_t travel_timeout_ms = TRAVEL_TIMEOUT_DEFAULT_MS;

uint32_t runtime_config_get_travel_timeout_ms(void) { return travel_timeout_ms; }

void runtime_config_set_travel_timeout_ms(uint32_t ms)
{
    travel_timeout_ms = ms;
    ESP_LOGI(TAG, "travel_timeout_ms actualizado a %lu ms", (unsigned long)ms);
}