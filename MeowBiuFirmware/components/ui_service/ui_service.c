/**
 * @file ui_service.c
 * @brief Central service for managing the User Interface.
 *
 * This service handles:
 * - UI Mode Switching (Face vs Clock)
 * - Input Event Handling (Taps, Spam Taps)
 * - Background Behavior (Moods, Blinking)
 * - LVGL Task Management
 */

#include "ui_service.h"

#include "clock_ui.h"
#include "face.h"
#include "rtc_service.h"
#include "service_manager.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "event_bus.h"

#include "esp_log.h"
#include "esp_random.h"
#include "lvgl.h"

static const char *TAG = "ui_service";

/* ==========================================================================
 * Private State & Configuration
 * ========================================================================== */

/**
 * @brief Internal state of the UI service.
 */
typedef struct {
  TaskHandle_t lvgl_task;     ///< Handle for the LVGL rendering task
  TaskHandle_t behavior_task; ///< Handle for the background behavior task
  SemaphoreHandle_t mutex;    ///< Mutex for thread safety (unused currently)
  ui_mode_t current_mode;     ///< Current active UI mode (Face or Clock)
  TimerHandle_t
      clock_to_face_timer; ///< Timer to auto-return to Face UI from Clock
} ui_service_state_t;

static ui_service_state_t priv = {
    .lvgl_task = NULL,
    .behavior_task = NULL,
    .mutex = NULL,
    .current_mode = UI_MODE_FACE, // Default to face UI
    .clock_to_face_timer = NULL,
};

// Mood/behavior state
static int current_mood_index = -1;
static bool is_blinking = false;
static bool angry_animation_playing =
    false; // Block taps during angry animation

/* ==========================================================================
 * Forward Declarations
 * ========================================================================== */

void ui_switch_mode(ui_mode_t new_mode);

/* ==========================================================================
 * Helper Functions (Moods, Behavior)
 * ========================================================================== */

/**
 * @brief Callback invoked when a "normal" mood animation finishes.
 *        Handles blinking logic and random idle movements.
 */
static void normal_mood_callback(emotion_t emotion, void *user_data) {
  if (is_blinking) {
    ESP_LOGI(TAG, "normal_mood_callback: Ending blink");
    is_blinking = false;
    emotion_t next_emotion =
        MOOD_NORMAL_EMOTIONS[esp_random() % sizeof(MOOD_NORMAL_EMOTIONS) /
                             sizeof(MOOD_NORMAL_EMOTIONS[0])];
    uint8_t loop_count = 2 + esp_random() % 6;
    ESP_LOGI(TAG, "Setting NORMAL mood with emotion %d with %d loop",
             next_emotion, loop_count);
    face_set_loop_count(next_emotion, loop_count);
    face_set_emotion(next_emotion);
    return;
  }
  // 30% chance to blink
  if ((esp_random() % 100) < 30) {
    ESP_LOGI(TAG, "normal_mood_callback: Initiating blink");
    uint8_t loop_count = 1 + esp_random() % 4;
    face_set_loop_count(EMOTION_BLINK, loop_count);
    is_blinking = true;
    emotion_t next_emotion = EMOTION_BLINK;
    face_set_emotion(next_emotion);
    return;
  }

  ESP_LOGI(TAG, "normal_mood_callback");
}

/**
 * @brief Sets the current mood of the face.
 * @param mood_index Index of the mood (0: Happy, 1: Furious, 2: Depressed, 3:
 * Normal)
 */
static void set_mood(int mood_index) {
  // Clear callback first to prevent race conditions
  face_set_animation_finished_callback(NULL, NULL);

  // Update current mood index before setting emotion
  current_mood_index = mood_index;

  // Reset blinking state when switching moods
  is_blinking = false;

  // Small delay to ensure callback clearing is processed
  vTaskDelay(pdMS_TO_TICKS(10));

  if (mood_index == 0) { // happy
    face_set_loop_count(EMOTION_EXCITED, 1 + esp_random() % 4);
    face_set_emotion(
        MOOD_HAPPY_EMOTIONS[esp_random() % sizeof(MOOD_HAPPY_EMOTIONS) /
                            sizeof(MOOD_HAPPY_EMOTIONS[0])]);
  } else if (mood_index == 1) { // furious
    face_set_loop_count(EMOTION_EXCITED, 1 + esp_random() % 4);
    face_set_emotion(
        MOOD_FURIOUS_EMOTIONS[esp_random() % sizeof(MOOD_FURIOUS_EMOTIONS) /
                              sizeof(MOOD_FURIOUS_EMOTIONS[0])]);
  } else if (mood_index == 2) { // depressed
    face_set_loop_count(EMOTION_EXCITED, 1 + esp_random() % 4);
    face_set_emotion(
        MOOD_DEPRESSED_EMOTIONS[esp_random() % sizeof(MOOD_DEPRESSED_EMOTIONS) /
                                sizeof(MOOD_DEPRESSED_EMOTIONS[0])]);
  } else if (mood_index == 3) { // normal
    face_set_animation_finished_callback(normal_mood_callback, NULL);
    normal_mood_callback(EMOTION_IDLE, NULL);
  }
}

/* ==========================================================================
 * Event Callbacks (Input, Time, Animation)
 * ========================================================================== */

/**
 * @brief Callback for RTC time updates. Updates the clock UI.
 */
static void time_update_callback(event_id_t event_id, void *event_data,
                                 void *user_data) {
  rtc_time_t *time = (rtc_time_t *)event_data;
  if (!time)
    return;

  // Update clock UI with real time from DS3231
  clock_ui_set_time(time->hour, time->minute, time->second);
  clock_ui_set_date(time->year, time->month, time->day, time->weekday);
}

/**
 * @brief Timer callback to automatically switch back to Face UI after
 * inactivity.
 */
static void clock_to_face_timer_callback(TimerHandle_t xTimer) {
  ESP_LOGI(TAG, "8-second timer expired, switching back to face UI");
  if (priv.current_mode == UI_MODE_CLOCK_UI) {
    ui_switch_mode(UI_MODE_FACE);
  }
}

/**
 * @brief Callback invoked when the Angry animation finishes.
 *        Resets the face color and resumes normal behavior.
 */
static void angry_animation_finished_cb(emotion_t emotion, void *user_data) {
  if (emotion == EMOTION_ANGRY) {
    ESP_LOGI(TAG, "Angry animation finished, resuming normal behavior");
    angry_animation_playing = false;

    // Clear the callback
    face_set_animation_finished_callback(NULL, NULL);

    // Reset animation color back to white
    face_set_animation_color(255, 255, 255);

    // Resume normal idle behavior first
    face_set_emotion(EMOTION_IDLE_LOOK_RIGHT);

    // Resume behavior task
    if (priv.behavior_task) {
      vTaskResume(priv.behavior_task);
      ESP_LOGI(TAG, "behavior_task RESUMED after angry animation");
    }
  }
}

/**
 * @brief Callback for SPAM TAP event. Triggers the Angry Face animation.
 */
static void spam_tap_detected_callback(event_id_t event_id, void *event_data,
                                       void *user_data) {
  ESP_LOGW(TAG, "SPAM TAP EVENT RECEIVED! Face is getting angry!");

  if (angry_animation_playing) {
    ESP_LOGI(TAG, "Already angry, ignoring");
    return;
  }

  angry_animation_playing = true;

  // Stop the clock timer
  if (priv.clock_to_face_timer) {
    xTimerStop(priv.clock_to_face_timer, 0);
  }

  // Switch to face mode and show angry emotion
  ui_switch_mode(UI_MODE_FACE);
  face_set_animation_color(255, 50, 50);
  face_set_emotion(EMOTION_ANGRY);
  // Suspend behavior task to prevent it from interrupting the angry animation
  if (priv.behavior_task) {
    vTaskSuspend(priv.behavior_task);
    ESP_LOGI(TAG, "behavior_task SUSPENDED for angry animation");
  }

  // Make eyes RED!

  // Set callback to know when animation finishes
  face_set_animation_finished_callback(angry_animation_finished_cb, NULL);
}

/**
 * @brief Callback for Single Tap event. Switches between Face and Clock UI.
 */
static void single_tap_detected_callback(event_id_t event_id, void *event_data,
                                         void *user_data) {
  // Ignore taps during angry animation
  if (angry_animation_playing) {
    return;
  }

  if (priv.current_mode == UI_MODE_FACE) {
    ui_switch_mode(UI_MODE_CLOCK_UI);
    ESP_LOGI(TAG, "Single tap detected - switching to clock UI");
  } else { // reset timer if already in clock UI
    if (priv.clock_to_face_timer) {
      xTimerReset(priv.clock_to_face_timer, 0);
    }
    ESP_LOGI(TAG, "Single tap detected - already in clock UI, resetting timer");
  }
}

/**
 * @brief Callback for Double Tap event. Toggles Clock Face style.
 */
static void double_tap_detected_callback(event_id_t event_id, void *event_data,
                                         void *user_data) {
  // Ignore taps during angry animation
  if (angry_animation_playing) {
    return;
  }

  clock_face_toggle_mode();
  ESP_LOGI(TAG, "Double tap detected");
}

/**
 * @brief Callback for Triple Tap event. (Currently unused).
 */
static void triple_tap_detected_callback(event_id_t event_id, void *event_data,
                                         void *user_data) {
  // Ignore taps during angry animation
  if (angry_animation_playing) {
    return;
  }

  ESP_LOGI(TAG, "Triple tap detected");
}

/* ==========================================================================
 * Task Functions
 * ========================================================================== */

/**
 * @brief LVGL Timer Handler Task.
 *        Periodically calls lv_timer_handler() to drive the UI.
 */
static void lvgl_task(void *pvParameters) {
  lv_lock();
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0, 0, 0), LV_PART_MAIN);
  lv_unlock();
  while (1) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

/**
 * @brief Background Behavior Task.
 *        Randomly changes the face mood/emotion over time.
 */
static void behavior_task(void *pvParameters) {
  face_set_emotion(EMOTION_IDLE_LOOK_RIGHT);
  while (1) {
    // Random 0–99
    uint32_t r = esp_random() % 100;
    int mood_index;
    if (r < 80) {
      // 80% chance → normal mood
      mood_index = 3;
      ESP_LOGI("behavior_task", "[Pick] NORMAL mood (80%%)");
    } else {
      // 20% chance → pick one of [0,1,2]
      mood_index = esp_random() % 3;
      ESP_LOGI("behavior_task", "[Pick] SPECIAL mood %d (20%%)", mood_index);
    }

    uint32_t random_delay_seconds = 10 + (esp_random() % 20); // 10–29 seconds
    uint32_t delay_ms = random_delay_seconds * 1000;

    ESP_LOGI("behavior_task",
             "[Delay] Waiting %lu seconds before next mood change",
             (unsigned long)random_delay_seconds);
    set_mood(mood_index);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void ui_set_mode(ui_mode_t mode) { priv.current_mode = mode; }

void ui_switch_mode(ui_mode_t new_mode) {
  if (priv.current_mode == new_mode) {
    ESP_LOGI(TAG, "Already in mode %d", new_mode);
    return;
  }
  ESP_LOGI(TAG, "Switching from mode %d to mode %d", priv.current_mode,
           new_mode);
  if (new_mode == UI_MODE_CLOCK_UI) {
    // Hide face, show clock
    face_hide();
    // Suspend behavior task (saves CPU)
    if (priv.behavior_task) {
      vTaskSuspend(priv.behavior_task);
      ESP_LOGI(TAG, "behavior_task SUSPENDED");
    }

    clock_ui_show();

    // Start 8-second timer to return to face UI
    if (priv.clock_to_face_timer) {
      xTimerReset(priv.clock_to_face_timer, 0);
    }
  } else if (new_mode == UI_MODE_FACE) {
    // Hide clock, show face
    clock_ui_hide();

    // Resume behavior task
    if (priv.behavior_task) {
      vTaskResume(priv.behavior_task);
      ESP_LOGI(TAG, "behavior_task RESUMED");
    }

    face_show();

    // Stop the timer when switching back to face UI
    if (priv.clock_to_face_timer) {
      xTimerStop(priv.clock_to_face_timer, 0);
    }
  }

  priv.current_mode = new_mode;
}

esp_err_t ui_init(service_t *svc) {
  // Initialize UI components (both start hidden by default)
  face_init();
  clock_ui_init();

  // Show the appropriate UI based on initial mode
  if (priv.current_mode == UI_MODE_CLOCK_UI) {
    ESP_LOGI(TAG, "Starting in CLOCK_UI mode");
    face_hide();
    clock_ui_show();
  } else if (priv.current_mode == UI_MODE_FACE) {
    ESP_LOGI(TAG, "Starting in FACE mode");
    clock_ui_hide();
    face_show();
  }

  // Create timer for auto-return to face UI (8 seconds, one-shot)
  priv.clock_to_face_timer =
      xTimerCreate("clock_to_face_timer", pdMS_TO_TICKS(8000), pdFALSE,
                   (void *)0, clock_to_face_timer_callback);

  if (priv.clock_to_face_timer == NULL) {
    ESP_LOGE(TAG, "Failed to create clock_to_face_timer");
    return ESP_FAIL;
  }

  // Subscribe to tap events
  event_bus_subscribe(APP_EVENT_SINGLE_TAP_DETECTED,
                      single_tap_detected_callback, NULL);
  event_bus_subscribe(APP_EVENT_DOUBLE_TAP_DETECTED,
                      double_tap_detected_callback, NULL);
  event_bus_subscribe(APP_EVENT_TRIPLE_TAP_DETECTED,
                      triple_tap_detected_callback, NULL);
  event_bus_subscribe(APP_EVENT_SPAM_TAP_DETECTED, spam_tap_detected_callback,
                      NULL);

  // Subscribe to RTC time updates
  event_bus_subscribe(APP_EVENT_TIME_UPDATE, time_update_callback, NULL);

  // Set initial clock mode
  clock_face_set_mode(CLOCK_FACE_OLD_FASHION);

  return ESP_OK;
}

esp_err_t ui_start(service_t *svc) {
  BaseType_t ret;

  // Create LVGL task
  ret = xTaskCreate(lvgl_task, "lvgl_task", 8192, NULL, 5, &priv.lvgl_task);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create LVGL task");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "LVGL task created successfully");

  // Create behavior task
  ret = xTaskCreate(behavior_task, "behavior_task", 8192, NULL, 5,
                    &priv.behavior_task);
  if (ret != pdPASS) {
    ESP_LOGE(TAG, "Failed to create behavior task");
    return ESP_FAIL;
  }
  ESP_LOGI(TAG, "Behavior task created successfully");

  // Suspend behavior task if starting in clock UI mode
  if (priv.current_mode == UI_MODE_CLOCK_UI) {
    vTaskSuspend(priv.behavior_task);
  }

  return ESP_OK;
}

/* ==========================================================================
 * Service Registration
 * ========================================================================== */

static const service_ops_t UI_OPS = {
    .init = ui_init,
    .start = ui_start,
};

DEFINE_SERVICE(ui_service, &UI_OPS);