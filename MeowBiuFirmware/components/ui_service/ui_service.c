#include "service_manager.h"

#include "ui_service.h"
#include "face.h"
#include "clock_ui.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "event_bus.h"

#include "lvgl.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "ui_service";

int emotion_index = 0;

typedef struct
{
    TaskHandle_t lvgl_task;
    TaskHandle_t behavior_task;
    TaskHandle_t clock_ui_task;
    SemaphoreHandle_t mutex;
    ui_mode_t current_mode;
} ui_service_state_t;

static ui_service_state_t priv = {
    .lvgl_task = NULL,
    .behavior_task = NULL,
    .clock_ui_task = NULL,
    .mutex = NULL,
    .current_mode = UI_MODE_CLOCK_UI, // Default to clock UI
};

// Track current mood to prevent callback interference
static int current_mood_index = -1;
// Track the current normal emotion so we can return to it after blinking
// Track if we're currently blinking (to return to normal emotion after)
static bool is_blinking = false;

// void behavior_task_done_callback(emotion_t emotion, void *user_data)
// {
//     static const emotion_t emotion_sequence[] = {
//         EMOTION_EXCITED,
//         EMOTION_ANGRY,
//         EMOTION_BLINK,
//         EMOTION_IDLE_LOOK_RIGHT,
//         EMOTION_IDLE_LOOK_LEFT,
//         EMOTION_SAD,
//         EMOTION_SCARED,
//         EMOTION_DGAF};
//     static const int emotion_sequence_length = sizeof(emotion_sequence) / sizeof(emotion_sequence[0]);

//     // Move to next emotion in sequence
//     emotion_index = (emotion_index + 1) % emotion_sequence_length;
//     emotion_t next_emotion = emotion_sequence[emotion_index];
//     face_set_emotion(next_emotion);
// }

void ui_switch_mode(ui_mode_t new_mode)
{
    if (priv.current_mode == new_mode) {
        ESP_LOGI(TAG, "Already in mode %d", new_mode);
        return;
    }
    ESP_LOGI(TAG, "Switching from mode %d to mode %d", priv.current_mode, new_mode);
    lv_lock();
    if (new_mode == UI_MODE_CLOCK_UI) {
        // Hide face, show clock
        face_hide(); 
        
        // Suspend behavior task (saves CPU)
        if (priv.behavior_task) {
            vTaskSuspend(priv.behavior_task);
        }
        
        // Resume clock task
        if (priv.clock_ui_task) {
            vTaskResume(priv.clock_ui_task);
        }
        clock_ui_show();
    }
    else if (new_mode == UI_MODE_FACE) {
        // Hide clock, show face
        clock_ui_hide();
        
        // Suspend clock task
        if (priv.clock_ui_task) {
            vTaskSuspend(priv.clock_ui_task);
        }
        
        // Resume behavior task
        if (priv.behavior_task) {
            vTaskResume(priv.behavior_task);
        }
        
        face_show();
    }
    
    lv_unlock();
    
    priv.current_mode = new_mode;
}


void normal_mood_callback(emotion_t emotion, void *user_data)
{
    if (is_blinking)
    {
        ESP_LOGI(TAG, "normal_mood_callback: Ending blink");
        is_blinking = false;
        emotion_t next_emotion = MOOD_NORMAL_EMOTIONS[esp_random() % sizeof(MOOD_NORMAL_EMOTIONS) / sizeof(MOOD_NORMAL_EMOTIONS[0])];
        uint8_t loop_count = 2 + esp_random() % 6;
        ESP_LOGI(TAG, "Setting NORMAL mood with emotion %d with %d loop", next_emotion, loop_count);
        face_set_loop_count(next_emotion, loop_count);
        face_set_emotion(next_emotion);
        return;
    }
    // 30% chance to blink
    if ((esp_random() % 100) < 30)
    {
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

void set_mood(int mood_index)
{
    // Clear callback first to prevent race conditions
    face_set_animation_finished_callback(NULL, NULL);

    // Update current mood index before setting emotion
    // This ensures callbacks can check if they should still be active
    current_mood_index = mood_index;

    // Reset blinking state when switching moods
    is_blinking = false;

    // Small delay to ensure callback clearing is processed
    vTaskDelay(pdMS_TO_TICKS(10));

    if (mood_index == 0) // happy
    {
        face_set_loop_count(EMOTION_EXCITED, 1 + esp_random() % 4);
        face_set_emotion(MOOD_HAPPY_EMOTIONS[esp_random() % sizeof(MOOD_HAPPY_EMOTIONS) / sizeof(MOOD_HAPPY_EMOTIONS[0])]);
    }
    else if (mood_index == 1) // furious
    {
        face_set_loop_count(EMOTION_EXCITED, 1 + esp_random() % 4);
        face_set_emotion(MOOD_FURIOUS_EMOTIONS[esp_random() % sizeof(MOOD_FURIOUS_EMOTIONS) / sizeof(MOOD_FURIOUS_EMOTIONS[0])]);
    }
    else if (mood_index == 2) // depressed
    {
        face_set_loop_count(EMOTION_EXCITED, 1 + esp_random() % 4);
        face_set_emotion(MOOD_DEPRESSED_EMOTIONS[esp_random() % sizeof(MOOD_DEPRESSED_EMOTIONS) / sizeof(MOOD_DEPRESSED_EMOTIONS[0])]);
    }
    else if (mood_index == 3) // normal
    {
        face_set_animation_finished_callback(normal_mood_callback, NULL); // Re-register callback
        normal_mood_callback(EMOTION_IDLE, NULL);                         // EMOTION_IDLE is a placeholder here // this goes into a loop with random blinks and normal emotions
    }
}

void behavior_task(void *pvParameters)
{
    face_set_emotion(EMOTION_IDLE_LOOK_RIGHT);
    while (1)
    {
        // Random 0–99
        uint32_t r = esp_random() % 100;
        int mood_index;
        if (r < 80)
        {
            // 80% chance → normal mood
            mood_index = 3;
            ESP_LOGI("behavior_task", "[Pick] NORMAL mood (80%%)");
        }
        else
        {
            // 20% chance → pick one of [0,1,2]
            mood_index = esp_random() % 3;
            ESP_LOGI("behavior_task", "[Pick] SPECIAL mood %d (20%%)", mood_index);
        }

        uint32_t random_delay_seconds = 10 + (esp_random() % 20); // 10–29 seconds
        uint32_t delay_ms = random_delay_seconds * 1000;          // milliseconds

        ESP_LOGI("behavior_task",
                 "[Delay] Waiting %lu seconds before next mood change",
                 (unsigned long)random_delay_seconds);
        set_mood(mood_index);
        vTaskDelay(pdMS_TO_TICKS(delay_ms)); // Delay before next mood change
    }
}

void clock_ui_task(void *pvParameters)
{
    // Initialize the clock UI layout with LVGL lock
    clock_ui_layout_init();
    
    while (1)
    {
        static int day = 1;
        day++;
        
        // Lock LVGL before updating UI elements
        clock_ui_set_time(2024, 1, day, 1);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void lvgl_task(void *pvParameters)
{
    lv_lock();
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0, 0, 0), LV_PART_MAIN);
    lv_unlock();
    while (1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void ui_set_mode(ui_mode_t mode)
{
    priv.current_mode = mode;
}

void single_tap_detected_callback(event_id_t event_id, void *event_data, void *user_data)
{
    if(priv.current_mode == UI_MODE_CLOCK_UI){
        ui_switch_mode(UI_MODE_FACE);
    }
    else{
        ui_switch_mode(UI_MODE_CLOCK_UI);
    }
    ESP_LOGI(TAG, "Single tap detected");
}

void double_tap_detected_callback(event_id_t event_id, void *event_data, void *user_data)
{
    ESP_LOGI(TAG, "Double tap detected");
}

void triple_tap_detected_callback(event_id_t event_id, void *event_data, void *user_data)
{
    ESP_LOGI(TAG, "Triple tap detected");
}

esp_err_t ui_init(service_t *svc)
{
    ui_set_mode(UI_MODE_FACE);
    face_init();
    clock_ui_init();
    event_bus_subscribe(APP_EVENT_SINGLE_TAP_DETECTED, single_tap_detected_callback, NULL);
    event_bus_subscribe(APP_EVENT_DOUBLE_TAP_DETECTED, double_tap_detected_callback, NULL);
    event_bus_subscribe(APP_EVENT_TRIPLE_TAP_DETECTED, triple_tap_detected_callback, NULL);
    return ESP_OK;
}

esp_err_t ui_start(service_t *svc)
{
    BaseType_t ret = xTaskCreate(lvgl_task, "lvgl_task", 8192, NULL, 5, &priv.lvgl_task);

    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "LVGL task created successfully");

    ret = xTaskCreate(behavior_task, "behavior_task", 8192, NULL, 5, &priv.behavior_task);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create behavior task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Behavior task created successfully");
    
    vTaskSuspend(priv.behavior_task);

    ret = xTaskCreate(clock_ui_task, "clock_ui_task", 2048, NULL, 5, &priv.clock_ui_task);
    if (ret != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create clock UI task");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Clock UI task created successfully");

    
    return ESP_OK;
}

static const service_ops_t UI_OPS = {
    .init = ui_init,
    .start = ui_start,
    // .stop, .deinit optional for now
};

DEFINE_SERVICE(ui_service, &UI_OPS);