/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "esp_lcd_gc9a01.h"

// ============================================================================
// Configuration
// ============================================================================

#define LCD_HOST                    SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ          (80 * 1000 * 1000)  // 80 MHz (max stable)
#define LCD_H_RES                   240
#define LCD_V_RES                   240
#define LCD_CMD_BITS                8
#define LCD_PARAM_BITS              8
#define LCD_BUFFER_LINES            60  // Half screen for optimal performance

// Pin Configuration
#define PIN_LCD_SCLK                8
#define PIN_LCD_MOSI                10
#define PIN_LCD_MISO                -1
#define PIN_LCD_DC                  2
#define PIN_LCD_RST                 -1
#define PIN_LCD_CS                  3

// Timing
#define INIT_DELAY_MS               100
#define SLEEP_OUT_DELAY_MS          120
#define DISPLAY_ON_DELAY_MS         20

static const char *TAG = "display";

// ============================================================================
// Global Handles
// ============================================================================

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static lv_display_t *disp = NULL;
static lv_color_t *lvbuf = NULL;

// ============================================================================
// GC9A01 Initialization Commands (TFT_eSPI compatible)
// ============================================================================

static const gc9a01_lcd_init_cmd_t gc9a01_init_cmds[] = {
    {0xEF, (uint8_t[]){0x00}, 0, 0},
    {0xEB, (uint8_t[]){0x14}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 0, 0},
    {0xEF, (uint8_t[]){0x00}, 0, 0},
    {0xEB, (uint8_t[]){0x14}, 1, 0},
    {0x84, (uint8_t[]){0x40}, 1, 0},
    {0x85, (uint8_t[]){0xFF}, 1, 0},
    {0x86, (uint8_t[]){0xFF}, 1, 0},
    {0x87, (uint8_t[]){0xFF}, 1, 0},
    {0x88, (uint8_t[]){0x0A}, 1, 0},
    {0x89, (uint8_t[]){0x21}, 1, 0},
    {0x8A, (uint8_t[]){0x00}, 1, 0},
    {0x8B, (uint8_t[]){0x80}, 1, 0},
    {0x8C, (uint8_t[]){0x01}, 1, 0},
    {0x8D, (uint8_t[]){0x01}, 1, 0},
    {0x8E, (uint8_t[]){0xFF}, 1, 0},
    {0x8F, (uint8_t[]){0xFF}, 1, 0},
    {0xB6, (uint8_t[]){0x00, 0x20}, 2, 0},
    {0x3A, (uint8_t[]){0x05}, 1, 0},
    {0x90, (uint8_t[]){0x08, 0x08, 0x08, 0x08}, 4, 0},
    {0xBD, (uint8_t[]){0x06}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},
    {0xFF, (uint8_t[]){0x60, 0x01, 0x04}, 3, 0},
    {0xC3, (uint8_t[]){0x13}, 1, 0},
    {0xC4, (uint8_t[]){0x13}, 1, 0},
    {0xC9, (uint8_t[]){0x22}, 1, 0},
    {0xBE, (uint8_t[]){0x11}, 1, 0},
    {0xE1, (uint8_t[]){0x10, 0x0E}, 2, 0},
    {0xDF, (uint8_t[]){0x21, 0x0c, 0x02}, 3, 0},
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
    {0x35, (uint8_t[]){0x00}, 0, 0},
    {0x21, (uint8_t[]){0x00}, 0, 0},
};

// ============================================================================
// RGB565 Byte Swap Optimization (Maximum Performance)
// ============================================================================

static inline uint32_t swap16x2_u32(uint32_t v)
{
    return ((v & 0x00FF00FFu) << 8) | ((v & 0xFF00FF00u) >> 8);
}

static inline uint16_t swap16_u16(uint16_t v)
{
    return __builtin_bswap16(v);
}

/**
 * @brief Ultra-optimized RGB565 byte swap
 * - 16x unrolled loop for better instruction cache usage
 * - Processes 32 pixels per iteration
 * - Minimizes branch mispredictions
 */
static void rgb565_swap(void *buf, uint32_t buf_size_px)
{
    if (buf_size_px == 0) return;
    
    uint8_t *b = (uint8_t *)buf;

    // Align to 32-bit boundary
    if (((uintptr_t)b) & 0x2)
    {
        uint16_t *p16 = (uint16_t *)b;
        *p16 = swap16_u16(*p16);
        b += 2;
        buf_size_px--;
    }

    uint32_t *p32 = (uint32_t *)b;
    uint32_t pairs = buf_size_px >> 1;

    // Main loop: 16x unroll (32 pixels per iteration)
    while (pairs >= 16)
    {
        uint32_t v0  = p32[0],  v1  = p32[1],  v2  = p32[2],  v3  = p32[3];
        uint32_t v4  = p32[4],  v5  = p32[5],  v6  = p32[6],  v7  = p32[7];
        uint32_t v8  = p32[8],  v9  = p32[9],  v10 = p32[10], v11 = p32[11];
        uint32_t v12 = p32[12], v13 = p32[13], v14 = p32[14], v15 = p32[15];

        v0  = swap16x2_u32(v0);  v1  = swap16x2_u32(v1);
        v2  = swap16x2_u32(v2);  v3  = swap16x2_u32(v3);
        v4  = swap16x2_u32(v4);  v5  = swap16x2_u32(v5);
        v6  = swap16x2_u32(v6);  v7  = swap16x2_u32(v7);
        v8  = swap16x2_u32(v8);  v9  = swap16x2_u32(v9);
        v10 = swap16x2_u32(v10); v11 = swap16x2_u32(v11);
        v12 = swap16x2_u32(v12); v13 = swap16x2_u32(v13);
        v14 = swap16x2_u32(v14); v15 = swap16x2_u32(v15);

        p32[0]  = v0;  p32[1]  = v1;  p32[2]  = v2;  p32[3]  = v3;
        p32[4]  = v4;  p32[5]  = v5;  p32[6]  = v6;  p32[7]  = v7;
        p32[8]  = v8;  p32[9]  = v9;  p32[10] = v10; p32[11] = v11;
        p32[12] = v12; p32[13] = v13; p32[14] = v14; p32[15] = v15;

        p32 += 16;
        pairs -= 16;
    }

    // Remainder: 4x unroll
    while (pairs >= 4)
    {
        uint32_t v0 = p32[0], v1 = p32[1], v2 = p32[2], v3 = p32[3];
        v0 = swap16x2_u32(v0); v1 = swap16x2_u32(v1);
        v2 = swap16x2_u32(v2); v3 = swap16x2_u32(v3);
        p32[0] = v0; p32[1] = v1; p32[2] = v2; p32[3] = v3;
        p32 += 4;
        pairs -= 4;
    }

    // Final pairs
    while (pairs--)
    {
        *p32 = swap16x2_u32(*p32);
        p32++;
    }

    // Odd pixel
    if (buf_size_px & 1u)
    {
        uint16_t *tail = (uint16_t *)p32;
        *tail = swap16_u16(*tail);
    }
}

// ============================================================================
// LVGL Callbacks
// ============================================================================

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    uint32_t pixel_count = (x2 - x1 + 1) * (y2 - y1 + 1);
    rgb565_swap(px_map, pixel_count);
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2 + 1, y2 + 1, px_map);
}

static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    if (disp != NULL)
    {
        lv_display_flush_ready(disp);
    }
    return false;
}

static uint32_t lvgl_tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

// ============================================================================
// Public API
// ============================================================================

void SPI_Setup(void)
{
    ESP_LOGI(TAG, "Initializing SPI bus");
    
    spi_bus_config_t bus_config = {
        .sclk_io_num = PIN_LCD_SCLK,
        .mosi_io_num = PIN_LCD_MOSI,
        .miso_io_num = PIN_LCD_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_BUFFER_LINES * sizeof(uint16_t),
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Configuring panel IO");
    
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_LCD_DC,
        .cs_gpio_num = PIN_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 16,
        .on_color_trans_done = notify_lvgl_flush_ready,
        .user_ctx = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, 
                                              &io_config, &io_handle));

    ESP_LOGI(TAG, "Installing GC9A01 panel driver");
    
    gc9a01_vendor_config_t vendor_config = {
        .init_cmds = gc9a01_init_cmds,
        .init_cmds_size = sizeof(gc9a01_init_cmds) / sizeof(gc9a01_lcd_init_cmd_t),
    };

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_endian = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));

    vTaskDelay(pdMS_TO_TICKS(INIT_DELAY_MS));

    ESP_LOGI(TAG, "Initializing display");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(SLEEP_OUT_DELAY_MS));

    ESP_LOGI(TAG, "Turning on display");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_ON_DELAY_MS));

    ESP_LOGI(TAG, "Display initialization complete");
}

void LVGL_Setup(void)
{
    ESP_LOGI(TAG, "Initializing LVGL");
    
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    size_t buffer_size = LCD_H_RES * LCD_BUFFER_LINES * sizeof(lv_color_t);
    
    // Try aligned allocation first, fall back to regular if unavailable
    lvbuf = heap_caps_aligned_alloc(32, buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (lvbuf == NULL) {
        lvbuf = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    }
    assert(lvbuf != NULL);
    
    lv_color_t *lvbuf2 = heap_caps_aligned_alloc(32, buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (lvbuf2 == NULL) {
        lvbuf2 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    }
    assert(lvbuf2 != NULL);

    disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_buffers(disp, lvbuf, lvbuf2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_default(disp);

    ESP_LOGI(TAG, "LVGL init complete (Buffer: %d lines, %d bytes)", 
             LCD_BUFFER_LINES, buffer_size);
}