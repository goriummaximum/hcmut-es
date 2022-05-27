/*
CPU stat collect by idle hook. Print to screen the collected stat for every 2 seconds by print_cpu_stat task.

Example:
Collected at: 64802 (ticks)

Task----------absolute time (us)----% time
IDLE            648009966               99%
IDLE            645660772               99%
print_cpu_stat  2343335         <1%
ipc0            27192           <1%
esp_timer       30              <1%
ipc1            32222           <1%

Total CPU time spending for certain tasks (in running state).
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

char cpu_stat[1000];

void vApplicationIdleHook(void) {
    vTaskGetRunTimeStats(cpu_stat);
}

void print_cpu_stat(void *pvParameters) {
    for (;;) {
        printf("Collected at: %d (ticks)\n", xTaskGetTickCount());
        printf("%s\n", cpu_stat);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    //set priority for app_main to be highest among other tasks to avoid others task block app_main after initializaion
    //after that kill app_main fr not blocking othe tasks
    vTaskPrioritySet(NULL, 15);
    
    if (xTaskCreate(&print_cpu_stat, "print_cpu_stat", 2048, NULL, 1, NULL) == pdPASS) printf("print_cpu_stat created successfully\n");
    
    vTaskPrioritySet(NULL, 1);
}
