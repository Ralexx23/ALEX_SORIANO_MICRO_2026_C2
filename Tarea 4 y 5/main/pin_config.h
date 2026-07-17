/**
 * pin_config.h
 *
 * Mapeo centralizado de pines del control RC (ESP32-S3).
 *
 * IMPORTANTE: Estos valores están basados en la nomenclatura de tu
 * esquemático (ADC_JOY*, BTNL*, I2C*, IO17/IO18) pero el PDF que
 * enviaste es un "Print to PDF" de KiCad sin capa de texto seleccionable,
 * así que algunos números de GPIO de los botones individuales no se
 * pudieron confirmar al 100% por resolución de imagen.
 *
 * ACCION REQUERIDA: Verifica cada #define contra tu esquemático /
 * netlist real y ajusta los que no coincidan. Todo el resto del
 * firmware referencia SOLO estas macros, así que corregir aquí basta.
 */
#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"

/* ===================== JOYSTICKS (ADC) =====================
 * ADC_JOY* -> 4 canales analógicos, 2 joysticks de 2 ejes c/u.
 * ESP32-S3 ADC1 tiene canales en GPIO1-GPIO10 (recomendado usar
 * solo ADC1 porque ADC2 comparte hardware con WiFi y da lecturas
 * erráticas cuando el WiFi está activo, y aquí SÍ usamos WiFi/MQTT).
 */
#define PIN_ADC_JOY_LX_GPIO   GPIO_NUM_1   // ADC1_CH0
#define PIN_ADC_JOY_LX_CH     ADC_CHANNEL_0

#define PIN_ADC_JOY_LY_GPIO   GPIO_NUM_2   // ADC1_CH1
#define PIN_ADC_JOY_LY_CH     ADC_CHANNEL_1

#define PIN_ADC_JOY_RX_GPIO   GPIO_NUM_3   // ADC1_CH2
#define PIN_ADC_JOY_RX_CH     ADC_CHANNEL_2

#define PIN_ADC_JOY_RY_GPIO   GPIO_NUM_4   // ADC1_CH3
#define PIN_ADC_JOY_RY_CH     ADC_CHANNEL_3

/* ===================== BOTONES (digitales, BTNL*) =====================
 * 4 gatillos (triggers, uno en cada esquina trasera del control)
 * 4 frontales (tipo A/B/X/Y)
 * 2 superiores usados para el combo de calibración (mantener 3s)
 * Todos activos en LOW con pull-up interno (botón a GND).
 */
#define PIN_BTN_TRIG_L1   GPIO_NUM_5
#define PIN_BTN_TRIG_L2   GPIO_NUM_6
#define PIN_BTN_TRIG_R1   GPIO_NUM_7
#define PIN_BTN_TRIG_R2   GPIO_NUM_8

#define PIN_BTN_FRONT_A   GPIO_NUM_9
#define PIN_BTN_FRONT_B   GPIO_NUM_10
#define PIN_BTN_FRONT_X   GPIO_NUM_11
#define PIN_BTN_FRONT_Y   GPIO_NUM_12

/* Botones superiores usados para calibración (combo 3s) */
#define PIN_BTN_TOP_L     GPIO_NUM_13
#define PIN_BTN_TOP_R     GPIO_NUM_14

/* ===================== I2C (OLED + MPU-6050 compartido) ===================== */
#define PIN_I2C_SDA       GPIO_NUM_15
#define PIN_I2C_SCL       GPIO_NUM_16
#define I2C_PORT_NUM      I2C_NUM_0
#define I2C_CLK_HZ        400000

#define MPU6050_I2C_ADDR  0x68   // AD0 a GND. Si tu módulo tiene AD0 a 3V3, usa 0x69
#define OLED_I2C_ADDR     0x3C   // dirección típica SSD1306 128x64. Ajusta si tu módulo usa 0x3D

/* ===================== ENLACE RF ===================== */
#define PIN_RF_RX         GPIO_NUM_18   // entrada del demodulador RF (según tu nota)
#define PIN_RF_TX         GPIO_NUM_17   // salida hacia el módulo TX (trama codificada)

/* ===================== CALIBRACIÓN ===================== */
#define CALIBRATION_HOLD_MS   3000   // mantener BTN_TOP_L + BTN_TOP_R 3s

/* ===================== NOTAS DE PINES A EVITAR EN S3 =====================
 * GPIO19/20: USB D-/D+ (no usar si necesitas USB nativo / consola JTAG-USB)
 * GPIO26-32: normalmente reservados para PSRAM/Flash en muchos módulos WROOM-1
 *            (verifica el datasheet de tu módulo específico antes de reasignar)
 * GPIO0: strapping boot, evitar como entrada con pull activo en arranque
 */
