#ifndef OLED_H
#define OLED_H

#include "fsm.h"

void oled_init(void);
void oled_show_mode_idle(game_mode_t mode);
void oled_show_wait_hold(void);
void oled_show_get_ready(void);
void oled_show_react_now(void);
void oled_show_wait_second(void);
void oled_show_result_m1(result_m1_t result);
void oled_show_fault(void);

void oled_show_countdown_m2(int seconds_left);
void oled_show_mash_running(int32_t count);
void oled_show_mash_error(int32_t count);
void oled_show_result_m2(result_m2_t result);

#endif // OLED_H