#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Pin Definitions
#define PIN_NUM_MISO   -1  // Not used (ST7735 is write-only)
#define PIN_NUM_MOSI   9   // SPI MOSI (Data)
#define PIN_NUM_CLK    10  // SPI Clock
#define PIN_NUM_CS     11  // Chip Select
#define PIN_NUM_DC     8   // Data/Command
#define PIN_NUM_RST    18  // Reset
#define PIN_NUM_BL     17  // Backlight control

// Screen Dimensions
#define TFT_WIDTH   128
#define TFT_HEIGHT  160
#define OFFSET_X    2   // Offset ngang
#define OFFSET_Y    1   // Offset dọc

// ST7735 Commands
#define ST7735_NOP      0x00
#define ST7735_SWRESET  0x01
#define ST7735_SLPOUT   0x11
#define ST7735_COLMOD   0x3A
#define ST7735_MADCTL   0x36
#define ST7735_CASET    0x2A
#define ST7735_RASET    0x2B
#define ST7735_RAMWR    0x2C
#define ST7735_DISPON   0x29

// Colors (RGB565)
#define COLOR_RED    0xF800
#define COLOR_GREEN  0x07E0
#define COLOR_BLUE   0x001F
#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xFFFF

static const char *TAG = "TFT";
static spi_device_handle_t spi;

// Gửi lệnh
void send_cmd(uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .flags = 0
    };
    gpio_set_level(PIN_NUM_DC, 0); // Command mode
    ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write command 0x%02X failed: %s", cmd, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Write command 0x%02X: OK", cmd);
    }
}

// Gửi dữ liệu
void send_data(uint8_t *data, uint16_t len) {
    if (len == 0) return;
    esp_err_t ret;
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .flags = 0
    };
    gpio_set_level(PIN_NUM_DC, 1); // Data mode
    ret = spi_device_polling_transmit(spi, &t);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write data (%d bytes) failed: %s", len, esp_err_to_name(ret));
    }
}

// Đặt vùng địa chỉ
void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];
    send_cmd(ST7735_CASET);
    data[0] = (x0 + OFFSET_X) >> 8; data[1] = (x0 + OFFSET_X) & 0xFF;
    data[2] = (x1 + OFFSET_X) >> 8; data[3] = (x1 + OFFSET_X) & 0xFF;
    send_data(data, 4);

    send_cmd(ST7735_RASET);
    data[0] = (y0 + OFFSET_Y) >> 8; data[1] = (y0 + OFFSET_Y) & 0xFF;
    data[2] = (y1 + OFFSET_Y) >> 8; data[3] = (y1 + OFFSET_Y) & 0xFF;
    send_data(data, 4);

    send_cmd(ST7735_RAMWR);
}

// Xóa màn hình (tối ưu để tránh WDT)
void fill_screen(uint16_t color) {
    uint8_t data[2] = {color >> 8, color & 0xFF};
    set_addr_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    for (int y = 0; y < TFT_HEIGHT; y++) {
        for (int x = 0; x < TFT_WIDTH; x++) {
            send_data(data, 2);
        }
        vTaskDelay(1 / portTICK_PERIOD_MS); // Nhường CPU để tránh WDT
    }
    ESP_LOGI(TAG, "Screen filled with color 0x%04X", color);
}

// Vẽ pixel
void draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    uint8_t data[2] = {color >> 8, color & 0xFF};
    set_addr_window(x, y, x, y);
    send_data(data, 2);
}

// Font 5x7 cho các ký tự trong "hello world"
const uint8_t font5x7[][5] = {
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}  // Z
};

// Vẽ ký tự
void draw_char(spi_device_handle_t spi, char c, int x, int y, uint16_t color) {
    if (c < 'A' || c > 'Z') return;
    const uint8_t *bitmap = font5x7[c - 'A'];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (bitmap[col] & (1 << row)) {
                draw_pixel(spi, x + col, y + row, color);
            }
        }
    }
}

// Vẽ chuỗi văn bản
void draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg_color, uint16_t bg_color) {
    int orig_x = x;
    while (*str) {
        draw_char(x, y, *str, fg_color, bg_color);
        x += 6; // Chiều rộng ký tự + khoảng cách
        str++;
    }
    ESP_LOGI(TAG, "Drew string at (%d, %d): %s", orig_x, y, str);
}

// Kiểm tra GPIO
void test_gpio() {
    ESP_LOGI(TAG, "Testing GPIO signals");
    for (int i = 0; i < 3; i++) {
        gpio_set_level(PIN_NUM_DC, 1);
        gpio_set_level(PIN_NUM_RST, 1);
        ESP_LOGI(TAG, "DC and RST set to HIGH");
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(PIN_NUM_DC, 0);
        gpio_set_level(PIN_NUM_RST, 0);
        ESP_LOGI(TAG, "DC and RST set to LOW");
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

// Khởi tạo SPI
void init_spi() {
    ESP_LOGI(TAG, "Initializing SPI");

    // Giải phóng SPI bus
    esp_err_t ret = spi_bus_free(SPI2_HOST);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPI bus freed successfully");
    } else {
        ESP_LOGW(TAG, "SPI bus free failed: %s", esp_err_to_name(ret));
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2 + 8
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000, // 4 MHz
        .mode = 0,                         // SPI Mode 0
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 10,                  // Tăng queue_size
        .flags = 0,                        // Full-duplex
        .pre_cb = NULL,
        .post_cb = NULL
    };

    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPI bus init: OK");
    }
    ESP_ERROR_CHECK(ret);

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPI device add: OK");
    }
    ESP_ERROR_CHECK(ret);
}

// Khởi tạo màn hình
void init_display() {
    // Cấu hình GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "GPIO config: OK");
    }
    ESP_ERROR_CHECK(ret);

    // Đặt backlight ở mức HIGH
    gpio_set_level(PIN_NUM_BL, 1);
    ESP_LOGI(TAG, "Backlight set to HIGH (GPIO %d)", PIN_NUM_BL);

    // Kiểm tra GPIO DC và RST
    test_gpio();

    // Reset cứng
    ESP_LOGI(TAG, "Resetting display (GPIO %d)", PIN_NUM_RST);
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(120 / portTICK_PERIOD_MS);

    // Khởi tạo SPI
    init_spi();

    // Chuỗi khởi tạo ST7735S/R
    send_cmd(ST7735_NOP);
    send_cmd(ST7735_SWRESET);
    vTaskDelay(150 / portTICK_PERIOD_MS);

    send_cmd(ST7735_SLPOUT);
    vTaskDelay(120 / portTICK_PERIOD_MS);

    // Frame Rate Control (ST7735S)
    send_cmd(0xB1);
    send_data((uint8_t[]){0x05, 0x3C, 0x3C}, 3);
    send_cmd(0xB2);
    send_data((uint8_t[]){0x05, 0x3C, 0x3C}, 3);
    send_cmd(0xB3);
    send_data((uint8_t[]){0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C}, 6);

    // Inverter Control
    send_cmd(0xB4);
    send_data((uint8_t[]){0x03}, 1);

    // Power Control
    send_cmd(0xC0);
    send_data((uint8_t[]){0xA2, 0x02, 0x84}, 3);
    send_cmd(0xC1);
    send_data((uint8_t[]){0xC5}, 1);
    send_cmd(0xC2);
    send_data((uint8_t[]){0x0A, 0x00}, 2);
    send_cmd(0xC3);
    send_data((uint8_t[]){0x8A, 0x2A}, 2);
    send_cmd(0xC4);
    send_data((uint8_t[]){0x8A, 0xEE}, 2);
    send_cmd(0xC5);
    send_data((uint8_t[]){0x0E}, 1);

    // Cấu hình định dạng màu (RGB565)
    send_cmd(ST7735_COLMOD);
    send_data((uint8_t[]){0x05}, 1);

    // Cấu hình hướng màn hình
    send_cmd(ST7735_MADCTL);
    send_data((uint8_t[]){0xC0}, 1); // RGB, xoay khác (thử 0x08, 0x48, 0x88 nếu cần)

    // Gamma Correction
    send_cmd(0xE0);
    send_data((uint8_t[]){0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22,
                          0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10}, 16);
    send_cmd(0xE1);
    send_data((uint8_t[]){0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E,
                          0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10}, 16);

    // Bật hiển thị
    send_cmd(ST7735_DISPON);
    vTaskDelay(10 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Display initialized successfully");
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting application");
    init_display();

    // Test màu để kiểm tra màn hình
    ESP_LOGI(TAG, "Testing colors");
    fill_screen(COLOR_RED); // Đỏ
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    fill_screen(COLOR_GREEN); // Xanh
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    fill_screen(COLOR_BLUE); // Xanh dương
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    fill_screen(COLOR_BLACK); // Đen

    // Hiển thị "hello world"
    draw_string(10, 10, "hello world", COLOR_WHITE, COLOR_BLACK);

    // Giữ chương trình chạy
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
