#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/*
code description:
There are 3 tasks (priority): Idle (0) - Task2 (0) - Task1 (10)
Task1 has highest priority and repeat for every 50 ticks
Task2 and Idle have the same priority and run continuously.

sdkconfig:
- run only on first core
- support legacy FreeRTOS API
- use FreeRTOS Idle hook

FreeRTOSConfig.h:
- With time slicing:
#define configUSE_PREEMPTION                            1
#define configUSE_TIME_SLICING                          1
#define configIDLE_SHOULD_YIELD                         0

- With time slicing + IDLE yield (not wait until end of timesclice to continue)
#define configUSE_PREEMPTION                            1
#define configUSE_TIME_SLICING                          1
#define configIDLE_SHOULD_YIELD                         1

- Without time slicing
#define configUSE_PREEMPTION                            1
#define configUSE_TIME_SLICING                          0
#define configIDLE_SHOULD_YIELD                         0
*/

uint32_t IdleTaskTick;

void vApplicationIdleHook(void) {
    IdleTaskTick = xTaskGetTickCount();
}

void vTask1(void *pvParameters) {
    for (;;) {
        printf("%d:   vTask1 start, ", xTaskGetTickCount());
        /*  assume doing sth here start  */

        /*  assume doing sth here end  */
        printf("%d:   vTask1 end\n", xTaskGetTickCount());
        vTaskDelay(50);
    }

    vTaskDelete(NULL);
}

void vTask2(void *pvParameters) {
    for (;;) {
        //because IdleHook cannot print, so delay to vTask2 to print the tick count of Idle Task.
        printf("%d:   TaskIdle end (print from vTask2), %d:   vTask2 start, ", IdleTaskTick, xTaskGetTickCount());
        /*  assume doing sth here start  */

        /*  assume doing sth here end  */
        printf("%d:   vTask2 end\n", xTaskGetTickCount());
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(&vTask1, "vTask1", 2048, NULL, 10, NULL);
    xTaskCreate(&vTask2, "vTask2", 2048, NULL, 0, NULL);
}
