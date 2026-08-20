#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "fsm.h"
#include <stdint.h>
#include <stdbool.h>

void mqtt_manager_init(void);
void mqtt_publish_status(gate_state_t state);
void mqtt_publish_limits(bool open_active, bool close_active);
void mqtt_publish_obstacle(bool active);
void mqtt_publish_fault(const char *reason);

#endif