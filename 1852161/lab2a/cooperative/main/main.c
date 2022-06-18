#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

/*
code description:
There are 3 tasks (priority): Task1 (15) - Task2 (14) - Task3 (13)
Task3 run for 50 ticks, Task2 delay for 20 ticks, Task1 delay for 30 ticks
-> Then task1 run for 20 ticks
-> Then task2 run for 30 ticks

freeRTOSconfig.h:
#define configUSE_PREEMPTION                            0
#define configUSE_TIME_SLICING                          0
#define configIDLE_SHOULD_YIELD                         0

sdkconfig:
- run only on first core
    + turn off FreeRTOS optimization
- support legacy FreeRTOS API
*/

const char *LOG_TAG_MAIN = "MAIN";
const char *LOG_TAG_VTASK1 = "VTASK1";
const char *LOG_TAG_VTASK2 = "VTASK2";
const char *LOG_TAG_VTASK3 = "VTASK3";

void vTask1(void *pvParameters) {
    vTaskDelay(30);

    uint32_t curr_tick;
    printf("(%d) %s: vTask1 start\n", xTaskGetTickCount(), LOG_TAG_VTASK1);
    curr_tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - curr_tick < 20) {}
    printf("(%d) %s: vTask1 end\n", xTaskGetTickCount(), LOG_TAG_VTASK1);
    taskYIELD();

    vTaskDelete(NULL);
}

void vTask2(void *pvParameters) {
    vTaskDelay(20);

    uint32_t curr_tick;
    printf("(%d) %s: vTask2 start\n", xTaskGetTickCount(), LOG_TAG_VTASK2);
    curr_tick = xTaskGetTickCount();
    while (xTaskGetTickCount() - curr_tick < 30) {}
    printf("(%d) %s: vTask2 end\n", xTaskGetTickCount(), LOG_TAG_VTASK2);
    taskYIELD();

    vTaskDelete(NULL);
}

void vTask3(void *pvParameters) {
    uint32_t curr_tick;

    for (;;) {
        printf("(%d) %s: vTask3 start\n", xTaskGetTickCount(), LOG_TAG_VTASK3);
        curr_tick = xTaskGetTickCount();
        while (xTaskGetTickCount() - curr_tick < 50) {}
        printf("(%d) %s: vTask3 end\n", xTaskGetTickCount(), LOG_TAG_VTASK3);
        taskYIELD();
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    //set priority for app_main to be highest among other tasks to avoid others task block app_main after initializaion
    //after that kill app_main fr not blocking othe tasks
    vTaskPrioritySet(NULL, 15);
    
    if (xTaskCreate(&vTask1, "vTask1", 2048, NULL, 10, NULL) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask1 created successfully");
    if (xTaskCreate(&vTask2, "vTask2", 2048, NULL, 9, NULL) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask2 created successfully");
    if (xTaskCreate(&vTask3, "vTask3", 2048, NULL, 8, NULL) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask3 created successfully");
    
    //set back to original priority for app_main task for not blocking other running tasks
    vTaskPrioritySet(NULL, 1);
}
