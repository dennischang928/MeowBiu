#include "input_service.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "event_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "service_manager.h"
#include <stdbool.h>

#define SHOCK_SENSOR_PIN 9
#define ESP_INTR_FLAG_DEFAULT 0

// Timing constants (in milliseconds)
#define DEBOUNCE_TIME_MS 100 // Ignore events within 100ms of a tap (debounce)
#define TAP_WINDOW_MS 500    // Time window to detect multiple taps
#define MAX_TAPS 2           // Maximum taps to detect

#define SPAM_TAP_THRESHOLD 8    // Number of taps to trigger angry
#define SPAM_TAP_WINDOW_MS 3000 // Time window in ms

static QueueHandle_t gpio_evt_queue = NULL;
static const char *TAG = "input_service";

static int spam_tap_count = 0;
static TimerHandle_t spam_tap_timer = NULL;

static void spam_tap_timer_callback(TimerHandle_t xTimer) {
  spam_tap_count = 0;
  ESP_LOGI(TAG, "Spam tap counter reset");
}

static void IRAM_ATTR shock_sensor_isr_handler(void *arg) {
  uint32_t gpio_num = (uint32_t)arg;
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR();
  }
}

static void shock_sensor_task(void *arg) {
  uint32_t io_num;
  TickType_t last_event_time = 0;
  const TickType_t debounce_ticks = pdMS_TO_TICKS(DEBOUNCE_TIME_MS);
  const TickType_t window_ticks = pdMS_TO_TICKS(TAP_WINDOW_MS);

  while (1) {
    // --- PHASE 1: Wait indefinitely for the FIRST tap ---
    if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
      TickType_t now = xTaskGetTickCount();

      // 1. Global Debounce: Ignore if too close to the previous sequence
      if ((now - last_event_time) < debounce_ticks) {
        last_event_time = now; // Update time to suppress trailing bounces
        continue;
      }

      // We have a valid FIRST tap
      last_event_time = now;
      ESP_LOGI(TAG, "Tap 1 detected");

      // Update spam counter
      spam_tap_count++;
      if (spam_tap_timer) {
        xTimerReset(spam_tap_timer, 0);
      }

      bool spam_triggered = false;
      if (spam_tap_count >= SPAM_TAP_THRESHOLD) {
        spam_triggered = true;
        spam_tap_count = 0;
      }

      // --- PHASE 2: Listen for SECOND tap within window ---
      bool double_tap = false;
      TickType_t deadline = now + window_ticks;

      while (xTaskGetTickCount() < deadline) {
        // Calculate remaining wait time
        TickType_t remaining = deadline - xTaskGetTickCount();

        // Safety: if we wrapped around or are past deadline, break
        if (remaining > window_ticks)
          break;

        // Wait for next event
        if (xQueueReceive(gpio_evt_queue, &io_num, remaining)) {
          now = xTaskGetTickCount();

          // 2. Window Debounce: Ignore noise inside the window
          if ((now - last_event_time) < debounce_ticks) {
            last_event_time = now;
            continue;
          }

          // We have a valid SECOND tap
          double_tap = true;
          last_event_time = now;

          // Update spam counter for second tap
          spam_tap_count++;
          if (spam_tap_timer) {
            xTimerReset(spam_tap_timer, 0);
          }
          if (spam_tap_count >= SPAM_TAP_THRESHOLD) {
            spam_triggered = true;
            spam_tap_count = 0;
          }

          break; // Stop listening
        } else {
          // Timeout occurred: No second tap received
          break;
        }
      }

      // --- PHASE 3: Classify ---
      if (spam_triggered) {
        ESP_LOGW(TAG, "=== SPAM TAP DETECTED ===");
        event_bus_post(APP_EVENT_SPAM_TAP_DETECTED, NULL);
      } else if (double_tap) {
        ESP_LOGI(TAG, "=== DOUBLE TAP DETECTED ===");
        event_bus_post(APP_EVENT_DOUBLE_TAP_DETECTED, NULL);
      } else {
        ESP_LOGI(TAG, "=== SINGLE TAP DETECTED ===");
        event_bus_post(APP_EVENT_SINGLE_TAP_DETECTED, NULL);
      }
    }
  }
}

esp_err_t input_service_init(service_t *service) {
  ESP_LOGI(TAG, "Initializing input service...");

  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

  // Create spam tap timer
  spam_tap_timer =
      xTimerCreate("spam_tap_timer", pdMS_TO_TICKS(SPAM_TAP_WINDOW_MS), pdFALSE,
                   (void *)0, spam_tap_timer_callback);

  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_NEGEDGE,
      .pin_bit_mask = (1ULL << SHOCK_SENSOR_PIN),
      .mode = GPIO_MODE_INPUT,
  };

  gpio_config(&io_conf);
  gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  gpio_isr_handler_add(SHOCK_SENSOR_PIN, shock_sensor_isr_handler,
                       (void *)SHOCK_SENSOR_PIN);

  xTaskCreate(shock_sensor_task, "shock_sensor_task", 2048, NULL, 3, NULL);

  ESP_LOGI(TAG, "Input service initialized.");
  return ESP_OK;
}

esp_err_t input_service_start(service_t *service) {
  ESP_LOGI(TAG, "Starting input service...");
  ESP_LOGI(TAG, "Input service started.");
  return ESP_OK;
}

static const service_ops_t INPUT_OPS = {
    .init = input_service_init,
    .start = input_service_start,
};

DEFINE_SERVICE(input_service, &INPUT_OPS);