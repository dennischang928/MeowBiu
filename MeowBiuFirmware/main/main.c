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

// Configuration
#define LVGL_TASK_PRIORITY 5
#define LVGL_TASK_STACK_SIZE 8192
#define LVGL_TICK_PERIOD_MS 5
#define MAIN_LOOP_DELAY_MS 1000

// Logging tags
static const char *TAG = "main";
static const char *TAG_LVGL = "LVGL";
static const char *TAG_CHANGE = "EMOTION CHANGE";

// Task handle
static TaskHandle_t display_task_handle = NULL;

// Emotion cycling state
static int emotion_index = 0;
static const emotion_t emotion_sequence[] = {
    // EMOTION_IDLE,
    EMOTION_EXCITED,
    EMOTION_ANGRY,
    EMOTION_BLINK,
    EMOTION_IDLE_LOOK_RIGHT,
    EMOTION_SAD,
    EMOTION_SCARED,
    EMOTION_DGAF};
static const int emotion_sequence_length = sizeof(emotion_sequence) / sizeof(emotion_sequence[0]);

/**
 * @brief LVGL log callback - forwards LVGL logs to ESP-IDF logging system
 */
static void lvgl_log_callback(lv_log_level_t level, const char *buf)
{
    switch (level)
    {
    case LV_LOG_LEVEL_TRACE:
        ESP_LOGV(TAG_LVGL, "%s", buf);
        break;
    case LV_LOG_LEVEL_INFO:
    case LV_LOG_LEVEL_USER:
        ESP_LOGI(TAG_LVGL, "%s", buf);
        break;
    case LV_LOG_LEVEL_WARN:
        ESP_LOGW(TAG_LVGL, "%s", buf);
        break;
    case LV_LOG_LEVEL_ERROR:
        ESP_LOGE(TAG_LVGL, "%s", buf);
        break;
    default:
        ESP_LOGI(TAG_LVGL, "%s", buf);
        break;
    }
}

/**
 * @brief Animation finished callback - switches to next emotion
 */
void my_animation_done_callback(emotion_t emotion, void *user_data)
{
    ESP_LOGI(TAG, "Animation finished for emotion: %d", emotion);

    // Move to next emotion in sequence
    emotion_index = (emotion_index + 1) % emotion_sequence_length;
    emotion_t next_emotion = emotion_sequence[emotion_index];

    const char *emotion_names[] = {"EMOTION_IDLE",
                                   "EMOTION_EXCITED",
                                   "EMOTION_ANGRY",
                                   "EMOTION_BLINK",
                                   "EMOTION_IDLE_LOOK_RIGHT",
                                   "EMOTION_SAD",
                                   "EMOTION_SCARED",
                                   "EMOTION_DGAF"};
    ESP_LOGI(TAG_CHANGE, "Changing emotion to %s", emotion_names[emotion_index]);
    face_set_emotion(next_emotion);
}

/**
 * @brief LVGL task - handles UI rendering and timer updates
 */
static void display_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LVGL task started (FreeRTOS tick rate: %d Hz)", configTICK_RATE_HZ);

    // Set black background
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0, 0, 0), LV_PART_MAIN);

    // Initialize face UI
    face_init();
    face_set_emotion(emotion_sequence[emotion_index]);
    face_set_animation_finished_callback(my_animation_done_callback, NULL);

    // Start with first emotion

    // face_set_emotion(EMOTION_SCARED);
    // face_set_loop_count(EMOTION_SCARED, 4);
    // Main LVGL loop
    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(LVGL_TICK_PERIOD_MS));
    }
}

/**
 * @brief Print LVGL memory statistics
 */
static void print_memory_stats(void)
{
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    ESP_LOGI(TAG, "LVGL Memory - Free: %d bytes, Fragmentation: %d%%",
             mon.free_size, mon.frag_pct);
}

/**
 * @brief Application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Initializing display and LVGL...");

    // Initialize hardware and LVGL
    SPI_Setup();
    LVGL_Setup();

    // Register LVGL log callback
    lv_log_register_print_cb(lvgl_log_callback);

    // Create LVGL task
    ESP_LOGI(TAG, "Creating LVGL task...");
    BaseType_t ret = xTaskCreate(
        display_task,
        "LVGL_Task",
        LVGL_TASK_STACK_SIZE,
        NULL,
        LVGL_TASK_PRIORITY,
        &display_task_handle);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return;
    }

    ESP_LOGI(TAG, "LVGL task created successfully");

    // Disable logs for "face" and "main" tags
    esp_log_level_set("face", ESP_LOG_NONE);
    esp_log_level_set("main", ESP_LOG_NONE);

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_MS));
        print_memory_stats();
    }
}