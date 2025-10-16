/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "display.h"

#include "face.h"
#include "lv_examples.h"

static const char *TAG = "main";

// LVGL task handle
static TaskHandle_t lvgl_task_handle = NULL;

// LVGL task function - runs continuously in separate thread
static void lvgl_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL task started");
    ESP_LOGI(TAG, "FreeRTOS tick rate: %d Hz, 1 tick = %d ms", configTICK_RATE_HZ, 1000 / configTICK_RATE_HZ);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), LV_PART_MAIN);
    // Create UI elements
    // lv_obj_t *label = lv_label_create(lv_screen_active());
    // lv_label_set_text(label, "Hello Arduino, I'm LVGL!");
    // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    // Create GIF animation

    // lv_example_gif_1();

    face_init();
    // char buf[32];

    while (1)
    {
        // static int log_counter = 0;
        // if (log_counter++ % 10 == 0)
        // {

        //     // snprintf(buf, sizeof(buf), "Hello! %lld ms", esp_timer_get_time()/1000ULL);
        //     // lv_label_set_text(label, buf);
        // }

        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    vTaskDelete(NULL);
}

void my_log_cb(lv_log_level_t level, const char *buf)
{
    const char *lvgl_tag = "LVGL";
    switch (level)
    {
    case LV_LOG_LEVEL_TRACE:
        ESP_LOGV(lvgl_tag, "%s", buf);
        break;
    case LV_LOG_LEVEL_INFO:
        ESP_LOGI(lvgl_tag, "%s", buf);
        break;
    case LV_LOG_LEVEL_WARN:
        ESP_LOGW(lvgl_tag, "%s", buf);
        break;
    case LV_LOG_LEVEL_ERROR:
        ESP_LOGE(lvgl_tag, "%s", buf);
        break;
    case LV_LOG_LEVEL_USER:
        /* treat user logs as info */
        ESP_LOGI(lvgl_tag, "%s", buf);
        break;
    default:
        ESP_LOGI(lvgl_tag, "%s", buf);
        break;
    }
}

void app_main()
{
    ESP_LOGI(TAG, "Initializing display and LVGL...");

    // Initialize SPI and LVGL
    SPI_Setup();
    LVGL_Setup();

    lv_log_register_print_cb(my_log_cb);

    ESP_LOGI(TAG, "Creating LVGL task...");

    // Create LVGL task with appropriate priority and stack size
    BaseType_t ret = xTaskCreate(
        lvgl_task,        // Task function
        "LVGL_Task",      // Task name
        8192,             // Stack size
        NULL,             // Parameters
        5,                // Priority
        &lvgl_task_handle // Task handle
    );

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return;
    }

    ESP_LOGI(TAG, "LVGL task created successfully on core 1");

    while (1)
    {
        // Do other non-UI work here
        vTaskDelay(pdMS_TO_TICKS(1000));
        // ESP_LOGI(TAG, "Main task running, free heap: %lu bytes", esp_get_free_heap_size());
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        ESP_LOGI("face", "Memory after delete - free: %d, frag: %d%%",
                 mon.free_size, mon.frag_pct);
    }
}