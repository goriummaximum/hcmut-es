#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define IN_BUTTON_PIN       GPIO_NUM_21
#define DEBOUNCE_INTERVAL   5
#define BUTTON_PRESSED      0   //value when reading
#define BUTTON_RELEASED     1   //value when reading

void print_student_id(void *pvParameters) {
    for (;;) {
        printf("1852161\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}



void poll_button(void *pvParameters) {
    uint8_t debounce_buffer_1 = gpio_get_level(IN_BUTTON_PIN);
    uint8_t debounce_buffer_2;
    uint64_t prev_tick = 0;
    uint64_t curr_tick;

    for (;;) {
        curr_tick = xTaskGetTickCount();
        //debounce button with 2 filter layers
        if (curr_tick - prev_tick >= DEBOUNCE_INTERVAL) {
            debounce_buffer_2 = debounce_buffer_1;
            debounce_buffer_1 = gpio_get_level(IN_BUTTON_PIN);

            if (debounce_buffer_2 == BUTTON_PRESSED && debounce_buffer_1 == BUTTON_RELEASED) { //check if button is released
                printf("ESP32\n");
            }

            prev_tick = curr_tick;
        }
    }

    vTaskDelete(NULL);
}

void init() {
    gpio_set_direction(IN_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(IN_BUTTON_PIN, GPIO_PULLUP_ONLY);
}

void app_main(void)
{
    init();

    xTaskCreate(&print_student_id, "print_student_id", 2048, NULL, 0, NULL);
    xTaskCreate(&poll_button, "poll_button", 2048, NULL, 0, NULL);
   
}
