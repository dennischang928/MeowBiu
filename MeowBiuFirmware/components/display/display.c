/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_heap_caps.h"

#define CONFIG_EXAMPLE_LCD_CONTROLLER_GC9A01 1

#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))


#include "esp_lcd_gc9a01.h"

static const char *TAG = "example";

/* Using SPI2 in the example */
#define LCD_HOST SPI2_HOST

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// GC9A01 Display Configuration ///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000) // 40 MHz for GC9A01

#define EXAMPLE_PIN_NUM_SCLK 8     // SPI Clock
#define EXAMPLE_PIN_NUM_MOSI 10    // SPI MOSI
#define EXAMPLE_PIN_NUM_MISO -1    // Not used
#define EXAMPLE_PIN_NUM_LCD_DC 2   // Data/Command
#define EXAMPLE_PIN_NUM_LCD_RST -1 // No dedicated reset (connected to EN)
#define EXAMPLE_PIN_NUM_LCD_CS 3   // Chip Select

/* The pixel number in horizontal and vertical */
#define EXAMPLE_LCD_H_RES 240
#define EXAMPLE_LCD_V_RES 240

// static lv_color_t *lvbuf; // RGB565 pixels (2 bytes each)
// static lv_color_t *lvbuf_2; // RGB565 pixels (2 bytes each)

/* Bit number used to represent command and parameter */
#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
esp_lcd_touch_handle_t tp = NULL;
#endif

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
lv_display_t *disp = NULL;

static void example_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // because SPI LCD is big-endian, we need to swap the RGB bytes order
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));

    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    if (disp == NULL)
        return false;
    lv_display_flush_ready(disp);
    return false;
}

/* Return the number of milliseconds since boot.
 * LVGL will call this when configured with lv_tick_set_cb(). */
static uint32_t my_tick(void)
{
    /* esp_timer_get_time() returns microseconds since boot */
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// TFT_eSPI-style custom initialization commands
static const gc9a01_lcd_init_cmd_t tft_espi_init_cmds[] = {
    {0xEF, (uint8_t[]){0x00}, 0, 0},
    {0xEB, (uint8_t[]){0x14}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 0, 0},
    {0xEF, (uint8_t[]){0x00}, 0, 0},
    {0xEB, (uint8_t[]){0x14}, 1, 0},
    {0x84, (uint8_t[]){0x40}, 1, 0}, // TFT_eSPI value
    {0x85, (uint8_t[]){0xFF}, 1, 0},
    {0x86, (uint8_t[]){0xFF}, 1, 0},
    {0x87, (uint8_t[]){0xFF}, 1, 0},
    {0x88, (uint8_t[]){0x0A}, 1, 0},
    {0x89, (uint8_t[]){0x21}, 1, 0}, // TFT_eSPI value
    {0x8A, (uint8_t[]){0x00}, 1, 0},
    {0x8B, (uint8_t[]){0x80}, 1, 0},
    {0x8C, (uint8_t[]){0x01}, 1, 0},
    {0x8D, (uint8_t[]){0x01}, 1, 0}, // TFT_eSPI value
    {0x8E, (uint8_t[]){0xFF}, 1, 0},
    {0x8F, (uint8_t[]){0xFF}, 1, 0},
    {0xB6, (uint8_t[]){0x00, 0x20}, 2, 0}, // CRITICAL: Display Function Control
    {0x3A, (uint8_t[]){0x05}, 1, 0},       // 16-bit color (handled by driver but included for completeness)
    {0x90, (uint8_t[]){0x08, 0x08, 0x08, 0x08}, 4, 0},
    {0xBD, (uint8_t[]){0x06}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x60, 0x01, 0x04}, 3, 0},
    {0xC3, (uint8_t[]){0x13}, 1, 0},
    {0xC4, (uint8_t[]){0x13}, 1, 0},
    {0xC9, (uint8_t[]){0x22}, 1, 0}, // TFT_eSPI value
    {0xBE, (uint8_t[]){0x11}, 1, 0},
    {0xE1, (uint8_t[]){0x10, 0x0E}, 2, 0},
    {0xDF, (uint8_t[]){0x21, 0x0c, 0x02}, 3, 0},
    // Gamma settings
    {0xF0, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6, 0},
    {0xF1, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6, 0},
    {0xF2, (uint8_t[]){0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 6, 0},
    {0xF3, (uint8_t[]){0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 6, 0},
    {0xED, (uint8_t[]){0x1B, 0x0B}, 2, 0},
    {0xAE, (uint8_t[]){0x77}, 1, 0},
    {0xCD, (uint8_t[]){0x63}, 1, 0},
    {0x70, (uint8_t[]){0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9, 0},
    {0xE8, (uint8_t[]){0x34}, 1, 0},
    {0x62, (uint8_t[]){0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 12, 0},
    {0x63, (uint8_t[]){0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 12, 0},
    {0x64, (uint8_t[]){0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 7, 0},
    {0x66, (uint8_t[]){0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 10, 0},
    {0x67, (uint8_t[]){0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 10, 0},
    {0x74, (uint8_t[]){0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 7, 0},
    {0x98, (uint8_t[]){0x3e, 0x07}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 0, 0}, // Tearing effect line on
    {0x21, (uint8_t[]){0x00}, 0, 0}, // Display inversion on
};

void SPI_Setup(void)
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = EXAMPLE_PIN_NUM_SCLK,
        .mosi_io_num = EXAMPLE_PIN_NUM_MOSI,
        .miso_io_num = EXAMPLE_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * 80 * sizeof(uint16_t),
    };

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = NULL, // will be set later
    };

    /* Attach the LCD to the SPI bus */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // Configure vendor-specific init commands
    gc9a01_vendor_config_t vendor_config = {
        .init_cmds = tft_espi_init_cmds,
        .init_cmds_size = sizeof(tft_espi_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_endian = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config, // Use TFT_eSPI init sequence
    };

    ESP_LOGI(TAG, "Install GC9A01 panel driver with TFT_eSPI init sequence");
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Initializing display with custom commands...");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(120)); // Sleep out delay

    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(20));

    ESP_LOGI(TAG, "Display initialization complete");
}

void LVGL_Setup()
{
    lv_init();
    lv_tick_set_cb(my_tick);
    // lvgl_start_tick_timer();
    static uint8_t buf1[240 * 240 / 3 * BYTES_PER_PIXEL];
    // static uint8_t buf2[240 * 240 / 5 * BYTES_PER_PIXEL];

    disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, example_lvgl_flush_cb);

    lv_display_set_buffers(disp,
                           buf1, NULL, // Pass one buffer
                           sizeof(buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_default(disp);
}