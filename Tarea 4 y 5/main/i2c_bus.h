#pragma once
#include "driver/i2c_master.h"

/* Inicializa el bus I2C maestro compartido (SDA/SCL de pin_config.h).
 * Llamar UNA vez antes de mpu6050_init() y display_init(). */
esp_err_t i2c_bus_init(void);

/* Handle del bus para que otros módulos agreguen sus dispositivos con
 * i2c_master_bus_add_device() */
i2c_master_bus_handle_t i2c_bus_get_handle(void);
