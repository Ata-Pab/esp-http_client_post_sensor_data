#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void http_client_init(void);
esp_err_t http_client_post_json(const char *url, const char *json, TickType_t timeout_ticks);
