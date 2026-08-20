// actuator.c
#include "actuator.h"
#include "motor.h"
#include "rgb.h"
#include "buzzer.h"
#include "mqtt_manager.h"
#include "config.h"

void actuator_apply_state(gate_state_t state)
{
    switch (state) {
        case GATE_STATE_OPENING:
            motor_open();
            break;
        case GATE_STATE_CLOSING:
            motor_close();
            break;
        default:
            motor_stop();
            break;
    }
    
    rgb_set_state(state);
    buzzer_set_state(state);
    mqtt_publish_status(state);
}