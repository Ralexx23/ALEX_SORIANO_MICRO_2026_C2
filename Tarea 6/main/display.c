#include "display.h"
#include "ssd1306.h"
#include "font8x8_basic.h"
#include <stdio.h>

static void draw_char(int x, int y, char c)
{
    const uint8_t *glyph = (const uint8_t *)font8x8_basic[(uint8_t)c];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            bool on = (bits >> col) & 1;
            ssd1306_set_pixel(x + col, y + row, on);
        }
    }
}

static void draw_string(int x, int y, const char *str)
{
    while (*str) {
        draw_char(x, y, *str);
        x += 8;
        str++;
    }
}

void display_init(void)
{
    ssd1306_init();
}

static const char *display_state_short(gate_state_t state)
{
    switch (state) {
        case GATE_STATE_INIT:        return "INIT";
        case GATE_STATE_CLOSED:      return "CLOSED";
        case GATE_STATE_OPENING:     return "OPENING";
        case GATE_STATE_OPEN:        return "OPEN";
        case GATE_STATE_CLOSING:     return "CLOSING";
        case GATE_STATE_STOPPED:     return "STOPPED";
        case GATE_STATE_FAULT:       return "FAULT";
        case GATE_STATE_CALIBRATION: return "CALIB";
        default:                     return "?";
    }
}

void display_update(gate_state_t state, const char *motor_status, bool obstacle_active)
{
    ssd1306_clear();

    char line1[22];
    snprintf(line1, sizeof(line1), "Estado: %s", display_state_short(state));
    draw_string(0, 0, line1);

    draw_string(0, 16, motor_status);

    char line3[22];
    snprintf(line3, sizeof(line3), "Obst: %s", obstacle_active ? "SI" : "NO");
    draw_string(0, 32, line3);

    ssd1306_flush();
}