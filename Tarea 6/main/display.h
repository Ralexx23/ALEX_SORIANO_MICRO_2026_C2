#ifndef DISPLAY_H
#define DISPLAY_H

#include "fsm.h"
#include <stdint.h>

void display_init(void);
void display_update(gate_state_t state, const char *motor_status, bool obstacle_active);

#endif