#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"

#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK  GPIO_NUM_18
#define CS           GPIO_NUM_5
#define DC           GPIO_NUM_2
#define RST          GPIO_NUM_4

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

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes;
} lcd_init_cmd_t;

static const lcd_init_cmd_t lcd_init_cmds[] = {
    {0x00, {0}, 0},
    {0x01, {0}, 0x80},      
    {0x11, {0}, 0x80}, 
    {0xB1, {0x05, 0x3C, 0x3C}, 3},
    {0xB2, {0x05, 0x3C, 0x3C}, 3},
    {0xB3, {0x05, 0x3C, 0x3C, 0x05, 0x3C, 0x3C}, 6},
    {0xB4, {0x03}, 1},
    {0xC0, {0xA2, 0x02, 0x84}, 3},
    {0xC1, {0xC5}, 1},
    {0xC2, {0x0A, 0x00}, 2},
    {0xC3, {0x8A, 0x2A}, 2},
    {0xC4, {0x8A, 0xEE}, 2},
    {0xC5, {0x0E}, 1},
    {0x3A, {0x05}, 1},
    {0x36, {0xC0}, 1},  
    {0xE0, {0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28, 0x20, 0x22,
            0x1F, 0x1B, 0x23, 0x37, 0x00, 0x07, 0x02, 0x10}, 16},
    {0xE1, {0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C, 0x29, 0x2E,
            0x30, 0x30, 0x39, 0x3F, 0x00, 0x07, 0x03, 0x10}, 16},

    {0x29, {0}, 0x80}, 
    {0, {0}, 0xFF} 
};


void send_cmd (spi_device_handle_t spi, const uint8_t cmd){
	spi_transaction_t t;							
	memset(&t, 0, sizeof(t));						
	t.flags = SPI_TRANS_USE_TXDATA;					
	t.length = 8;									
	t.tx_data[0] = cmd;								
	gpio_set_level(DC, 0);							
    esp_err_t ret = spi_device_polling_transmit(spi, &t);
	assert(ret == ESP_OK);							
}

void send_data(spi_device_handle_t spi, const uint8_t *data, int length){
	spi_transaction_t t;							
	memset(&t, 0, sizeof(t)); 	         			
	t.length = 8*length;							
	t.tx_buffer = data;								
	gpio_set_level(DC, 1);							
    esp_err_t ret = spi_device_polling_transmit(spi, &t); 
	assert(ret == ESP_OK);							
}

void lcd_Init (spi_device_handle_t spi){
	int cmd = 0;
	while (lcd_init_cmds[cmd].databytes!=0xff) {	
			send_cmd(spi, lcd_init_cmds[cmd].cmd);  
	        if (lcd_init_cmds[cmd].databytes == 0) { 
		        vTaskDelay(100 / portTICK_RATE_MS);}
	        else{
	        send_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes); 
	        }
	        cmd++;
	    }
}

void draw_pixel(spi_device_handle_t spi, int x, int y, uint16_t color) {
    uint8_t caset[] = {0x00, x, 0x00, x};
    uint8_t raset[] = {0x00, y, 0x00, y};
    uint8_t color_bytes[] = {color >> 8, color & 0xFF};

    send_cmd(spi, 0x2A); send_data(spi, caset, 4);
    send_cmd(spi, 0x2B); send_data(spi, raset, 4);
    send_cmd(spi, 0x2C); send_data(spi, color_bytes, 2);
}

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

void draw_string(spi_device_handle_t spi, const char *str, int x, int y, uint16_t color) {
    while (*str) {
        draw_char(spi, *str, x, y, color);
        x += 6;
        str++;
    }
}

void app_main(void) {
    gpio_set_direction (DC, GPIO_MODE_OUTPUT);  
	gpio_set_direction (RST, GPIO_MODE_OUTPUT); 

    esp_err_t ret;  							
    spi_device_handle_t spi;					
    spi_bus_config_t Busconfig={				
        .mosi_io_num=PIN_NUM_MOSI,				
        .sclk_io_num=PIN_NUM_CLK,				
        .quadwp_io_num=-1,						
        .quadhd_io_num=-1,						
        .max_transfer_sz=2*128*160				
    };

    spi_device_interface_config_t DeviceConfig={ 
    	.command_bits = 0,						 
		.address_bits = 0,						 
		.dummy_bits = 0,						 
		.mode = 0,								 
		.clock_speed_hz = 8000000,				 
		.spics_io_num = CS						 
    };
    gpio_set_level(RST, 0); //Reset LCD
    vTaskDelay(100);
    gpio_set_level(RST, 1);
    ret = spi_bus_initialize (SPI3_HOST, &Busconfig, SPI_DMA_CH_AUTO); 
    assert(ret == ESP_OK);
    ret = spi_bus_add_device(SPI3_HOST, &DeviceConfig, &spi);		   
    assert(ret == ESP_OK);											   
    lcd_Init(spi);

    uint8_t caset[] = {0x00, 0x00, 0x00, 127};
    uint8_t raset[] = {0x00, 0x00, 0x00, 159};
    send_cmd(spi, 0x2A); send_data(spi, caset, 4);
    send_cmd(spi, 0x2B); send_data(spi, raset, 4);
    send_cmd(spi, 0x2C);
    uint8_t black[2] = {0x00, 0x00};
    for (int i = 0; i < 128 * 160; i++) {
        send_data(spi, black, 2);
    }

    draw_string(spi, "HELLO", 10, 10, 0xFFFF); 
    draw_string(spi, "WORLD", 10, 30, 0xFFFF); 
}
