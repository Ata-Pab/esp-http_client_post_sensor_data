#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "wifi_manager.h"
#include "http_client.h"

static const char *TAG = "MAIN";

/* Configurable constants */
#define HTTP_POST_URL "https://httpbin.org/post" // plain HTTPS endpoint for testing
#define PRODUCER_INTERVAL_MS 5000               // produce every 5s
#define QUEUE_LENGTH 8
#define QUEUE_ITEM_SIZE 256                    // bytes per message payload
#define HTTP_TIMEOUT_TICKS pdMS_TO_TICKS(5000) // 5s timeout

/* FreeRTOS objects */
static QueueHandle_t xMessageQueue = NULL;
static TaskHandle_t xHttpTaskHandle = NULL;
static TaskHandle_t xProducerTaskHandle = NULL;
static SemaphoreHandle_t xNetReadySem = NULL;

typedef struct
{
    char payload[QUEUE_ITEM_SIZE];
} message_t;

/* Producer: generate dummy sensor data and push to queue */
static void producer_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t counter = 0;

    while (1)
    {
        message_t msg;
        int len = snprintf(msg.payload, sizeof(msg.payload),
                           "{\"device\":\"esp32\",\"counter\":%u,\"value\":%d}", (unsigned)counter++, (int)(rand() % 100));
        if (len < 0)
        {
            ESP_LOGE(TAG, "Failed to create JSON");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (xQueueSend(xMessageQueue, &msg, pdMS_TO_TICKS(1000)) != pdPASS)
        {
            ESP_LOGW(TAG, "Queue full — drop message");
        }
        else
        {
            ESP_LOGI(TAG, "[Producer] Enqueued: %s", msg.payload);
            // Optionally notify the HTTP task:
            xTaskNotifyGive(xHttpTaskHandle);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(PRODUCER_INTERVAL_MS));
    }
}

/* HTTP client task: waits for network and messages, performs HTTP POST */
static void http_task(void *arg)
{
    message_t msg;
    const TickType_t wait_ticks = pdMS_TO_TICKS(1000);

    ESP_LOGI(TAG, "HTTP task started — waiting for network...");

    for (;;)
    {
        // Wait until Wi-Fi connected (block until connection is established)
        if (wifi_manager_wait_connected(portMAX_DELAY) != pdTRUE)
        {
            ESP_LOGW(TAG, "wifi_manager_wait_connected returned false — retrying...");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG, "Network ready - HTTP task operational");

        // Main loop: handle queued messages
        while (wifi_manager_wait_connected(0) == pdTRUE) // Check if still connected
        {
            // Block waiting for an item
            if (xQueueReceive(xMessageQueue, &msg, wait_ticks) == pdPASS)
            {
                ESP_LOGI(TAG, "[HTTP] Got message to send: %s", msg.payload);
                esp_err_t err = http_client_post_json(HTTP_POST_URL, msg.payload, HTTP_TIMEOUT_TICKS);

                if (err == ESP_OK)
                {
                    ESP_LOGI(TAG, "[HTTP] Sent successfully");
                }
                else
                {
                    // HTTPS failed - this is likely a TLS certificate issue, NOT Wi-Fi
                    ESP_LOGW(TAG, "[HTTP] Send failed: %s — Wi-Fi is connected, likely TLS issue", esp_err_to_name(err));
                    
                    // Requeue message for retry
                    if (xQueueSend(xMessageQueue, &msg, pdMS_TO_TICKS(100)) == pdPASS)
                    {
                        ESP_LOGI(TAG, "[HTTP] Requeued message for retry");
                    }
                    else
                    {
                        ESP_LOGW(TAG, "[HTTP] Requeue failed (queue full)");
                    }
                    
                    // Add delay to avoid rapid retries
                    vTaskDelay(pdMS_TO_TICKS(3000));
                }
            }
        }
        
        ESP_LOGW(TAG, "Network lost - waiting for reconnection...");
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting wifi_http_client_demo");

    // Init Wi-Fi manager
    wifi_manager_init();

    // Init HTTP client layer
    http_client_init();

    // Create queue
    xMessageQueue = xQueueCreate(QUEUE_LENGTH, sizeof(message_t));
    if (xMessageQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create queue");
        return;
    }

    // Create tasks
    xTaskCreate(producer_task, "producer_task", 4096, NULL, 5, &xProducerTaskHandle);
    xTaskCreate(http_task, "http_task", 8192, NULL, 5, &xHttpTaskHandle);

    ESP_LOGI(TAG, "Tasks created");
}
