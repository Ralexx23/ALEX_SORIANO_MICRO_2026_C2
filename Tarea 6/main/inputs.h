#ifndef INPUTS_H
#define INPUTS_H

#include <stdbool.h>

void inputs_init(void);
bool inputs_is_obstacle_active(void);
bool inputs_limit_open_active(void);
bool inputs_limit_close_active(void);

#endif