#include "control_task.h"

void app_main(void)
{
    control_task_start();
    /* app_main puede retornar; la tarea de control sigue corriendo en
     * su propio contexto FreeRTOS. */
}
