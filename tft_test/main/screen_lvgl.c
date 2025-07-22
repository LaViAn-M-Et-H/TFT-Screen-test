#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "time.h"

/* LVGL includes */
#include "lvgl.h"
#include "lvgl_helpers.h"

/* Pin Definitions */
#define PIN_NUM_MISO   -1
#define PIN_NUM_MOSI   23
#define PIN_NUM_CLK    18
#define PIN_NUM_CS     5
#define PIN_NUM_DC     2
#define PIN_NUM_RST    4
#define PIN_NUM_BL     15

/* Screen Dimensions */
#define TFT_WIDTH   128
#define TFT_HEIGHT  160

static const char *TAG = "LVGL_Demo";

void initialize_sntp() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void lv_tick_task(void *arg) {
    (void) arg;
    lv_tick_inc(10); // Update LVGL internal tick every 10ms
}

static void update_time_task(lv_obj_t *label) {
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        char time_str[16];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        
        lv_label_set_text(label, time_str);
        lv_task_handler(); // Call LVGL task handler
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SNTP for time synchronization
    initialize_sntp();

    // Initialize LVGL
    lv_init();

    // Initialize display and input devices
    lvgl_driver_init();

    // Configure display resolution
    static lv_disp_buf_t disp_buf;
    static lv_color_t buf1[TFT_WIDTH * 10];
    static lv_color_t buf2[TFT_WIDTH * 10];
    lv_disp_buf_init(&disp_buf, buf1, buf2, TFT_WIDTH * 10);

    // Initialize the display
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // Create a simple label style
    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_text_color(&style, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_text_font(&style, LV_STATE_DEFAULT, &lv_font_montserrat_16);

    // Create a label for the title
    lv_obj_t *title_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(title_label, "Lich nhac!");
    lv_obj_align(title_label, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    lv_obj_add_style(title_label, LV_LABEL_PART_MAIN, &style);

    // Create a label for the reminder
    lv_obj_t *reminder_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(reminder_label, "Uong thuoc luc:");
    lv_obj_align(reminder_label, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 20);
    lv_obj_add_style(reminder_label, LV_LABEL_PART_MAIN, &style);

    // Create a label for the time display
    lv_obj_t *time_label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(time_label, "00:00:00");
    lv_obj_align(time_label, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 40);
    lv_obj_add_style(time_label, LV_LABEL_PART_MAIN, &style);

    // Create a task to update the time
    xTaskCreate(update_time_task, "update_time", 2048, time_label, 5, NULL);

    // Create and start a tick task for LVGL
    xTaskCreate(lv_tick_task, "lv_tick", 2048, NULL, 5, NULL);

    // Run LVGL task handler periodically
    while (1) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        lv_task_handler();
    }
}
