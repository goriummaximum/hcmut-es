#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
//#include "esp_random.h"
#include "esp_log.h"

/*
a counter-intuiative approach LOL is about to described...
- funtional tasks only peek for a copy of message, then decide to discard or take it later.
- a garbage collector is responsible for recmoving that message after all the tasks peeked.

how to know if all the tasks have peeked?
- After a task peek, it will set a bit saying "i peeked it!" and wait for garbage collector to allow it to continue.
- Garbage collector continuously check if all tasks raise the bit, if all bits are set, it will remove the message, after removing,
it will raise a bit saying "you guys can continue"
- Then all tasks will process the copy of the message by their own, then check for the new message.
*/

/* DEFINITIONS */
#define CMD_QUEUE_MAX_LENGTH    5

#define CAMERA_QUALITY_PEEKED_BIT  BIT0
#define CAMERA_FLASH_PEEKED_BIT    BIT1
#define CAMERA_RESET_PEEKED_BIT    BIT2
#define ALL_TASKS_CONTINUE         BIT3

const char *LOG_TAG_MAIN = "MAIN";
const char *LOG_TAG_CMD_RECEPTION = "CMD_RECEPTION_HANLDER";
const char *LOG_TAG_QUALITY = "CAMERA_QUALITY_HANDLER";
const char *LOG_TAG_RESET = "CAMERA_RESET_HANDLER";
const char *LOG_TAG_FLASH = "CAMERA_FLASH_HANDLER";
const char *LOG_TAG_GARBAGE_COLLECTOR = "GARBAGE_COLLECTOR";

const uint8_t camera_quality_handler_ID =   0;
const uint8_t camera_flash_handler_ID =     1;
const uint8_t camera_reset_handler_ID =     2;

typedef struct {
    uint8_t id;
    uint32_t cmd;
} cmd_t;

inline void change_camera_quality(void) {
    ESP_LOGI(LOG_TAG_QUALITY, "change quality");
}

inline void toggle_camera_flash(void) {
    ESP_LOGI(LOG_TAG_FLASH, "toggle flash");
}

inline void reset_camera(void) {
    ESP_LOGI(LOG_TAG_FLASH, "reset");
}

/* IMPLEMENTATION */
QueueHandle_t cmd_q;
EventGroupHandle_t tasks_event_group;

//simulate recv packet from webserver
cmd_t randomize_pkt(void) {
    cmd_t cmd_pkt;

    //randomize id
    cmd_pkt.id = (uint8_t)(esp_random() % 4);
    //randomize cmd
    cmd_pkt.cmd = esp_random();

    return cmd_pkt;
}

void cmd_reception_handler(void *pvParameters) {
    cmd_t recv_cmd_pkt;

    for (;;) {
        //randomize cmd
        recv_cmd_pkt = randomize_pkt();

        //filter corrupt pkt
        //if (recv_cmd_pkt.id > (uint8_t)2) continue;

        //send to cmd_q
        if (cmd_q == 0) continue;

        if (xQueueSendToBack(cmd_q, (void *)&recv_cmd_pkt, (TickType_t)10) == pdPASS) { //if send successfully
            ESP_LOGI(LOG_TAG_CMD_RECEPTION, "q_length: %d/%d, sent cmd {id:%d,cmd:%x} successfully",
                    (int)uxQueueMessagesWaiting(cmd_q), 
                    (int)CMD_QUEUE_MAX_LENGTH,
                    recv_cmd_pkt.id, 
                    recv_cmd_pkt.cmd
            );
        }

        else {
            ESP_LOGI(LOG_TAG_CMD_RECEPTION, "q_length: %d/%d, sent cmd {id:%d,cmd:%x} failed",
                (int)uxQueueMessagesWaiting(cmd_q), 
                (int)CMD_QUEUE_MAX_LENGTH,
                recv_cmd_pkt.id, 
                recv_cmd_pkt.cmd
            );
        }

        vTaskDelay(10); //delay for 10 ticks and generate next cmd pkt
    }

    vTaskDelete(NULL);
}

void camera_quality_handler(void *pvParameters) {
    cmd_t recv_cmd_pkt;

    for (;;) {
        //get cmd pkt from the queue
        if (cmd_q == 0) continue;
        //wake up only there is a cmd pkt in the queue, else waiting forever
        //peek to see if the pkt is for this task, not removed from the queue yet
        if (xQueuePeek(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;

        ESP_LOGI(LOG_TAG_QUALITY, "q_length: %d/%d, peek cmd {id:%d,cmd:%x}",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );

        //sync point: this tasks have just peeked and wait for continue
        xEventGroupSync(tasks_event_group, CAMERA_QUALITY_PEEKED_BIT, ALL_TASKS_CONTINUE, portMAX_DELAY);
        xEventGroupClearBits(tasks_event_group, ALL_TASKS_CONTINUE);

        //check if the pkt is not for this task
        if (recv_cmd_pkt.id != camera_quality_handler_ID) {
            continue;
        }

        //do sth
        ESP_LOGI(LOG_TAG_QUALITY, "q_length: %d/%d, recv cmd {id:%d,cmd:%x} successfully",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );
        change_camera_quality();
    }

    vTaskDelete(NULL);
}

void camera_flash_handler(void *pvParameters) {
    cmd_t recv_cmd_pkt;

    for (;;) {
        //get cmd pkt from the queue
        if (cmd_q == 0) continue;
        //wake up only there is a cmd pkt in the queue, else waiting forever
        //peek to see if the pkt is for this task, not removed from the queue yet
        if (xQueuePeek(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        
        ESP_LOGI(LOG_TAG_FLASH, "q_length: %d/%d, peek cmd {id:%d,cmd:%x}",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );

        //sync point: this tasks have just peeked and wait for continue
        xEventGroupSync(tasks_event_group, CAMERA_FLASH_PEEKED_BIT, ALL_TASKS_CONTINUE, portMAX_DELAY);
        xEventGroupClearBits(tasks_event_group, ALL_TASKS_CONTINUE);
        
        //check if the pkt is not for this task
        if (recv_cmd_pkt.id != camera_flash_handler_ID) {
            continue;
        }

        //do sth
        ESP_LOGI(LOG_TAG_FLASH, "q_length: %d/%d, recv cmd {id:%d,cmd:%x} successfully",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );
        toggle_camera_flash();
    }

    vTaskDelete(NULL);
}

void camera_reset_handler(void *pvParameters) {
    cmd_t recv_cmd_pkt;

    for (;;) {
        //get cmd pkt from the queue
        if (cmd_q == 0) continue;
        //wake up only there is a cmd pkt in the queue, else waiting forever
        //peek to see if the pkt is for this task, not removed from the queue yet
        if (xQueuePeek(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        
        ESP_LOGI(LOG_TAG_RESET, "q_length: %d/%d, peek cmd {id:%d,cmd:%x}",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );
        
        //sync point: this tasks have just peeked and wait for continue
        xEventGroupSync(tasks_event_group, CAMERA_RESET_PEEKED_BIT, ALL_TASKS_CONTINUE, portMAX_DELAY);
        xEventGroupClearBits(tasks_event_group, ALL_TASKS_CONTINUE);

        //check if the pkt is not for this task
        if (recv_cmd_pkt.id != camera_reset_handler_ID) {
            continue;
        }

        //do sth
        ESP_LOGI(LOG_TAG_RESET, "q_length: %d/%d, recv cmd {id:%d,cmd:%x} successfully",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );
        reset_camera();
    }

    vTaskDelete(NULL);
}

void q_garbage_collector(void *pvParameters) {
    cmd_t recv_cmd_pkt;

    for (;;) {
        //if the pkt is not for other functional tasks
        xEventGroupWaitBits(tasks_event_group, CAMERA_QUALITY_PEEKED_BIT | CAMERA_FLASH_PEEKED_BIT | CAMERA_RESET_PEEKED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        ESP_LOGI(LOG_TAG_GARBAGE_COLLECTOR, "q_length: %d/%d, garbage cmd {id:%d,cmd:%x} collected",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );

        xEventGroupSetBits(tasks_event_group, ALL_TASKS_CONTINUE);
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    //set priority for app_main to be highest among other tasks to avoid others task block app_main after initializaion
    //after that kill app_main for not blocking other tasks
    vTaskPrioritySet(NULL, 15);

    //create tasks event group
    tasks_event_group = xEventGroupCreate();

    //createQueue
    cmd_q = xQueueCreate(CMD_QUEUE_MAX_LENGTH, sizeof(cmd_t));
    if (cmd_q == 0) {
        ESP_LOGI(LOG_TAG_MAIN, "cmd_q created failed!");
        return;
    }

    xQueueReset(cmd_q);
    ESP_LOGI(LOG_TAG_MAIN, "cmd_q created successfully!");

    //createTask
    if (xTaskCreate(&cmd_reception_handler, "cmd_reception_handler", 1024 * 2, NULL, 1, NULL) != pdPASS) {
        ESP_LOGI(LOG_TAG_MAIN, "cmd_reception_handler created failed!");
        return;
    }
    ESP_LOGI(LOG_TAG_MAIN, "cmd_reception_handler created successfully!");

    if (xTaskCreate(&camera_quality_handler, "camera_quality_handler", 1024 * 2, NULL, 0, NULL) != pdPASS){
        ESP_LOGI(LOG_TAG_MAIN, "camera_quality_handler created failed!");
        return;
    }
    ESP_LOGI(LOG_TAG_MAIN, "camera_quality_handler created successfully!");

    if (xTaskCreate(&camera_flash_handler, "camera_flash_handler", 1024 * 2, NULL, 0, NULL) != pdPASS) {
        ESP_LOGI(LOG_TAG_MAIN, "camera_flash_handler created failed!");
        return;
    }
    ESP_LOGI(LOG_TAG_MAIN, "camera_flash_handler created successfully!");

    if (xTaskCreate(&camera_reset_handler, "camera_reset_handler", 1024 * 2, NULL, 0, NULL) != pdPASS) {
        ESP_LOGI(LOG_TAG_MAIN, "camera_reset_handler created failed!");
        return;
    }
    ESP_LOGI(LOG_TAG_MAIN, "camera_reset_handle created successfully!");

    if (xTaskCreate(&q_garbage_collector, "q_garbage_collector", 1024 * 2, NULL, 0, NULL) != pdPASS) {
        ESP_LOGI(LOG_TAG_MAIN, "q_garbage_collector created failed!");
        return;
    }
    ESP_LOGI(LOG_TAG_MAIN, "q_garbage_collector created successfully!");

    vTaskPrioritySet(NULL, 1);
}
