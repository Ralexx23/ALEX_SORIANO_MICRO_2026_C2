#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    int8_t lx; // -100..+100
    int8_t ly;
    int8_t rx;
    int8_t ry;
} joystick_data_t;

/* Inicializa ADC (modo oneshot, ADC1, 12 bits) para los 4 canales */
esp_err_t joystick_init(void);

/* Lee los 4 ejes ya mapeados a -100..+100, con zona muerta aplicada */
void joystick_read(joystick_data_t *out);

/* Captura la posición actual de los 4 ejes como "centro" (offset de
 * calibración) y la guarda en NVS. Se llama desde el combo de
 * calibración (botones superiores 3s). */
esp_err_t joystick_calibrate_center(void);

/* Carga el offset de calibración guardado en NVS (llamar en boot) */
esp_err_t joystick_load_calibration(void);
