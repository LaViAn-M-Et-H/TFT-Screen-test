#define TAG "TimeSync"
#define WIFI_SSID "dlacm"
#define WIFI_PASS "xiaomi111"
#define MAX_RETRY 10

static EventGroupHandle_t wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;
static int retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START: Connecting to AP '%s'...", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        const char* reason_str;
        switch (event->reason) {
            case WIFI_REASON_AUTH_EXPIRE: reason_str = "Authentication expired"; break;
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: reason_str = "4-way handshake timeout"; break;
            case WIFI_REASON_NO_AP_FOUND: reason_str = "AP not found"; break;
            case WIFI_REASON_AUTH_FAIL: reason_str = "Authentication failed (wrong password?)"; break;
            case WIFI_REASON_ASSOC_FAIL: reason_str = "Association failed"; break;
            default: reason_str = "Other reason"; break;
        }
        ESP_LOGE(TAG, "WIFI_EVENT_STA_DISCONNECTED: reason: %d (%s)", event->reason, reason_str);
        if (retry_num < MAX_RETRY) {
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP (attempt %d/%d)", retry_num, MAX_RETRY);
        } else {
            ESP_LOGI(TAG, "Setting WIFI_FAIL_BIT");
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Failed to connect to AP after %d attempts", MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP: Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        ESP_LOGI(TAG, "Setting WIFI_CONNECTED_BIT");
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Free heap in IP event: %lu bytes", (unsigned long)esp_get_free_heap_size());
    }
}

void wifi_init_sta(void) {
    ESP_LOGI(TAG, "Initializing Wi-Fi...");
    ESP_LOGI(TAG, "Free heap before Wi-Fi init: %lu bytes", (unsigned long)esp_get_free_heap_size());
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi event group created");
    esp_err_t ret = nvs_flash_erase();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "NVS erased successfully");
    }
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "NVS initialized");
    ret = esp_netif_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize netif: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Netif initialized");
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Event loop created");
    esp_netif_create_default_wifi_sta();
    ESP_LOGI(TAG, "Default Wi-Fi STA created");
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi initialized");
    ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Wi-Fi event handler: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi event handler registered");
    ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "IP event handler registered");
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Wi-Fi mode: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi mode set");
    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Wi-Fi config: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi config set");
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "Wi-Fi started");
    ESP_LOGI(TAG, "Waiting for connection...");
    ESP_LOGI(TAG, "Before xEventGroupWaitBits, free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdTRUE, 10000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "xEventGroupWaitBits returned, bits: 0x%x", (unsigned int)bits);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully");
        vTaskDelay(3000 / portTICK_PERIOD_MS); // Wait for network stability
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection failed or timed out");
        esp_wifi_stop();
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler);
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler);
        esp_wifi_deinit();
        if (wifi_event_group != NULL) {
            vEventGroupDelete(wifi_event_group);
            wifi_event_group = NULL;
        }
        return;
    }
    ESP_LOGI(TAG, "Free heap after Wi-Fi init: %lu bytes", (unsigned long)esp_get_free_heap_size());
}