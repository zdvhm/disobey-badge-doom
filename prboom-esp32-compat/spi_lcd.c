// Copyright 2016-2017 Espressif Systems (Shanghai) PTE LTD
// Adapted for Disobey Badge 2025/2026 (ESP32-S3, ST7789 320x170)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"

#include "sdkconfig.h"

// Disobey Badge 2025/2026 LCD GPIO pins
#ifndef CONFIG_HW_LCD_MOSI_GPIO
#define CONFIG_HW_LCD_MOSI_GPIO 5
#endif
#ifndef CONFIG_HW_LCD_CLK_GPIO
#define CONFIG_HW_LCD_CLK_GPIO  4
#endif
#ifndef CONFIG_HW_LCD_CS_GPIO
#define CONFIG_HW_LCD_CS_GPIO   6
#endif
#ifndef CONFIG_HW_LCD_DC_GPIO
#define CONFIG_HW_LCD_DC_GPIO   15
#endif
#ifndef CONFIG_HW_LCD_RESET_GPIO
#define CONFIG_HW_LCD_RESET_GPIO 7
#endif
#ifndef CONFIG_HW_LCD_BL_GPIO
#define CONFIG_HW_LCD_BL_GPIO   19
#endif

// Badge display: ST7789 320x170 (1.9" landscape)
// ST7789 controller is 240x320. Physical display is 170x320.
// In landscape mode (MADCTL MV+MX): width=320, height=170
// Page (Y) offset = (240-170)/2 = 35
#define LCD_WIDTH   320
#define LCD_HEIGHT  170
#define LCD_OFFSET_X 0
#define LCD_OFFSET_Y 35

#define PIN_NUM_MOSI CONFIG_HW_LCD_MOSI_GPIO
#define PIN_NUM_CLK  CONFIG_HW_LCD_CLK_GPIO
#define PIN_NUM_CS   CONFIG_HW_LCD_CS_GPIO
#define PIN_NUM_DC   CONFIG_HW_LCD_DC_GPIO
#define PIN_NUM_RST  CONFIG_HW_LCD_RESET_GPIO
#define PIN_NUM_BCKL CONFIG_HW_LCD_BL_GPIO

// Double buffer for smooth rendering
#define DOUBLE_BUFFER

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; // bit 7 = delay after set; 0xFF = end of cmds.
} ili_init_cmd_t;

// ST7789V initialization for 170x320 display (Disobey Badge 2025/2026)
static const ili_init_cmd_t ili_init_cmds[]={
    // Software reset
    {0x01, {0}, 0x80},
    // Sleep out
    {0x11, {0}, 0x80},
    // Memory Data Access Control - landscape mode
    // 0x60 = MX=1, MV=1 -> Landscape (rotate 90° clockwise)
    {0x36, {0x60}, 1},
    // Interface Pixel Format: 16bit/pixel RGB565
    {0x3A, {0x55}, 1},
    // Porch Setting
    {0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33}, 5},
    // Gate Control
    {0xB7, {0x35}, 1},
    // VCOM Setting
    {0xBB, {0x19}, 1},
    // LCM Control
    {0xC0, {0x2C}, 1},
    // VDV and VRH Command Enable
    {0xC2, {0x01}, 1},
    // VRH Set
    {0xC3, {0x12}, 1},
    // VDV Set
    {0xC4, {0x20}, 1},
    // Frame Rate Control in Normal Mode (60Hz)
    {0xC6, {0x0F}, 1},
    // Power Control 1
    {0xD0, {0xA4, 0xA1}, 2},
    // Positive Voltage Gamma Control
    {0xE0, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}, 14},
    // Negative Voltage Gamma Control
    {0xE1, {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}, 14},
    // Display Inversion On (ST7789 needs this for correct colors)
    {0x21, {0}, 0},
    // Display on
    {0x29, {0}, 0x80},
    {0, {0}, 0xff}
};

static spi_device_handle_t spi;

void ili_cmd(spi_device_handle_t spi, const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length=8;
    t.tx_buffer=&cmd;
    t.user=(void*)0;
    ret=spi_device_transmit(spi, &t);
    assert(ret==ESP_OK);
}

void ili_data(spi_device_handle_t spi, const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;
    memset(&t, 0, sizeof(t));
    t.length=len*8;
    t.tx_buffer=data;
    t.user=(void*)1;
    ret=spi_device_transmit(spi, &t);
    assert(ret==ESP_OK);
}

void ili_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

void ili_init(spi_device_handle_t spi)
{
    int cmd=0;
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);

    // Enable backlight
    if (PIN_NUM_BCKL >= 0) {
        gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
        gpio_set_level(PIN_NUM_BCKL, 1);
    }

    // Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    // Send all initialization commands
    while (ili_init_cmds[cmd].databytes!=0xff) {
        uint8_t dmdata[16];
        ili_cmd(spi, ili_init_cmds[cmd].cmd);
        memcpy(dmdata, ili_init_cmds[cmd].data, 16);
        ili_data(spi, dmdata, ili_init_cmds[cmd].databytes&0x1F);
        if (ili_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        cmd++;
    }
}


static void send_header_start(spi_device_handle_t spi, int xpos, int ypos, int w, int h)
{
    esp_err_t ret;
    int x;
    static spi_transaction_t trans[5];

    for (x=0; x<5; x++) {
        memset(&trans[x], 0, sizeof(spi_transaction_t));
        if ((x&1)==0) {
            trans[x].length=8;
            trans[x].user=(void*)0;
        } else {
            trans[x].length=8*4;
            trans[x].user=(void*)1;
        }
        trans[x].flags=SPI_TRANS_USE_TXDATA;
    }
    // Apply display offsets for ST7789 170x320 panel
    int col_start = LCD_OFFSET_X + xpos;
    int col_end = LCD_OFFSET_X + xpos + w - 1;
    int row_start = LCD_OFFSET_Y + ypos;
    int row_end = LCD_OFFSET_Y + ypos + h - 1;

    trans[0].tx_data[0]=0x2A;           //Column Address Set
    trans[1].tx_data[0]=col_start>>8;
    trans[1].tx_data[1]=col_start&0xff;
    trans[1].tx_data[2]=col_end>>8;
    trans[1].tx_data[3]=col_end&0xff;
    trans[2].tx_data[0]=0x2B;           //Page address set
    trans[3].tx_data[0]=row_start>>8;
    trans[3].tx_data[1]=row_start&0xff;
    trans[3].tx_data[2]=row_end>>8;
    trans[3].tx_data[3]=row_end&0xff;
    trans[4].tx_data[0]=0x2C;           //memory write

    for (x=0; x<5; x++) {
        ret=spi_device_queue_trans(spi, &trans[x], portMAX_DELAY);
        assert(ret==ESP_OK);
    }
}


void send_header_cleanup(spi_device_handle_t spi)
{
    spi_transaction_t *rtrans;
    esp_err_t ret;
    for (int x=0; x<5; x++) {
        ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret==ESP_OK);
    }
}


#ifndef DOUBLE_BUFFER
volatile static uint16_t *currFbPtr=NULL;
#else
static uint32_t *currFbPtr=NULL;
#endif
SemaphoreHandle_t dispSem = NULL;
SemaphoreHandle_t dispDoneSem = NULL;

#define NO_SIM_TRANS 5
#define MEM_PER_TRANS (LCD_WIDTH*2) // 2 rows per transfer = 640 pixels

// DOOM renders at 320x200. Badge display is 320x170.
// Crop 15 rows from top and 15 from bottom.
#define DOOM_WIDTH 320
#define DOOM_HEIGHT 200
#define CROP_TOP 15

extern int16_t lcdpal[256];

void IRAM_ATTR displayTask(void *arg) {
	int x, i;
	int idx=0;
	int inProgress=0;
	static uint16_t *dmamem[NO_SIM_TRANS];
	spi_transaction_t trans[NO_SIM_TRANS];
	spi_transaction_t *rtrans;

    esp_err_t ret;
    spi_bus_config_t buscfg={
        .miso_io_num=-1,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=(MEM_PER_TRANS*2)+16
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=40000000,               // 40MHz SPI clock
        .mode=0,                                // SPI mode 0
        .spics_io_num=PIN_NUM_CS,               // CS pin
        .queue_size=NO_SIM_TRANS,               // Queue depth
        .pre_cb=ili_spi_pre_transfer_callback,  // D/C line callback
    };

	printf("*** Display task starting.\n");

    // ESP32-S3: Use SPI2_HOST with auto DMA channel
    ret=spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    assert(ret==ESP_OK);
    ret=spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);
    ili_init(spi);

	for (x=0; x<NO_SIM_TRANS; x++) {
        dmamem[x]=heap_caps_malloc(MEM_PER_TRANS*2, MALLOC_CAP_DMA);
		assert(dmamem[x]);
		memset(&trans[x], 0, sizeof(spi_transaction_t));
		trans[x].length=MEM_PER_TRANS*2;
		trans[x].user=(void*)1;
		trans[x].tx_buffer=&dmamem[x];
	}
	xSemaphoreGive(dispDoneSem);

	while(1) {
		xSemaphoreTake(dispSem, portMAX_DELAY);

		// Get pointer to Doom's 320x200 framebuffer (8-bit indexed)
		uint8_t *fbData = (uint8_t*)currFbPtr;

		// Send header for full 320x170 display area
		send_header_start(spi, 0, 0, LCD_WIDTH, LCD_HEIGHT);
		send_header_cleanup(spi);

		// Crop: skip CROP_TOP rows, send LCD_HEIGHT rows
		// Each MEM_PER_TRANS = 640 pixels = 2 rows of 320
		int startPixel = CROP_TOP * DOOM_WIDTH;
		int totalPixels = LCD_HEIGHT * DOOM_WIDTH; // 170 * 320 = 54400

		for (x = 0; x < totalPixels; x += MEM_PER_TRANS) {
			int remaining = totalPixels - x;
			int count = (remaining < MEM_PER_TRANS) ? remaining : MEM_PER_TRANS;

#ifdef DOUBLE_BUFFER
			for (i = 0; i < count; i += 4) {
				int srcOff = (startPixel + x + i) / 4;
				uint32_t d = currFbPtr[srcOff];
				dmamem[idx][i+0] = lcdpal[(d>>0)&0xff];
				dmamem[idx][i+1] = lcdpal[(d>>8)&0xff];
				if (i+2 < count) dmamem[idx][i+2] = lcdpal[(d>>16)&0xff];
				if (i+3 < count) dmamem[idx][i+3] = lcdpal[(d>>24)&0xff];
			}
#else
			uint8_t *src = fbData + startPixel + x;
			for (i = 0; i < count; i++) {
				dmamem[idx][i] = lcdpal[src[i]];
			}
#endif
			trans[idx].length = count * 16;
			trans[idx].user = (void*)1;
			trans[idx].tx_buffer = dmamem[idx];
			ret = spi_device_queue_trans(spi, &trans[idx], portMAX_DELAY);
			assert(ret == ESP_OK);

			idx++;
			if (idx >= NO_SIM_TRANS) idx = 0;

			if (inProgress == NO_SIM_TRANS-1) {
				ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
				assert(ret == ESP_OK);
			} else {
				inProgress++;
			}
		}

#ifndef DOUBLE_BUFFER
		xSemaphoreGive(dispDoneSem);
#endif
		while(inProgress) {
			ret=spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
			assert(ret==ESP_OK);
			inProgress--;
		}
	}
}

#include    <xtensa/config/core.h>
#include    <xtensa/corebits.h>
#include    <xtensa/config/system.h>

void spi_lcd_wait_finish() {
#ifndef DOUBLE_BUFFER
	xSemaphoreTake(dispDoneSem, portMAX_DELAY);
#endif
}

void spi_lcd_send(uint16_t *scr) {
#ifdef DOUBLE_BUFFER
	memcpy(currFbPtr, scr, DOOM_WIDTH*DOOM_HEIGHT);
#else
	currFbPtr=scr;
#endif
	xSemaphoreGive(dispSem);
}

void spi_lcd_init() {
	printf("spi_lcd_init() - Disobey Badge 2025/2026\n");
	printf("LCD: ST7789 %dx%d (offset X=%d Y=%d)\n", LCD_WIDTH, LCD_HEIGHT, LCD_OFFSET_X, LCD_OFFSET_Y);
	printf("DOOM: %dx%d, crop top=%d rows\n", DOOM_WIDTH, DOOM_HEIGHT, CROP_TOP);
    dispSem=xSemaphoreCreateBinary();
    dispDoneSem=xSemaphoreCreateBinary();
#ifdef DOUBLE_BUFFER
    currFbPtr=heap_caps_malloc(DOOM_WIDTH*DOOM_HEIGHT, MALLOC_CAP_32BIT);
#endif
#if CONFIG_FREERTOS_UNICORE
	xTaskCreatePinnedToCore(&displayTask, "display", 6000, NULL, 6, NULL, 0);
#else
	xTaskCreatePinnedToCore(&displayTask, "display", 6000, NULL, 6, NULL, 1);
#endif
}
