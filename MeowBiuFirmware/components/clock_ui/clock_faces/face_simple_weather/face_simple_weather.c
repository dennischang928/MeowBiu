#include "face_simple_weather.h"
#include <stdio.h>

static lv_obj_t *container = NULL;
static lv_obj_t *label_hour = NULL;
static lv_obj_t *label_minute = NULL;
static lv_obj_t *label_date = NULL;
static lv_obj_t *label_temp = NULL;
static lv_obj_t *weather_icon = NULL;

static void hide_simple_weather(void) {
    if (!container) return;
    lv_lock();
    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void show_simple_weather(void) {
    if (!container) return;
    lv_lock();
    lv_obj_remove_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void destroy_simple_weather(void) {
    if (!container) return;
    lv_lock();
    lv_obj_del(container);
    container = NULL;
    label_hour = NULL;
    label_minute = NULL;
    label_date = NULL;
    label_temp = NULL;
    weather_icon = NULL;
    lv_unlock();
}

static void init_simple_weather(void) {
    lv_lock();
    container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(container, 240, 240);
    lv_obj_set_style_bg_color(container, lv_color_black(), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(container);

    // TIME BLOCK - Hour and minute as one visual unit
    // Hour Label - left side, moved down for better vertical balance
    label_hour = lv_label_create(container);
    lv_obj_set_style_text_font(label_hour, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_hour, lv_color_white(), 0);
    lv_label_set_text(label_hour, "14");
    lv_obj_set_pos(label_hour, 60, 80);

    // Minute Label - directly under hour, close spacing for unified time block
    label_minute = lv_label_create(container);
    lv_obj_set_style_text_font(label_minute, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_minute, lv_color_white(), 0);
    lv_label_set_text(label_minute, "37");
    lv_obj_set_pos(label_minute, 60, 120); // Only 40px gap - feels like one unit

    // SECONDARY INFO CLUSTER - Weather, date, temp grouped together on right
    // Weather icon - right side, separate from time block
    weather_icon = lv_obj_create(container);
    lv_obj_set_size(weather_icon, 24, 24);
    lv_obj_set_style_bg_color(weather_icon, lv_color_white(), 0); // placeholder for weather icon
    lv_obj_set_style_radius(weather_icon, 12, 0);
    lv_obj_set_pos(weather_icon, 170, 90); // Further right, moved down
    lv_obj_set_style_border_width(weather_icon, 0, 0);
    // TODO: Replace with actual weather icon image or custom draw

    // Date Label - directly under weather icon, compact cluster
    label_date = lv_label_create(container);
    lv_obj_set_style_text_font(label_date, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_date, lv_color_make(200, 200, 200), 0); // Silver for visual hierarchy
    lv_label_set_text(label_date, "12.10");
    lv_obj_set_pos(label_date, 170, 120); // Right-aligned with weather icon

    // Temperature Label - under date, smaller font so it doesn't compete with time
    label_temp = lv_label_create(container);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_14, 0); // Reduced from 24 to 14
    lv_obj_set_style_text_color(label_temp, lv_color_make(200, 200, 200), 0); // Silver for visual hierarchy
    lv_label_set_text(label_temp, "24°C");
    lv_obj_set_pos(label_temp, 170, 145); // Right-aligned, compact cluster

    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void set_time_simple_weather(int hour, int minute, int second) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02d", hour);
    lv_label_set_text(label_hour, buf);
    snprintf(buf, sizeof(buf), "%02d", minute);
    lv_label_set_text(label_minute, buf);
}

static void set_date_simple_weather(int year, int month, int day, int weekoftheday) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d.%02d", month, day);
    lv_label_set_text(label_date, buf);
}

// Dummy set_weather for temperature; replace with real data as needed
static void set_weather_simple_weather(int temp_c) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d°C", temp_c);
    lv_label_set_text(label_temp, buf);
}

const clock_face_t face_simple_weather = {
    .id = "simple_weather",
    .name = "Simple Weather",
    .init = init_simple_weather,
    .show = show_simple_weather,
    .hide = hide_simple_weather,
    .destroy = destroy_simple_weather,
    .set_time = set_time_simple_weather,
    .set_date = set_date_simple_weather,
};
