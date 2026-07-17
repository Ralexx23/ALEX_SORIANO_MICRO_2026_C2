#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    float accel_x_g, accel_y_g, accel_z_g;  // en g's, dato crudo para mostrar en pantalla (req. 8)
    float gyro_x_dps, gyro_y_dps, gyro_z_dps; // grados/seg
    float roll_deg, pitch_deg;               // ángulo estimado (filtro complementario)
    int8_t tilt_x;  // roll/pitch mapeados a -100..+100, mismo rango que joystick (req. 7)
    int8_t tilt_y;
} imu_data_t;

esp_err_t mpu6050_init(void);

/* Debe llamarse periódicamente (p.ej. cada 20ms) para actualizar el
 * filtro complementario; internamente usa esp_timer para dt real */
void mpu6050_update(imu_data_t *out);

/* Captura el ángulo/offset actual como posición "0" de reposo.
 * Se llama desde el mismo combo de calibración que el joystick. */
esp_err_t mpu6050_calibrate_zero(void);

esp_err_t mpu6050_load_calibration(void);
