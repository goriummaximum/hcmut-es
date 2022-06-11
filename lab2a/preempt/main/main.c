#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

/*
code description:
There are 3 tasks (priority): Idle (0) - Task2 (0) - Task1 (10)
Task1 has highest priority and repeat for every 50 ticks
Task2 and Idle have the same priority and run continuously.

sdkconfig:
- run only on first core
    + turn off FreeRTOS optimization
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

const char *LOG_TAG_MAIN = "MAIN";
const char *LOG_TAG_VTASK1 = "VTASK1";
const char *LOG_TAG_VTASK2 = "VTASK2";
const char *LOG_TAG_VTASK3 = "VTASK3";
const char *LOG_TAG_IDLE = "IDLE";

void vApplicationIdleHook(void) {
    ESP_LOGI(LOG_TAG_IDLE, "#%d, IdleTask running", xTaskGetTickCount());
}


void vTask1(void *pvParameters) {
    for (;;) {
        ESP_LOGI(LOG_TAG_VTASK1, "#%d, vTask1 running", xTaskGetTickCount());
        vTaskDelay(50);
    }

    vTaskDelete(NULL);
}

void vTask2(void *pvParameters) {
    for (;;) {
        ESP_LOGI(LOG_TAG_VTASK2, "#%d, vTask2 running", xTaskGetTickCount());
    }

    vTaskDelete(NULL);
}

void vTask3(void *pvParameters) {
    for (;;) {
        ESP_LOGI(LOG_TAG_VTASK3, "#%d, vTask3 running", xTaskGetTickCount());
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    //set priority for app_main to be highest among other tasks to avoid others task block app_main after initializaion
    vTaskPrioritySet(NULL, 15);
    
    if (xTaskCreate(&vTask1, "vTask1", 2048, NULL, 10, NULL) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask1 created successfully");
    if (xTaskCreate(&vTask2, "vTask2", 2048, NULL, tskIDLE_PRIORITY, NULL) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask2 created successfully");
    //if (xTaskCreate(&vTask3, "vTask3", 2048, NULL, tskIDLE_PRIORITY, NULL) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask3 created successfully");
    
    //set back to original priority for app_main task for not blocking other running tasks
    vTaskPrioritySet(NULL, 1);
}
