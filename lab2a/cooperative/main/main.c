#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

void vTask1(void *pvParameters) {
    vTaskDelay(30);

    uint32_t curr_tick;
    printf("%d:   vTask1 start\n", xTaskGetTickCount());
    curr_tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - curr_tick < 20) {}
    printf("%d:   vTask1 end\n\n", xTaskGetTickCount());
    taskYIELD();

    vTaskDelete(NULL);
}

void vTask2(void *pvParameters) {
    vTaskDelay(20);

    uint32_t curr_tick;
    printf("%d:   vTask2 start\n", xTaskGetTickCount());
    curr_tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - curr_tick < 30) {}
    printf("%d:   vTask2 end\n\n", xTaskGetTickCount());
    taskYIELD();

    vTaskDelete(NULL);
}

void vTask3(void *pvParameters) {
    uint32_t curr_tick;

    for (;;) {
        printf("%d:   vTask3 start\n", xTaskGetTickCount());
        curr_tick = xTaskGetTickCount();
        while (xTaskGetTickCount() - curr_tick < 50) {}
        printf("%d:   vTask3 end\n\n", xTaskGetTickCount());
        taskYIELD();
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(&vTask1, "vTask1", 2048, NULL, 15, NULL);
    xTaskCreate(&vTask2, "vTask2", 2048, NULL, 14, NULL);
    xTaskCreate(&vTask3, "vTask3", 2048, NULL, 13, NULL);
}
