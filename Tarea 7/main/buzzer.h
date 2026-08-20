#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void buzzer_init(void);
void buzzer_beep(uint32_t duration_ms);  // no bloqueante, se apaga sola via timer
void buzzer_beep_double(uint32_t duration_ms, uint32_t gap_ms);

#endif // BUZZER_H