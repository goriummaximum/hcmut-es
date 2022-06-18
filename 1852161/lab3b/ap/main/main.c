/*
 *                            default handler              user handler
 *  -------------             ---------------             ---------------
 *  |           |   event     |             | callback or |             |
 *  |   tcpip   | --------->  |    event    | ----------> | application |
 *  |   stack   |             |     task    |  event      |    task     |
 *  |-----------|             |-------------|             |-------------|
 *                                  /|\                          |
 *                                   |                           |
 *                            event  |                           |
 *                                   |                           |
 *                                   |                           |
 *                             ---------------                   |
 *                             |             |                   |
 *                             | WiFi Driver |/__________________|
 *                             |             |\     API call
 *                             |             |
 *                             |-------------|
*/


#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define WIFI_SSID       "esp32"
#define WIFI_PSWD       "12345678"
#define WIFI_CHANNEL    2
#define WIFI_MAX_CONN   5

/* a TAG to used when log to screen */
static const char *TAG = "wifi ap";


/* callback for event loop. event_base is the category, in each category (WIFI, IP, etc.), there are event_ids */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID = %d", MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID = %d", MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_ap(void) {
    /* Initialize the underlying TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    /* create esp event loop, this loop has nothing to do with event group of FreeRTOS, it is the event loop of ESP */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* Creates default WIFI AP driver attaching with TCP/IP stack. In case of any init error this API aborts. */
    esp_netif_create_default_wifi_ap();

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

    ESP_LOGI(TAG, "wifi_init_ap finished.");

   ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d", WIFI_SSID, WIFI_PSWD, WIFI_CHANNEL);
}

void app_main(void)
{
    //init NVS flash
    esp_err_t ret = nvs_flash_init();
    //if the NVS flash is full or the version is not match, then erase NVS flash and init again
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_ap();
}