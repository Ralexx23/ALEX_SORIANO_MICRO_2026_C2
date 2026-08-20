#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>

void rgb_led_init(void);
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);
void rgb_led_off(void);

#endif // RGB_LED_H