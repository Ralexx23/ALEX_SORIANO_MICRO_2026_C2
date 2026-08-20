#include "fsm.h"
#include "esp_log.h"

static const char *TAG = "FSM";
static gate_state_t current_state = GATE_STATE_INIT;
static gate_state_t state_before_calibration = GATE_STATE_CLOSED;

const char *fsm_state_to_str(gate_state_t state)
{
    switch (state) {
        case GATE_STATE_INIT:        return "INIT";
        case GATE_STATE_CLOSED:      return "CLOSED";
        case GATE_STATE_OPENING:     return "OPENING";
        case GATE_STATE_OPEN:        return "OPEN";
        case GATE_STATE_CLOSING:     return "CLOSING";
        case GATE_STATE_STOPPED:     return "STOPPED";
        case GATE_STATE_FAULT:       return "FAULT";
        case GATE_STATE_CALIBRATION: return "CALIBRATION";
        default:                     return "UNKNOWN";
    }
}

static void transition_to(gate_state_t new_state)
{
    ESP_LOGI(TAG, "%s -> %s", fsm_state_to_str(current_state), fsm_state_to_str(new_state));
    current_state = new_state;
}

void fsm_init(void)
{
    current_state = GATE_STATE_INIT;
    ESP_LOGI(TAG, "FSM inicializado en estado %s", fsm_state_to_str(current_state));
}

gate_state_t fsm_get_state(void)
{
    return current_state;
}

void fsm_handle_reset(bool limit_open_active, bool limit_close_active)
{
    if (current_state != GATE_STATE_FAULT) {
        ESP_LOGW(TAG, "Reset ignorado, el porton no esta en FAULT");
        return;
    }
    transition_to(GATE_STATE_INIT);
    fsm_resolve_boot_state(limit_open_active, limit_close_active);
}

void fsm_resolve_boot_state(bool limit_open_active, bool limit_close_active)
{
    if (limit_open_active && limit_close_active) {
        ESP_LOGW(TAG, "Ambos limit switch activos -> FAULT");
        transition_to(GATE_STATE_FAULT);
    } else if (limit_open_active) {
        transition_to(GATE_STATE_OPEN);
    } else if (limit_close_active) {
        transition_to(GATE_STATE_CLOSED);
    } else {
        ESP_LOGW(TAG, "Posicion desconocida -> STOPPED");
        transition_to(GATE_STATE_STOPPED);
    }
}

void fsm_handle_event(gate_event_t event)
{
    // Falla dura: limit switches contradictorios, válido en cualquier estado
    if (event == EVENT_BOTH_LIMITS_ACTIVE || event == EVENT_OBSTACLE_FAULT) {
        transition_to(GATE_STATE_FAULT);
        return;
    }

    switch (current_state) {

        case GATE_STATE_INIT:
            if (event == EVENT_BOOT_DONE) {
                // en fases reales aquí se decide según limit switches leídos
                transition_to(GATE_STATE_CLOSED);
            }
            break;

        case GATE_STATE_CLOSED:
            if (event == EVENT_CMD_OPEN)          transition_to(GATE_STATE_OPENING);
            else if (event == EVENT_CALIB_TOGGLE) {
                state_before_calibration = current_state;
                transition_to(GATE_STATE_CALIBRATION);
            }
            break;

        case GATE_STATE_OPEN:
            if (event == EVENT_CMD_CLOSE)         transition_to(GATE_STATE_CLOSING);
            else if (event == EVENT_CALIB_TOGGLE) {
                state_before_calibration = current_state;
                transition_to(GATE_STATE_CALIBRATION);
            }
            break;

        case GATE_STATE_OPENING:
            if (event == EVENT_LIMIT_OPEN_REACHED) transition_to(GATE_STATE_OPEN);
            else if (event == EVENT_CMD_STOP)      transition_to(GATE_STATE_STOPPED);
            else if (event == EVENT_TRAVEL_TIMEOUT) transition_to(GATE_STATE_FAULT);
            else if (event == EVENT_OBSTACLE_DETECTED) transition_to(GATE_STATE_STOPPED); // <-- NUEVO
            break;

        case GATE_STATE_CLOSING:
            if (event == EVENT_LIMIT_CLOSE_REACHED) transition_to(GATE_STATE_CLOSED);
            else if (event == EVENT_CMD_STOP)       transition_to(GATE_STATE_STOPPED);
            else if (event == EVENT_TRAVEL_TIMEOUT) transition_to(GATE_STATE_FAULT);
            else if (event == EVENT_OBSTACLE_DETECTED) {
                // el comportamiento real (stop/resume/reverse) se conecta en fase de fotocelda
                transition_to(GATE_STATE_STOPPED);
            }
            break;

        case GATE_STATE_STOPPED:
            if (event == EVENT_CMD_OPEN)       transition_to(GATE_STATE_OPENING);
            else if (event == EVENT_CMD_CLOSE) transition_to(GATE_STATE_CLOSING);
            break;

        case GATE_STATE_FAULT:
            ESP_LOGW(TAG, "Evento ignorado en FAULT, usa fsm_handle_reset()"); // El reset ahora se maneja aparte con fsm_handle_reset(), no por evento
            break;

        case GATE_STATE_CALIBRATION:
            if (event == EVENT_CALIB_TOGGLE) transition_to(state_before_calibration);
            break;
    }
}