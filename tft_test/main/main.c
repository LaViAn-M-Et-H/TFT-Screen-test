#include <stdio.h>
#include "display.h"
#include "wifi.h"
#include "sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "TimeSync"

#define TIME_SYNC_TASK_STACK_SIZE 8192
#define TIME_SYNC_TASK_PRIORITY 5
#define TIME_SYNC_TASK_CORE_ID 0
#define MAIN_TASK_STACK_SIZE 16384 
#define MAIN_TASK_PRIORITY 15 
#define MAIN_TASK_CORE_ID 0

void main_task(void *pvParam) {
    ESP_LOGI(TAG, "Main task started");
    ESP_LOGI(TAG, "Free heap before main task: %lu bytes", (unsigned long)esp_get_free_heap_size());
    wifi_init_sta();
    ESP_LOGI(TAG, "Free heap after Wi-Fi init: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Checking Wi-Fi connection status");
    if (xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi is connected, proceeding to SNTP");
        ESP_LOGI(TAG, "Calling obtain_time");
        obtain_time();
        ESP_LOGI(TAG, "Free heap after obtain_time: %lu bytes", (unsigned long)esp_get_free_heap_size());
        ESP_LOGI(TAG, "Creating print time task");
        BaseType_t ret = xTaskCreatePinnedToCore(print_time_task, "print_time", TIME_SYNC_TASK_STACK_SIZE, NULL, TIME_SYNC_TASK_PRIORITY, NULL, TIME_SYNC_TASK_CORE_ID);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create print time task: %d", ret);
        } else {
            ESP_LOGI(TAG, "Print time task created successfully");
        }
    } else {
        ESP_LOGE(TAG, "Wi-Fi not connected, skipping SNTP and task creation");
    }
    ESP_LOGI(TAG, "Main task completed");
    if (wifi_event_group != NULL) {
        vEventGroupDelete(wifi_event_group);
        wifi_event_group = NULL;
        ESP_LOGI(TAG, "Wi-Fi event group deleted");
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Application started");
    ESP_LOGI(TAG, "Free heap before app_main: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Minimum free heap: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());

    init_display();

    BaseType_t ret = xTaskCreatePinnedToCore(main_task, "main_task", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, NULL, MAIN_TASK_CORE_ID);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create main task: %d", ret);
    } else {
        ESP_LOGI(TAG, "Main task created successfully");
    }
    ESP_LOGI(TAG, "app_main completed");
    ESP_LOGI(TAG, "Free heap after app_main: %lu bytes", (unsigned long)esp_get_free_heap_size());
}