// runtime_config.h
#ifndef RUNTIME_CONFIG_H
#define RUNTIME_CONFIG_H

#include <stdint.h>

typedef enum {
    OBSTACLE_STOP_ONLY = 0,
    OBSTACLE_STOP_AND_RESUME,
    OBSTACLE_STOP_AND_REVERSE
} obstacle_behavior_t;

void runtime_config_init(void);
obstacle_behavior_t runtime_config_get_obstacle_behavior(void);
void runtime_config_set_obstacle_behavior(obstacle_behavior_t behavior);
obstacle_behavior_t runtime_config_obstacle_behavior_from_str(const char *str);

uint32_t runtime_config_get_travel_timeout_ms(void);
void runtime_config_set_travel_timeout_ms(uint32_t ms);

#endif