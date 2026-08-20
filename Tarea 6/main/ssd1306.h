#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include <stdbool.h>

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

void ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_set_pixel(int x, int y, bool on);
void ssd1306_flush(void);

#endif