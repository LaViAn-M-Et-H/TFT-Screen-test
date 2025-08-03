#pragma once
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

void send_cmd(uint8_t cmd);
void send_data(uint8_t *data, uint16_t len);
void set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void fill_screen(uint16_t color);
void draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void draw_char(char c, int x, int y, uint16_t color);
void draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color);
void init_spi(void)
void test_gpio(void)
void init_display(void);