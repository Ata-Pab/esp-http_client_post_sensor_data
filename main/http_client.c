#include <string.h>
#include <stdio.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "http_client.h"

static const char *TAG = "http_client";

void http_client_init(void)
{
    // Nothing heavy to initialize for plain HTTP; keep function for symmetry and future TLS init.
    ESP_LOGI(TAG, "HTTP client initialized");
}

esp_err_t http_client_post_json(const char *url, const char *json, TickType_t timeout_ticks)
{
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = (int)(portTICK_PERIOD_MS * timeout_ticks),
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err;
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));

    err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d", status, len);
    }
    else
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}
