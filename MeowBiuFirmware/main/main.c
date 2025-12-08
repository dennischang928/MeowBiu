#include "display.h"
#include "esp_log.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "service_manager.h"

static const char *TAG = "main";
static const char *TAG_LVGL = "LVGL";

extern service_t ui_service;
extern service_t input_service;
extern service_t rtc_service;
extern service_t network_service;

/**
 * @brief LVGL log callback - forwards LVGL logs to ESP-IDF logging system
 */
static void lvgl_log_callback(lv_log_level_t level, const char *buf) {
  switch (level) {
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
 * @brief Print LVGL memory statistics
 */
static void print_memory_stats(void *pvParameters) {
  while (1) {
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    ESP_LOGI(TAG, "LVGL Memory - Free: %d bytes, Fragmentation: %d%%",
             mon.free_size, mon.frag_pct);
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void app_main(void) {
  ESP_LOGI(TAG, "Initializing system...");
  // disable input_service logs
  esp_log_level_set("input_service", ESP_LOG_NONE);
  esp_log_level_set("face", ESP_LOG_NONE);
  esp_log_level_set("main", ESP_LOG_NONE);
  esp_log_level_set("event_bus", ESP_LOG_NONE);
  esp_log_level_set("clock_ui", ESP_LOG_NONE);
  esp_log_level_set("rtc_service", ESP_LOG_NONE);

  // Register LVGL log callback
  lv_log_register_print_cb(lvgl_log_callback);

  // Initialize hardware
  SPI_Setup();
  LVGL_Setup();

  // Initialize infrastructure
  event_bus_init();
  service_manager_init();

  // Register and start services
  service_manager_register(&ui_service);
  service_manager_register(&input_service);
  service_manager_register(&rtc_service);
  service_manager_register(&network_service);
  service_manager_start_all();

  // Create memory stats task
  // xTaskCreate(print_memory_stats, "mem_stats", 2048, NULL, 1, NULL);
  ESP_LOGI(TAG, "System running");
}
