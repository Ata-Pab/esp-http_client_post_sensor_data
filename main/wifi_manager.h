#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void wifi_manager_init(void);
BaseType_t wifi_manager_wait_connected(TickType_t ticks_to_wait); // returns pdTRUE if connected
void wifi_manager_request_reconnect(void);
