#ifndef BUZZER_H
#define BUZZER_H

#include "fsm.h"

void buzzer_init(void);
void buzzer_set_state(gate_state_t state);

#endif