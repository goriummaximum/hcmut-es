#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_random.h"

/*
sdkconfig:
- support legacy FreeRTOS API
*/

/* DEFINITIONS */
#define CMD_QUEUE_MAX_LENGTH    5

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
            printf("[cmd_reception_handler] #%d: q_length: %d/%d, sent cmd {id:%d,cmd:%x} successfully\n", 
                xTaskGetTickCount(), 
                (int)uxQueueMessagesWaiting(cmd_q), 
                (int)CMD_QUEUE_MAX_LENGTH,
                recv_cmd_pkt.id, 
                recv_cmd_pkt.cmd
            );
        }

        else {
            printf("[cmd_reception_handler] #%d: q_length: %d/%d, sent cmd {id:%d,cmd:%x} failed\n", 
                xTaskGetTickCount(), 
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
        //check if the pkt is for this task
        if (recv_cmd_pkt.id != (uint8_t)0) continue;
        //this pkt is for this task, recv completely and pop this cmd out of the queue
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //do sth
        printf("[camera_quality_handler[0]] #%d: q_length: %d/%d, recv cmd {id:%d,cmd:%x} successfully\n",
            xTaskGetTickCount(),
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
        //check if the pkt is for this task
        if (recv_cmd_pkt.id != (uint8_t)1) continue;
        //this pkt is for this task, recv completely and pop this cmd out of the queue
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //do sth
        printf("[camera_flash_handler[1]] #%d: q_length: %d/%d, recv cmd {id:%d,cmd:%x} successfully\n",
            xTaskGetTickCount(),
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
        //check if the pkt is for this task
        if (recv_cmd_pkt.id != (uint8_t)2) continue;
        //this pkt is for this task, recv completely and pop this cmd out of the queue
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //do sth
        printf("[camera_reset_handler[2]] #%d: q_length: %d/%d, recv cmd {id:%d,cmd:%x} successfully\n",
            xTaskGetTickCount(),
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
        //get cmd pkt from the queue
        if (cmd_q == 0) continue;
        //wake up only there is a cmd pkt in the queue, else waiting forever
        //peek to see if the pkt is for this task, not removed from the queue yet
        if (xQueuePeek(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //check if the pkt is not for other functional tasks
        if (recv_cmd_pkt.id < 3) continue;
        //this pkt is for this task, recv completely and pop this cmd out of the queue
        if (xQueueReceive(cmd_q, (void *)&recv_cmd_pkt, portMAX_DELAY) != pdPASS) continue;
        //do sth
        printf("[q_garbage_collector] #%d: q_length: %d/%d, remove garbage cmd {id:%d,cmd:%x} successfully\n",
            xTaskGetTickCount(),
            (int)uxQueueMessagesWaiting(cmd_q),
            (int)CMD_QUEUE_MAX_LENGTH,
            recv_cmd_pkt.id,
            recv_cmd_pkt.cmd
        );
        
        //vTaskDelay(10); //check for garbage for every 10 ticks
    }

    vTaskDelete(NULL);
}

void app_main(void)
{
    //createQueue
    cmd_q = xQueueCreate(CMD_QUEUE_MAX_LENGTH, sizeof(cmd_t));
    if (cmd_q == 0) {
        printf("[MAIN] cmd_q created failed!\n");
        return;
    }

    xQueueReset(cmd_q);
    printf("[MAIN] cmd_q created successfully!\n");

    /*
    create tasks
    camera_quality_handler -> camera_flash_handler -> camera_reset_handler -> q_garbage_collector -> cmd_reception_handler
    cmd_reception_handler is created last because when start first and start to send pkt to the queue, other tasks cannot
    start anymore (have not know why yet, but this start order works!)
    */
    if (xTaskCreate(&camera_quality_handler, "camera_quality_handler", 1024 * 2, NULL, 9, NULL) != pdPASS){
        printf("[MAIN] camera_quality_handler created failed!\n");
        return;
    }
    printf("[MAIN] camera_quality_handler created successfully!\n");

    if (xTaskCreate(&camera_flash_handler, "camera_flash_handler", 1024 * 2, NULL, 9, NULL) != pdPASS) {
        printf("[MAIN] camera_flash_handler created failed!\n");
        return;
    }
    printf("[MAIN] camera_flash_handler created successfully!\n");

    if (xTaskCreate(&camera_reset_handler, "camera_reset_handler", 1024 * 2, NULL, 9, NULL) != pdPASS) {
        printf("[MAIN] camera_reset_handler created failed!\n");
        return;
    }
    printf("[MAIN] camera_reset_handler created successfully!\n");

    if (xTaskCreate(&q_garbage_collector, "q_garbage_collector", 1024 * 2, NULL, 9, NULL) != pdPASS) {
        printf("[MAIN] q_garbage_collector created failed!\n");
        return;
    }
    printf("[MAIN] q_garbage_collector created successfully!\n");

    if (xTaskCreate(&cmd_reception_handler, "cmd_reception_handler", 1024 * 2, NULL, 10, NULL) != pdPASS) {
        printf("[MAIN] cmd_reception_handler created failed!\n");
        return;
    }
    printf("[MAIN] cmd_reception_handler created successfully!\n");
}
