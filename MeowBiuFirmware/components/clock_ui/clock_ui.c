/**
 * @file clock_ui.c
 * @brief Manages the Clock User Interface.
 *
 * This file handles the rendering of both Digital (Text) and Analog clock faces
 * using LVGL. It supports mode switching, animations (show/hide), and
 * asynchronous time updates.
 */

#include "clock_ui.h"
#include "esp_log.h"
#include "lvgl.h"
#include "network_service.h"
#include <stdlib.h>

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "clock_faces/face_registry/face_registry.h"

static const char *TAG = "clock_ui";

// Struct for passing time data to async callback
typedef struct
{
  int hour;
  int minute;
  int second;
} clock_time_t;

static size_t current_face_index = 2;
static bool current_face_supports_weather(void)
{
  const clock_face_t *face = clock_face_at(current_face_index);
  return face && face->weather_compatible && face->set_weather;
}

/* ==========================================================================
 * Private Helper Functions (Layout & Animations)
 * ========================================================================== */

/**
 * @brief Creates the layout for the Digital (Text) Clock.
 */
// static void create_text_clock_layout(void) {
//   // Create time label - white text, centered (shows HH:MM:SS)
//   time_label = lv_label_create(clock_container);
//   lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
//   lv_obj_set_style_text_color(time_label, lv_color_make(255, 255, 255), 0);
//   lv_label_set_text(time_label, "12:34:56");
//   lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);

//   // Create date label - white text
//   date_label = lv_label_create(clock_container);
//   lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
//   lv_obj_set_style_text_color(date_label, lv_color_make(255, 255, 255), 0);
//   lv_obj_set_style_text_opa(date_label, LV_OPA_80, 0);
//   lv_label_set_text(date_label, "2024-01-01");
//   lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 20);

//   // Create day of week label - white text
//   day_label = lv_label_create(clock_container);
//   lv_obj_set_style_text_font(day_label, &lv_font_montserrat_20, 0);
//   lv_obj_set_style_text_color(day_label, lv_color_make(255, 255, 255), 0);
//   lv_obj_set_style_text_opa(day_label, LV_OPA_70, 0);
//   lv_label_set_text(day_label, "MONDAY");
//   lv_obj_align(day_label, LV_ALIGN_CENTER, 0, 45);
// }

/**
 * @brief Animation callback for opacity.
 */
static void anim_opa_cb(void *var, int32_t v)
{
  lv_obj_set_style_opa(var, v, 0);
}

/**
 * @brief Async callback to update the clock UI from the LVGL thread.
 */
static void clock_ui_update_async_cb(void *data)
{
  clock_time_t *time_data = (clock_time_t *)data;
  int hour = time_data->hour;
  int minute = time_data->minute;
  int second = time_data->second;
  clock_face_at(current_face_index)->set_time(hour, minute, second);
  free(time_data);
}

typedef struct
{
  weather_data_t weather;
} clock_weather_t;

static void clock_ui_weather_async_cb(void *data)
{
  clock_weather_t *w = (clock_weather_t *)data;
  const clock_face_t *face = clock_face_at(current_face_index);
  if (face && face->weather_compatible && face->set_weather)
    face->set_weather(&w->weather);
  free(w);
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void clock_ui_init(void)
{
  for (size_t i = 0; i < clock_face_count(); i++)
  {
    clock_face_at(i)->init();
  }

}

void clock_face_set_mode(size_t new_face_index)
{
  switch_to(new_face_index);
}


void clock_ui_show(void)
{
  clock_face_at(current_face_index)->show();
  // if (lv_obj_has_flag(clock_container, LV_OBJ_FLAG_HIDDEN))
  // {
  //   lv_obj_clear_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
  //   lv_obj_set_style_opa(clock_container, LV_OPA_TRANSP, 0);

  //   // Fade in animation with ease-out (pops in)
  //   lv_anim_t a;
  //   lv_anim_init(&a);
  //   lv_anim_set_var(&a, clock_container);
  //   lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
  //   lv_anim_set_time(&a, 200);
  //   lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  //   lv_anim_set_exec_cb(&a, anim_opa_cb);
  //   lv_anim_start(&a);
  // }
}

void clock_ui_hide(void)
{
  clock_face_at(current_face_index)->hide();
  // if (!lv_obj_has_flag(clock_container, LV_OBJ_FLAG_HIDDEN))
  // {
  //   // Fade out animation with ease-in (fades away)
  //   lv_anim_t a;
  //   lv_anim_init(&a);
  //   lv_anim_set_var(&a, clock_container);
  //   lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
  //   lv_anim_set_time(&a, 200);
  //   lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
  //   lv_anim_set_exec_cb(&a, anim_opa_cb);
  //   lv_anim_set_completed_cb(&a, anim_hide_ready_cb);
  //   lv_anim_start(&a);
  // }
}

void clock_ui_set_date(int year, int month, int day, int weekoftheday)
{
  clock_face_at(current_face_index)->set_date(year, month, day, weekoftheday);
  
}

void clock_ui_set_time(int hour, int minute, int second)
{
  clock_time_t *time_data = malloc(sizeof(clock_time_t));
  if (time_data)
  {
    time_data->hour = hour;
    time_data->minute = minute;
    time_data->second = second;
    lv_async_call(clock_ui_update_async_cb, time_data);
  }
}

bool clock_ui_weather_supported(void)
{
  return current_face_supports_weather();
}

void clock_ui_set_weather(const weather_data_t *data)
{
  if (!data || !current_face_supports_weather())
    return;

  clock_weather_t *payload = malloc(sizeof(clock_weather_t));
  if (!payload)
    return;
  payload->weather = *data;
  lv_async_call(clock_ui_weather_async_cb, payload);
}
