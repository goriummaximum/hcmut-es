#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_random.h"
#include "esp_log.h"

/*
how to know if the cmd is not for any functional tasks and need to be removed?
- using event group to set flag. Each funtional task have a bit.
+ if bit is set, indicate this cmd is not for this task
+ if bit is unset, indicate this cmd is for this task
+ garbage collector remove the trash cmd if all bits are set.

what if the bit of a task A is set for previous cmd still maintain set in current cmd, but task B recv current cmd for task A first?
- All bits is set, garbage collector have the right to remove the cmd for task A. -> error, cmd is lost!!!
- to solve this issue, whenever a task recv its cmd, it should clear all bits for all tasks.
Therefore, every tasks get back to its initial state to judge the new cmd.
Example:
- task A and B are unset.
- cmd 0 for task B arrived, if task A peek cmd first, it will set, then task B recv, then clear all bits for both tasks.
if task B peek cmd first, ofcouse every bits is unset.
- task A and B are unset.
- cmd 1 for task A arrived, if task B peek cmd first, it will set, if task B read again, it already set, the cmd still there in the queue,
then task A recv, then clear all bits for both tasks.
- task A and B are unset.
- cmd 2 not for any task, whenever task A or B peek cmd first, then at the end both tasks are set, then garbage collector remove.
=> The order of tasks is not matter now, if the cmd is valid, at the end, there is one and only one task is unset, which is the task recv its cmd.
then we unset all bits to reset to initial state ready to read new cmd.

CONDITIONS FOR THE ABOVE METHOD TO WORK:
- tasks cannot peek the already recv pkt

ISSUES:
- sometimes garbage collector remove wrong cmd, functional tasks receive wrong cmd.
do not know the exact cause yet, 
but maybe due to tasks can peek the already recv pkt, in addtition to using event group,
which causes cmd go to wrong task?
*/

/* DEFINITIONS */
#define CMD_QUEUE_MAX_LENGTH    5

#define NOT_CAMERA_QUALITY_BIT  BIT0
#define NOT_CAMERA_FLASH_BIT    BIT1
#define NOT_CAMERA_RESET_BIT    BIT2

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
    //printf("#%d: change camera quality\n", xTaskGetTickCount());
}

inline void toggle_camera_flash(void) {
    //printf("#%d: toggle camera flash\n", xTaskGetTickCount());
}

inline void reset_camera(void) {
    //printf("#%d: reset camera\n", xTaskGetTickCount());
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

        //check if the pkt is not for this task
        if (recv_cmd_pkt.id != camera_quality_handler_ID) {
            xEventGroupSetBits(tasks_event_group, NOT_CAMERA_QUALITY_BIT);
            continue;
        }
        //this pkt is for this task, recv completely and pop this cmd out of the queue
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //clear all set bit if the pkt is for this task, return every bit to initial state
        xEventGroupClearBits(tasks_event_group, NOT_CAMERA_QUALITY_BIT | NOT_CAMERA_FLASH_BIT | NOT_CAMERA_RESET_BIT);
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
        
        //check if the pkt is not for this task
        if (recv_cmd_pkt.id != camera_flash_handler_ID) {
            xEventGroupSetBits(tasks_event_group, NOT_CAMERA_FLASH_BIT);
            continue;
        }
        //this pkt is for this task, recv completely and pop this cmd out of the queue
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //clear all set bit if the pkt is for this task, return every bit to initial state
        xEventGroupClearBits(tasks_event_group, NOT_CAMERA_QUALITY_BIT | NOT_CAMERA_FLASH_BIT | NOT_CAMERA_RESET_BIT);
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
        
        //check if the pkt is not for this task
        if (recv_cmd_pkt.id != camera_reset_handler_ID) {
            xEventGroupSetBits(tasks_event_group, NOT_CAMERA_RESET_BIT);
            continue;
        }
        //this pkt is for this task, recv completely and pop this cmd out of the queue
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //clear all set bit if the pkt is for this task, return every bit to initial state
        xEventGroupClearBits(tasks_event_group, NOT_CAMERA_QUALITY_BIT | NOT_CAMERA_FLASH_BIT | NOT_CAMERA_RESET_BIT);
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
        xEventGroupWaitBits(tasks_event_group, NOT_CAMERA_QUALITY_BIT | NOT_CAMERA_FLASH_BIT | NOT_CAMERA_RESET_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        ESP_LOGI(LOG_TAG_GARBAGE_COLLECTOR, "q_length: %d/%d, garbage cmd {id:%d,cmd:%x} collected",
            (int)uxQueueMessagesWaiting(cmd_q), 
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id, 
            recv_cmd_pkt.cmd
        );
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
