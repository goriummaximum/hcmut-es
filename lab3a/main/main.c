#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/gpio.h"

/*
freeRTOSconfig.h:
#define configUSE_TIMERS            1

sdkconfig:
- run only on first core
    + turn off FreeRTOS optimization
- support legacy FreeRTOS API
- FreeRTOS timer task priority: 10
*/

#define pdTICKS_TO_S(xTicks) pdTICKS_TO_MS(xTicks) / 1000
#define pdS_TO_TICKS(s) pdMS_TO_TICKS(s * 1000)

#define NUMBER_OF_TIMERS    2

TimerHandle_t soft_timers[NUMBER_OF_TIMERS];
uint8_t soft_timers_interval_s[NUMBER_OF_TIMERS] = {2, 3};
uint8_t soft_timers_counter_max[NUMBER_OF_TIMERS] = {10, 5};
uint8_t soft_timers_counter[NUMBER_OF_TIMERS] = {0, 0};

void SoftTimerCallback(TimerHandle_t xTimer) {
    if (xTimer == soft_timers[0]) {
        printf("[stimer0] %d (s): ahihi\n", pdTICKS_TO_S(xTaskGetTickCount()));

        soft_timers_counter[0]++;
        //why need to check counter > max while == max is enough? in case the xtimer stop failed, the next callback will try to stop again
        if (soft_timers_counter[0] >= soft_timers_counter_max[0]) {
            if (xTimerStop(xTimer, 0) == pdPASS) {
                soft_timers_counter[0] = 0;
                printf("[stimer0] %d (s): stopped\n", pdTICKS_TO_S(xTaskGetTickCount()));
            };
        }
    }

    else if (xTimer == soft_timers[1]) {
        printf("[stimer1] %d (s): ihaha\n", pdTICKS_TO_S(xTaskGetTickCount()));

        soft_timers_counter[1]++;
        //why need to check counter > max while == max is enough? in case the xtimer stop failed, the next callback will try to stop again
        if (soft_timers_counter[1] >= soft_timers_counter_max[1]) {
            if (xTimerStop(xTimer, 0) == pdPASS) {
                soft_timers_counter[1] = 0;
                printf("[stimer1] %d (s): stopped\n", pdTICKS_TO_S(xTaskGetTickCount()));
            };
        }
    }
}

void app_main(void)
{
    //set priority for app_main to be highest among other tasks to avoid others task block app_main after initializaion
    //after that kill app_main fr not blocking othe tasks
    vTaskPrioritySet(NULL, 15);

    int pc_name_buf[10];
    for (int i = 0; i < NUMBER_OF_TIMERS; i++) {
        //create soft timer
        sprintf(pc_name_buf, "stimer%d", i);
        soft_timers[i] = xTimerCreate(pc_name_buf, pdS_TO_TICKS(soft_timers_interval_s[i]), pdTRUE, i, SoftTimerCallback);
        if (soft_timers[i] != NULL) {
            printf("[stimer%d] %d (s): created\n", (int)pvTimerGetTimerID(soft_timers[i]), pdTICKS_TO_S(xTaskGetTickCount()));
            //start software timer
            if (xTimerStart(soft_timers[i], 10) == pdPASS) {
                printf("[stimer%d] %d (s): started\n", (int)pvTimerGetTimerID(soft_timers[i]), pdTICKS_TO_S(xTaskGetTickCount()));
            }
        }
    }
    
    //set back to original priority for app_main task for not blocking other running tasks
    vTaskPrioritySet(NULL, 1);
}
