#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    bool trig_l1, trig_l2, trig_r1, trig_r2;
    bool front_a, front_b, front_x, front_y;
    bool top_l, top_r;
} button_state_t;

esp_err_t buttons_init(void);

/* Lee el estado ya debounceado de todos los botones */
void buttons_read(button_state_t *out);

/* true una sola vez cuando el combo de calibración (top_l + top_r
 * sostenidos 3s) se completa. Se limpia automáticamente al leerse. */
bool buttons_calibration_triggered(void);
