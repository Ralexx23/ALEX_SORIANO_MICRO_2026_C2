#pragma once
#include <stdbool.h>

/* ===== Máquina de estados de 2 estados ===== */
typedef enum {
    FSM_STATE_OFF = 0,
    FSM_STATE_ON  = 1,
} fsm_state_t;

/* Comandos que puede recibir la FSM, sin importar si vienen del botón
 * físico o de un mensaje MQTT desde el celular */
typedef enum {
    FSM_CMD_OFF,
    FSM_CMD_ON,
    FSM_CMD_TOGGLE,
} fsm_cmd_t;

/* Inicializa LED, botón (con debounce) y la tarea de la FSM.
 * on_state_changed se invoca cada vez que el estado cambia
 * efectivamente (se usa para publicar por MQTT). Puede ser NULL. */
void fsm_led_init(void (*on_state_changed)(fsm_state_t new_state));

/* Pide una transición de estado. Thread-safe: se puede llamar tanto
 * desde la tarea del botón como desde el callback de MQTT. */
void fsm_led_request(fsm_cmd_t cmd);

fsm_state_t fsm_led_get_state(void);
