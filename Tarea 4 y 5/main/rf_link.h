#pragma once
#include <stdint.h>
#include "esp_err.h"

/* Trama de control que se transmite por RF.
 * NOTA: el formato de bits en el aire (rf_link.c) es un framing OOK/PWM
 * genérico tipo "rc-switch" (compatible con la mayoría de módulos ASK
 * 433/315MHz baratos). Si tu receptor final espera otro protocolo,
 * ajusta encode_frame_to_symbols() en rf_link.c — el resto del sistema
 * no depende de esos detalles.
 */
typedef struct __attribute__((packed)) {
    int8_t  joy_lx, joy_ly, joy_rx, joy_ry;  // -100..100 (joystick puro)
    int8_t  cmd_x, cmd_y;                     // -100..100 (joystick + giroscopio fusionados, req. 7)
    uint16_t buttons;                          // bitmask, ver rf_link_button_bits
    uint8_t checksum;                          // XOR simple de todos los bytes anteriores
} rf_frame_t;

enum {
    RF_BTN_TRIG_L1 = 1 << 0,
    RF_BTN_TRIG_L2 = 1 << 1,
    RF_BTN_TRIG_R1 = 1 << 2,
    RF_BTN_TRIG_R2 = 1 << 3,
    RF_BTN_FRONT_A = 1 << 4,
    RF_BTN_FRONT_B = 1 << 5,
    RF_BTN_FRONT_X = 1 << 6,
    RF_BTN_FRONT_Y = 1 << 7,
};

esp_err_t rf_link_init(void);

/* Calcula checksum y transmite la trama por PIN_RF_TX (RMT, no bloqueante) */
esp_err_t rf_link_send_frame(rf_frame_t *frame);

/* Diagnóstico: cantidad de símbolos (transiciones) capturados en la
 * última recepción cruda del demodulador en PIN_RF_RX. Útil para
 * analizar el protocolo real de tu control original con un osciloscopio
 * / logic analyzer mientras decides cómo decodificarlo. */
int rf_link_get_last_rx_symbol_count(void);
