#pragma once
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;
extern const int WIFI_FAIL_BIT;

void wifi_init_sta(void);
