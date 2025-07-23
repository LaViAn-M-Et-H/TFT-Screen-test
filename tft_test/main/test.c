#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK GPIO_NUM_18
#define CS GPIO_NUM_5
#define DC GPIO_NUM_2
#define RST GPIO_NUM_4

typedef struct {
	uint8_t cmd;
	uint8_t data[16];
	uint8_t databytes;
}lcd_init_cmd_t;

uint8_t data [] = { //hình ảnh
};

static const lcd_init_cmd_t lcd_init_cmds [] = {
		{0x01, {0}, 0},         		// Software reset, 0 argument
		{0x11, {0}, 0},                 // Out of sleep mode, 0 argument
		{0x36, {0x00}, 1},				// Memory Data Access Control (MY, MX, MV, ML, RGB, MH)
		{0x3A, {0x05}, 1},              // Set color mode, 1 arg, no delay: 16-bit color 565 RGB, 2bytes/pixel
		{0x13, {0}, 1},       			// Normal display on, no argument
		{0x29, {0}, 1},       			// Main screen turn on, no argument
		{0, {0}, 0xff}					// End Init Flag
};

void send_cmd (spi_device_handle_t spi, const uint8_t cmd){
	spi_transaction_t t;							//create a spi transaction
	memset(&t, 0, sizeof(t));						//Zero out of the transaction
	t.flags = SPI_TRANS_USE_TXDATA;					//Use SPI_TRANS_USE_TXDATA Flag -> Transfer size < 4 bytes
	t.length = 8;									//Total Data length in bit
	t.tx_data[0] = cmd;								//Data need to be sent
	gpio_set_level(DC, 0);							//DC low - Send command / DC high - Send Data/Parameter
    esp_err_t ret = spi_device_polling_transmit(spi, &t); //Transmit data
	assert(ret == ESP_OK);							//Check no issues
}

void send_data(spi_device_handle_t spi, const uint8_t *data, int length){
	spi_transaction_t t;							//create a spi transaction
	memset(&t, 0, sizeof(t)); 	         			//Zero out of the transaction
	t.length = 8*length;							//Total Data length in bit
	t.tx_buffer = data;								//Data need to be sent
	gpio_set_level(DC, 1);							//DC low - Send command / DC high - Send Data/Parameter
    esp_err_t ret = spi_device_polling_transmit(spi, &t); //Transmit data
	assert(ret == ESP_OK);							//Check no issues
}

void lcd_Init (spi_device_handle_t spi){
    //Initial parameter
	int cmd = 0;
	while (lcd_init_cmds[cmd].databytes!=0xff) {	//if .databyte = 0xff, end lcd_Init
			send_cmd(spi, lcd_init_cmds[cmd].cmd);  //Send command first

	        if (lcd_init_cmds[cmd].databytes == 0) { //Command without parameter
		        vTaskDelay(100 / portTICK_RATE_MS);}
	        else{
	        send_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes); //Send parameter
	        }
	        cmd++; // next command
	    }
}

void app_main(void)
{
	gpio_set_direction (DC, GPIO_MODE_OUTPUT);  //Set pin output mode - RS(D/C)  GPIO2
	gpio_set_direction (RST, GPIO_MODE_OUTPUT); //Set pin output mode - Reset - GPIO 4

    esp_err_t ret;  							//esp error check
    spi_device_handle_t spi;					//create a spi device handle variable
    spi_bus_config_t Busconfig={				//Configuration for spi bus
        .mosi_io_num=PIN_NUM_MOSI,				//Master out Slave in - VSPI - GPI 23
        .sclk_io_num=PIN_NUM_CLK,				//Clock - VSPI - GPIO 18
        .quadwp_io_num=-1,						//not used
        .quadhd_io_num=-1,						//not used
        .max_transfer_sz=2*128*160				//Max tranfer data size (byte): Use display resolution 128*160 pixel + 2 bytes/pixel
    };

    spi_device_interface_config_t DeviceConfig={ //Config Device for SPI bus
    	.command_bits = 0,						 //No command bit (see in LCD manual)
		.address_bits = 0,						 //No address bit (see in LCD manual)
		.dummy_bits = 0,						 //No dummy bit (see in LCD manual)
		.mode = 0,								 //CPOL, CPHA
		.clock_speed_hz = 8000000,				 //Frequently
		.spics_io_num = CS						 //Chip Select for this device - GPIO 5
    };
    gpio_set_level(RST, 0); //Reset LCD
    vTaskDelay(100);
    gpio_set_level(RST, 1);
    ret = spi_bus_initialize (SPI3_HOST, &Busconfig, SPI_DMA_CH_AUTO); //SPI bus initialize - VSPI = SPI3_Host, Flag: SPI_DMA_DISABLED - Max transfer < 64 bytes
    assert(ret == ESP_OK);
    ret = spi_bus_add_device(SPI3_HOST, &DeviceConfig, &spi);		   //spi add device
    assert(ret == ESP_OK);											   //Initial step for LCD (Reset, Out of sleep mode, data access control, nornal mode on, turn on screen
    lcd_Init(spi);
    uint8_t caset[] = {0, 0, 0, 127};						//width 128 pixels Column address set
    uint8_t raset[] = {0, 0, 0, 159};						        //heigth 160 pixels Row address set
    send_cmd(spi, 0x2A);
    send_data(spi, caset, 4);
    send_cmd(spi, 0x2B);
    send_data(spi, raset, 4);
    vTaskDelay(100);
    uint8_t *data_buffer = heap_caps_malloc(128*2*160, MALLOC_CAP_DMA); //Dynamic allocate capacital with size 2byte/pixel
    for (int j = 0; j < 128*2*160; j++)
    	{
    		data_buffer[j] = data[j];									//Add data to data_buffer
    	}
    send_cmd(spi, 0x2C);												//Write memory command
    send_data(spi, data_buffer, 128*2*160);								//Transfer data_buffer to lcd

}
