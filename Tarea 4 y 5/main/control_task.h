#pragma once

/* Inicializa todos los subsistemas (ADC, botones, IMU, display, RF,
 * WiFi/MQTT) y lanza la tarea FreeRTOS que corre el ciclo principal:
 * leer -> fusionar joystick+giroscopio -> armar trama -> transmitir RF
 * -> refrescar pantalla -> publicar MQTT. */
void control_task_start(void);
