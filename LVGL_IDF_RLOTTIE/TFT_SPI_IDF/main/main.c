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

#include "esp_lcd_gc9a01.h"

static const char *TAG = "example";

/* Using SPI2 in the example */
#define LCD_HOST SPI2_HOST

/* -------------------------------------------------------------------------- */
/* Please update the following configuration according to your LCD spec */
/* -------------------------------------------------------------------------- */
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// GC9A01 Display Configuration (Aligned with TFT_eSPI Wiring) ///////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (40 * 1000 * 1000)   // 40 MHz for GC9A01

#define EXAMPLE_PIN_NUM_SCLK           8    // SPI Clock
#define EXAMPLE_PIN_NUM_MOSI           10   // SPI MOSI
#define EXAMPLE_PIN_NUM_MISO           -1   // Not used
#define EXAMPLE_PIN_NUM_LCD_DC         2    // Data/Command
#define EXAMPLE_PIN_NUM_LCD_RST        -1   // No dedicated reset (connected to EN)
#define EXAMPLE_PIN_NUM_LCD_CS         3    // Chip Select
#if 0
/* Not used in this simplified example: backlight and touch chip select pins */
#define EXAMPLE_PIN_NUM_BK_LIGHT       -1   // Backlight (not used / tied to VCC)
#define EXAMPLE_PIN_NUM_TOUCH_CS       -1   // No touch controller
#endif

/* The pixel number in horizontal and vertical */
#define EXAMPLE_LCD_H_RES 240
#define EXAMPLE_LCD_V_RES 240



static lv_color_t *lvbuf;  // RGB565 pixels (2 bytes each)
#define DRAW_BUF_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

/* Bit number used to represent command and parameter */
#define EXAMPLE_LCD_CMD_BITS 8
#define EXAMPLE_LCD_PARAM_BITS 8

#if 0
/* LVGL tick period (unused because we provide lv_tick_set_cb) */
#define EXAMPLE_LVGL_TICK_PERIOD_MS 2
#endif

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
esp_lcd_touch_handle_t tp = NULL;
#endif

esp_lcd_panel_handle_t panel_handle = NULL;
lv_display_t *disp = NULL;

static void example_lvgl_flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map)
{
  /* Retrieve your panel handle (user data stored when creating the display) */
  //   example_lvgl_port_update_callback(disp);
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
   if(disp == NULL) return false;
    lv_display_flush_ready(disp);
    return false;
}


/* Return the number of milliseconds since boot.
 * LVGL will call this when configured with lv_tick_set_cb(). */
static uint32_t my_tick(void)
{
  /* esp_timer_get_time() returns microseconds since boot */
  return (uint32_t)(esp_timer_get_time() / 1000);
}



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
  esp_lcd_panel_io_handle_t io_handle = NULL;
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

  esp_lcd_panel_dev_config_t panel_config = {
    .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
    .rgb_endian = LCD_RGB_ELEMENT_ORDER_RGB,
    .bits_per_pixel = 16,
  };

  ESP_LOGI(TAG, "Install GC9A01 panel driver");
  ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
  /* Optional: invert color if your panel wiring requires it. Commented out in this minimal example. */
  /*  */
}

void LVGL_Setup()
{
    lv_init();
    lv_tick_set_cb(my_tick);

    lvbuf = heap_caps_malloc(EXAMPLE_LCD_H_RES * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(lvbuf);
    disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);          // << important
    lv_display_set_flush_cb(disp, example_lvgl_flush_cb);
    lv_display_set_buffers(disp,
                           lvbuf, NULL,
                           EXAMPLE_LCD_H_RES * 40 * sizeof(lv_color_t), // BYTES
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
  
}

void app_main()
{

  
  SPI_Setup();
  LVGL_Setup();

  lv_obj_t *label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Hello Arduino, I'm LVGL!");
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);    

  char buf[64];  // buffer for formatted text
  int i = 0;
  while (1) {
    snprintf(buf, sizeof(buf), "Hello Arduino, I'm LVGL! %d", i++);
    lv_label_set_text(label, buf);
    /* raise the task priority of LVGL and/or reduce the handler period can improve the performance */
    vTaskDelay(pdMS_TO_TICKS(10));
    /* The task running lv_timer_handler should have lower priority than that running `lv_tick_inc` */
    lv_timer_handler();
  }
}

