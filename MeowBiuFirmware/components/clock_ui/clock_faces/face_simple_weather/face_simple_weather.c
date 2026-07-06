#include "face_simple_weather.h"
#include "network_service.h"
#include <stdio.h>

static lv_obj_t *container = NULL;
static lv_obj_t *label_time = NULL;
static lv_obj_t *label_day = NULL;
static lv_obj_t *label_temp = NULL;
static lv_obj_t *weather_icon = NULL;
static lv_obj_t *bottom_bar = NULL;

static void hide_simple_weather(void)
{
    if (!container)
        return;
    lv_lock();
    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void show_simple_weather(void)
{
    if (!container)
        return;
    lv_lock();
    lv_obj_remove_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void destroy_simple_weather(void)
{
    if (!container)
        return;
    lv_lock();
    lv_obj_del(container);
    container = NULL;
    label_time = NULL;
    label_day = NULL;
    label_temp = NULL;
    weather_icon = NULL;
    bottom_bar = NULL;
    lv_unlock();
}

static void init_simple_weather(void)
{
    lv_lock();

    // Main container - light blue background
    container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(container, 240, 240);
    lv_obj_set_style_bg_color(container, lv_color_make(150, 200, 240), 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(container);

    // TIME - Large, positioned higher to avoid top bezel
    label_time = lv_label_create(container);
    lv_obj_set_style_text_font(label_time, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(label_time, lv_color_make(60, 100, 140), 0);
    lv_label_set_text(label_time, "16:05");
    lv_obj_align(label_time, LV_ALIGN_CENTER, 0, -45);

    // DAY OF WEEK - uppercase, under time
    label_day = lv_label_create(container);
    lv_obj_set_style_text_font(label_day, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_day, lv_color_make(60, 100, 140), 0);
    lv_label_set_text(label_day, "WEDNESDAY");
    lv_obj_align(label_day, LV_ALIGN_CENTER, 0, -3);

    // BOTTOM BAR - weather info, smaller and higher to fit in circle
    bottom_bar = lv_obj_create(container);
    lv_obj_set_size(bottom_bar, 240, 100);
    lv_obj_set_style_bg_color(bottom_bar, lv_color_make(30, 30, 30), 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_pad_all(bottom_bar, 0, 0);
    lv_obj_set_style_radius(bottom_bar, 0, 0);
    lv_obj_clear_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);

    // Weather icon - yellow/green circle on left inside bottom_bar
    weather_icon = lv_obj_create(bottom_bar);
    lv_obj_set_size(weather_icon, 38, 38);
    lv_obj_set_style_bg_color(weather_icon, lv_color_make(180, 200, 120), 0);
    lv_obj_set_style_radius(weather_icon, 19, 0);
    lv_obj_set_style_border_width(weather_icon, 0, 0);
    lv_obj_align(weather_icon, LV_ALIGN_LEFT_MID, 50, 0);

    // Temperature - large, right side of bottom_bar
    label_temp = lv_label_create(bottom_bar);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_temp, lv_color_make(180, 210, 240), 0);
    lv_label_set_text(label_temp, "30.7");
    lv_obj_align(label_temp, LV_ALIGN_RIGHT_MID, -70, 0);

    lv_obj_add_flag(container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void set_time_simple_weather(int hour, int minute, int second)
{
    if (!label_time)
        return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    lv_lock();
    lv_label_set_text(label_time, buf);
    lv_unlock();
}

static void set_date_simple_weather(int year, int month, int day, int weekoftheday)
{
    if (!label_day)
        return;

    static const char *days[] = {"MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
                                 "FRIDAY", "SATURDAY", "SUNDAY"};
    const char *months[] = {"", "january", "february", "march", "april", "may", "june",
                            "july", "august", "september", "october", "november", "december"};

    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%s %d, %d", months[month], day, year);

    lv_lock();
    lv_label_set_text(label_day, days[weekoftheday]);
    lv_unlock();
}

static void set_weather_simple_weather(const weather_data_t *data)
{
    if (!data || !label_temp)
        return;

    char temp_buf[16];
    snprintf(temp_buf, sizeof(temp_buf), "%.1f", data->temperature_f);

    lv_lock();
    lv_label_set_text(label_temp, temp_buf);
    lv_unlock();
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
    .weather_compatible = true,
    .set_weather = set_weather_simple_weather,
};