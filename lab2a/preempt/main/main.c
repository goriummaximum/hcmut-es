#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

/*
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

uint32_t idle_timestamp = 0;
/*
void vApplicationIdleHook(void) {
    idle_timestamp = esp_log_early_timestamp();
}
*/

void vTask1(void *pvParameters) {
    for (;;) {
        printf("(%d) %s: running, (%d) %s: running\n", esp_log_early_timestamp(), LOG_TAG_VTASK1, idle_timestamp, LOG_TAG_IDLE);
        vTaskDelay(10);
    }

    vTaskDelete(NULL);
}

void vTask2(void *pvParameters) {
    for (;;) {
        printf("(%d) %s: running, (%d) %s: running\n", esp_log_early_timestamp(), LOG_TAG_VTASK2, idle_timestamp, LOG_TAG_IDLE);
    }

    vTaskDelete(NULL);
}

void vTask3(void *pvParameters) {
    for (;;) {
        printf("(%d) %s: running, (%d) %s: running\n", esp_log_early_timestamp(), LOG_TAG_VTASK3, idle_timestamp, LOG_TAG_IDLE);
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    //set priority for app_main to be highest among other tasks to avoid others task block app_main after initializaion
    vTaskPrioritySet(NULL, 15);
    
    if (xTaskCreatePinnedToCore(&vTask1, "vTask1", 2048, NULL, 10, NULL, 0) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask1 created successfully");
    if (xTaskCreatePinnedToCore(&vTask2, "vTask2", 2048, NULL, tskIDLE_PRIORITY, NULL, 0) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask2 created successfully");
    if (xTaskCreatePinnedToCore(&vTask3, "vTask3", 2048, NULL, tskIDLE_PRIORITY, NULL, 0) == pdPASS) ESP_LOGI(LOG_TAG_MAIN, "vTask3 created successfully");
    
    //set back to original priority for app_main task for not blocking other running tasks
    vTaskPrioritySet(NULL, 1);
}
