#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define pdTICKS_TO_S(xTicks) pdTICKS_TO_MS(xTicks) / 1000
#define pdS_TO_TICKS(s) pdMS_TO_TICKS(s * 1000)

#define IN_BUTTON_PIN       GPIO_NUM_21
#define DEBOUNCE_INTERVAL   2
#define BUTTON_PRESSED      0   //value when reading
#define BUTTON_RELEASED     1   //value when reading

const char *LOG_TAG_MAIN = "MAIN";
const char *LOG_TAG_PRINT = "print_student_id";
const char *LOG_TAG_POLL = "poll_button";

//cyclic task: start executing a block of code for every fixed interval, using vTaskDelayUntil
void print_student_id(void *pvParameters) {
    uint64_t start_tick;

    for (;;) {
        start_tick = xTaskGetTickCount();
        ESP_LOGI(LOG_TAG_PRINT, "1852161");
        vTaskDelayUntil(start_tick, pdS_TO_TICKS(1)); //delay for 1s for start_stick
    }

    vTaskDelete(NULL);
}

//acyclic task: start executing a block of code but not nessessary meet the fixed time interval, can use vTaskDelay
void poll_button(void *pvParameters) {
    uint8_t debounce_buffer_1 = gpio_get_level(IN_BUTTON_PIN);
    uint8_t debounce_buffer_2;
    uint8_t valid_buffer;
    uint8_t valid_buffer_prev;

    for (;;) {
        //debounce button with 2 filter layers
        debounce_buffer_2 = debounce_buffer_1;
        debounce_buffer_1 = gpio_get_level(IN_BUTTON_PIN);

        if (debounce_buffer_2 == debounce_buffer_1) { //valid pressed, not noise
            valid_buffer_prev = valid_buffer;
            valid_buffer = debounce_buffer_1;

            //print 'ESP32' only then button is first released after pressing
            if (valid_buffer == BUTTON_RELEASED && valid_buffer_prev == BUTTON_PRESSED) {
                ESP_LOGI(LOG_TAG_POLL, "ESP32");
            }
        }

        vTaskDelay(DEBOUNCE_INTERVAL);
    }

    vTaskDelete(NULL);
}

void gpio_init() {
    gpio_set_direction(IN_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(IN_BUTTON_PIN, GPIO_PULLUP_ONLY);
    ESP_LOGI(LOG_TAG_MAIN, "GPIO PIN init successfully");
}

void tasks_init() {
    if (xTaskCreate(&print_student_id, "print_student_id", 2048, NULL, 10, NULL) == pdPASS) {
        ESP_LOGI(LOG_TAG_MAIN, "print_student_id created successfully");
    }

    if (xTaskCreate(&poll_button, "poll_button", 2048, NULL, 9, NULL) == pdPASS) {
        ESP_LOGI(LOG_TAG_MAIN, "poll_button created successfully");
    }
}

void app_main(void)
{
    //set priority for app_main to be highest among other tasks to avoid others task block app_main after initializaion
    //after that kill app_main for not blocking other tasks
    vTaskPrioritySet(NULL, 15);
    
    gpio_init();
    tasks_init();

    vTaskPrioritySet(NULL, 1);
   
}
