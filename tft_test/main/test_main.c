#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "display.h"
#include "wifi_app.h"
#include "sntp.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include <stdbool.h>

#define TAG "TimeSync"

#define UI_TASK_STACK_SIZE       12288
#define UI_TASK_PRIORITY         10
#define UI_TASK_CORE_ID          1      // core APP

#define TIME_SYNC_TASK_STACK_SIZE 8192
#define TIME_SYNC_TASK_PRIORITY 5
#define TIME_SYNC_TASK_CORE_ID 0
#define MAIN_TASK_STACK_SIZE 16384 
#define MAIN_TASK_PRIORITY 15 
#define MAIN_TASK_CORE_ID 0

void main_task(void *pvParam) {
    ESP_LOGI(TAG, "Main task started");
    ESP_LOGI(TAG, "Free heap before main task: %lu bytes", (unsigned long)esp_get_free_heap_size());
    wifi_connect();
    ESP_LOGI(TAG, "Free heap after Wi-Fi init: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Checking Wi-Fi connection status");
    if (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) {
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
    if (s_wifi_event_group != NULL) {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
        ESP_LOGI(TAG, "Wi-Fi event group deleted");
    }
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "Application started");
    ESP_LOGI(TAG, "Free heap before app_main: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Minimum free heap: %lu bytes", (unsigned long)esp_get_minimum_free_heap_size());

    init_display();

    // [ADDED] Tạo UI task trước để phản hồi nút ngay
    BaseType_t ret_ui = xTaskCreatePinnedToCore(
        ui_task, "ui_task",
        UI_TASK_STACK_SIZE, NULL,
        UI_TASK_PRIORITY, NULL, UI_TASK_CORE_ID
    );
    if (ret_ui != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI task: %d", ret_ui);
    }

    // Tạo main_task lo Wi-Fi + SNTP + spawn print_time_task
    BaseType_t ret = xTaskCreatePinnedToCore(
        main_task, "main_task",
        MAIN_TASK_STACK_SIZE, NULL,
        MAIN_TASK_PRIORITY, NULL, MAIN_TASK_CORE_ID
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create main_task: %d", ret);
    } else {
        ESP_LOGI(TAG, "main_task created");
    }
}