#ifndef FSM_H
#define FSM_H

#include <stdbool.h> 

typedef enum {
    GATE_STATE_INIT = 0,
    GATE_STATE_CLOSED,
    GATE_STATE_OPENING,
    GATE_STATE_OPEN,
    GATE_STATE_CLOSING,
    GATE_STATE_STOPPED,
    GATE_STATE_FAULT,
    GATE_STATE_CALIBRATION
} gate_state_t;

typedef enum {
    EVENT_BOOT_DONE,
    EVENT_CMD_OPEN,
    EVENT_CMD_CLOSE,
    EVENT_CMD_STOP,
    EVENT_CMD_RESET,
    EVENT_CALIB_TOGGLE,
    EVENT_LIMIT_OPEN_REACHED,
    EVENT_LIMIT_CLOSE_REACHED,
    EVENT_BOTH_LIMITS_ACTIVE,
    EVENT_OBSTACLE_DETECTED,   // solo se procesa si estamos cerrando
    EVENT_OBSTACLE_CLEARED,
    EVENT_OBSTACLE_FAULT,   // fotocelda activada con el porton ya CLOSED
    EVENT_TRAVEL_TIMEOUT
} gate_event_t;

void fsm_init(void);
void fsm_handle_event(gate_event_t event);
gate_state_t fsm_get_state(void);
const char *fsm_state_to_str(gate_state_t state);
void fsm_resolve_boot_state(bool limit_open_active, bool limit_close_active);
void fsm_handle_reset(bool limit_open_active, bool limit_close_active);

#endif