#include "face.h"
#include "esp_log.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"

#include "./assets/idle_blink/idle_blink.h"

#include "./assets/excited_start/excited_start.h"
#include "./assets/excited_loop/excited_loop.h"

#include "./assets/angry_start/angry_start.h"
#include "./assets/angry_loop/angry_loop.h"
// Add more emotion includes as needed

static const char *TAG = "face";
static lv_obj_t *animimg = NULL;

typedef enum
{
  ANIM_STATE_START,
  ANIM_STATE_LOOP,
  ANIM_STATE_REVERSE,
  ANIM_STATE_TRANSITIONING
} anim_state_t;

typedef struct
{
  const void **start_frames;
  uint32_t start_frame_count;
  uint16_t start_end_frame_duration;

  const void **loop_frames;
  uint32_t loop_frame_count;
  uint16_t loop_frame_duration;

} emotion_assets_t;

static struct
{
  emotion_t current_emotion;
  emotion_t target_emotion;
  anim_state_t anim_state;
  bool transition_pending;
  uint16_t loop_repeat_count;

  face_animation_finished_cb_t user_callback;
  void *user_callback_arg;

} face_state = {
    .current_emotion = EMOTION_IDLE,
    .target_emotion = EMOTION_IDLE,
    .anim_state = ANIM_STATE_START,
    .transition_pending = false,
    .user_callback = NULL,
    .user_callback_arg = NULL};

void face_set_animation_finished_callback(face_animation_finished_cb_t callback, void *user_data)
{
  face_state.user_callback = callback;
  face_state.user_callback_arg = user_data;
}

// Emotion asset definitions
static const emotion_assets_t emotion_assets[] = {
    [EMOTION_IDLE] = {
        .start_frames = NULL,
        .start_frame_count = 0,
        .start_end_frame_duration = 0,
        .loop_frames = NULL,
        .loop_frame_count = 0,
        .loop_frame_duration = 0
    },
    [EMOTION_BLINK] = {
        .start_frames = (const void **)idle_blink_anim_frames,
        .start_frame_count = IDLE_BLINK_ANIM_FRAME_COUNT,
        .start_end_frame_duration = 1000,
        .loop_frames = (const void **)idle_blink_anim_frames,
        .loop_frame_count = IDLE_BLINK_ANIM_FRAME_COUNT,
        .loop_frame_duration = 1000
    },
    [EMOTION_EXCITED] = {
        .start_frames = (const void **)excited_start_anim_frames,
        .start_frame_count = EXCITED_START_ANIM_FRAME_COUNT,
        .start_end_frame_duration = 1000,
        .loop_frames = (const void **)excited_loop_anim_frames,
        .loop_frame_count = EXCITED_LOOP_ANIM_FRAME_COUNT,
        .loop_frame_duration = 1000
    },
    [EMOTION_ANGRY] = {
        .start_frames = (const void **)angry_start_anim_frames,
        .start_frame_count = ANGRY_START_ANIM_FRAME_COUNT,
        .start_end_frame_duration = 1000,
        .loop_frames = (const void **)angry_loop_anim_frames,
        .loop_frame_count = ANGRY_LOOP_ANIM_FRAME_COUNT,
        .loop_frame_duration = 1000
    },
    // Add more emotions here
};

// Forward declarations
static void play_start_animation(void);
static void play_loop_animation(void);
static void play_reverse_animation(void);
static void handle_transition(void);

static void configure_animation(const void **frames, uint32_t count, uint32_t duration,
                                uint16_t repeat, lv_anim_completed_cb_t callback)
{
  if (frames)
  {
    lv_animimg_set_src(animimg, frames, count);
  }
  lv_animimg_set_duration(animimg, duration);
  lv_animimg_set_repeat_count(animimg, repeat);

  lv_obj_set_style_image_recolor(animimg, lv_color_white(), 0);
  lv_obj_set_style_image_recolor_opa(animimg, LV_OPA_COVER, 0);
  lv_obj_center(animimg);

  lv_anim_t *anim = lv_animimg_get_anim(animimg);
  if (anim && callback)
  {
    lv_anim_set_completed_cb(anim, callback);
  }

  lv_animimg_start(animimg);
  lv_image_set_scale(animimg, 384);
}

static void on_reverse_finished(lv_anim_t *anim)
{
  ESP_LOGI(TAG, "Reverse animation finished");
  face_state.anim_state = ANIM_STATE_START;

  if (face_state.transition_pending)
  {
    // Start the new emotion
    handle_transition();
  }
  else
  {
    if (face_state.user_callback)
    {
      face_state.user_callback(face_state.current_emotion, face_state.user_callback_arg);
    }
    // Loop current emotion
    vTaskDelay(pdMS_TO_TICKS(1000));
    play_start_animation();
  }
}

static void on_loop_finished(lv_anim_t *anim)
{
  ESP_LOGI(TAG, "Loop animation finished");

  if (face_state.transition_pending)
  {
    // Transition requested, play reverse
    ESP_LOGI(TAG, "Transition pending, playing reverse");
    play_reverse_animation();
  }
  else
  {
    // Continue looping
    play_reverse_animation();
  }
}

static void on_start_finished(lv_anim_t *anim)
{
  ESP_LOGI(TAG, "Start animation finished");
  face_state.anim_state = ANIM_STATE_LOOP;
  play_loop_animation();
}

void face_set_loop_count(uint32_t count)
{
  if (count == 0)
  {
    ESP_LOGW(TAG, "Loop count must be at least 1, setting to 1");
    count = 1;
  }
  face_state.loop_repeat_count = count;
  ESP_LOGI(TAG, "Loop repeat count set to %lu", count);
}

static void play_start_animation(void)
{
  emotion_t emotion = face_state.current_emotion;

  if (emotion == EMOTION_IDLE || !emotion_assets[emotion].start_frames)
  {
    return;
  }

  face_state.anim_state = ANIM_STATE_START;

  configure_animation(emotion_assets[emotion].start_frames,
                      emotion_assets[emotion].start_frame_count,
                      emotion_assets[emotion].start_end_frame_duration, 1, on_start_finished);
}

static void play_loop_animation(void)
{
  emotion_t emotion = face_state.current_emotion;

  if (emotion == EMOTION_IDLE || !emotion_assets[emotion].loop_frames)
  {
    return;
  }

  face_state.anim_state = ANIM_STATE_LOOP;

  configure_animation(emotion_assets[emotion].loop_frames,
                      emotion_assets[emotion].loop_frame_count,
                      emotion_assets[emotion].loop_frame_duration, face_state.loop_repeat_count, on_loop_finished);
}

static void play_reverse_animation(void)
{
  emotion_t emotion = face_state.current_emotion;

  if (emotion == EMOTION_IDLE || !emotion_assets[emotion].start_frames)
  {
    return;
  }

  face_state.anim_state = ANIM_STATE_REVERSE;

  lv_animimg_set_src_reverse(animimg, emotion_assets[emotion].start_frames,
                             emotion_assets[emotion].start_frame_count);
  configure_animation(NULL, 0, emotion_assets[emotion].start_end_frame_duration, 1, on_reverse_finished);
}

static void handle_transition(void)
{
  ESP_LOGI(TAG, "Transitioning from %d to %d", face_state.current_emotion, face_state.target_emotion);

  face_state.current_emotion = face_state.target_emotion;
  face_state.transition_pending = false;
  face_state.anim_state = ANIM_STATE_START;

  play_start_animation();
}

// Public API
void face_init(void)
{
  if (animimg == NULL)
  {
    animimg = lv_animimg_create(lv_screen_active());
  }

  face_state.current_emotion = EMOTION_IDLE;
  face_state.target_emotion = EMOTION_IDLE;
  face_state.anim_state = ANIM_STATE_START;
  face_state.loop_repeat_count = 1; // Default to loop once
  face_state.transition_pending = false;
}

void face_set_emotion(emotion_t new_emotion)
{
  if (new_emotion == face_state.current_emotion)
  {
    ESP_LOGI(TAG, "Already in emotion %d", new_emotion);
    return;
  }

  ESP_LOGI(TAG, "Requesting emotion change to %d", new_emotion);
  face_state.target_emotion = new_emotion;
  face_state.transition_pending = true;

  // If we're idle or just starting, transition immediately
  if (face_state.current_emotion == EMOTION_IDLE ||
      face_state.anim_state == ANIM_STATE_START)
  {
    handle_transition();
  }
  // Otherwise wait for current animation to finish (handled in callbacks)
}

emotion_t face_get_emotion(void)
{
  return face_state.current_emotion;
}