#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "driver/gpio.h"

#define KEY_BUF_SIZE            50
#define VAL_BUF_SIZE            50

typedef struct {
    char key[KEY_BUF_SIZE];
    char value[VAL_BUF_SIZE];
} key_value_t;

/* a TAG to used when log to screen */
static const char *WIFI_TAG = "WIFI_AP";
static const char *HTTP_TAG = "HTTP_SERVER";
static const char *QUEUE_TAG = "QUEUE";
static const char *LED_TAG = "LED_HANDLER";
static const char *PRINT_TAG = "PRINT_HANDLER";
static const char *GARBAGE_COLLECTOR_TAG = "GARBAGE_COLLECTOR";

/*================= GPIO DEF =================*/
#define BUILTIN_LED_PIN     GPIO_NUM_2

uint8_t led_st = 1;

/*================= QUEUE DEF =================*/
#define QUEUE_MAX_LEN    5

QueueHandle_t q;

/*================= TASKS DEF =================*/
#define PEEK_LED_TASK_BIT       BIT0
#define PEEK_PRINT_TASK_BIT     BIT1
#define ALL_TASKS_CONTINUE      BIT2
static EventGroupHandle_t tasks_event_group;

/*================= WIFI AP DEF =================*/
#define WIFI_SSID       "esp32"
#define WIFI_PSWD       "12345678"
#define WIFI_CHANNEL    2
#define WIFI_MAX_CONN   5

static EventGroupHandle_t wifi_event_group;
#define WIFI_AP_START_BIT      BIT0

esp_netif_t *netif;
esp_netif_ip_info_t ip_info;

/*================= HTTP SERVER DEF =================*/
#define SIMPLE_SERVER_PORT      80
#define RESP_BUF_SIZE           500
#define REQ_BUF_SIZE            50

httpd_handle_t simple_server;


/*================= GPIO =================*/
void gpio_init(void) {
    gpio_set_direction(BUILTIN_LED_PIN, GPIO_MODE_OUTPUT);
    //gpio_set_pull_mode(BUILTIN_LED_PIN, GPIO_PULLUP_ONLY);
    gpio_set_level(BUILTIN_LED_PIN, led_st);
}

/*================= APP TASKS =================*/
void toggle_led(void) {
    led_st = !led_st;
    gpio_set_level(BUILTIN_LED_PIN, led_st);
    ESP_LOGI(LED_TAG, "LED state %s", led_st == 0 ? "ON" : "OFF");
}

void led_handler(void *pvParameters) {
    key_value_t recv_pkt;

    for (;;) {
        //get cmd pkt from the queue
        if (q == 0) continue;
        //wake up only there is a cmd pkt in the queue, else waiting forever
        //peek to see if the pkt is for this task, not removed from the queue yet
        if (xQueuePeek(q, (void *)&recv_pkt, portMAX_DELAY) != pdPASS) continue;

        //sync point
        xEventGroupSync(tasks_event_group, PEEK_LED_TASK_BIT, ALL_TASKS_CONTINUE, portMAX_DELAY);
        xEventGroupClearBits(tasks_event_group, ALL_TASKS_CONTINUE);

        //check if the pkt is not for this task
        if (strcmp(recv_pkt.key, "toggle") != 0) {
            continue;
        }

        //do sth
        toggle_led();
    }

    vTaskDelete(NULL);
}

void print_str(char *buf) {
    ESP_LOGI(PRINT_TAG, "%s", buf);
}

void print_handler(void *pvParameters) {
    key_value_t recv_pkt;

    for (;;) {
        //get cmd pkt from the queue
        if (q == 0) continue;
        //wake up only there is a cmd pkt in the queue, else waiting forever
        //peek to see if the pkt is for this task, not removed from the queue yet
        if (xQueuePeek(q, (void *)&recv_pkt, portMAX_DELAY) != pdPASS) continue;

        //sync point
        xEventGroupSync(tasks_event_group, PEEK_PRINT_TASK_BIT, ALL_TASKS_CONTINUE, portMAX_DELAY);
        xEventGroupClearBits(tasks_event_group, ALL_TASKS_CONTINUE);

        //check if the pkt is not for this task
        if (strcmp(recv_pkt.key, "str") != 0) {
            continue;
        }

        //do sth
        print_str(recv_pkt.value);
    }

    vTaskDelete(NULL);
}

void garbage_collector(void *pvParameters) {
    key_value_t recv_pkt;

    for (;;) {
        //if the pkt is not for other functional tasks
        xEventGroupWaitBits(tasks_event_group, PEEK_LED_TASK_BIT | PEEK_PRINT_TASK_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        xQueueReceive(q, (void *)&recv_pkt, portMAX_DELAY);
        ESP_LOGI(GARBAGE_COLLECTOR_TAG, "garbage collected");
        xEventGroupSetBits(tasks_event_group, ALL_TASKS_CONTINUE);
    }

    vTaskDelete(NULL);
}

void tasks_init(void) {
    tasks_event_group = xEventGroupCreate();

    if (xTaskCreate(&led_handler, "led_handler", 1024 * 2, NULL, 0, NULL) == pdPASS) {
        ESP_LOGI(LED_TAG, "led_handler created success");
    }
    if (xTaskCreate(&print_handler, "print_handler", 1024 * 2, NULL, 0, NULL) == pdPASS) {
        ESP_LOGI(PRINT_TAG, "print_handler created success");
    }
    if (xTaskCreate(&garbage_collector, "garbage_collector", 1024 * 2, NULL, 0, NULL) == pdPASS) {
        ESP_LOGI(GARBAGE_COLLECTOR_TAG, "garbage_collector created success");
    }
}

/*================= QUEUE =================*/
void queue_init(void) {
    q = xQueueCreate(QUEUE_MAX_LEN, sizeof(key_value_t));
    ESP_LOGI(QUEUE_TAG, "data size = %d", sizeof(key_value_t));
    while (q == 0) {
        ESP_LOGI(QUEUE_TAG, "q created failed");
        q = xQueueCreate(QUEUE_MAX_LEN, sizeof(key_value_t));
    }

    xQueueReset(q);
    ESP_LOGI(QUEUE_TAG, "q created success");
}

/*================= WIFI AP =================*/
/* callback for event loop. event_base is the category, in each category (WIFI, IP, etc.), there are event_ids */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" join, AID = %d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" leave, AID = %d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        xEventGroupSetBits(wifi_event_group, WIFI_AP_START_BIT);
    }
}

void flash_init(void) {
    //init NVS flash
    esp_err_t ret = nvs_flash_init();
    //if the NVS flash is full or the version is not match, then erase NVS flash and init again
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void wifi_init_ap(void) {
    wifi_event_group = xEventGroupCreate();

    /* Initialize the underlying TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    /* create esp event loop, this loop has nothing to do with event group of FreeRTOS, it is the event loop of ESP */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* glue wifi ap with tcp/ip layer */
    netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    /* Initialize WiFi Allocate resource for WiFi driver and create wifi task */
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    /* register esp events */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &event_handler,
        NULL,
        NULL
    ));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PSWD,
            .channel = WIFI_CHANNEL,
            .max_connection = WIFI_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    if (strlen(WIFI_PSWD) == 0) wifi_config.ap.authmode = WIFI_AUTH_OPEN;

    /* Set the WiFi operating mode Set the WiFi operating mode as station, soft-AP or station+soft-AP, The default mode is station mode. */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    /* Set the configuration of the ESP32 STA or AP */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    /* Start WiFi according to current configuration */
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_AP_START_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_AP_START_BIT) {
        ESP_LOGI(WIFI_TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", WIFI_SSID, WIFI_PSWD, WIFI_CHANNEL);
    }
}

/*================= HTTP server =================*/
key_value_t extract_content(char *buf, int buf_len) {
    key_value_t ret;

    //extract key
    int i;
    for (i = 0; buf[i] != '='; i++) {
        ret.key[i] = buf[i];
    }
    ret.key[i] = '\0';
    i++; //skip =

    //extract value
    int key_len = i;
    for (; i < buf_len; i++) {
        if (buf[i] == '+') ret.value[i - key_len] = ' ';
        else ret.value[i - key_len] = buf[i];
    }
    ret.value[i - key_len] = '\0';

    return ret;
}

int gen_index_page(char *buf) {
    strcat(buf, ""); //THIS BUF MUST BE CLEARG. IF NOT, THE INITIAL CONTENT IS UNKNOWN WHICH CAUSE UNEXPECTED CHARACTERS
    strcat(buf, "<title>ESP32 control</title>\n");
    strcat(buf, "</head>\n");
    strcat(buf, "<body>\n");
    strcat(buf, "<h2>ESP32 control</h2>\n");
    strcat(buf, "<form action=\"/\" method=\"post\">\n");
    strcat(buf, "<input type=\"submit\" id=\"toggle\" name=\"toggle\" value=\"toggleled\">\n");
    strcat(buf, "</form>\n");
    strcat(buf, "<form action=\"/\" method=\"post\">\n");
    strcat(buf, "<input type=\"text\" id=\"str\" name=\"str\">\n");
    strcat(buf, "<input type=\"submit\" value=\"send string\">\n");
    strcat(buf, " </form>\n");
    strcat(buf, "</body>");
    strcat(buf, "</html>\n");
    return (int)strlen(buf);
}

esp_err_t index_get_handler(httpd_req_t *req) {
    //send resp
    char resp_buf[RESP_BUF_SIZE] = "";
    int resp_buf_len = gen_index_page(resp_buf);
    ESP_LOGI(HTTP_TAG, "resp_buf_len = %d", resp_buf_len);

    esp_err_t err = httpd_resp_send(req, resp_buf, resp_buf_len);
    if (err != ESP_OK) {
        ESP_LOGI(HTTP_TAG, "error while sending /");
    }

    else {
        ESP_LOGI(HTTP_TAG, "success while sending /");
    }
    return err;
}

esp_err_t cmd_post_handler(httpd_req_t *req) {
    //process req
    char req_buf[REQ_BUF_SIZE] = "";
    int req_buf_len = req->content_len;
    if (req_buf_len > REQ_BUF_SIZE) return ESP_FAIL;

    int ret = httpd_req_recv(req, req_buf, req_buf_len);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req); //timeout
        }
        return ESP_FAIL;
    }
    ESP_LOGI(HTTP_TAG, "toggle req content: %s", req_buf);
    key_value_t extracted_content = extract_content(req_buf, req_buf_len);
    ESP_LOGI(HTTP_TAG, "toggle req content: key = %s, value = %s", extracted_content.key, extracted_content.value);
    
    //push recv pkt to the queue
    if (q != 0) {
        if (xQueueSendToBack(q, (void *)&extracted_content, (TickType_t)10) == pdPASS) { //if send successfully
            ESP_LOGI(HTTP_TAG, "send pkt to queue success");
        } //can we set a timeout in this http server task?
    }

    //send resp
    char resp_buf[RESP_BUF_SIZE] = "";
    int resp_buf_len = gen_index_page(resp_buf);
    ESP_LOGI(HTTP_TAG, "resp_buf_len = %d", resp_buf_len);
    esp_err_t err = httpd_resp_send(req, resp_buf, resp_buf_len);
    return err;
}

httpd_uri_t index_get_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
    .user_ctx = NULL
};

httpd_uri_t cmd_post_uri = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = cmd_post_handler,
    .user_ctx = NULL
};

void http_server_init(void) {
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.lru_purge_enable = true;
    http_cfg.server_port = SIMPLE_SERVER_PORT;

    //start server
    esp_netif_get_ip_info(netif, &ip_info);
    ESP_LOGI(HTTP_TAG, "starting server on "IPSTR":%d", IP2STR(&ip_info.ip), http_cfg.server_port);

    if (httpd_start(&simple_server, &http_cfg) == ESP_OK) {
        //register uris
        ESP_LOGI(HTTP_TAG, "register URIs");
        httpd_register_uri_handler(simple_server, &index_get_uri);
        httpd_register_uri_handler(simple_server, &cmd_post_uri);
    }
}

void app_main(void)
{
    gpio_init();
    flash_init();
    wifi_init_ap();
    http_server_init();
    queue_init();
    tasks_init();
}