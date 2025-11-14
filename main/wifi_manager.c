#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "wifi_manager.h"

static const char *TAG = "wifi_manager";

/*
 * Edit here for local network (or change to menuconfig / sdkconfig later) credentials
 * ESP32 only supports 2.4GHz WiFi
 */
#define WIFI_SSID "xxxxxxxxxx"
#define WIFI_PASS "xxxxxxxxxx"

static SemaphoreHandle_t s_wifi_connected_sem = NULL;
static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START)
        {
            esp_wifi_connect();
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START -> esp_wifi_connect()");
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            wifi_event_sta_disconnected_t *disconn_evt = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED -> reason=%d, trying to reconnect...", disconn_evt->reason);
            esp_wifi_connect();
            // Note: we do not clear the connected semaphore here; wait will block again
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
            xSemaphoreGive(s_wifi_connected_sem); // signal connected
        }
    }
}

void wifi_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Create the semaphore for connection status
    s_wifi_connected_sem = xSemaphoreCreateBinary();

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,  // Accept both WPA and WPA2
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };
    ESP_LOGI(TAG, "Setting WiFi SSID [%s]", WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

BaseType_t wifi_manager_wait_connected(TickType_t ticks_to_wait)
{
    if (s_wifi_connected_sem == NULL)
        return pdFALSE;
    // Wait for the semaphore to be given in IP_EVENT_STA_GOT_IP
    return xSemaphoreTake(s_wifi_connected_sem, ticks_to_wait);
}

void wifi_manager_request_reconnect(void)
{
    ESP_LOGI(TAG, "Requested Wi-Fi reconnect");
    esp_wifi_disconnect();
    esp_wifi_connect();
}
