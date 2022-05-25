#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

uint32_t IdleTaskTick;

void vApplicationIdleHook(void) {
    IdleTaskTick = xTaskGetTickCount();
}

void vTask1(void *pvParameters) {
    for (;;) {
        printf("%d:   vTask1\n", xTaskGetTickCount());
        vTaskDelay(50);
    }

    vTaskDelete(NULL);
}

void vTask2(void *pvParameters) {
    for (;;) {
        printf("%d:   TaskIdle,   %d:   vTask2\n", IdleTaskTick, xTaskGetTickCount()); //because IdleHook cannot print, so delay to vTask2 to print the tick count of Idle Task.
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(&vTask1, "vTask1", 2048, NULL, 10, NULL);
    xTaskCreate(&vTask2, "vTask2", 2048, NULL, 0, NULL);
}
