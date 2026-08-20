#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

void motor_init(void);
void motor_open(void);
void motor_close(void);
void motor_stop(void);
const char *motor_get_status_str(void);

#endif