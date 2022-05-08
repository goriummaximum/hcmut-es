#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define IN_BUTTON_PIN      GPIO_NUM_21

void print_student_id(void *pvParameters) {
    for (;;) {
        printf("1852161\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}



void poll_button(void *pvParameters) {
    for (;;) {
        if (gpio_get_level(IN_BUTTON_PIN) == 0)
        {
            printf("ESP32\n");
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

    xTaskCreate(&print_student_id, "print_student_id", 2048, NULL, 5, NULL);
    xTaskCreate(&poll_button, "poll_button", 2048, NULL, 5, NULL);
   
}
