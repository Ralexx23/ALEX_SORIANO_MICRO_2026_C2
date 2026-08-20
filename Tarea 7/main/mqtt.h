#ifndef MQTT_APP_H
#define MQTT_APP_H

#include <stdbool.h>
#include "fsm.h"

void mqtt_app_start(void);
bool mqtt_is_connected(void);

void mqtt_publish_result_m1(result_m1_t result);
void mqtt_publish_result_m2(result_m2_t result);
void mqtt_publish_mode_state(game_mode_t mode);

#endif // MQTT_APP_H