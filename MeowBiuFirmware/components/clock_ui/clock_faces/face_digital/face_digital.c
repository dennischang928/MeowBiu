#include "face_digital.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *clock_container = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *day_label = NULL;

static const char *days[] = { "MONDAY", "TUESDAY",  "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY" };

static int last_hour = -1, last_minute = -1, last_second = -1;
static int last_year = -1, last_month = -1, last_day = -1, last_weekday = -1;

static void hide_digital(void) {
    if (!clock_container) return;
    lv_lock();
    lv_obj_add_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void show_digital(void) {
    if (!clock_container) return;
    lv_lock();
    lv_obj_remove_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void destroy_digital(void) {
    if (!clock_container) return;
    lv_lock();
    lv_obj_del(clock_container);
    clock_container = NULL;
    time_label = NULL;
    date_label = NULL;
    day_label = NULL;
    lv_unlock();
}

static void init_digital(void) {
    lv_lock();
    clock_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(clock_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(clock_container);
    lv_obj_set_style_bg_color(clock_container, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_border_width(clock_container, 0, 0);
    lv_obj_clear_flag(clock_container, LV_OBJ_FLAG_SCROLLABLE);

    // Time label (center)
    time_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, lv_color_make(255, 255, 255), 0);
    lv_label_set_text(time_label, "12:34:56");
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -20);

    // Date label (below time)
    date_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(date_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_opa(date_label, LV_OPA_80, 0);
    lv_label_set_text(date_label, "2024-01-01");
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 30);

    // Day label (bottom)
    day_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(day_label, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_text_opa(day_label, LV_OPA_70, 0);
    lv_label_set_text(day_label, "MONDAY");
    lv_obj_align(day_label, LV_ALIGN_CENTER, 0, 60);

    lv_obj_add_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}

static void set_time_digital(int hour, int minute, int second) {
    if (last_hour == hour && last_minute == minute && last_second == second)
        return;
    last_hour = hour;
    last_minute = minute;
    last_second = second;
    if (!clock_container || !time_label) return;
    int display_hour = hour % 12;
    display_hour = (display_hour == 0) ? 12 : display_hour;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", display_hour, minute, second);
    lv_label_set_text(time_label, buf);
}

static void set_date_digital(int year, int month, int day, int weekoftheday) {
    if (last_year == year && last_month == month && last_day == day && last_weekday == weekoftheday)
        return;
    last_year = year;
    last_month = month;
    last_day = day;
    last_weekday = weekoftheday;
    if (!clock_container || !date_label || !day_label) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    lv_label_set_text(date_label, buf);
    if (weekoftheday >= 0 && weekoftheday < 7)
        lv_label_set_text(day_label, days[weekoftheday]);
}

const clock_face_t face_digital = {
    .id = "digital",
    .name = "Digital",
    .init = init_digital,
    .show = show_digital,
    .hide = hide_digital,
    .destroy = destroy_digital,
    .set_time = set_time_digital,
    .set_date = set_date_digital,
    .weather_compatible = false,
    .set_weather = NULL,
};
