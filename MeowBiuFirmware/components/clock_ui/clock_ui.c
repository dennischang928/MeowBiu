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
#include <math.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

static const char *TAG = "clock_ui";

/* ==========================================================================
 * Private State & Configuration
 * ========================================================================== */

// LVGL objects for the clock UI
static lv_obj_t *clock_container = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *seconds_label = NULL; // Seconds display for text mode
static lv_obj_t *date_label = NULL;
static lv_obj_t *day_label = NULL;

// Analog clock objects
static lv_obj_t *scale = NULL;
static lv_obj_t *hour_hand = NULL;
static lv_obj_t *minute_hand = NULL;
static lv_obj_t *second_hand = NULL;
static lv_obj_t *center_dot = NULL;

// Current clock face mode
static clock_face_t current_mode = CLOCK_FACE_OLD_FASHION;

// State tracking for mode switching (restores time/date after switch)
static int last_year = 2024;
static int last_month = 1;
static int last_day = 1;
static int last_weekday = 1;
static int last_hour = 12;
static int last_minute = 0;
static int last_second = 0;

// Rendering state (moved to file scope to allow reset)
static int last_minute_render = -1;
static int last_hour_render = -1;

// Arrays for labels
static const char *days[] = {"MONDAY", "TUESDAY",  "WEDNESDAY", "THURSDAY",
                             "FRIDAY", "SATURDAY", "SUNDAY"};
// 0:Monday, 1:Tuesday, 2:Wednesday, 3:Thursday, 4:Friday, 5:Saturday, 6:Sunday
static const char *scale_labels[] = {"12", "1", "2", "3",  "4",  "5", "6",
                                     "7",  "8", "9", "10", "11", NULL};

// Struct for passing time data to async callback
typedef struct {
  int hour;
  int minute;
  int second;
} clock_time_t;

// Scale values: 256 = 100%
#define CLOCK_SCALE_NORMAL 256
#define CLOCK_SCALE_SMALL 230 // ~90% of normal

/* ==========================================================================
 * Private Helper Functions (Layout & Animations)
 * ========================================================================== */

/**
 * @brief Creates the layout for the Digital (Text) Clock.
 */
static void create_text_clock_layout(void) {
  // Create time label - white text, centered (shows HH:MM:SS)
  time_label = lv_label_create(clock_container);
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
  lv_obj_set_style_text_color(time_label, lv_color_make(255, 255, 255), 0);
  lv_label_set_text(time_label, "12:34:56");
  lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);

  // Create date label - white text
  date_label = lv_label_create(clock_container);
  lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(date_label, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_text_opa(date_label, LV_OPA_80, 0);
  lv_label_set_text(date_label, "2024-01-01");
  lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 20);

  // Create day of week label - white text
  day_label = lv_label_create(clock_container);
  lv_obj_set_style_text_font(day_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(day_label, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_text_opa(day_label, LV_OPA_70, 0);
  lv_label_set_text(day_label, "MONDAY");
  lv_obj_align(day_label, LV_ALIGN_CENTER, 0, 45);
}

/**
 * @brief Creates the layout for the Analog Clock.
 */
static void create_analog_clock_layout(void) {
  int width = (LV_HOR_RES < LV_VER_RES ? LV_HOR_RES : LV_VER_RES);
  int scale_size = width - 0; // Add padding to prevent overflow

  // Create scale for ticks
  scale = lv_scale_create(clock_container);
  lv_obj_set_size(scale, scale_size, scale_size);
  lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
  lv_scale_set_post_draw(scale, true); // Draw on top of background
  lv_obj_center(scale);

  // Configure range and ticks (60 minutes)
  lv_scale_set_total_tick_count(scale, 61);
  lv_scale_set_major_tick_every(scale, 5);    // 5 minutes per major tick2
  lv_scale_set_label_show(scale, true);       // Show numbers
  lv_scale_set_text_src(scale, scale_labels); // Custom "12, 1, 2..." labels
  lv_scale_set_rotation(scale, 270);          // 0 at top
  lv_scale_set_angle_range(scale, 360);

  // Style the ticks and labels
  lv_obj_set_style_line_color(scale, lv_color_make(255, 255, 255),
                              LV_PART_ITEMS); // Minor ticks
  lv_obj_set_style_line_color(scale, lv_color_make(255, 255, 255),
                              LV_PART_INDICATOR);        // Major ticks
  lv_obj_set_style_length(scale, 5, LV_PART_ITEMS);      // Minor tick length
  lv_obj_set_style_length(scale, 10, LV_PART_INDICATOR); // Major tick length
  lv_obj_set_style_line_width(scale, 2, LV_PART_ITEMS);
  lv_obj_set_style_line_width(scale, 4, LV_PART_INDICATOR);

  // Style the numbering
  lv_obj_set_style_text_color(scale, lv_color_make(255, 255, 255),
                              LV_PART_INDICATOR);
  lv_obj_set_style_text_font(scale, &lv_font_montserrat_20, LV_PART_INDICATOR);

  // Create hour hand
  hour_hand = lv_line_create(clock_container);
  lv_obj_set_style_line_color(hour_hand, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_line_width(hour_hand, 6, 0);
  lv_obj_set_style_line_rounded(hour_hand, true, 0);
  lv_obj_set_size(hour_hand, scale_size, scale_size);
  lv_obj_center(hour_hand);

  // Create minute hand
  minute_hand = lv_line_create(clock_container);
  lv_obj_set_style_line_color(minute_hand, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_line_width(minute_hand, 4, 0);
  lv_obj_set_style_line_rounded(minute_hand, true, 0);
  lv_obj_set_size(minute_hand, scale_size, scale_size);
  lv_obj_center(minute_hand);

  // Create second hand
  second_hand = lv_line_create(clock_container);
  lv_obj_set_style_line_color(second_hand, lv_color_make(255, 200, 200), 0);
  lv_obj_set_style_line_width(second_hand, 2, 0);
  lv_obj_set_style_line_rounded(second_hand, true, 0);
  lv_obj_set_size(second_hand, scale_size, scale_size);
  lv_obj_center(second_hand);

  // Set padding of clock hands to 0 to fix alignment issue
  lv_obj_set_style_pad_all(hour_hand, 0, 0);
  lv_obj_set_style_pad_all(minute_hand, 0, 0);
  lv_obj_set_style_pad_all(second_hand, 0, 0);

  // Create center dot
  center_dot = lv_obj_create(clock_container);
  lv_obj_set_size(center_dot, 12, 12);
  lv_obj_set_style_radius(center_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(center_dot, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_border_width(center_dot, 0, 0);
  lv_obj_align(center_dot, LV_ALIGN_CENTER, 0, 0);

  // Create date label above center dot
  date_label = lv_label_create(clock_container);
  lv_obj_set_style_text_font(date_label, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(date_label, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_text_opa(date_label, LV_OPA_80, 0);
  lv_label_set_text(date_label, "12/05");
  lv_obj_align(date_label, LV_ALIGN_CENTER, 0, -30);

  // Create day of week label below center dot
  day_label = lv_label_create(clock_container);
  lv_obj_set_style_text_font(day_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(day_label, lv_color_make(255, 255, 255), 0);
  lv_obj_set_style_text_opa(day_label, LV_OPA_60, 0);
  lv_label_set_text(day_label, "MONDAY");
  lv_obj_align(day_label, LV_ALIGN_CENTER, 0, 35);
}

/**
 * @brief Animation callback for opacity.
 */
static void anim_opa_cb(void *var, int32_t v) {
  lv_obj_set_style_opa(var, v, 0);
}

/**
 * @brief Animation callback for scale (unused currently).
 */
static void anim_scale_cb(void *var, int32_t v) {
  lv_obj_set_style_transform_scale(var, v, 0);
}

/**
 * @brief Animation callback when hide animation completes.
 */
static void anim_hide_ready_cb(lv_anim_t *a) {
  lv_obj_add_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_transform_scale(clock_container, 256, 0); // Reset scale
}

/**
 * @brief Async callback to update the clock UI from the LVGL thread.
 */
static void clock_ui_update_async_cb(void *data) {
  clock_time_t *time_data = (clock_time_t *)data;
  int hour = time_data->hour;
  int minute = time_data->minute;
  int second = time_data->second;

  // No need for lv_lock() here as this runs in LVGL thread

  if (current_mode == CLOCK_FACE_TEXT) {
    // Update time label for text mode (hh:MM:SS in 12-hour format)
    if (time_label) {
      int display_hour = hour % 12;
      display_hour = display_hour == 0 ? 12 : display_hour;
      char time_str[16];
      snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", display_hour,
               minute, second);
      lv_label_set_text(time_label, time_str);
    }
  } else {
    // Update analog clock hands
    int center_x = LV_HOR_RES / 2;
    int center_y = LV_VER_RES / 2;
    // Radius should match the scale size (scale_size / 2)
    // scale_size was width - 40, so radius is (width - 40) / 2
    int radius = ((LV_HOR_RES < LV_VER_RES ? LV_HOR_RES : LV_VER_RES) - 40) / 2;

    // Only update Hour and Minute hands if the minute has changed
    if (minute != last_minute_render || hour != last_hour_render) {
      // Calculate angles (0 degrees = 12 o'clock, clockwise)
      float hour_angle =
          ((hour % 12) * 30.0f + minute * 0.5f - 90.0f) * M_PI / 180.0f;
      float minute_angle = (minute * 6.0f - 90.0f) * M_PI / 180.0f;

      // Hour hand (50% of radius)
      static lv_point_precise_t hour_points[2];
      hour_points[0].x = center_x;
      hour_points[0].y = center_y;
      hour_points[1].x = center_x + (int)(cos(hour_angle) * radius * 0.5f);
      hour_points[1].y = center_y + (int)(sin(hour_angle) * radius * 0.5f);

      if (hour_hand) {
        lv_line_set_points(hour_hand, hour_points, 2);
        lv_obj_invalidate(hour_hand);
      }

      // Minute hand (75% of radius)
      static lv_point_precise_t minute_points[2];
      minute_points[0].x = center_x;
      minute_points[0].y = center_y;
      minute_points[1].x = center_x + (int)(cos(minute_angle) * radius * 0.75f);
      minute_points[1].y = center_y + (int)(sin(minute_angle) * radius * 0.75f);

      if (minute_hand) {
        lv_line_set_points(minute_hand, minute_points, 2);
        lv_obj_invalidate(minute_hand);
      }

      last_minute_render = minute;
      last_hour_render = hour;
    }

    // Second hand always updates
    float second_angle = (second * 6.0f - 90.0f) * M_PI / 180.0f;

    // Second hand (90% of radius)
    static lv_point_precise_t second_points[2];
    second_points[0].x = center_x;
    second_points[0].y = center_y;
    second_points[1].x = center_x + (int)(cos(second_angle) * radius * 0.9f);
    second_points[1].y = center_y + (int)(sin(second_angle) * radius * 0.9f);

    if (second_hand) {
      lv_line_set_points(second_hand, second_points, 2);
      lv_obj_invalidate(second_hand);
    }
  }

  free(time_data);
}

/* ==========================================================================
 * Public API
 * ========================================================================== */

void clock_ui_layout_init(void) {
  lv_lock();
  ESP_LOGI(TAG, "Clock UI initialized with minimalistic design.");
  lv_obj_t *scr = lv_scr_act();

  // Create main container with solid black background
  clock_container = lv_obj_create(scr);
  lv_obj_set_size(clock_container, LV_HOR_RES, LV_VER_RES);
  lv_obj_center(clock_container);
  lv_obj_set_style_bg_color(clock_container, lv_color_make(0, 0, 0),
                            0); // Solid black
  lv_obj_set_style_border_width(clock_container, 0, 0);
  lv_obj_clear_flag(clock_container, LV_OBJ_FLAG_SCROLLABLE);

  // Create layout based on current mode
  if (current_mode == CLOCK_FACE_TEXT) {
    create_text_clock_layout();
  } else {
    create_analog_clock_layout();
  }

  lv_unlock();
}

void clock_ui_init(void) { clock_ui_layout_init(); }

void clock_face_set_mode(clock_face_t mode) {
  if (current_mode == mode)
    return;

  lv_lock();
  current_mode = mode;

  // Clear existing layout
  if (clock_container != NULL) {
    lv_obj_del(clock_container);
    clock_container = NULL;
    time_label = NULL;
    date_label = NULL;
    day_label = NULL;
    scale = NULL;
    hour_hand = NULL;
    minute_hand = NULL;
    second_hand = NULL;
    center_dot = NULL;
  }

  // Reset rendering state to force redraw of hands
  last_minute_render = -1;
  last_hour_render = -1;

  // Recreate layout with new mode
  lv_unlock();
  clock_ui_layout_init();
  // Restore state
  clock_ui_set_date(last_year, last_month, last_day, last_weekday);
  clock_ui_set_time(last_hour, last_minute, last_second);

  ESP_LOGI(TAG, "Clock face mode changed to: %s",
           mode == CLOCK_FACE_TEXT ? "TEXT" : "OLD_FASHION");
}

void clock_face_toggle_mode(void) {
  if (current_mode == CLOCK_FACE_TEXT) {
    clock_face_set_mode(CLOCK_FACE_OLD_FASHION);
  } else {
    clock_face_set_mode(CLOCK_FACE_TEXT);
  }
}

void clock_ui_show(void) {
  lv_lock();
  if (lv_obj_has_flag(clock_container, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_clear_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(clock_container, LV_OPA_TRANSP, 0);

    // Fade in animation with ease-out (pops in)
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, clock_container);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_start(&a);
  }
  lv_unlock();
}

void clock_ui_hide(void) {
  lv_lock();
  if (!lv_obj_has_flag(clock_container, LV_OBJ_FLAG_HIDDEN)) {
    // Fade out animation with ease-in (fades away)
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, clock_container);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&a, anim_opa_cb);
    lv_anim_set_completed_cb(&a, anim_hide_ready_cb);
    lv_anim_start(&a);
  }
  lv_unlock();
}

void clock_ui_set_date(int year, int month, int day, int weekoftheday) {
  // Save state
  last_year = year;
  last_month = month;
  last_day = day;
  last_weekday = weekoftheday;

  // Skip if labels don't exist yet
  if (!date_label || !day_label)
    return;

  lv_lock();
  // Update date label - use different format based on mode
  char date_str[11];
  if (current_mode == CLOCK_FACE_TEXT) {
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", year, month, day);
  } else {
    // Shorter format for analog clock
    snprintf(date_str, sizeof(date_str), "%02d/%02d", month, day);
  }
  lv_label_set_text(date_label, date_str);

  // Update day of the week label
  if (weekoftheday >= 0 && weekoftheday < 7) {
    ESP_LOGI(TAG, "Clock day of week changed to: %d, %s", weekoftheday,
             days[weekoftheday]);
    lv_label_set_text(day_label, days[weekoftheday]);
  }
  lv_unlock();
}

void clock_ui_set_time(int hour, int minute, int second) {
  // Save state
  last_hour = hour;
  last_minute = minute;
  last_second = second;

  clock_time_t *time_data = malloc(sizeof(clock_time_t));
  if (time_data) {
    time_data->hour = hour;
    time_data->minute = minute;
    time_data->second = second;
    lv_async_call(clock_ui_update_async_cb, time_data);
  }
}
