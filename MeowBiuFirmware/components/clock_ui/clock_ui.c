#include <stdio.h>
#include "clock_ui.h"
#include "esp_log.h"
#include "lvgl.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "clock_ui";

// LVGL objects for the clock UI
static lv_obj_t *clock_container = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *day_label = NULL;

// Days of the week
static const char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                             "Thursday", "Friday", "Saturday"};

void clock_ui_init(void)
{
}

void clock_ui_layout_init(void)
{
    lv_lock();
    // Initialization code for clock UI
    ESP_LOGI(TAG, "Clock UI initialized.");
    lv_obj_t *scr = lv_scr_act();

    // Create main container for clock
    clock_container = lv_obj_create(scr);
    lv_obj_set_size(clock_container, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(clock_container);
    lv_obj_set_style_bg_color(clock_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(clock_container, 0, 0);
    lv_obj_clear_flag(clock_container, LV_OBJ_FLAG_SCROLLABLE);

    // Create time label (HH:MM:SS)
    time_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xffffff), 0);
    lv_label_set_text(time_label, "00:00:00");
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -30);

    // Create date label
    date_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(date_label, lv_color_hex(0xaaaaaa), 0);
    lv_label_set_text(date_label, "2024-01-01");
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 30);

    // Create day of week label
    day_label = lv_label_create(clock_container);
    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(day_label, lv_color_hex(0x16c79a), 0);
    lv_label_set_text(day_label, "Monday");
    lv_obj_align(day_label, LV_ALIGN_CENTER, 0, 60);
    // Create a container for the clock UI
    lv_unlock();
}

void clock_ui_show(void)
{
    lv_lock();
    lv_obj_clear_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}
void clock_ui_hide(void)
{
    lv_lock();
    lv_obj_add_flag(clock_container, LV_OBJ_FLAG_HIDDEN);
    lv_unlock();
}


void clock_ui_set_time(int year, int month, int day, int weekoftheday)
{
    lv_lock();
    // Update date label
    char date_str[11];
    snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", year, month, day);
    lv_label_set_text(date_label, date_str);

    // Update day of the week label
    if (weekoftheday >= 0 && weekoftheday < 7)
    {
        lv_label_set_text(day_label, days[weekoftheday]);
    }
    lv_unlock();
}