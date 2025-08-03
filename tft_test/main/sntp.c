#define TAG "TimeSync"

void time_sync_notification_cb(struct timeval *tv) {
    if (tv) {
        ESP_LOGI(TAG, "Time synchronized");
    } else {
        ESP_LOGE(TAG, "SNTP callback: Invalid timeval");
    }
}

void initialize_sntp(void) {
    ESP_LOGI(TAG, "Initializing SNTP with servers: time.nist.gov, ntp.ubuntu.com, pool.ntp.org");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.nist.gov");
    esp_sntp_setservername(1, "ntp.ubuntu.com");
    esp_sntp_setservername(2, "pool.ntp.org");
    esp_sntp_set_sync_interval(15000);
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP initialized successfully");
}

void obtain_time(void) {
    ESP_LOGI(TAG, "Starting SNTP sync...");
    if (!(xEventGroupGetBits(wifi_event_group) & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "Wi-Fi not connected, cannot sync SNTP");
        return;
    }
    ESP_LOGI(TAG, "Free heap before SNTP init: %lu bytes", (unsigned long)esp_get_free_heap_size());
    initialize_sntp();
    setenv("TZ", "ICT-7", 1);
    tzset();
    ESP_LOGI(TAG, "Timezone set to ICT-7");
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    int retry = 60;
    const int retry_count = 60;
    while (retry-- > 0) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year > (2016 - 1900)) {
            strftime(strftime_buf, sizeof(strftime_buf), "%d.%m.%Y %H:%M:%S", &timeinfo);
            ESP_LOGI(TAG, "Time synchronized: %s", strftime_buf);
            return;
        }
        ESP_LOGI(TAG, "Waiting for SNTP sync, attempt %d/%d", retry_count - retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    ESP_LOGE(TAG, "SNTP sync failed after %d attempts", retry_count);
}

typedef struct {
    int hour;
    int minute;
    const char *message;
} Reminder;

Reminder reminders[] = {
    {8, 0, "UONG THUOC SANG"},
    {12, 30, "UONG THUOC TRUA"},
    {20, 0, "UONG THUOC TOI"}
};

const int num_reminders = sizeof(reminders) / sizeof(reminders[0]);

void print_time_task(void *pvParam) {
    ESP_LOGI(TAG, "Starting reminder task");
    int last_checked_minute = -1;

    while (1) {
        time_t now;
        struct tm timeinfo;
        char time_buf[64];

        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year < (2016 - 1900)) {
            ESP_LOGI(TAG, "CHUA DONG BO THOI GIAN");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        if (timeinfo.tm_min != last_checked_minute) {
            last_checked_minute = timeinfo.tm_min;

            for (int i = 0; i < num_reminders; i++) {
                if (timeinfo.tm_hour == reminders[i].hour && timeinfo.tm_min == reminders[i].minute) {
                    strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
                    ESP_LOGI(TAG, "REMINDER %s: %s", time_buf, reminders[i].message);

                    fill_screen(COLOR_BLACK);
                    draw_string(10, 10, "NHAC NHO:", COLOR_GREEN);
                    draw_string(10, 40, time_buf, COLOR_WHITE);
                    draw_string(10, 70, reminders[i].message, COLOR_WHITE);
                    break;
                }
            }
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS); 
    }
}

