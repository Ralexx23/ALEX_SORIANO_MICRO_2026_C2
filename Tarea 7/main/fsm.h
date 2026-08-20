#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdbool.h>

/* ---------------- MODOS DE JUEGO ---------------- */
typedef enum {
    GAME_MODE_REACTION = 1,
    GAME_MODE_MASH     = 2
} game_mode_t;

/* ---------------- ESTADOS ---------------- */
typedef enum {
    // Modo 1 - Reaction Timer
    STATE_IDLE_M1 = 0,
    STATE_WAIT_PB1_HOLD,
    STATE_RANDOM_DELAY,
    STATE_LED_ON_WAIT_RELEASE,
    STATE_WAIT_PB2_PRESS,
    STATE_RESULT_M1,
    STATE_FAULT_EARLY_RELEASE,

    // Modo 2 - Mash Test
    STATE_IDLE_M2,
    STATE_COUNTDOWN_M2,
    STATE_RUNNING_M2,
    STATE_RESULT_M2,

    STATE_COUNT
} fsm_state_t;

/* ---------------- EVENTOS ---------------- */
typedef enum {
    EVT_PB1_PRESS = 0,
    EVT_PB1_RELEASE,
    EVT_PB2_PRESS,
    EVT_PB2_RELEASE,
    EVT_BOOT_PRESS,
    EVT_TIMER_EXPIRED,
    EVT_COUNTDOWN_TICK,
    EVT_MASH_WINDOW_EXPIRED,
    EVT_MQTT_MODE_SET,
    EVT_MQTT_SET_FAULT_ACTION,
    EVT_MQTT_SET_MASH_ERROR_ACTION,
    EVT_MQTT_SET_MASH_WINDOW,
} fsm_event_type_t;

typedef struct {
    fsm_event_type_t type;
    int32_t param;   // uso genérico: nuevo modo, valor de config, etc.
} fsm_event_t;

/* ---------------- API ---------------- */
void fsm_init(void);
void fsm_start_task(void);
bool fsm_post_event(fsm_event_t evt);   // llamado desde ISR-safe wrapper o tasks
bool fsm_is_round_active(void);
game_mode_t fsm_get_current_mode(void);

typedef struct {
    int32_t reaction_ms;      // tiempo entre LED ON y soltar PB1
    int32_t second_press_ms;  // tiempo entre soltar PB1 y presionar PB2
    bool fault;                // true si hubo suelta temprana (modo penalize)
} result_m1_t;

result_m1_t fsm_get_last_result_m1(void);

/* ---------------- Resultado Modo 2 ---------------- */
typedef struct {
    int32_t total_presses;
    int32_t avg_interval_ms;
    int32_t window_ms;
} result_m2_t;

result_m2_t fsm_get_last_result_m2(void);

#endif // FSM_H