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
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "display.h"

#include "lv_examples.h"
void app_main()
{
  SPI_Setup();
  LVGL_Setup();
  
  // lv_obj_t *label = lv_label_create(lv_screen_active());
  // lv_label_set_text(label, "Hello Arduino, I'm LVGL!");
  // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);    
  lv_example_gif_1();
  // lv_example_lottie_1();
  

  while (1) {
    // snprintf(buf, sizeof(buf), "Hello Arduino, I'm LVGL! %d", i++);
    // lv_label_set_text(label, buf);
    /* raise the task priority of LVGL and/or reduce the handler period can improve the performance */
    vTaskDelay(pdMS_TO_TICKS(10));
    /* The task running lv_timer_handler should have lower priority than that running `lv_tick_inc` */
    lv_timer_handler();
  }
}